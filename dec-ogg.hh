// #include "libs/tremor/ivorbiscodec.h"
// #include "libs/tremor/ivorbisfile.h"
#include <tremor/ivorbiscodec.h>
#include <tremor/ivorbisfile.h>
#include "wave.hh"
#include "thread.hh"

#include <semaphore.h>

#include "utildefs.hh"

class DecodeOgg: Thread {
	pthread_mutex_t closing;
	sem_t newFile;
	Util* util;
	DWORD nchannels;
	DWORD samplerate;
	DWORD needBufferSize;
	DWORD trackSampleCount;
	float seclen;
	int pipefd[2];
	const char *filename;
	int seeksecs;
	OggVorbis_File vf;
	bool vfInit;
	char* data;
	FILE *oggin;
	FILE *output;
	
	void init(Util *util);
protected:
	void Execute(void *arg);
public:
	int errorCode;
	
	DecodeOgg() { init(new Util("dec-ogg")); }
	DecodeOgg(Util* util) { init(util); }
	bool isValid(const char *filename);
	bool isValid(const char *filename, DWORD& nchannels, DWORD& samplerate, DWORD& trackSampleCount, float& seclen);
	FILE *Open(const char *filename, int seeksecs); /** Output from FILE* is raw PCM data **/
	void Finish(int code);
	float SecLen(const char *filename); /** Returns length of file in seconds **/
};
