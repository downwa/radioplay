#ifndef __WAVEREADERHH__
#define __WAVEREADERHH__

#include <string.h>
#include <stdio.h>

#include "wave.hh"
#include "util.hh"

class WaveReader {
	Util* util;
public:
	WaveReader(Util* util) { this->util=util; }
	FILE *readWaveHeader(const char *filename, DWORD& nchannels, DWORD& samplerate, DWORD& trackSampleCount);
};

#endif