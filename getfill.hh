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

class Filler: Thread {
	int oneSecondLen;
	int prevFill;
	map<string,vector<string> > filler; 
	vector<strings> fillPlaylist;
protected:
	void Execute(void *arg);
public:
	Util* util;

	Filler();

	bool maybeSabbath(time_t playTime);
	string latestNews(bool& refresh);
	strings getFiller(int maxLen, string& fillname);
};
