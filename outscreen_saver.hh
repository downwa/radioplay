#ifndef __OUTSCREEN_SAVER__
#define __OUTSCREEN_SAVER__

#include "thread.hh"

#include "out.hh"
#include "util.hh"

#ifdef ARM
#include "ezfb.h"
#endif

#define SAVEDELAY 60

class OutScreenSaver: Thread {
	Out* out;
	Util* util;
	static int tty_mode_was;
	static struct ezfb fb;
	
	void Execute(void* arg);
public:
	//OutScreenSaver() { }
	void backLight(int onOff /* 1=on, 0=off*/);
	void screenOnOff(int onOff);
	void screenReset();
	void screenSave();
	OutScreenSaver(Util* util, Out* display) { this->util=util; this->out=display; Start(NULL,1024); }
	void ShowScreen();
	void HideScreen();
};

#endif