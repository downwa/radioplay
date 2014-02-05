#ifndef __OUTSCREENINPUTHH__
#define __OUTSCREENINPUTHH__

#include <curses.h>
#include "thread.hh"

#include "out.hh"
#include "util.hh"

extern pthread_cond_t      cond;
extern pthread_mutex_t     mutex;

#ifdef ARM
#include <tslib.h>
#endif

#define KEYUP 1
#define KEYDN 2
#define KEYRT 3
#define KEYLT 4

class OutScreenInput: Thread {
	friend class OutScreen;
	Util* util;
	WINDOW *mainwnd;
	Out* out;
	int savedigit[10];
	bool isEsc;
	int mouseEsc;
	int cx,cy;
	volatile int maxx,maxy;			// Number of characters in the x,y directions

#ifdef ARM
	time_t prevTouchTime;
	time_t wakeTime;
	suseconds_t prevus;
	struct ts_sample dragSamp;
	bool dragging;
	volatile int screenx,screeny; // Number of pixels in the x,y directions
	volatile int calibrate; // 0=calibrated,1=calibrate (top/left), 2=calibrate (top/right), 3=calibrate (bottom/right), 4=calibrate (bottom/left)
	int scrollx;
	long elapsedSamples;
	volatile long decBytes;
#endif
	
	void NotifyInput();
	void Execute(void* arg);
public:
	void arrowKey(int dir);
	OutScreenInput(Util* util, WINDOW *wnd, Out* actor);
	~OutScreenInput();
};

#endif // __OUTSCREENINPUTHH__
