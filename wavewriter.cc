#include "wavewriter.hh"
#include "utildefs.hh"

void WaveWriter::WriteHeader(const char *filename, FILE *fp, WORD nChannels, DWORD samplerate, DWORD sampleCount) {
	if(!fp) { return; }
	const char *err="";
	WaveChunkType wavChunk;
	WORD blockAlign=2;
	DWORD dataSize=(nChannels*sampleCount*blockAlign);
	
	memcpy(wavChunk.a.ckID,"RIFF",4);
	wavChunk.a.cksize=dataSize+36;
	memcpy(wavChunk.a.WAVEID,"WAVE",4);
	if(fwrite(&wavChunk, sizeof(wavChunk.a), 1, fp)<1) { err="Short Write #1."; goto end; }
	memcpy(wavChunk.b.ckID,"fmt ",4);
	wavChunk.b.cksize=sizeof(wavChunk.b)-8;
	wavChunk.b.sampleDataFormat=1;
	wavChunk.b.nChannels=nChannels;
	wavChunk.b.samplesPerSecond=samplerate;
	wavChunk.b.avgNBytesPerSecond=blockAlign*nChannels*samplerate;
	wavChunk.b.blockAlign=blockAlign;
	wavChunk.b.sigBitsPerSample=blockAlign*8;
	if(fwrite(&wavChunk, sizeof(wavChunk.b), 1, fp)<1) { err="Short Write #2."; goto end; }
	memcpy(wavChunk.c.ckID,"data",4);
	wavChunk.c.cksize=dataSize;
	if(fwrite(&wavChunk, 8, 1, fp)<1) { err="Short Write #3."; goto end; }
	return;
end:
	fclose(fp);
	syslog(LOG_ERR,"WriteHeader: %s: %s",filename,err);
}