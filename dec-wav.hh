#include "wavereader.hh"

#include "utildefs.hh"

class DecodeWav {
	Util* util;
	DWORD nchannels;
	DWORD samplerate;
	DWORD trackSampleCount;
	float seclen;

	void init(Util *util);
public:
	DecodeWav() { init(new Util("dec-wav")); }
	DecodeWav(Util* util) { init(util); }
	bool isValid(const char *filename);
	bool isValid(const char *filename, DWORD& nchannels, DWORD& samplerate, DWORD& trackSampleCount, float& seclen);
	FILE *Open(const char *filename, int seeksecs); /** Output from FILE* is raw PCM data **/
	float SecLen(const char *filename); /** Returns length of file in seconds **/
};
