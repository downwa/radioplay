#ifndef __OUTSCREENHH__
#define __OUTSCREENHH__


#include <curses.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "thread.hh"
//#include "play3abn.h"
#include "outscreen_saver.hh"
#include "outscreen_input.hh"

class OutScreen: public Out {
	FILE *tty;
	SCREEN *mainscr;
	WINDOW *mainwnd;
	int screenx,screeny; // Number of pixels in the x,y directions
	int calibrate; // 0=calibrated,1=calibrate (top/left), 2=calibrate (top/right), 3=calibrate (bottom/right), 4=calibrate (bottom/left)
	
	void fullscr_msg(const char* line1, const char* line2, const char* line3);
	void calibrate_msg(int xx, int yy, const char* msg1, const char* msg2);
	bool DoCalibrate();
	void sleepWaitInput(int seconds);
	void Execute(void* arg);
public:
	OutScreenSaver *saver;
	OutScreenInput *input;
	OutScreen(const char* ttypath, Util* util);
	~OutScreen();
	void attrOn(unsigned int attr);
	void attrOff(unsigned int attr);
	virtual int screenWrite(unsigned int yy, unsigned int xx, const char *msg);
	void Shutdown();
};

#endif