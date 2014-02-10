#include "outscreen.hh"

void OutScreen::attrOn(unsigned int attr) { attron(attr); }
void OutScreen::attrOff(unsigned int attr) { attroff(attr); }

#ifdef ARM
void OutScreen::fullscr_msg(const char* line1, const char* line2, const char* line3) {
	int midy=(maxy/2)-2;	// 2=(text rows/2)
	int midx=(maxx/2)-15; // 15=(text width/2)
	for(unsigned int xa=0; xa<maxy; xa++) { screenWrite(xa,0,(char *)"                                                            "); }
	screenWrite(midy,  midx,(char *)line1);
	screenWrite(midy+1,midx,(char *)line2);
	screenWrite(midy+2,midx,(char *)line3);
}
void OutScreen::calibrate_msg(int xx, int yy, const char* msg1, const char* msg2) {
	char line1[1024];
	char line2[1024];
	snprintf(line1,sizeof(line1),"Please touch the %12s",msg2);
	snprintf(line2,sizeof(line2),"corner of the screen (%s) to ",msg1);
	fullscr_msg(line1,line2,     "calibrate the touch screen.  ");
	screenWrite(xx,yy,(char *)msg1);
}
#else
void OutScreen::fullscr_msg(const char* line1, const char* line2, const char* line3) { }
void OutScreen::calibrate_msg(int xx, int yy, const char* msg1, const char* msg2) { }
#endif

bool OutScreen::DoCalibrate() {
	#ifdef ARM
		int debug=getId("debug",0);
		if(calibrate==1) { calibrate_msg(0,     0,     "|\\","top left");     if(debug) syslog(LOG_ERR,"calibrate1"); return false; }
		if(calibrate==2) { calibrate_msg(0,     maxx-2,"/|", "top right");    if(debug) syslog(LOG_ERR,"calibrate2"); return false; }
		if(calibrate==3) { calibrate_msg(maxy-1,maxx-2,"\\|","bottom right"); if(debug) syslog(LOG_ERR,"calibrate3"); return false; }
		if(calibrate==4) { calibrate_msg(maxy-1,0,     "|/", "bottom left");  if(debug) syslog(LOG_ERR,"calibrate4"); return false; }
		//if(missingDrive) { fullscr_msg(screenfp,"DVD device is plugged in but ","no hard drive for caching is ","plugged in.                  "); return false; }
	#endif
	return true;
}

void OutScreen::sleepWaitInput(int seconds) {
  int               rc;
  struct timespec   ts;
  struct timeval    tp;
  if(gettimeofday(&tp, NULL)<0) { syslog(LOG_ERR,"sleepWaitInput: gettimeofday: %s:",strerror(errno)); }
  
	ts.tv_sec  = tp.tv_sec; /** Convert from timeval to timespec **/
	ts.tv_nsec = tp.tv_usec * 1000;
	ts.tv_sec += seconds;
	if((rc=pthread_mutex_lock(&mutex)) != 0) { syslog(LOG_ERR,"sleepWaitInput: pthread_mutex_lock: rc=%d",rc); }
	rc=pthread_cond_timedwait(&cond, &mutex, &ts); /** mutex unlocked, perform wait, mutex locked on notify or timeout **/
	if(rc!=0 && rc!=ETIMEDOUT) { syslog(LOG_ERR,"sleepWaitInput: pthread_cond_timedwait: rc=%d",rc); }
	else {
		if((rc=pthread_mutex_unlock(&mutex)) != 0) { syslog(LOG_ERR,"sleepWaitInput: pthread_mutex_unlock: rc=%d",rc); }
	}
}

void OutScreen::Execute(void* arg) {
	syslog(LOG_ERR,"OutScreen:Execute:Handling screen updates...");
	//----------------- BOOST THREAD PRIORITY ---------------
	struct sched_param sp;
	memset(&sp, 0, sizeof(sp));
	sp.sched_priority=sched_get_priority_max(SCHED_RR); // SCHED_FIFO, SCHED_RR
	int rc=pthread_setschedparam(pthread_self(), SCHED_RR, &sp);
	if(rc!=0) { syslog(LOG_ERR,"pthread_setschedparam: error %d",rc); }
	//----------------- BOOST PROCESS PRIORITY ---------------
	if(setpriority(PRIO_PROCESS, 0, -19)<0) { /*perror("setpriority"); sleep(1); */ }
	//
	while(!Util::checkStop()) {
MARK
		int saving=getIT("saving");
		int doLogStderr=getIT("doLogStderr");
		if(!saving && !doLogStderr) {
MARK
			unsigned int oldx=maxx,oldy=maxy;
			getmaxyx(stdscr,maxy,maxx);
			if(oldx!=maxx || oldy!=maxy) { syslog(LOG_ERR,"maxx=%d,maxy=%d",maxx,maxy); }
			//if(DoCalibrate()) { Output(); }
			Output();
			//mvwprintw(stdscr,1,1,"@"); sleep(1);
			isWide=maxx>=80;
 			if(!clickShown) {
				attrOff(A_REVERSE|A_BOLD);
				attrOn(COLOR_PAIR(COLOR_RED));
				clickShown=true; mvwprintw(stdscr,currow,curcol+(isWide?-1:0),"*");		
			}
			move(currow/*y*/,curcol+(isWide?-1:0)/*x*/);
			wmove(stdscr,currow,curcol+(isWide?-1:0));
			curs_set(2); /** very visible (block cursor) **/
//#endif
MARK
			redrawwin(stdscr);
			wrefresh(stdscr);
MARK
		}
		if(doLogStderr) {
			endwin();
			saver->ShowScreen();
		}
		sleepWaitInput(1);
		calibrate=util->getInt("calibrate",1);
MARK
	}
	syslog(LOG_ERR,"OutScreen::Execute:EXITED");
MARK
	Shutdown();
MARK
}

OutScreen::OutScreen(const char* ttypath, Util* util): Out(util) {
/** VmSize=3908 **/
	setIT("scrnum",0);
	tty=fopen(ttypath,"rw");
	if(!tty) { syslog(LOG_ERR,"OutScreen: %s: %s",strerror(errno),ttypath); abort(); }
	sleep(1);
	calibrate=util->getInt("calibrate",1);
	setIT("doLogStderr",getId("doLogStderr",0));
	mainscr=newterm((char *)"xterm", tty, tty);
	mainwnd=initscr();
	move(0/*y*/,0/*x*/);
	wmove(mainwnd,0,0);
	curs_set(2); /** very visible (block cursor) **/
	refresh();
	wrefresh(mainwnd);
/** VmSize=3908 **/
	getmaxyx(stdscr,maxy,maxx);
	if(maxy<=0) { maxy=20; }
	isWide=maxx>=80;
	attrOff(A_REVERSE|A_BOLD);
	attrOn(COLOR_PAIR(COLOR_RED));
	if(isWide) box(stdscr, ACS_VLINE, ACS_HLINE);
	mmask_t mask=mousemask(ALL_MOUSE_EVENTS,NULL);
/** VmSize=3908 **/
	//syslog(LOG_ERR,"Mouse mask=%d",(int)mask); sleep(1); exit(0);
	if(mask==0) { syslog(LOG_ERR,"no mouse"); sleep(1); }
	initColors();
	input=new OutScreenInput(util,mainwnd,this);
/** VmSize=12280 **/
	//util->longsleep(3600);	
	
	saver=new OutScreenSaver(new Util("saver"), this);
	Activate();
/** VmSize=20472 **/	
	Execute(NULL);
	//Start();
}

/*#FIXME rowAction(13,10,1): Quick Restart
#FIXME first showed up as
#FIXME rowAction(13,7,1):
#FIXME Action not set probably due to OutScreen::screenWrite clearing menus for each line as it is written.
*/
int OutScreen::screenWrite(unsigned int yy, unsigned int xx, const char *msg) {
	int saving=getIT("saving");
	if(!saving) {
		mvwprintw(mainwnd,yy,xx,msg);
		if(curlineMenus.size()>0 && (yy>=menus.size() || menus[yy].size()==0)) {
			//syslog(LOG_INFO,"MENUS.size=%d,yy=%d; curlineMenus.size=%d",menus.size(),yy,curlineMenus.size());
			//syslog(LOG_INFO,"MENUS.size=%d,yy=%d; menus[%d].size=%d; curlineMenus.size=%d",menus.size(),yy,menus[yy].size(),curlineMenus.size());
			//MenuMap mm;
			if(yy>=menus.size()) { menus.resize(yy+1,curlineMenus/*mm*/); }
			else { menus[yy]=curlineMenus; }
// 			menus[yy].clear(); // Update menus for current line from curlineMenus (plus xx)
// 			for(MenuMap::iterator iter=curlineMenus.begin(); iter != curlineMenus.end(); ++iter) {
// 				menus[yy][xx+(*iter).first]=(*iter).second;
// 			}
			//menus[yy]=curlineMenus;
 			for(MenuMap::iterator iter=curlineMenus.begin(); iter != curlineMenus.end(); ++iter) {
				syslog(LOG_INFO,"MENU[%d][%d]=%s",yy,xx+(*iter).first,(*iter).second.c_str());
 			}
		}
	}
	return yy+1;
}

void OutScreen::Shutdown() {
MARK
	//Stop();
MARK
	bool flag=mainwnd||stdscr;
	mainwnd=NULL; stdscr=NULL;
MARK
  if(flag) { endwin(); }
MARK
	setIT("doLogStderr",1); // Now that the windowing system is shut down
MARK
	//if(system("reset")==-1) {};
MARK
	saver->ShowScreen();
}

OutScreen::~OutScreen() {
	Shutdown();
}
