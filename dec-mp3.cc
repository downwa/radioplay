#include "util.hh"
#include "dec-mp3.hh"

void DecodeMp3::init(Util *util) {
	samplerate=nchannels=trackSampleCount=0;
	seclen=0; this->util=util;
}

bool DecodeMp3::isValid(const char *filename) {
	return isValid(filename,nchannels,samplerate,trackSampleCount,seclen);
}

bool DecodeMp3::isValid(const char *filename, DWORD& nchannels, DWORD& samplerate, DWORD& trackSampleCount, float& seclen) {
	seclen=SecLen(filename);
	nchannels=this->nchannels;
	samplerate=this->samplerate;
	trackSampleCount=this->trackSampleCount;
	return seclen>=0;
}

/** NOTE: Output from returned FILE* is raw PCM data **/
FILE *DecodeMp3::Open(const char *filename, int seeksecs) {
	char cmd[1024];
	snprintf(cmd,sizeof(cmd),"madplay --output=raw:- --bit-depth=16 --start=%d \"%s\" 2>%s/madplay.log.txt",seeksecs,filename,Util::LOGSPATH);
	return popen(cmd,"r");
}

float DecodeMp3::SecLen(const char *filename) {
	char cmd[1024];
	char out[128]={0};
	struct stat statbuf;
	if(!filename || stat(filename, &statbuf)==-1 || statbuf.st_size==0) {  return -1; }
	snprintf(cmd,sizeof(cmd),"madplay --output=null:null -v --start=0 --time=0.00001 \"%s\" 2>&1",filename);
	/** NOTE: Example output
MPEG Audio Decoder 0.15.2 (beta) - Copyright (C) 2000-2004 Robert Leslie et al.
error: frame 0: lost synchronization
 00:00:00 Layer II, 418 kbps, 44100 Hz, stereo, CRC
1 frame decoded (0:00:00.0), -inf dB peak amplitude, 0 clipped samples
	***/
	//****************
	FILE *fp=popen(cmd,"r"); // e=close on exec, useful for multithreaded programs
	if(!fp) { syslog(LOG_ERR,"DecodeMp3::SecLen: %s: %s",cmd,strerror(errno)); return -2; }
	const char *kbps=NULL;
	while(fp && !feof(fp) && fgets(out,sizeof(out),fp)) {
		if(strncmp(out,"error: frame",12)==0) { pclose(fp); return -3; } // Not a valid MP3 file!
		if(strncmp(out," 00:00:00 Layer",15)==0) { kbps=strchr(out,','); break; }
	}
	pclose(fp);
	if(!kbps) { return -4; }
	//****************
	out[127]=0; // Just to be safe
	int datalen=statbuf.st_size-4096;
	kbps+=2;
	int bitrate=atoi(kbps);
	const char *hz=strchr(kbps,',');
	if(!hz) { return -5; }
	hz+=2;
	samplerate=atoi(hz);
	const char *chan=strstr(hz,"stereo");
	nchannels=chan?2:1;
	seclen=datalen/(bitrate*(1000.0/8));
	trackSampleCount=seclen*samplerate;
	//syslog(LOG_ERR,"seclen=%5.2f",seclen);
	return seclen;
}