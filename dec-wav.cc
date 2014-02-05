#include "util.hh"
#include "dec-wav.hh"

void DecodeWav::init(Util *util) {
	samplerate=nchannels=trackSampleCount=0;
	seclen=0; this->util=util;
}

bool DecodeWav::isValid(const char *filename) {
	return isValid(filename,nchannels,samplerate,trackSampleCount,seclen);
}

bool DecodeWav::isValid(const char *filename, DWORD& nchannels, DWORD& samplerate, DWORD& trackSampleCount, float& seclen) {
	seclen=SecLen(filename);
	nchannels=this->nchannels;
	samplerate=this->samplerate;
	trackSampleCount=this->trackSampleCount;
	return seclen>=0;
}

FILE *DecodeWav::Open(const char *filename, int seeksecs) {
	WaveReader* wr=new WaveReader(util);
	FILE* fp=wr->readWaveHeader(filename,nchannels, samplerate, trackSampleCount);
	if(!fp) { syslog(LOG_ERR,"MainDecoder::Decode: readWaveHeader failed: %s",filename); return NULL; }
	return fp;
}

float DecodeWav::SecLen(const char *filename) {
	if(!filename) { return -4; }
	WaveReader* wr=new WaveReader(util);
	FILE* fp=wr->readWaveHeader(filename,nchannels, samplerate, trackSampleCount);
	if(!fp) { return -1; }
	fclose(fp);
	return ((float)trackSampleCount)/((float)samplerate);
}