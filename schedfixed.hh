#include <pcre.h>

#include <sys/types.h>
#include <utime.h>

#include <fstream>
#include <iostream>

#include "maindec.hh"

#include "thread.hh"
#include "util.hh"

#include "tschedline.hh"

#include <algorithm>
#include <vector>
#include <string>
#include <map>

#include "utildefs.hh"

//#include "linux/ioprio.h"

#ifndef ARM
#ifndef RPI
		extern int sys_ioprio_set(int, int, int);
		extern int sys_ioprio_get(int, int);
			
		#if defined(__i386__)
		#define __NR_ioprio_set		289
		#define __NR_ioprio_get		290
		#elif defined(__ppc__)
		#define __NR_ioprio_set		273
		#define __NR_ioprio_get		274
		#elif defined(__x86_64__)
		#define __NR_ioprio_set		251
		#define __NR_ioprio_get		252
		#elif defined(__ia64__)
		#define __NR_ioprio_set		1274
		#define __NR_ioprio_get		1275
		#else
		#error "Unsupported arch"
		#endif

		static inline int ioprio_set(int which, int who, int ioprio)
		{
			return syscall(__NR_ioprio_set, which, who, ioprio);
		}


		#define IOPRIO_CLASS_SHIFT      (13)
		#define IOPRIO_PRIO_VALUE(class, data)  (((class) << IOPRIO_CLASS_SHIFT) | data)

		enum {
						IOPRIO_CLASS_NONE,
						IOPRIO_CLASS_RT,
						IOPRIO_CLASS_BE,
						IOPRIO_CLASS_IDLE,
		};

		/*
		* 8 best effort priority levels are supported
		*/
		#define IOPRIO_BE_NR    (8)

		enum {
						IOPRIO_WHO_PROCESS = 1,
						IOPRIO_WHO_PGRP,
						IOPRIO_WHO_USER,
		};
#endif
#endif

#include <sys/time.h>
#include <sys/resource.h>


#define NOPEGCPU 250000

using namespace std;

class SchedFixed: Thread {
	Util* util;
	Decoder* decoder;
	map<string, unsigned int> curProgramIndex; // Index of currently playing program in list (used to ensure we don't get stuck if there are duplicate files in list)
	map<string, string> curProgram; // Program name, Path of playing file
	//char prgcur[1024]; // Path of currently selected file
	int prevFill;
	static const char* playq;
	int chooseCount; // Number of items chosen during this run
	map<string,vector<string> > codes;
	map<string,vector<string> > schedules;
	map<string,vector<string> > programs;
protected:
	void Scheduler();
	void ScheduleDay(time_t schedNow, time_t now);
	void ScheduleBufferX(string outbuffer);
public:
	static string lencachepath;

	SchedFixed();
};
