#include "src-fill.hh"

#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h> /* Definition of AT_* constants */
#include <unistd.h>
#include <string>
#include <map>
#include <vector>
#include <alsa/asoundlib.h>
#include <sys/soundcard.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <utime.h>

#ifndef ARMx
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <pulse/pulseaudio.h>
#include <pulse/sample.h>
#endif

#include "thread.hh"
#include "util.hh"
#include "maindec.hh"

class Scheduled: Thread {
	FILE *fillsReq;
	FILE *fillsRpy;
  char playqdir[1024];
	int oneSecondLen;
	int prevFill;
	vector<strings> fillPlaylist;
protected:
	void Execute(void *arg);
public:
	Util* util;

	Scheduled();

	void initFiller();
	strings getFiller(int maxLen);
	strings getScheduled(bool& isFiller, time_t now);
};
