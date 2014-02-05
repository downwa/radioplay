#ifndef __WAVEWRITERHH__
#define __WAVEWRITERHH__

#include <string.h>
#include <stdio.h>

#include "wave.hh"
#include "util.hh"

class WaveWriter {
	Util* util;
public:
	WaveWriter(Util* util) { this->util=util; }
	void WriteHeader(const char *filename, FILE *fp, WORD nChannels, DWORD samplerate, DWORD sampleCount);
};

#endif