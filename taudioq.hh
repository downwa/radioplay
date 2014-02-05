#include <queue>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "tqnode.hh"
#include "util.hh"
//#include "utildefs.hh"


using namespace std;

class Taudioq {
	Util* util;
	queue<TQNode> aq;
	pthread_mutex_t qmut;
	TQNode overflow;
	volatile bool overflowed;
public:
	Taudioq(Util* util);
	void enq(char *data, size_t datalen, long fileindex, long blockindex, int samplerate=SAMPLE_RATE, int nchannels=NUM_CHANNELS, string fname="", long trackSampleCount=0, bool doBoost=false, SAMPLE maxsamp=0, SAMPLE amaxsamp=0);
	TQNode deq(int expectSize);
};
