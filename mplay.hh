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
//#include "getsched.hh"

class Player: Thread {
	FILE *schedRpy;
	Taudioq* audioq;
	Decoder* decoder;
//	SrcFill* srcfill;
	int oneSecondLen;
	bool tripleRate;
	int testVol;
	snd_pcm_sframes_t frames;
	int curChannels;
	int curHz;
	int fd_out;
	snd_pcm_t* handle;
	long totalPlaySec;
/*	int prevFill;
	map<string,vector<string> > filler; 
	vector<strings> fillPlaylist;
*/
	
#ifndef ARM	
  pa_sample_spec ss;
  pa_simple *sa;
#endif
protected:
	void Execute(void *arg);
public:
	Util* util;

	Player();

	int set_volume(int mixer_fd, int left, int right, int ctl);
	int get_volume(int mixer_fd, int &left, int &right, int ctl);
	int setVol(int vol);
	int incVol(int inc);

/*	bool maybeSabbath(time_t playTime);
	string latestNews(bool& refresh);
*/
	strings getScheduled();
	void initScheduled();
	void write_sinewave(int sample_rate, int fileindex);
	int open_audio_device_ex(const char *name, int mode, int channels, int sample_rate);
	int open_audio_device(const char *name, int mode);
	snd_pcm_t *openALSA(int channels, int sample_rate);
	int openTestAudio(const char *name_out);
	void WriteAudio(char *curaudio, int len);
	void Playback();
};
