#include "wavereader.hh"
#include "utildefs.hh"

FILE *WaveReader::readWaveHeader(const char *filename, DWORD& nchannels, DWORD& samplerate, DWORD& trackSampleCount) {
	WaveChunkType wavChunk;
	WORD blockAlign;
	const char *err="";
	struct stat statbuf;
	errno=0;
	FILE *fp=NULL;
	if(stat(filename, &statbuf)==-1 || statbuf.st_size==0) { err=(errno==0)?"Empty file.":strerror(errno); goto end; }
	fp=fopen(filename,"rb");
	if(!fp) { err=strerror(errno); goto end; }
	if(fread(&wavChunk, sizeof(wavChunk.a), 1, fp)<1) { err="Short Read #1."; goto end; }
	if(memcmp(wavChunk.a.ckID,"RIFF",4)!=0) { err="Not RIFF."; goto end; }
	if((DWORD)statbuf.st_size != (wavChunk.a.cksize+8)) { err="Corrupt file #1."; goto end; }
	if(memcmp(wavChunk.a.WAVEID,"WAVE",4)!=0) { err="Not WAVE."; goto end; }
	if(fread(&wavChunk, sizeof(wavChunk.b), 1, fp)<1) { err="Short Read #2."; goto end; }
	if(memcmp(wavChunk.b.ckID,"fmt ",4)!=0) { err="Missing 'fmt ' chunk."; goto end; }
	if(wavChunk.b.sampleDataFormat!=1) { err="Not PCM Format."; goto end; }
	if(wavChunk.b.nChannels>2) { err="Too many channels."; goto end; }
	if(wavChunk.b.sigBitsPerSample!=16 || (wavChunk.b.blockAlign/wavChunk.b.nChannels)!=2) {
		//syslog(LOG_ERR,"sigBitsPerSample=%d,blockAlign=%d",wavChunk.b.sigBitsPerSample,wavChunk.b.blockAlign);
		err="Not 16 bit PCM."; goto end; }
	if(wavChunk.b.cksize!=(sizeof(wavChunk.b)-8)) { err="Unrecognized 'fmt ' chunk."; goto end; }
	nchannels=wavChunk.b.nChannels;
	samplerate=wavChunk.b.samplesPerSecond;
	blockAlign=wavChunk.b.blockAlign;
	if(fread(&wavChunk, 8, 1, fp)<1) { err="Short Read #3."; goto end; }
	if(memcmp(wavChunk.c.ckID,"data",4)!=0) { err="Expected 'data' chunk."; goto end; }
	if((DWORD)statbuf.st_size != (wavChunk.c.cksize+44)) { err="Corrupt file #2."; goto end; }
	trackSampleCount=wavChunk.c.cksize/blockAlign;
	return fp;
end:
	if(fp) { fclose(fp); fp=NULL; }
	syslog(LOG_ERR,"readWaveHeader: %s: %s",filename,err);
	trackSampleCount=0;
	return NULL;
}
