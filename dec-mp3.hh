#include "wave.hh"

#include "utildefs.hh"

class DecodeMp3 {
	Util* util;
	DWORD nchannels;
	DWORD samplerate;
	DWORD trackSampleCount;
	float seclen;

	void init(Util *util);
public:
	DecodeMp3() { init(new Util("dec-mp3")); }
	DecodeMp3(Util* util) { init(util); }
	bool isValid(const char *filename);
	bool isValid(const char *filename, DWORD& nchannels, DWORD& samplerate, DWORD& trackSampleCount, float& seclen);
	FILE *Open(const char *filename, int seeksecs); /** Output from FILE* is raw PCM data **/
	float SecLen(const char *filename); /** Returns length of file in seconds **/
};
