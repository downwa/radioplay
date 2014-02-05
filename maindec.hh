#ifndef __MAINDECHH__
#define __MAINDECHH__

#include "taudioq.hh"

#include "util.hh"
#include "dec-ogg.hh"
#include "dec-mp3.hh"
#include "dec-wav.hh"

//#include "utildefs.hh"
#define ADJLEN 10 /* Number of seconds over which to determine adjustment value */
#define FADEINMAX 5
#define FADEOUTMAX 5

class Decoder {
	vector<SAMPLE> maxsamps;
	int fadein;
	Util* util;
	DecodeMp3* decmp3;
	DecodeOgg* decogg;
	DecodeWav* decwav;
	volatile bool isMp3;
	volatile bool isOgg;
	volatile bool isWav;
	DWORD nchannels;
	DWORD samplerate;
	DWORD trackSampleCount;
	const char *filename;
	int seeksecs;
	float seclen;
	Taudioq* audioq;

	void init(Taudioq* audioq, Util *util);
public:
	volatile int blockindex;

	Decoder(Taudioq* audioq) { init(audioq,new Util("maindec")); }
	Decoder(Taudioq* audioq, Util* util) { init(audioq,util); }
	int Decode(const char *filename, int seeksecs);
	int Enqueue(FILE *pcmin, int seeksecs); /** Reads raw pcm data and queues it for playback **/
	float SecLen(const char *filename);
	bool fixAmplitude(SAMPLE *samples,int numSamples, bool doBoost, SAMPLE &maxsamp, SAMPLE &amaxsamp);
};

#endif
