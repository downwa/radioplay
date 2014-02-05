#ifndef __TQNODEHH__
#define __TQNODEHH__

#include <string>

using namespace std;

#ifdef RPIx
#define SAMPLE_RATE 48000
#else
#define SAMPLE_RATE 16000
#endif

#define NUM_CHANNELS 1
typedef short SAMPLE;

class TQNode {
	public:
	long fileindex;
	long blockindex;
	long trackSampleCount;
	int samplerate;
	int nchannels;
	int datalen;
	bool doBoost;
	SAMPLE maxsamp,amaxsamp;
	string fname;
	char *data;

	inline TQNode() {
		data=NULL; datalen=0; doBoost=false; fileindex=blockindex=trackSampleCount=0; 
		samplerate=nchannels=0; maxsamp=amaxsamp=0; fname="";
	}
	inline TQNode(char *data, size_t datalen, long fileindex, long blockindex, int samplerate=SAMPLE_RATE, int nchannels=NUM_CHANNELS, string fname="", long trackSampleCount=0, bool doBoost=false, SAMPLE maxsamp=0, SAMPLE amaxsamp=0) {
		this->data=data; this->datalen=datalen; this->fileindex=fileindex; this->blockindex=blockindex;
		this->samplerate=samplerate; this->nchannels=nchannels; this->fname=fname; this->trackSampleCount=trackSampleCount;
		this->doBoost=doBoost; this->maxsamp=maxsamp; this->amaxsamp=amaxsamp;
	}
	//~TQNode() { delete data; }
};

#endif
