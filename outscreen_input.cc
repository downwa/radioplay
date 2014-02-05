//#include "play3abn.h"

#include "outscreen_input.hh"
#define EXECDELAY 100000

pthread_cond_t      cond  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t     mutex = PTHREAD_MUTEX_INITIALIZER;

void OutScreenInput::NotifyInput() {
	int rc;
  if((rc=pthread_mutex_lock(&mutex)) != 0) { syslog(LOG_ERR,"NotifyInput: pthread_mutex_lock: rc=%d",rc); }
  if((rc=pthread_cond_signal(&cond)) != 0) { syslog(LOG_ERR,"NotifyInput: pthread_cond_signal: rc=%d",rc); }
  if((rc=pthread_mutex_unlock(&mutex)) != 0) { syslog(LOG_ERR,"NotifyInput: pthread_mutex_unlock: rc=%d",rc); }
}

#ifdef ARM

void OutScreenInput::Execute(void* arg) {
MARK
	int debug=util->getInt("debug",0);
retry:
	syslog(LOG_ERR,"Opening touchscreen");
  struct tsdev *ts=ts_open("/dev/input/ts0",0); // /dev/input/ts0
  if(!ts) { syslog(LOG_ERR,"Error:touchScreen:ts_open:%s",strerror(errno)); return; }
	syslog(LOG_ERR,"Configuring touchscreen");
  if(ts_config(ts)) { syslog(LOG_ERR,"Error:touchScreen:ts_config:%s",strerror(errno)); return; }
	syslog(LOG_ERR,"Reading touchscreen");
MARK
	time_t pressTime=0;
	time_t pressDiff=0;
  while(!Util::checkStop()) {
MARK
    struct ts_sample samp;
    int ret=ts_read(ts, &samp, 1);
		getmaxyx(stdscr,maxy,maxx);
MARK
    if(ret < 0) { syslog(LOG_ERR,"Error:touchScreen:ts_read:%s",strerror(errno)); sleep(1); goto retry; }
MARK
    if(ret != 1) { continue; }
		calibrate=util->getInt("calibrate",1);
    
		if(samp.pressure==1 && pressTime==0) {
			pressTime=samp.tv.tv_sec;
			if(debug) {
				syslog(LOG_ERR,"AT %ld.%06ld: x=%6d y=%6d pressure=%6d,calibrate=%d",
					samp.tv.tv_sec, samp.tv.tv_usec, samp.x, samp.y, samp.pressure,calibrate);
			}
		}
		else if(samp.pressure==0) {
			pressDiff=samp.tv.tv_sec-pressTime; pressTime=0;
		}
//     if(samp.pressure==0) { syslog(LOG_ERR,"AT %ld.%06ld: x=%6d y=%6d pressure=%6d,calibrate=%d",
//  		 samp.tv.tv_sec, samp.tv.tv_usec, samp.x, samp.y, samp.pressure,calibrate); }

//#ERROR Positive x,y are getting transformed to negative!
// 2011/03/03 02:58:25 (+10800)::AT 1299153505.920247: x=   234 y=   316 pressure=     0
// 2011/03/03 02:58:25 (+10800)::1299153505.920247:   -316   -234      0,decBytes=0,sampSec=0,extraSamps=0,cx=0,cy=0,dcx=0,dcy=0

    
    
MARK
		int saving=getIT("saving");
		if(saving) { setIT("saving",0); wakeTime=time(NULL); continue; } // Do not register the touch which canceled the screen saver
		else { setIT("saveCounter",SAVEDELAY); }
MARK
		if(samp.tv.tv_sec-wakeTime < 2) { continue; } // Do not register a touch within a second of screen saver cancel
MARK
		//*************** NORMALIZE **************
		//int gx=samp.x+24;   // Range 0-54
		//int gy=samp.y-247;  // Range 0-101
		//int cx=gx*100/180;  // Range 0-29
		//int cy=gy*100/510;  // Range 0-19
		// RANGES: x=0..248, y=0..323
		screenx=0;
		screeny=0;
		if(pressDiff>10) {
			pressDiff=0;
			setI("calibrate",calibrate=1);
			remove("vars/swapxy.txt");
			remove("vars/sx1.txt");
			remove("vars/sx2.txt");
			remove("vars/sx3.txt");
			remove("vars/sx4.txt");
			remove("vars/sy1.txt");
			remove("vars/sy2.txt");
			remove("vars/sy3.txt");
			remove("vars/sy4.txt");
			continue;
		}
		if     (calibrate==1 && samp.pressure==0) { setI("sx1",samp.x); setI("sy1",samp.y); setI("calibrate",calibrate=2); continue; }
		else if(calibrate==2 && samp.pressure==0) { setI("sx2",samp.x); setI("sy2",samp.y); setI("calibrate",calibrate=3); continue; }
		else if(calibrate==3 && samp.pressure==0) { setI("sx3",samp.x); setI("sy3",samp.y); setI("calibrate",calibrate=4); continue; }
		else if(calibrate==4 && samp.pressure==0) { setI("sx4",samp.x); setI("sy4",samp.y); setI("calibrate",calibrate=0); continue; }
		
//		normalize(&samp,);
//void normalize(struct ts_sample* samp) {
//}
		if(calibrate!=0) { continue; } // cannot handle non-calibrated samples from here on
		/******** NORMALIZE SAMPLE if calibrated ***********/
MARK
calc:
		int sx1=util->getInt("sx1");
		int sy1=util->getInt("sy1");
		int sx2=util->getInt("sx2");
		int sy2=util->getInt("sy2");
		int sx3=util->getInt("sx3");
		int sy3=util->getInt("sy3");
		int sx4=util->getInt("sx4");
		int sy4=util->getInt("sy4");
		int dx1=abs(sx2-sx1); // Difference between screen x positions should be screenx
		int dx2=abs(sx3-sx4);
		int dy1=abs(sy3-sy2); // Difference between screen y positions should be screeny
		int dy2=abs(sy4-sy1);
		int adx=abs(dx1-dx2);
		int ady=abs(dy1-dy2);
		int swapxy=util->getInt("swapxy");
MARK
		if(!swapxy && (dx1<30 || dy1<30)) { // Should have significant width, else x/y may be swapped
MARK
			setI("sx1",sy1); setI("sy1",sx1); // Swap x/y
			setI("sx2",sy2); setI("sy2",sx2); // Swap x/y
			setI("sx3",sy3); setI("sy3",sx3); // Swap x/y
			setI("sx4",sy4); setI("sy4",sx4); // Swap x/y
			setI("swapxy",1);
			goto calc;
		}
MARK
		if(adx>30 || ady>30) { setI("calibrate",calibrate=1); } // Retry calibration if difference between widths is skewed (screen should be rectangular)
		screenx=(dx1+dx2)/2;
		screeny=(dy1+dy2)/2;
MARK
		if(swapxy) {
			int tmp=samp.x;
			samp.x=samp.y;
			samp.y=tmp;
		}
MARK
		if(sx2>sx1) { samp.x-=sx1; } // Subtract smallest x
		else { samp.x=sx1-samp.x; }		 // else subtract from largest x
		if(sy4>sy1) { samp.y-=sy1; } // Subtract smallest y
		else { samp.y=sy1-samp.y; }		 // else subtract from largest y
MARK
		/*************************************/
		///////////
		int nx=100*screenx/maxx; // x pixels per char
		if(nx==0) { syslog(LOG_ERR,"INVALID nx=0: screenx=%d,maxx=%d",screenx,maxx); nx=1; }
MARK
		int ny=100*screeny/maxy; // y pixels per char
		if(ny==0) { syslog(LOG_ERR,"INVALID ny=0: screeny=%d,maxy=%d",screeny,maxy); ny=1; }
MARK
		cx=samp.x*100/nx; //826;  // e.g. x=0..248
MARK
		cy=samp.y*100/ny; //1620; // e.g. y=0..323
MARK
		int dcx=dragSamp.x*100/nx; //826;  // e.g. x=0..248
MARK
		int dcy=dragSamp.y*100/ny; //1620; // e.g. y=0..323
MARK
    if(samp.pressure==0) {
			dragging=false; scrollx=0;
			int sampSec=0;
			long extraSamps=0;
			time_t secs=0;
			//int diff=samp.tv.tv_usec-prevus;
MARK
			if(prevTouchTime) {
MARK
				secs=samp.tv.tv_sec-prevTouchTime;
			  sampSec=elapsedSamples/secs;
				long expectedSamps=secs*16000;
				extraSamps=elapsedSamples-expectedSamps;
				prevus=samp.tv.tv_usec;
			}
MARK
MARK
			syslog(LOG_INFO,"%ld.%06ld: %6d %6d %6d,decBytes=%ld,sampSec=%d,extraSamps=%ld,cx=%d,cy=%d,dcx=%d,dcy=%d,ny=%d,screeny=%d,maxy=%d",
				samp.tv.tv_sec, samp.tv.tv_usec, samp.x, samp.y, samp.pressure,decBytes,sampSec,extraSamps,cx,cy,dcx,dcy,ny,screeny,maxy);
MARK
			int rownum=cy;
			int detailStatus=getIT("detailStatus");
			int detailRow=getIT("detailRow");
			int detailCount=getIT("detailCount");
			if(detailStatus && cy>detailRow) { rownum-=detailCount; }
MARK
			out->rowAction(rownum,cx,true);
MARK
    }
		else { // if(samp.pressure==0) {
			syslog(LOG_INFO,"Mouse down: cx=%d,cy=%d,pressure=%d",cx,cy,samp.pressure);
			out->rowSelect(cy,cx);
			
MARK
#define LISTROW 5 /* Starting row for displayed playlist */			
			if(cy>=LISTROW) {
				if(!dragging) {
					dragging=true;
					dragSamp=samp;
				}
				else {
					scrollx=(dcx-cx)*2;
					if(scrollx<0) { scrollx=0; }
					//syslog(LOG_ERR,"scrollx=%d",scrollx);
				}
MARK
			}
MARK
		}
MARK
		NotifyInput();
  }
	syslog(LOG_ERR,"touchScreen:EXITED");
}

OutScreenInput::OutScreenInput(Util* util, WINDOW *wnd, Out* actor) {
/** VmSize=3908 **/
	this->util=util;
	getmaxyx(stdscr,maxy,maxx);
	calibrate=util->getInt("calibrate",1);
	cx=cy=0;
	out=actor;
	mainwnd=wnd;
	noecho();
	cbreak();
	// Initialize variables
	prevTouchTime=0;
	wakeTime=0;
	prevus=0;
	dragging=false;
/** VmSize=3908 **/
	// Start main thread
	Start(NULL);
/** VmSize=3908, then 12280  **/	
}

/** ************************************************************************************************** **/
/** ************************************************************************************************** **/
/** ************************************************************************************************** **/
/** ************************************************************************************************** **/

#else

OutScreenInput::OutScreenInput(Util* util, WINDOW *wnd, Out* actor) {
	this->util=util;
	cx=cy=0;
	isEsc=false;
	mouseEsc=0;
	out=actor;
	mainwnd=wnd;
	noecho();
	cbreak();
	//nodelay(mainwnd, true);
	Start(NULL);
}

/* INFO: Mouse Down:
2011/03/03 01:52:38 (+10800)::ch=27,
011/03/03 01:52:38 (+10800)::ch=91,[
2011/03/03 01:52:38 (+10800)::ch=77,M
2011/03/03 01:52:38 (+10800)::ch=32,
2011/03/03 01:52:38 (+10800)::ch=42,*
2011/03/03 01:52:38 (+10800)::ch=34,"
*/
/* INFO: Mouse Up:
2011/03/03 01:52:41 (+10800)::ch=27,
011/03/03 01:52:41 (+10800)::ch=91,[
2011/03/03 01:52:41 (+10800)::ch=77,M
2011/03/03 01:52:41 (+10800)::ch=35,#
2011/03/03 01:52:41 (+10800)::ch=42,*
2011/03/03 01:52:41 (+10800)::ch=34,"
*/

void OutScreenInput::arrowKey(int dir) { // 1=UP,2=DN,3=RT,4=LT
	int setdate=getITd("setdate",0);
	int menurow=getITd("menurow",0);
	syslog(LOG_INFO,"arrowKey(%d): setdate=%d,menurow=%d",dir,setdate,menurow);
	if(setdate>0) {
		if(dir==KEYLT) {
			revCycleITd("setdate",7,1); // Cycle 7 values, 0..6
			return;
		}
		if(dir==KEYRT) {
			cycleITd("setdate",7,1); // Cycle 7 values, 0..6
			return;
		}
		int xo=0;
		if(setdate==1) { xo=7;  }
		if(setdate==2) { xo=11; }
		if(setdate==3) { xo=14; }
		if(setdate==4) { xo=17; }
		if(setdate==5) { xo=20; }
		if(setdate==6) { xo=23; }
		int ox=0;
		if(dir==KEYUP) { xo+=(setdate==1)?2:1; }
		int isWide=maxx>=80;
		if(isWide) { ox++; }
		out->rowAction(2,ox+xo,true);
	}
	else {
		if(dir==KEYUP) {
			revCycleITd("menurow",15,0); // Reverse cycle 15 values, 0..14
			return;
		}
		if(dir==KEYDN) {
			cycleITd("menurow",15,0); // Cycle 15 values, 0..14
			return;
		}
	}
}

void OutScreenInput::Execute(void* arg) {
/** VmSize=12280 already (not 3908) **/	
	//util->longsleep(3600);	
	
	syslog(LOG_ERR,"OutScreenInput:Execute:Handling input...");
	int digitCount=0;
	time_t lastCharAt=time(NULL);
	int fastCount=0;
	while(!Util::checkStop()) {
		int ch=wgetch(mainwnd);
		getmaxyx(stdscr,maxy,maxx);
	syslog(LOG_ERR,"IN ch=%d,%c,isEsc=%d",ch,ch>31?ch:' ',isEsc);
		time_t charAt=time(NULL);
		if((charAt-lastCharAt) < 1) { fastCount++; }
		else { fastCount=0; }
		if((charAt-lastCharAt) > 2) { isEsc=false; } // Too long waiting for escape sequence
		if(fastCount>10) { 
			fastCount=0; syslog(LOG_ERR,"Too many characters");
			if(usleep(EXECDELAY)==-1) { sleep(1); }
		}
		lastCharAt=charAt;
		int debug=util->getInt("debug",0);
		if(!isEsc) {
			if(debug) { syslog(LOG_ERR,"ch=%d,%c",ch,ch>31?ch:' '); }
			if(ch==' ') { out->rowAction(1,1,true); }
			if(ch==409) { isEsc=true; mouseEsc=0; continue; } // Get the rest of the sequence
			if(ch==27) { isEsc=true; mouseEsc=0; continue; } // Get the rest of the sequence
			if(ch=='q') { out->QuickExit(); }
			if(ch==10) {
				int menurow=getITd("menurow",0);
				if(menurow>0) {
					out->rowAction(1+menurow,1,true);
				}
			}
			if(isdigit(ch)) {
				savedigit[digitCount]=(ch-'0');
				digitCount++;
			}
			else { digitCount=0; }
			if(digitCount==2) {
				digitCount=0;
				int rowaction=(10*savedigit[0])+savedigit[1];
				out->rowAction(rowaction,1,true);
			}
		}
		else { // !isEsc
			bool mouseDown=false;
			bool mouseUp=false;
			if(mouseEsc==0 && ch=='[') { mouseEsc=1; }
			else if(mouseEsc==1 && ch=='A') { mouseEsc=0; isEsc=false; arrowKey(KEYUP); }
			else if(mouseEsc==1 && ch=='B') { mouseEsc=0; isEsc=false; arrowKey(KEYDN); }
			else if(mouseEsc==1 && ch=='C') { mouseEsc=0; isEsc=false; arrowKey(KEYRT); }
			else if(mouseEsc==1 && ch=='D') { mouseEsc=0; isEsc=false; arrowKey(KEYLT); }
			else if(mouseEsc==1 && ch=='M') { mouseEsc=2; }
			else if(mouseEsc==2 && ch==32) { mouseEsc=3; } // Mouse Up
			else if(mouseEsc==3) { cx=(ch-33); mouseEsc=4; } // X position
			else if(mouseEsc==4) { cy=(ch-33); mouseEsc=0; isEsc=false; mouseDown=true; }
			else if(mouseEsc==2 && ch=='#') { mouseEsc=5; } // Mouse Up
			else if(mouseEsc==5) { cx=(ch-33); mouseEsc=6; } // X position
			else if(mouseEsc==6) { cy=(ch-33); mouseEsc=0; isEsc=false; mouseUp=true; }
			if(mouseUp) {
				if(debug) { syslog(LOG_ERR,"Mouse up: cx=%d,cy=%d",cx,cy); }
				int rownum=cy;
				int detailStatus=getIT("detailStatus");
				int detailRow=getIT("detailRow");
				int detailCount=getIT("detailCount");
				if(detailStatus && cy>detailRow) { rownum-=detailCount; }
				out->rowAction(rownum,cx,true);
			}
			if(mouseDown) {
				if(debug) { syslog(LOG_ERR,"Mouse down: cx=%d,cy=%d",cx,cy); }
				out->rowSelect(cy,cx);
			}
		}
		NotifyInput();
	}
	syslog(LOG_ERR,"OutScreenInput::Execute:EXITED");
}

#endif

OutScreenInput::~OutScreenInput() {
	Stop();
}
