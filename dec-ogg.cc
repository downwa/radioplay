#include "util.hh"
#include "dec-ogg.hh"

void DecodeOgg::init(Util *util) {
	filename=NULL; seeksecs=0; errorCode=0;
	samplerate=nchannels=trackSampleCount=0;
	seclen=0; this->util=util; data=NULL; oggin=output=NULL; pipefd[0]=pipefd[1]=-1;
	if(pthread_mutex_init(&closing, NULL)<0) { syslog(LOG_ERR,"DecodeOgg:init:pthread_mutex_init#2: %s",strerror(errno)); exit(1); }
	Start(); // Start and wait for open event
}

bool DecodeOgg::isValid(const char *filename) {
	return isValid(filename,nchannels,samplerate,trackSampleCount,seclen);
}

bool DecodeOgg::isValid(const char *filename, DWORD& nchannels, DWORD& samplerate, DWORD& trackSampleCount, float& seclen) {
	seclen=SecLen(filename);
	nchannels=this->nchannels;
	samplerate=this->samplerate;
	trackSampleCount=this->trackSampleCount;
	return seclen>=0;
}

float DecodeOgg::SecLen(const char *filename) {
  FILE *oggin=fopen(filename,"rb");
  if(!oggin) { return -1; }
  return 3600;
//#error Stack Smashing Detected when we run the below ov_open
MARK
	OggVorbis_File vf;
	memset(&vf,0,sizeof(vf));
MARK
	if(ov_open(oggin, &vf, NULL, 0) < 0) { fclose(oggin); return -3; }
MARK
	trackSampleCount=(long)ov_pcm_total(&vf,-1); // Number of samples per track.
MARK
	vorbis_info *vi=ov_info(&vf,-1);
MARK
	nchannels=vi->channels;
	samplerate=vi->rate;
	seclen=((float)trackSampleCount)/((float)vi->rate);
MARK
	ov_clear(&vf); // Cleanup
MARK
	return seclen;
}

FILE *DecodeOgg::Open(const char *filename, int seeksecs) {
	if(pipe(pipefd) == -1) { syslog(LOG_ERR,"DecodeOgg::Open: pipe: %s",strerror(errno)); return NULL; }
	this->filename=filename;
	this->seeksecs=seeksecs;
	struct stat statbuf;
	if(!filename || stat(filename, &statbuf)==-1 || statbuf.st_size==0) { Finish(-1); return NULL; }
  oggin=fopen(filename,"rb");
  if(!oggin) { syslog(LOG_ERR,"DecodeOgg::Decode: %s: %s",filename,strerror(errno)); Finish(-1); return NULL; }
	//OggVorbis_File vf;
	if(ov_open(oggin, &vf, NULL, 0) < 0) {
		syslog(LOG_ERR,"DecodeOgg::Decode: Input does not appear to be an Ogg bitstream: %s",filename);
		Finish(-2); return NULL;
	}
	vfInit=true;
	int debug=util->getInt("debug",0);
	char **ptr=ov_comment(&vf,-1)->user_comments;
	//int seclen=0;

	while(*ptr){
		char *comment=*ptr;
		if(debug) { syslog(LOG_ERR,"comment: %s",comment); }
		++ptr;
		//if(strncmp(comment,"seclen=",7)==0) { seclen=atoi(&comment[7]); }
	}
	long trackSampleCount=(long)ov_pcm_total(&vf,-1);
	vorbis_info *vi=ov_info(&vf,-1);
	//int secs=trackSampleCount/vi->rate;
	if(seeksecs>0) {

		trackSampleCount-=(seeksecs*vi->rate); // Reduce by this many samples per second of seek
		ogg_int64_t pos=seeksecs*vi->channels*vi->rate;
		int code=ov_pcm_seek(&vf,pos);
		if(code) {
			char *msg=(char *)"Unknown error.";
			switch(code) {
				case OV_ENOSEEK:  msg=(char *)"Bitstream is not seekable"; break;
				case OV_EINVAL:   msg=(char *)"Invalid argument value"; break;
				case OV_EREAD:    msg=(char *)"A read from media returned an error"; break;
				case OV_EFAULT:   msg=(char *)"Internal logic fault; indicates a bug or heap/stack corruption"; break;
				case OV_EBADLINK: msg=(char *)"Invalid stream section supplied to libvorbisidec, or the requested link is corrupt"; break;
			}
			syslog(LOG_ERR,"DecodeOgg::Decode: Seek failure: %s: in file %s,seeksecs=%d",msg,filename,seeksecs);
			Finish(-3); return NULL;
		}
	}
	nchannels=vi->channels;
	samplerate=vi->rate;
	needBufferSize=nchannels*samplerate*sizeof(SAMPLE);
	data=new char[needBufferSize];	
	if(!data) { syslog(LOG_ERR,"dec-ogg: memory failure: %s",strerror(errno)); Finish(-4); return NULL; }
	output=fdopen(pipefd[1], "w");
	return fdopen(pipefd[0], "r");
}
void DecodeOgg::Finish(int code) {
	pthread_mutex_lock(&closing);
	if(oggin && !vfInit) { fclose(oggin); }
	if(output) { fclose(output); output=NULL; syslog(LOG_INFO,"FINISH OUTPUT#1"); } // Reader will see EOF
	errorCode=code;
	if(vfInit) {
		ov_clear(&vf); /* Cleanup */
		vfInit=false;
	}
	if(data) { 	delete data; data=NULL; }
	oggin=NULL;
	if(pipefd[1]>=0) close(pipefd[1]); // May return error if fclose already did the job (above) but we can safely ignore it 
	pipefd[0]=pipefd[1]=-1;
	pthread_mutex_unlock(&closing);
}

void DecodeOgg::Execute(void *arg) {
	while(!Util::checkStop()) {
		while(output==NULL || data==NULL) { sleep(1); }
		int eof=0;
		int current_section;
		int bufferLeft=0;
		while(!eof) {
			if(bufferLeft==0) { bufferLeft=needBufferSize; }
			int bufferSize=bufferLeft;
			int ofs=0;

			while(bufferLeft>0 /*&& !doExit*/) {
				long ret=ov_read(&vf,&data[ofs],bufferLeft,&current_section);
				if (ret == 0) { eof=1; break; }
				else if (ret < 0) { /* error in the stream.  Not a problem, just reporting it in case we (the app) cares.  In this case, we don't. */
				}
				else { /* we don't bother dealing with sample rate changes, etc, but you'll have to*/
					//fwrite(&data[ofs],1,ret,output);
					ofs+=ret; bufferLeft-=ret;
				}
			}
			if(fwrite(data,bufferSize-bufferLeft,1,output)<1) { eof=1; break; }
		}
		Finish(0);
	}
}

#ifdef TESTHARNESS
int main(int argc, char **argv) {
	if(argc<3) { Util *util=new Util("dec-ogg"); syslog(LOG_ERR,"Usage: dec-ogg [filename] [seeksecs]"); exit(1); }
	return (new DecodeOgg())->Decode(argv[1],atoi(argv[2]));
}
#endif
