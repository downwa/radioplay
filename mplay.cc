//#FIXME crash at "397" see crash.txt but it doesn't show the Decode msg just before crash

#include "mplay.hh"
#define EXECDELAY 100000 /* 0.1 seconds */

Player::Player() {
	tripleRate=false;
	testVol=3; // up to 10
	curChannels=curHz=0;
	fd_out=-1;
	totalPlaySec=0L;
	util=new Util("mplay#1"); // Need separate util instance for each thread (to avoid locking issues)
	syslog(LOG_ERR,"Player: CLOCKSKEW=%d",CLOCKSKEW);
MARK
	util->removeDirFiles(Util::TEMPPATH);
MARK
	syslog(LOG_ERR,"tzofs=%ld,timezone=%ld,zone=%s",Util::tzofs,timezone,Util::zone);
	setIT("silentfill",0);
MARK
	audioq=new Taudioq(new Util("mplay#2"));
MARK
	decoder=new Decoder(audioq);
MARK
	initScheduled();
MARK
}

//----------------------- VOLUME CONTROL ---------------------------
int Player::set_volume(int mixer_fd, int left, int right, int ctl) {
#ifdef ARM
	syslog(LOG_DEBUG,"set_volume(%d,%d):ctl=%s",left,right,ctl==0?"SOUND_MIXER_WRITE_VOLUME":"SOUND_MIXER_WRITE_PCM");
  int vol = 0;
  /* make sure left and right are in the range 0-100 */
  left = (left < 0) ? 0 : (left > 100) ? 100 : left;
  right = (right < 0) ? 0 : (right > 100) ? 100 : right;
  /* left channel uses the low byte, and the right uses the high byte */
  vol = left | (right << 8);
  return ioctl(mixer_fd, ctl==0?SOUND_MIXER_WRITE_VOLUME:SOUND_MIXER_WRITE_PCM, &vol);
#else
	syslog(LOG_DEBUG,"set_volume: Skipping volume set on non-ARM platform.");
	return 0;
#endif
}

#define LEFT_CHANNEL_VOLUME(v) ((v) & 0xff)
#define RIGHT_CHANNEL_VOLUME(v) (((v) & 0xff00)>> 8)
int Player::get_volume(int mixer_fd, int &left, int &right, int ctl) {
  int vol;
  int ret=ioctl(mixer_fd, ctl==0?SOUND_MIXER_READ_VOLUME:SOUND_MIXER_READ_PCM, &vol);
  left = LEFT_CHANNEL_VOLUME(vol);
  right = RIGHT_CHANNEL_VOLUME(vol);
  //syslog(LOG_INFO,"Left (%d%%) Right(%d%%)", left, right);
  return ret;
}
int Player::setVol(int vol) {
#ifndef ARM
	return vol; // For now, don't bother setting volume except on actual platform
#endif
  int fd = open("/dev/mixer", O_RDWR);
  if (fd == -1) { syslog(LOG_ERR,"setVol:Cannot open mixer: %s",strerror(errno)); sleep(1); return vol; }
	if(vol==0) { vol=80; }
	if(vol<30) { vol=30; }
	else if(vol>100) { vol=100; }
  if(set_volume(fd,vol,vol,0)==-1) { syslog(LOG_ERR,"setVol:set_volume: %s",strerror(errno)); sleep(1); return vol; }
  //exit(0);
	setI("volume",vol);
	close(fd);
	return vol;
}
int Player::incVol(int inc) {
	int vol=getId("volume",50);
	vol=setVol(vol+inc);
	return vol;
}
/*******************************************************************/
void Player::initScheduled() {
	syslog(LOG_INFO,"Scheduled::initScheduled");
	if(schedRpy) { fclose(schedRpy); }
	schedRpy=popen("./getsched","r");
	if(!schedRpy) { syslog(LOG_ERR,"./getsched failed: %s",strerror(errno)); exit(1); }
}

strings Player::getScheduled() {
  syslog(LOG_INFO,"getScheduled");
  if(!schedRpy) { initScheduled(); }
  char buf[1024];
  char *rpy=fgets(buf, sizeof(buf), schedRpy);
  if(!rpy) {
    syslog(LOG_ERR,"schedRpy: %s",strerror(errno));
    initScheduled();
  }
  int len=strlen(rpy);
  if(rpy[len-1]=='\n' || rpy[len-1]=='\r') { rpy[len-1]=0; }
  char *secLen=NULL;
  char *dispname=NULL;
  char *path=NULL;

  errno = 0;

  secLen=buf;
  char *pp=strchr(buf,'|');
  if(pp) { *pp=0; pp++; dispname=pp; }
  pp=strchr(dispname,'|');
  if(pp) { *pp=0; pp++; path=pp; }
  syslog(LOG_INFO,"getScheduled: secLen=%s,dispname=%s,path=%s",secLen,dispname,path);
    //printf("secLen=%d,dispname=%s,path=%s\n",secLen,dispname,path);
  // NOTE:    infs.one=2014-01-22 19:00:00 -1
  // NOTE:    infs.two=BIBLESTORIES 1377 /tmp/play3abn/cache/Radio/Bible stories/Vol 01/V1-06a  The Disappearing Idols.ogg
  strings ent(Util::fmtTime(0)+" -1 "+string(dispname),"FIXMECAT "+string(secLen)+" "+string(path)); // Unscheduled, age one year, include length and
  //fillname=dispname;
  return ent;
}

/** NOTE: Read a directory of links of the following form:
 * "2011-06-09 12:40:00 0066 IT'S A WONDERFUL DAY-Heritage Singers.ogg"
 * Each link specifies the date, time, length, and display name of a file to play.
 * The target of the link is the actual file to play.
 */
void Player::Execute(void *arg) {
	int count=0;
        char playqdir[1024];
        snprintf(playqdir,sizeof(playqdir),"%s/playq",Util::TEMPPATH);
	syslog(LOG_ERR,"Player::Execute: Starting playback loop");
	while(!Util::checkStop()) {
		count++;
		syslog(LOG_ERR,"mplay:Player::Execute: count=%d",count);
		strings ent=getScheduled();
		incVol(0); // Set volume from variable
		if(getIT("pause1")) { sleep(2); continue; }
MARK
		/** Parse info string **/
		time_t playAt=0;
		int seclen=0;
		string dispname="";
		int flag=0;
		string catcode="",url="";
		int result=util->itemDecode(ent, playAt, flag, seclen, catcode, dispname, url);
		string playlink=string(playqdir)+"/"+ent.one;
		const char *playlinkS=playlink.c_str();
		if(result<0) {
			syslog(LOG_ERR,"Player::Execute: Invalid format1 (result=%d): %s => %s",result,ent.one.c_str(),ent.two.c_str());
			remove(playlinkS); continue;
		}
		time_t now=time(NULL);
		int elapsed=(now-playAt);
		const char *pname=ent.one.c_str();
		const char *ppath=url.c_str();
		syslog(LOG_ERR,"1seclen=%d,one=%s,two=%s",seclen,pname,ppath);		
/** PLAY IT only if found **/
		struct stat statbuf;
		if(stat(ppath, &statbuf)==-1) {
MARK
			syslog(LOG_ERR,"Player::Execute: Missing: %s.",ppath);
			if(playlink.length()>0) { remove(playlinkS); }
			continue;
		} // Non-existent
		
		//syslog(LOG_ERR,"Player::Execute: %s %04d %s",Util::fmtTime(playAt).c_str(),seclen,dispname.c_str());

		/** Copy to temp dir so filesystem can be unmounted while playing (for switching program sources) **/
		char playtemp[1024];
		strings ent2=util->itemEncode(playAt, flag, seclen, catcode, url);
		snprintf(playtemp,sizeof(playtemp),"%s/playtmp-%s %s",	Util::TEMPPATH,ent2.one.c_str(),ent2.two.c_str());
MARK
		util->copyFile(ppath,playtemp);
// FIXME BELOW LINES ARE THEY NEEDED?
//		strncpy(prevfile,ppath,sizeof(prevfile));
		bool isFilling=false; // FIXME
//		setIT("isfilling",isFilling);
		setIT("flag",flag);
MARK
/** FIXME Crashed in decode.  Why? **/
		int seekSecs=elapsed<30?0:elapsed;
		syslog(LOG_ERR,"Decode(playtemp=%s,seekSecs=%d)",playtemp,seekSecs);
		fflush(stderr);
MARK
		decoder->Decode(playtemp,seekSecs);
MARK
		syslog(LOG_ERR,"Decode Finished(playtemp=%s,seekSecs=%d)",playtemp,seekSecs);
MARK
		utime(ppath,NULL); // Mark file as recently played
		
		if(flag==366) { // Flag this as coming from schedfixed:: need to save current item
			char catcode[32];
			snprintf(catcode,sizeof(catcode)-1,"%s",dispname.c_str());
			char *pp=strchr(catcode,' ');
			if(pp) { *pp=0; }
			pp=strchr(catcode,':');
			if(pp) { *pp=0; }
			char cpCode[64];
			snprintf(cpCode,sizeof(cpCode),"curProgram-%s",catcode);
			util->set(cpCode,ppath);
			
			snprintf(cpCode,sizeof(cpCode),"curProgramIndex-%s",catcode);
			util->inc(cpCode);
		}
				
		/** Clean up after playback **/
MARK
		remove(playtemp);
MARK
		// Remove link to the file (unless -Filler.ogg)
		if(!isFilling && playlink.length()>0) {
			syslog(LOG_INFO,"REMOVING playlink=%s",playlinkS);
			remove(playlinkS);
		}
		else { syslog(LOG_INFO,"NOT removing playlink=%s (isFilling=%d)",playlinkS,isFilling); }
MARK
	}
MARK
}

void Player::write_sinewave(int sample_rate, int fileindex) {
  static unsigned int phase = 0;	/* Phase of the sine wave */
  unsigned int p;
  int i;
  short buf[1024];		/* 1024 samples/write is a safe choice */
  int outsz = sizeof (buf) / 2;
#ifndef ARM
	sample_rate=sample_rate*2;
#endif
  static int sinebuf[48] = {
    0, 4276, 8480, 12539, 16383, 19947, 23169, 25995,    28377, 30272, 31650, 32486, 32767, 32486, 31650, 30272,
    28377, 25995, 23169, 19947, 16383, 12539, 8480, 4276,    0, -4276, -8480, -12539, -16383, -19947, -23169, -25995,
    -28377, -30272, -31650, -32486, -32767, -32486, -31650, -30272,    -28377, -25995, -23169, -19947, -16383, -12539, -8480, -4276
  };
  for (i = 0; i < outsz; i++) {
		p = (phase * sample_rate) / SAMPLE_RATE;
		phase = (phase + 1) % 4800;
		buf[i] = sinebuf[p % 32]*testVol/50;
  }
  char *enqbuf=new char[sizeof(buf)];
	if(enqbuf) {
		memcpy(enqbuf,buf,sizeof(buf));
		audioq->enq(enqbuf,sizeof(buf),fileindex,decoder->blockindex++);
	}
	else { syslog(LOG_ERR,"Player::write_sinewave: malloc error: %s",strerror(errno)); }
}

int Player::open_audio_device_ex(const char *name, int mode, int channels, int sample_rate) {
	int bits=AFMT_S16_NE;		/* Native 16 bits */
#ifndef ARM
	sa=NULL;
#endif
	int fd=-2;
#ifndef ARM
        int err;
        if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
                syslog(LOG_ERR,"Playback open error#1: %s", snd_strerror(err));
                abort();
        }
        if ((err = snd_pcm_set_params(handle, // TRY ALSA
                                      SND_PCM_FORMAT_S16_LE,
                                      SND_PCM_ACCESS_RW_INTERLEAVED,
                                      channels, // channels
                                      sample_rate, // rate
                                      1, // allow resampling
                                      5000000)) < 0) {   /* 0.5sec latency */
                syslog(LOG_ERR,"Playback open error#2: %s", snd_strerror(err));
                abort();
        }
	curChannels=channels; curHz=sample_rate;
	goto success;
#else
	bool firstTry=true;
	int tries=0;
retry:
	if(firstTry) {
		firstTry=false;
		if((fd=open(name, mode, 0)) == -1) {
		syslog(LOG_ERR,"open_audio_device_ex: Unable to open %s: %s",name,strerror(errno));
		sleep(1);
		if(tries++<30) { goto retry; }
		return -1;
	}
	if(ioctl(fd, SNDCTL_DSP_SETFMT, &bits) == -1) { syslog(LOG_ERR,"open_audio_device_ex: %s: SNDCTL_DSP_SETFMT",strerror(errno)); abort(); }
	if(bits != AFMT_S16_NE) { syslog(LOG_ERR,"open_audio_device_ex: The device doesn't support the 16 bit sample format."); abort(); }
	int req=channels;
	if(ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) == -1) { syslog(LOG_ERR,"open_audio_device_ex: %s: SNDCTL_DSP_CHANNELS",strerror(errno)); abort(); }
	if(channels != req) { syslog(LOG_ERR,"The device doesn't support the requested number of channels (%d).",req); abort(); }
	curChannels=channels;
	if(ioctl (fd, SNDCTL_DSP_SPEED, &sample_rate) == -1) { syslog(LOG_ERR,"open_audio_device_ex: %s: SNDCTL_DSP_SPEED",strerror(errno)); abort(); }
	curHz=sample_rate;
#endif
/*			ss.format = PA_SAMPLE_S16LE;
			ss.rate = sample_rate;
			ss.channels = channels;// ss;
			// Create a new portaudio playback stream
			int error;
			if (!(sa=pa_simple_new(NULL, "play3abn", PA_STREAM_PLAYBACK, NULL, "mplay", &ss, NULL, NULL, &error))) {
				syslog(LOG_ERR,"pa_simple_new() failed: %s", pa_strerror(error));
				abort();
			}
			curChannels=channels;
			curHz=sample_rate;
			syslog(LOG_ERR,"PulseAudio playback using %s: %d bits,%d channel%s,%d Hz",name,ss.format==3?16:0,channels,channels==1?"":"s",sample_rate);
			return -2;
*/
success:
	syslog(LOG_ERR,"Audio playback using %s: %d bits,%d channel%s,%d Hz",name,bits,channels,channels==1?"":"s",curHz);
	return fd;
}

int Player::open_audio_device(const char *name, int mode) {
	return open_audio_device_ex(name,mode,NUM_CHANNELS,SAMPLE_RATE);
}

int Player::openTestAudio(const char *name_out) {
	if(!name_out) { name_out = "/dev/dsp"; }
syslog(LOG_ERR,"Calling open_audio_device from openTestAudio");
	fd_out=open_audio_device(name_out, O_WRONLY); // Was O_RDWR but fails on ARM
	if(fd_out==-1) return fd_out;
	int fileindex=0;
	setIT("fileindex",fileindex);
	setST("playfile","");
	setIT("playsec",0);
	setIT("curplaylen",0);
	
	for(int xa=0; xa<2; xa++) { write_sinewave(SAMPLE_RATE,fileindex); }
	testVol=0;
	for(int xa=0; xa<6; xa++) { write_sinewave(SAMPLE_RATE,fileindex); }
	char buf[curHz*2];
	char *enqbuf=new char[sizeof(buf)];
	if(enqbuf) { // Fill buffer with enough silence to trigger playback readiness
		memset(enqbuf,0,sizeof(buf));
		audioq->enq(enqbuf,sizeof(buf),fileindex++,decoder->blockindex++);
		if(system("ls -ld /proc/$(pidof mplay.bin)/fd/* >/tmp/mplay.err")<0) { syslog(LOG_ERR,"ls(pid)#1: %s",strerror(errno)); }
		util->setInt("fileindex",fileindex);
	}
	else { syslog(LOG_ERR,"test audio2:malloc error: %s",strerror(errno)); }
	return fd_out;
}

void Player::WriteAudio(char *curaudio, int len) {
	if(len==0) { return; } // Nothing to write!
	int ret=len;
MARK	
//  int error;
MARK	
	int olen=len;
	int divisor=20;
	if(fd_out==-2) { divisor=1; }
	len=oneSecondLen/divisor;
	for(int xa=0; xa<olen; xa+=len) {
		if(fd_out!=-2) {
	MARK	
			//syslog(LOG_ERR,"WriteAudio: fd_out=%d,write(len=%d)",fd_out,len);
			ret=write(fd_out, &curaudio[xa], len);
	MARK	
		}
		else {
#ifndef ARM
//			syslog(LOG_ERR,"WriteAudio: len=%d",len);
/*			if(sa) {
			    if(pa_simple_write(sa, &curaudio[xa], (size_t) len, &error) < 0) {
	MARK	
				syslog(LOG_ERR,"WriteAudio: pa_simple_write() failed: sa=%d,error=%d: %s (curaudio=%08x,len=%d)",
							 (int)sa,error,pa_strerror(error),(unsigned int)curaudio,len);
	MARK	
				sleep(1);
				syslog(LOG_ERR,"WriteAudio: exiting");
				exit(-1);
			    }
			    syslog(LOG_ERR,"WriteAudio: pa_simple_write(len=%d) FINISHED",len);
			}
			else {
*/
        			frames = snd_pcm_writei(handle, &curaudio[xa], len/2);
                		if (frames < 0) frames = snd_pcm_recover(handle, frames, 0);
                		if (frames < 0) {
                        		syslog(LOG_ERR,"WriteAudio: snd_pcm_writei failed: %s", snd_strerror(errno));
					//snd_pcm_close(handle);
					//fd_out=open_audio_device_ex("/dev/dsp", O_WRONLY, 1, curHz);
					//if(fd_out==-1) { exit(1); }
                        		//break;
					abort();
                		}
                		if (frames > 0 && frames < (long)(len/2)) syslog(LOG_ERR,"Short write (expected %li, wrote %li)\n", (long)len/2, frames);
//			}
#endif	
		}
	}
MARK
	if(ret==-1) { syslog(LOG_ERR,"Write error: %s",strerror(errno)); Util::Stop(); }
	else if(ret<len) { syslog(LOG_ERR,"Short write: %d < %d",ret,len); }
MARK
}

void Player::Playback() {
	//syslog(LOG_ERR,"Player::Playback");
	if(openTestAudio(NULL)==-1) { exit(1); }
	Start(NULL);
	//----------------- BOOST THREAD PRIORITY ---------------
	struct sched_param sp;
	memset(&sp, 0, sizeof(sp));
	sp.sched_priority=sched_get_priority_max(SCHED_RR); // SCHED_FIFO, SCHED_RR
	int rc=pthread_setschedparam(pthread_self(), SCHED_RR, &sp);
	if(rc!=0) { syslog(LOG_ERR,"pthread_setschedparam: error %d",rc); }
	//----------------- BOOST PROCESS PRIORITY ---------------
	if(setpriority(PRIO_PROCESS, 0, -19)<0) { /*perror("setpriority"); sleep(1); */ }
	//*****************************************************************
	char *prevData=NULL;
	int prevDataLen=0;
	while(!Util::checkStop()) {
MARK
		incVol(0);
		if(getIT("pause2")) { sleep(2); continue; }
		oneSecondLen=curHz*curChannels*sizeof(SAMPLE);
		char curaudio[oneSecondLen];
		int left=oneSecondLen-prevDataLen;
		int ofs=0;
MARK
		if(prevDataLen) {
MARK
			memcpy(&curaudio[ofs],prevData,prevDataLen);
MARK
			delete prevData; 
MARK
			prevData=NULL; prevDataLen=0;
		}
MARK
		bool reopen=false;
		int nchannels=0,samplerate=0;
		for(ofs=prevDataLen; left>0 && !Util::checkStop();) {
MARK
			TQNode node=audioq->deq(left);

#ifdef RPIx
			if(node.samplerate == 16000) { node.samplerate=48000; tripleRate=true; }
#endif

MARK
			setIT("amaxsamp",node.amaxsamp);
			//setIT("amaxsamp",node.amaxsamp);
			reopen=node.samplerate!=curHz || node.nchannels!=curChannels;
if(reopen) syslog(LOG_ERR,"Setting reopen because node.samplerate=%d != curHz=%d OR node.nchannels=%d != curChannels=%d in Playback()", node.samplerate,curHz,node.nchannels,curChannels);
MARK
			if(reopen) { // Need to re-open device at new rate
MARK
				nchannels=node.nchannels; samplerate=node.samplerate;
				prevData=node.data; prevDataLen=node.datalen;
				memset(&curaudio[ofs],0,left); ofs=oneSecondLen; left=0; // Fill old second with zeroes
MARK
				break;
			}
MARK
	//syslog(LOG_ERR,"curaudio=%ld,ofs=%ld,data=%ld,writelen=%d,len=%d",curaudio,ofs,node.data,node.datalen,oneSecondLen);
			memcpy(&curaudio[ofs],node.data,node.datalen); ofs+=node.datalen; left-=node.datalen;
MARK
			delete node.data; node.data=NULL;
MARK
		}
MARK
		//checkAmplitude((SAMPLE *)curaudio,oneSecondLen/sizeof(SAMPLE));: NOTE: SEE Decoder::fixAmplitude in maindec.cc
		WriteAudio(curaudio, ofs);
MARK
		if(reopen) {
			if(fd_out>=0) {
				close(fd_out);
			}
			else { snd_pcm_close(handle); }
syslog(LOG_ERR,"Calling open_audio_device from PlayBack (reopen) after calling WriteAudio");
			fd_out=open_audio_device_ex("/dev/dsp", O_WRONLY, nchannels, samplerate);
			if(fd_out==-1) { exit(1); }
		}
MARK
		totalPlaySec++;
	}
#ifndef ARM
	  /* Make sure that every single portaudio sample was played */
//	int error;
//  if(sa && pa_simple_drain(sa, &error) < 0) {
//    syslog(LOG_ERR,"Playback: pa_simple_drain() failed: %s", pa_strerror(error));
//    abort();
//  }
//  if (sa) { pa_simple_free(sa); }
#endif
}

int main(int argc, char **argv) {
	syslog(LOG_ERR,"uid=%d,euid=%d.  Waiting 10 seconds to begin playback.",getuid(),geteuid());
	//sleep(10);
	//while(true) { }
	Player* player=new Player();
	player->Playback();
}
