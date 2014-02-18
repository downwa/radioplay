#include <curses.h>

#include "thread.hh"
//#include "play3abn.h"
#include "out.hh"
//#include "regdata.hh"

char Out::screenNames[5][32]={
	"Main>Menu",
	"Menu>Choose",
	"Menu>Choose",
	"Menu>Choose",
	"Menu>Choose"
};

void Out::QuickExit() {
	syslog(LOG_ERR,"Quick Exit");
	Util::Stop();
	Shutdown(); // Shut down the windowing system
	exit(0);
}

int Out::screenWriteUrl(unsigned int yy, unsigned int xx, const char *msg, string tip, string url) {
	screenWrite(yy,xx,msg); yy++;
	return yy;
}
void Out::Shutdown() {}
void Out::Execute(void* arg) {}
void Out::attrOn(unsigned int attr) {}
void Out::attrOff(unsigned int attr) {}

void Out::Activate() {
	currow=curcol=-1;
	util->get("release",release,sizeof(release)); // Immediately after activating the screen
	return;
	/** __DATE__ e.g. "Feb 12 1996" (space padded) 
	 * 01 Jan 
	 * 06 Jun 
	 * 02 Feb 
	 * 03 Mar 
	 * 04 Apr 
	 * 05 May 
	 * 07 Jul 
	 * 08 Aug 
	 * 09 Sep 
	 * 10 Oct 
	 * 11 Nov 
	 * 12 Dec
			__TIME__ e.g. "23:59:01
  **/
/*
#define c __DATE__[1]
#define d __DATE__[2]
	struct stat statbuf;
	if(stat("/md5sums.txt", &statbuf)==-1) {
		snprintf(release,sizeof(release),"%4s%02d%02d%02d%02d%02d.",&__DATE__[7],
			(d=='n'?(c=='a'?1:6):(d=='b'?2:(d=='r'?(c=='a'?3:4):(d=='y'?5:(d=='l'?7:(d=='g'?8:(d=='p'?9:(d=='t'?10:(d=='v'?11:(d=='c'?12:0)))))))))),
			atoi(&__DATE__[4]),
			atoi(__TIME__),atoi(&__TIME__[3]),atoi(&__TIME__[6])
		);
	}
	else {
		time_t t=time(NULL);
		struct tm tms;
		struct tm* tptr=localtime_r(&t,&tms);
		snprintf(release,sizeof(release),"%04d%02d%02d%02d%02d%02d*",
			1900+tptr->tm_year,tptr->tm_mon+1,tptr->tm_mday,tptr->tm_hour,tptr->tm_min,tptr->tm_sec);
	}
	//updateStatus[0]=0;
*/
}

string Out::geoLocate(string publicip) {
	char cmd[64];
	snprintf(cmd,sizeof(cmd),"./locate.sh %s >/dev/null 2>&1",publicip.c_str());
	if(system(cmd)==-1) { syslog(LOG_ERR,"geoLocate:system: %s failed: %s",cmd,strerror(errno)); }
	snprintf(cmd,sizeof(cmd),"geo%s",publicip.c_str());
	util->get(cmd, geolocation, sizeof(geolocation));
	return string(geolocation);
}
/*******************************************************************/
void Out::rowSelect(int rownum, int colnum) {
	this->currow=rownum;
	this->curcol=colnum;
}
void Out::rowAction(int rownum, int colnum, bool isLocal) {
	syslog(LOG_INFO,"rowAction(%d,%d,%d)",rownum,colnum,isLocal);
MARK
	this->clickShown=false;
	int debug=util->getInt("debug",0);
	int schedulerow=getITd("schedulerow",6);
	scrnum=getITd("scrnum",0);
MARK
	if(scrnum==0 && rownum>=schedulerow) {
		int detailStatus=getITd("detailStatus",0);
		int detailRow=getITd("detailRow",0);
		if(detailStatus && rownum==detailRow) { setIT("detailStatus",0); setIT("detailRow",0); }
		else { setIT("detailStatus",1); setIT("detailRow",rownum); }
		return;
	}
	else { setIT("detailStatus",0); setIT("detailRow",0); }
	string actionItemStr="";
MARK
	for(string::size_type yy=0; yy<menus.size(); yy++) {
		MenuMap mm=menus[yy];
		for(MenuMap::iterator iter=mm.begin(); iter != mm.end(); ++iter) {
			int xx=(*iter).first;
			string item=(*iter).second;
			if((int)yy==rownum && colnum>=(int)xx && colnum<((int)(xx+item.length()))) {
MARK
				actionItemStr=mm[xx];
MARK
				if(debug) { syslog(LOG_ERR,"menus[%d][%d]=%s",rownum,xx,mm[xx].c_str()); }
			}
		}
	}
MARK
	const char *actionItem=actionItemStr.c_str();
MARK
	if(debug) { syslog(LOG_ERR,"rowAction(%d,%d,%d): %s",rownum,colnum,isLocal,actionItem); }
	/** ACTIONS **/
	int aa=6,bb=30,cc=aa-2;
	if(rownum==0) { togITd("scrnum",0); menus.clear(); }
	else if(match(&actionItem[1],"Main>Menu")) { setIT("scrnum",1); menus.clear(); }
	else if(match(&actionItem[1],"Menu>Choose")) { setIT("scrnum",0); menus.clear(); }
	else if(ACT("VOL")) {
		int ofs=colnum-cc;
		int scale=bb-aa;
		int pct=100*ofs/scale;
		if(pct>100) { pct=100; }
		if(pct<10) { pct=10; }
		//setVol(pct);
		setI("volume",pct);
	}
	else if(ACT("AMP")) { setI("isboosted",getId("isboosted",1)?0:1); }
	else if(ACT("Date: ")) { //"Set Date/Time: 2012-02-07 15:24:11"
		clickAction="";
		time_t t=time(NULL);
		struct tm tms;
		struct tm *tmp=localtime_r(&t,&tms);
		int xo=(isWide?8:7);
syslog(LOG_INFO,"DATE: colnum=%d,xo=%d",colnum,xo);
		if(colnum<xo) { cycleITd("setdate",7,1); } // Cycle 7 values, 0..6
		if(colnum>=xo+0 && colnum<=xo+1) { clickAction="Y-"; tmp->tm_year--; }
		else if(colnum>=xo+2 && colnum<=xo+3) { clickAction="Y+";  tmp->tm_year++;}
		else if(colnum==xo+4) { clickAction="M-";  tmp->tm_mon--;}
		else if(colnum>=xo+5 && colnum<=xo+6) { clickAction="M+";  tmp->tm_mon++;}
		else if(colnum==xo+7) { clickAction="D-";  tmp->tm_mday--;}
		else if(colnum>=xo+8 && colnum<=xo+9) { clickAction="D+";  tmp->tm_mday++;}
		else if(colnum==xo+10) { clickAction="h-";  tmp->tm_hour--;}
		else if(colnum>=xo+11 && colnum<=xo+12) { clickAction="h+";  tmp->tm_hour++;}
		else if(colnum==xo+13) { clickAction="m-";  tmp->tm_min--;}
		else if(colnum>=xo+14 && colnum<=xo+15) { clickAction="m+";  tmp->tm_min++;}
		else if(colnum==xo+16) { clickAction="s-";  tmp->tm_sec--;}
		else if(colnum>=xo+17 && colnum<=xo+18) { clickAction="s+";  tmp->tm_sec++;}
		time_t newnow=mktime(tmp);
		setI("date",newnow);
		struct timeval tv;
		tv.tv_sec=newnow;
		tv.tv_usec=0;
		if(settimeofday(&tv,NULL)<0) { syslog(LOG_ERR,"Out::rowAction: Unable to update time: %s",strerror(errno)); }
	}
	else if(ACT("NTP Date")) {
		syslog(LOG_ERR,"Starting ./ntpdate.sh");
		if(system("./ntpdate.sh 2>/tmp/play3abn/ntpdate.log")<0) { syslog(LOG_ERR,"./ntpdate.sh failure"); }
	}
	else if(ACT("Cancel Update")) {
		syslog(LOG_ERR,"Cancelling update.sh");
		if(system("./update.sh </dev/null")==-1) { syslog(LOG_ERR,"rowAction:system(./update.sh): %s",strerror(errno)); }
	}
	else if(ACT("Update")) {
		syslog(LOG_ERR,"Starting update.sh");
		if(system("./update.sh </dev/null")==-1) { syslog(LOG_ERR,"rowAction:system(./update.sh): %s",strerror(errno)); }
	}
	else if(ACT("Custom Sched")) {
		togId("customsched",0);
		syslog(LOG_ERR,"Toggle customsched");
	}
	else if(ACT("Use Cache")) {
		togId("cache",1);
	}
	else if(ACT("Toggle Mount")) {
		int denyMount=1-getId("denymount",0);
		setI("denymount",denyMount);
		if(denyMount) {
			int err1=0,err2=0,err3=0;
			for(int xa=0; xa<10; xa++) {
				if(umount2("/udisk",MNT_DETACH)==-1  || umount("/udisk")==-1)  { err1=errno; }
				if(umount2("/sdcard",MNT_DETACH)==-1 || umount("/sdcard")==-1) { err2=errno; }
				if(umount2("/cdrom",MNT_DETACH)==-1  || umount("/cdrom")==-1)  { err3=errno; }
				if(err1!=EBUSY && err2!=EBUSY && err3!=EBUSY) { break; }
				break;
			}
			if(err1!=0) { syslog(LOG_ERR,"umount /udisk: %s",strerror(err1)); }
			if(err2!=0) { syslog(LOG_ERR,"umount /sdcard: %s",strerror(err2)); }
			if(err3!=0) { syslog(LOG_ERR,"umount /cdrom: %s",strerror(err3)); }
		}
	}
	else if(ACT("Cycle Debug")) {
		cycleId("debug",5,0); // Cycle 5 values, 0..4
	}
	else if(ACT("Cycle Delay")) {
		cycleId("bufhrs",4,3); // Cycle 4 values, 0..3
		setI("bufsecs",getId("bufhrs",3)*3600);
	}
	else if(ACT("Show Playlist")) {
		setIT("scrnum",0); menus.clear(); 
	}
	else if(ACT("Reboot")) {
		if(system("./reboot.sh")==-1) { syslog(LOG_ERR,"rowAction:system(./reboot.sh): %s",strerror(errno)); }
	}
	else if(ACT("Shutdown")) {
		if(system("./poweroff.sh")==-1) { syslog(LOG_ERR,"rowAction:system(./poweroff.sh): %s",strerror(errno)); }
	}
	else if(ACT("IP    ")) { setI("showpublicip",1); }
	else if(ACT("Pub IP")) { setI("showpublicip",0); }
	else if(ACT("Quick Restart")) {
		QuickExit();
	}
	else {
		char cmd[1024];
		snprintf(cmd,sizeof(cmd),"./action %d %d %d \"%s\"",isLocal,rownum,colnum,actionItem);
		syslog(LOG_ERR,"rowAction: %s",cmd);
		int ret=system(cmd);
		if(ret!=0) { syslog(LOG_ERR,"rowAction: system: %s (%d)",ret==-1?strerror(errno):"returned",ret); }
	}
MARK
}

#define MENUCOLORS attrOff(A_BOLD|A_REVERSE); attrOn(COLOR_PAIR(COLOR_BLUE));
#define FIXOUT fixline(out,min(sizeof(out)-1,maxx)); OutputRow();
void Out::Output() {
	//syslog(LOG_ERR,"menus.size=%d",menus.size());
	unsigned int row=0;
	char out[1024];
	if(maxy<=0) { maxy=20; }
	isWide=maxx>=80;
	attrOff(A_REVERSE|A_BOLD);
	attrOn(COLOR_PAIR(COLOR_RED));
	if(isWide) box(stdscr, ACS_VLINE, ACS_HLINE);
	attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN));
	int offline=getId("offline",0);
	snprintf(out,sizeof(out),"%splay3abn-%s%sWDowns",isWide?" ":"",release,offline?"!":" "); // + <Vl>
	if(maxx<80) fixline(out,min(sizeof(out),maxx));									// 123456
	//row=screenWriteUrl(row,isWide?2:0,out,"Click to view stations","http://iglooware.ath.cx:1042?row=13&scrnum=1");
	row=screenWriteUrl(row,isWide?2:0,out,"Click to toggle menu","");
	attrOff(A_REVERSE|A_BOLD);
	attrOn(COLOR_PAIR(COLOR_RED));
	// *********** SCREEN ***********
	scrnum=getITd("scrnum",0);
	bool isFilling=getITd("isfilling",0);
	int scheduleAgeDays=getITd("scheduleagedays",999);
  int dateset=getITd("dateset",0); // If date was set this boot, we assume it's a LIVE date
	int scheduling=getITd("scheduling",0);
	int extracting=getITd("extracting",0);
	if(getId("customsched",0)==1) {
		snprintf(out,sizeof(out),"%12s %cCustom Schedule","",
						 scheduling?'*':
						 (extracting?'^':' '));
	}
	else if(scheduleAgeDays==999) {
		snprintf(out,sizeof(out),"%12s Fill File","");
	}
	else if(scheduleAgeDays>=365) {
		snprintf(out,sizeof(out),"%12s Fill Schedule","");
	}
	else {
		snprintf(out,sizeof(out),"%12s %s %d days old","",
						 isFilling?"FILL":	// FILL=mplay detected silence, or reached end of hour before it should have, and is filling
							(scheduleAgeDays==0?
								(dateset==1?"LIVE":"ODAT"): // LIVE=Date was set on this boot; ODAT=Date has not been set by radio system (or we're running on a test system)
								"AGED"),scheduleAgeDays); // Schedule is older than today
	}
	fixline(out,min(sizeof(out)-1,maxx));
	screenWrite(row,isWide?1:0,out);
	snprintf(out,sizeof(out),"{%c%-11s}",util->isMounted()?'*':' ',screenNames[scrnum]);
	// &fmtTime(chooser->choseScheduleAt)[11],chooser->choosingNN
	attrOff(A_BOLD|A_REVERSE);
	fixline(out,min(sizeof(out)-1,maxx));
	out[12]=0;
	attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN));
	row=screenWriteUrl(row,isWide?1:0,out,"Click to toggle menu","");
	attrOff(A_BOLD|A_REVERSE);
MARK
	int denyMount=util->getInt("denymount",0);
	char radiodir[256]; // radiodir
	getST(radiodir);
	// Warning message (if any)
	bool noStorage=(!radiodir[0] || denyMount);
	if(scrnum==0) {		
		row=Update(row,radiodir,denyMount);
		if(!noStorage) { row=UpdateRows(row); }
	}
	else if(scrnum==1) { // Upd Clr Rbt Off -Clk+ <Vl>
		char updatestatus[256];
		getST(updatestatus);
		char macaddr[18];
		getST(macaddr);
		//attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_BLUE));
		int menurow=getITd("menurow",0);
		MENUCOLORS; if(menurow==1) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {Date: %s} %s",Util::fmtTime(time(NULL)).c_str(),clickAction.c_str()); FIXOUT;
		int setdate=getITd("setdate",0);
		if(setdate>0) {
			int pos=0;
			if(setdate==1) { pos=7;  out[pos+4]=0; }
			if(setdate==2) { pos=12; out[pos+2]=0; }
			if(setdate==3) { pos=15; out[pos+2]=0; }
			if(setdate==4) { pos=18; out[pos+2]=0; }
			if(setdate==5) { pos=21; out[pos+2]=0; }
			if(setdate==6) { pos=24; out[pos+2]=0; }
			attrOff(A_BOLD|A_REVERSE);
			//attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN));
			row=screenWrite(row,pos+(isWide?1:0),&out[pos]);
			attrOn(COLOR_PAIR(COLOR_BLUE));
		}
		else { row++; }

		MENUCOLORS; if(menurow==2) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {NTP Date         }"); FIXOUT; row++;
		MENUCOLORS; if(menurow==3) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {%sUpdate %s        }",updatestatus[0]?"Cancel ":"",macaddr); FIXOUT; row++;
		MENUCOLORS; if(menurow==4) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {Custom Sched [%s]}",getId("customsched",0)?"on":"off"); FIXOUT; row++;
		MENUCOLORS; if(menurow==5) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {Use Cache    [%s]}",getId("cache",1)?"on":"off"); FIXOUT; row++;
		MENUCOLORS; if(menurow==6) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {Toggle Mount [%s]}",getId("denymount",0)?"off":"on"); FIXOUT; row++;
		MENUCOLORS; if(menurow==7) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {Cycle Debug  [%d]}",getId("debug",0)); FIXOUT; row++;
		MENUCOLORS; if(menurow==8) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {Cycle Delay  [%d]}",getId("bufhrs",3)); FIXOUT; row++;
		MENUCOLORS; if(menurow==9) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {Show Downloads   }"); FIXOUT; row++;
		MENUCOLORS; if(menurow==10) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {Show Log         }"); FIXOUT; row++;
		MENUCOLORS; if(menurow==11) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {Show Playlist    }"); FIXOUT; row++;
		MENUCOLORS; if(menurow==12) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {Reboot           }"); FIXOUT; row++;
		MENUCOLORS; if(menurow==13) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {Shutdown         }"); FIXOUT; row++;
		MENUCOLORS; if(menurow==14) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }
		snprintf(out,sizeof(out)," {Quick Restart    }"); FIXOUT; row++;
		MENUCOLORS; if(menurow==15) { attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN)); }

		attrOff(A_BOLD|A_REVERSE);
		out[0]=0;
		attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN));
MARK
		if(updatestatus[0]) {
			snprintf(out,sizeof(out),"%s",updatestatus);
			string truncated=fixline(out,min(sizeof(out)-1,maxx)); // Space padded, truncated to width
			OutputRow(); row++;
			char out2[1024];
			while(truncated!="" && row<maxy-(isWide?1:0)) {
				snprintf(out2,sizeof(out2),"%s",truncated.c_str());
				truncated=fixline(out2,min(sizeof(out2)-1,maxx)); // Space padded, truncated to width
				screenWrite(row++,isWide?1:0,out2);
			}
		}
		//snprintf(out,sizeof(out)," >Test");         fixline(out,min(sizeof(out)-1,maxx)); row=screenWrite(row,isWide?1:0,out,"Tip","http://iglooware.ath.cx");
		attrOff(A_BOLD|A_REVERSE);
	}
	else if(scrnum==2) {
		// *************** GET DOWNLOAD FILENAME ***************
// 		char ftpget[1024];
// 		getST(ftpget); // e.g. 12345 12999 somepath/somefile.ogg
// 		char *dlfile=ftpget;
// 		char *result;
// 		int dlBytes=(int)strtol(dlfile, &result, 10); // Decode first number (number of bytes already downloaded)
// 	//syslog(LOG_INFO,"UDR:dlBytes=%d,result=%s.",dlBytes,result);
// 		int expectBytes=(int)strtol(&result[1], &result, 10); // Skip first number and space, decode number of bytes to be downloaded
// 	//syslog(LOG_INFO,"UDR:expectBytes=%d,result=%s.",expectBytes,result);
// 		dlfile=&result[1]; // Skip leading numbers and following space
// 	//syslog(LOG_INFO,"UDR:dlfile=%s.",dlfile);
// 		char *dlf=strrchr(dlfile,'/'); // NOTE: Name of file being downloaded
// 	//syslog(LOG_INFO,"UDR:dlf=%s.",dlf);
// 		if(dlf) { dlf++; }
// 		else { dlf=dlfile; }
		// *************** LIST SCHEDULED DOWNLOADS ***************
		attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_BLUE));
		snprintf(out,sizeof(out),"Scheduled Downloads"); fixline(out,min(sizeof(out)-1,maxx)); OutputRow(); row++;
		attrOff(A_BOLD|A_REVERSE);
		attrOn(COLOR_PAIR(COLOR_BLUE));
/*		list<ToDownload> downloadList=curler->getDownloads();
		list<ToDownload>::const_iterator cij;
		for(cij=downloadList.begin(); cij!=downloadList.end(); cij++) {
			ToDownload down=(ToDownload)(*cij);
			string url=down.url;
			string::unsigned int loc=url.find("%s");
			if(loc != string::npos) { url.erase(loc,2); }
			const char *urlS=url.c_str();
			const char *pp=strrchr(urlS,'/');
			if(!pp) { pp=urlS; }
			else { pp=&pp[1]; }
			if(strncmp(dlf,pp,strlen(pp))==0) {
				float expectBytes=down.expectBytes;
				if(expectBytes<1) { expectBytes=1.0; }
				float progress=(float)dlBytes;
				float pct=progress*100.0/expectBytes;
				setInt("pctDown",(int)pct);
				snprintf(out,sizeof(out),"%5.2f%%%% %s",pct,pp);
			}
			else { snprintf(out,sizeof(out),"       %s",pp); }
			string truncated=fixline(out,sizeof(out)); OutputRow(); row++;
			int detailStatus=getIT("detailStatus");
			int detailRow=getIT("detailRow");
			if(detailStatus==1 && (row-1)==detailRow) {
				int dc=0;
				while(truncated!="") {
					snprintf(out,sizeof(out),"       %s",truncated.c_str());
					truncated=fixline(out,sizeof(out)); // Space padded, truncated to width
					screenWrite(row++,isWide?1:0,out);
					dc++;
				}
				detailCount=dc;
			}
			if(row>=maxy-(isWide?1:0)) { break; }
		}
*/
	}
	else if(scrnum==3) {
		// COUNT LOG LINES
		// FIXME: List files in mylogs/ give user choice of log to view.
		const char *mainlog=LOGFILE;//(string(Util::LOGSPATH)+"/play3abn.log.txt").c_str();
		FILE *fp=fopen(mainlog,"rb");
		int lines=0;
		if(fp) {
			char buf[1024]={0};
			while(!feof(fp) && !ferror(fp) && fgets(buf,sizeof(buf),fp)) { lines++; }
			fclose(fp); fp=NULL;
		}	
		int linesToShow=(maxy-4-(isWide?1:0));
		// SHOW LOG TITLE
		attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_BLUE));
		snprintf(out,sizeof(out),"Tail Log (%d of %d lines)",linesToShow,lines); fixline(out,min(sizeof(out)-1,maxx)); OutputRow(); row++;
		attrOff(A_BOLD|A_REVERSE);
		// SHOW LOG
		attrOn(COLOR_PAIR(COLOR_BLUE));
		if(lines>=0) { fp=fopen(mainlog,"rb"); }
		if(lines>0 && fp) {
			char buf[1024]={0};
			int lnum=0;
			while(!feof(fp) && !ferror(fp) && fgets(buf,sizeof(buf),fp)) {
				lnum++;
				if(lnum >= lines-linesToShow) {
					string line=string(buf);
					string::size_type loc=line.find("%s");
					if(loc != string::npos) { line.erase(loc,2); }
					loc=line.find("\r");
					if(loc != string::npos) { line.erase(loc,2); }
					snprintf(out,sizeof(out),"%04d %s",lnum,line.c_str());
					fixline(out,sizeof(out)); OutputRow(); row++;
				}
			}
			fclose(fp);
		}
	}
	else if(scrnum==4) {
		attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_BLUE));
		snprintf(out,sizeof(out),"Remote Clients"); fixline(out,sizeof(out)); OutputRow(); row++;
		attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN));
		snprintf(out,sizeof(out)," Mac Address       Public IP       Private IP      Location"); fixline(out,sizeof(out)); OutputRow(); row++;
		attrOff(A_BOLD|A_REVERSE);
		// SHOW CONNECTED CLIENTS
		attrOn(COLOR_PAIR(COLOR_BLUE));
/*		RegDataMap reg=ohttp->listClients();
		for(RegDataMap::iterator iter = reg.begin(); iter != reg.end() && row<maxy-(isWide?1:0); ++iter) {
			string mac=(*iter).first;
			RegData data=(*iter).second;
//			 	string ipaddr;  // -- Remote IP addr (from socket)
//	time_t reqtime; // -- Request time (to notice if the server is alive)
//	string village; // -- Deduced name of village (using geolocation service?)
//	int sock;				// -- Socket to which we should send request and expect reply
			snprintf(out,sizeof(out),"%s %-15s %-15s %s",mac.c_str(),data.publicip.c_str(),data.myip.c_str(),data.village.c_str());
			fixline(out,sizeof(out));
			char url[1024];
			snprintf(url,sizeof(url),"http://iglooware.ath.cx:1042/remote?mac=%s",mac.c_str());
			row=screenWriteUrl(row,isWide?1:0,out,"Click to view this client",url);
    }
*/    
	}
	out[0]=0;
	fixline(out,sizeof(out)); // Space padded, truncated to width
MARK
  for(unsigned int yy=row; yy<maxy-(isWide?1:0); yy++) { screenWrite(yy,isWide?1:0,out); }
MARK
	clickAction="";
}

int Out::Update(int row, const char *radiodir, bool denyMount) {
	char out[1024];
  // *********** VOLUME METER ***********
	char volmeter[METERLEN+1];
  int dx=util->getInt("volume")*METERLEN/100; //*2/3)/*Sample Max*/;
	setProgressBar(volmeter, METERLEN, dx, 2, '=');
	snprintf(out,sizeof(out),"{VOL  [%s]}",volmeter);
  fixline(out,min(sizeof(out)-1,maxx));
	attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_RED));
  row=screenWriteUrl(row,isWide?1:0,out,"Click to set volume","");
	attrOff(A_BOLD|A_REVERSE);
	
  // *********** AMP METER ***********
	int isBoosted=getId("isboosted",1);
	int isSilent=getITd("silentfill",0);
	int amaxsamp=getITd("amaxsamp",1);
	//int maxsamp=getITd("maxsamp",1);
  char ampmeter[METERLEN+1];
  //dx=(/*isBoosted?*/amaxsamp/*:maxsamp/*)*/*METERLEN/(32768); //*2/3)/*Sample Max*/;
  dx=amaxsamp*METERLEN/(32768); //*2/3)/*Sample Max*/;
  setProgressBar(ampmeter, METERLEN, dx, 2, '#');
	snprintf(out,sizeof(out),"{AMP%s%s[%s]}",isBoosted?"^":" ",isSilent?"S":" ",ampmeter);
  fixline(out,min(sizeof(out)-1,maxx));
	attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_RED));
  row=screenWriteUrl(row,isWide?1:0,out,(isBoosted?"Toggle boost [on]":"Toggle boost [off]"),"");
	attrOff(A_BOLD|A_REVERSE);
	//attrOn(COLOR_PAIR(COLOR_RED));

	// ************ IP ADDRESS ***************
	int showPublicIp=util->getInt("showpublicip",0);
	char publicip[16];
	char myip[16]={0};
	int isConn=getIT("isconn");
	getST(publicip);
	getST(myip);
	snprintf(out,sizeof(out)-1,"{%-6s: %-15s %-6s}",showPublicIp?"Pub IP":"IP",showPublicIp?publicip:myip,isConn?"CONN":"");
	fixline(out,min(sizeof(out)-1,maxx));
  row=screenWriteUrl(row,isWide?1:0,out,"Toggle private/public IP","");
	
	// ************ DATE/TIME + Play progress ***************
	int showScheduleDate=util->getInt("showscheduledate",0);
	if(showScheduleDate) { // Show top schedule date/time
		//snprintf(out,sizeof(out)-1,"s%s %d/%d",chooser->fillTopDate().c_str(),playSec,curPlayLen);
	}
	else { // Show Current Date/Time
		int playSec=getIT("playsec");
		int curPlayLen=getIT("curplaylen");
		viewtime2=Time(); //-bufferSeconds;
		struct tm tms;
		struct tm* vptr=localtime_r(&viewtime2,&tms);
		snprintf(out,sizeof(out)-1,"{%04d-%02d-%02d %02d:%02d:%02d %d/%d}",
			1900+vptr->tm_year,vptr->tm_mon+1,vptr->tm_mday,
			vptr->tm_hour,vptr->tm_min,vptr->tm_sec,
			playSec,curPlayLen);
	}
  fixline(out,min(sizeof(out)-1,maxx));
  row=screenWriteUrl(row,isWide?1:0,out,"Click to toggle Date/Time (c=Current, s=Schedule)","");

	// Warning message (if any)
	bool noStorage=(!radiodir[0] || denyMount);
	if(noStorage) {
		const char *msg=denyMount?"INSERT SD & Toggle Mount!!!":"PLEASE INSERT SD CARD!!!";
		//snprintf(out,sizeof(out),"%s: radiodir=%s,denyMount=%d",msg,radiodir,denyMount); fixline(out,min(sizeof(out)-1,maxx));
		snprintf(out,sizeof(out),"%s",msg); fixline(out,min(sizeof(out)-1,maxx));
		attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_RED));
		screenWrite(row++,isWide?1:0,out);
		attrOff(A_BOLD|A_REVERSE);
	}
	
	// Initialize schedule row
	//if(scheduleRow==0) { scheduleRow=row; }
	setIT("schedulerow",row); // FIXME: Perform action needs to be called
	
	if(!noStorage) { row=UpdatePlaying(row); }
	return row;
}

int Out::UpdatePlaying(int row) {
	char out[1024];
	char playfile[256];
	getST(playfile);
	//                          "2011-06-09 12:40:00 1 0066 IT'S A WONDERFUL DAY-Heritage Singers.ogg"
	// /tmp/play3abn/tmp/playtmp-2012-03-06 09:00:01 0 3479 One_Second_Time_Fill~Simulcast_Of_3abn_Tv's_Today_L~3ABN_TODAY_RADIO_BREAK_2~.ogg

	time_t playAt=0;
//	int flag,seclen=0;
//	string catcode="", url="";
	string dispname="(No file playing)";
	if(playfile[0]) {
		char *pp=strchr(playfile,'-');
		if(!pp) { syslog(LOG_ERR,"Out::UpdatePlaying: Invalid format0: %s",playfile); return row; }
		char *qq=strchr(&pp[1],' ');
		if(!qq) { syslog(LOG_ERR,"Out::UpdatePlaying: Invalid format1: %s",playfile); return row; }
		*qq=0;
		playAt=atol(&pp[1]);
		dispname=string(&qq[1]);
// 		strings ent=strings(string(&pp[1]),&qq[1]);
// 		int result=util->itemDecode(ent, playAt, flag, seclen, catcode, dispname, url);
// 		if(result<0) {
// 			syslog(LOG_ERR,"Out::UpdatePlaying: Invalid format2 (result=%d): %s => %s",result,ent.one.c_str(),ent.two.c_str());
// 			remove(playfile);
// 			return row;
// 		}
	}
	prevPlayAt=playAt;
	const char *playing=dispname.c_str();
	
/*	
	const char *pp=strchr(playfile,' '); // Skip to play time in path
	int hh=0,mm=0,ss=0;
	if(pp) {
		pp=&pp[1];   // hh:mm:ss
		hh=atoi(pp); // 01 34 67
		mm=atoi(&pp[3]);
		ss=atoi(&pp[6]);
		pp=strchr(pp,' '); // Locate flag (age of schedule)
	}
	if(pp) { pp=strchr(&pp[1],' '); } // Locate length of item (in seconds)
	if(pp) { pp=strchr(&pp[1],' '); } // Locate displayable filename
	const char *playing=pp?&pp[1]:playfile;
	snprintf(out,sizeof(out),"%02d%02d%02d %s",hh,mm,ss,playing);
*/	
	struct tm tms;
  struct tm* tptr=localtime_r(&playAt,&tms);
	if(!playfile[0]) { tptr->tm_hour=tptr->tm_min=tptr->tm_sec=0; } // Display default time if no file playing
	snprintf(out,sizeof(out),"%02d%02d%02d %s",tptr->tm_hour,tptr->tm_min,tptr->tm_sec,playing);
	int slenOut1=strlen(out);
	strncpy(savePlaying,out,sizeof(savePlaying)-1);
	string truncated=fixline(out,sizeof(out)); // Space padded, truncated to width
	int slenOut2=strlen(out);
	attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_CYAN));
	int orow=row;
	OutputRow(); row++;
	int detailStatus=getIT("detailStatus");
	int detailRow=getIT("detailRow");
	if(detailStatus==1 && (row-1)==detailRow) {
		char out2[1024];
		int dc=0;
		while(truncated!="") {
			snprintf(out2,sizeof(out2),"       %s",truncated.c_str());
			truncated=fixline(out2,sizeof(out2)); // Space padded, truncated to width
			screenWrite(row++,isWide?1:0,out2);
			dc++;
		}
		setIT("detailCount",dc);
	}
	attrOn(A_BOLD|A_REVERSE|COLOR_PAIR(COLOR_GREEN));
	// ********* SHOW PLAYBACK PERCENTAGE COMPLETE **********
	int playSec=getIT("playsec");
	int curPlayLen=getIT("curplaylen");
	if(curPlayLen>0) {
		int psec=playSec;
		if(psec>curPlayLen) { psec=curPlayLen; }
		unsigned int playpct=psec*100/curPlayLen;
		//if(strncmp(playingName,pname,strlen(pname))!=0) { playpct=0; }
		unsigned int playPctOfs=playpct*(strlen(out)-7)/100;
		attrOn(A_REVERSE|A_BOLD);
		if(7+playPctOfs>sizeof(out)-1) { syslog(LOG_ERR,"ERROR: playPctOfs=%d,playpct=%d,psec=%d,curPlayLen=%d,slenOut1=%d,slenOut2=%d,out=%s****",
			playPctOfs,playpct,psec,curPlayLen,slenOut1,slenOut2,out); playPctOfs=sizeof(out)-8; }
		out[7+playPctOfs]=0;
		screenWrite(orow,isWide?1+7:0+7,&out[7]);
		attrOff(A_REVERSE|A_BOLD);
	}
	attrOff(A_REVERSE|A_BOLD);
	return row;
}

int Out::textlen(const char *str) {
	const char *pp=&str[strlen(str)-1];
	while(*pp==' ') { pp--; }
	return (pp-str);
}

bool Out::isFiller(const char* path) {
	const char *pname=strrchr(path,'/');
	if(pname) { pname++; }
	else { pname=path; }
	if(strncmp(pname,"FullBreak~",10)==0 ||
		strncmp(pname,"AfflBreak~",10)==0 ||
		strncmp(pname,"FullBreak-",10)==0 ||
		strncmp(pname,"AfflBreak-",10)==0 ||
		strncmp(pname,"Filler",6)==0 ||
		strncmp(pname,"NEWSBREAK-",10)==0 ||
		strncmp(pname,"Network_ID",10)==0
		) { return true; }
	return false;
}

int Out::UpdateRows(int startRow) {
	char out[1024];
	//list<TSchedLine>::const_iterator cii;
	//list<TSchedLine> li; //=chooser->getList();
	unsigned int row=startRow; //1+(isWide?1:0);
	//char *pname;
  int oddeven=0;
	// *************** GET DOWNLOAD FILENAME ***************
	char ftpget[1024];
	getST(ftpget); // e.g. 12345 12999 somepath/somefile.ogg
	char *dlfile=ftpget;
	char *result;
	int dlBytes=(int)strtol(dlfile, &result, 10); // Decode first number (number of bytes already downloaded)
//syslog(LOG_INFO,"UDR:dlBytes=%d,result=%s.",dlBytes,result);
	int expectBytes=(int)strtol(&result[1], &result, 10); // Skip first number and space, decode number of bytes to be downloaded
//syslog(LOG_INFO,"UDR:expectBytes=%d,result=%s.",expectBytes,result);
	dlfile=&result[1]; // Skip leading numbers and following space
//syslog(LOG_INFO,"UDR:dlfile=%s.",dlfile);
	char *dlf=strrchr(dlfile,'/'); // NOTE: Name of file being downloaded
//syslog(LOG_INFO,"UDR:dlf=%s.",dlf);
	if(dlf) { dlf++; }
	else { dlf=dlfile; }
	// *************** SHOW ROWS ***************
	// FIXME: bool firstRow=true;
	const char* playq="/tmp/play3abn/tmp/playq";
	struct stat statbuf;
	//for(cii=li.begin(); cii!=li.end(); cii++) {
	/** NOTE: EACH ENTRY e.g. '2011-06-30 19:58:45 0060 AfflBreak~20110625_195845~.ogg' **/
	vector<strings> playlist=util->getPlaylist(playq,0/*nolimit*/,1/*sorted*/);
MARK
	bool prevExistsFile=false;
	bool existsFile=false;
	int hhmmss0=0;
	//syslog(LOG_ERR,"Out::UpdateRows: size=%d",playlist.size());
	
	
	bool isCustomSchedule=(getId("customsched",0)==1);
	for(unsigned int xa=0; xa<playlist.size(); xa++) {
		//usleep(NOPEGCPU); // Sleep a while to avoid pegging the CPU
		strings ent=playlist[xa];
		int flag=0;
		int playLen=0;
		time_t playTime;
		string dispname="";
		string catcode="", url="";
		int result=util->itemDecode(ent, playTime, flag, playLen, catcode, dispname, url);
		if(result<0) { syslog(LOG_ERR,"Out::UpdateRows(itemDecode): Invalid format (result=%d): %s => %s",result,ent.one.c_str(),ent.two.c_str()); 	}
		const char *filePath=ent.two.c_str();
		if(isCustomSchedule && strncmp(filePath,"http://",7) == 0) { continue; } // Don't display downloading files on custom schedule
MARK
		if(strstr(filePath,"/r/rec2")) { continue; } // Don't display schedules on schedule
		struct tm tms;
		struct tm* sptr=localtime_r(&playTime,&tms);
		int hh=sptr->tm_hour; int mm=sptr->tm_min; int ss=sptr->tm_sec;
		prevExistsFile=existsFile;
		existsFile=true; //sLine.isComplete(); //true; // FIXME: Had tried using isComplete() but was too slow!
		char fullpath[1024];
		const char* pname=dispname.c_str(); //strrchr(filePath,'/');
		strncpy(fullpath,filePath,sizeof(fullpath)); //pname++; //}
		if(stat(fullpath, &statbuf)==-1) { existsFile=false; }
		else if(statbuf.st_size==0) { existsFile=false; } // was 1500000 for 1/4/ hour

// 		if(strstr(pname,"BLS128") || strstr(pname,"OTR314")) {
// 			syslog(LOG_ERR,"urlDecode out: %s",pname);
// 		}
// 		if(!existsFile && strncmp(pname,"HYTH141",7)==0) {
// 			syslog(LOG_INFO,"HYTH141: %s,stat=%d,size=%d",fullpath,stat(fullpath, &statbuf),statbuf.st_size);
// 		}

		int debug=util->getInt("debug",0);
MARK
		// *************** OUTPUT ROW ***************
		string from="";
		snprintf(out,sizeof(out),"%02d%02d%02d %s%s",hh,mm,ss,pname,from.c_str());
		const char *ply=savePlaying;
		int hhmmss1=atoi(ply);
		int hhmmss2=atoi(out);
		bool skip=false; // SKIP if NOT debugging AND (this is filler OR both previous and current file exist at the same time slot)
		if(!debug && (isFiller(pname) || (prevExistsFile && existsFile && hhmmss0==hhmmss2 && playTime==prevPlayAt))) { skip=true; }
		hhmmss0=hhmmss2; // Save to check for duplicate time slots
		if(skip) { continue; }
// 		if(prevExistsFile && existsFile) {
// 			//if(playTime==prevPlayAt && !debug) { continue; } // Just saw something at this time
// 			prevPlayAt=playTime;
// 		}
		int diff=hhmmss1-hhmmss2;
		if(diff>0 && diff<3600) { continue; } // Skip showing a scheduled row if it has already played in last hour
		int plylen=textlen(ply); // Length of non-space text
		int toplen=textlen(out);
		if(strncmp(ply,out,min(plylen,toplen))==0) { continue; } // Skip row if same as playing
		
		// *************** SET OUTPUT COLORS ****************
		attrOff(A_REVERSE);
		if(existsFile) {
			if(oddeven) { attrOn(A_REVERSE|COLOR_PAIR(COLOR_MAGENTA)); }
			else {        attrOn(A_REVERSE|COLOR_PAIR(COLOR_BLUE)); }
		}
		else {
			if(oddeven) { attrOn(/*A_REVERSE|*/COLOR_PAIR(COLOR_MAGENTA)); }
			else {        attrOn(/*A_REVERSE|*/COLOR_PAIR(COLOR_BLUE)); }
		}
		oddeven=1-oddeven;
		// *******************************
		string truncated=fixline(out,sizeof(out)); // Space padded, truncated to width

		//syslog(LOG_INFO,"PERCENTAGE: dlf=%s,pname=%s,eq=%d,pct=%d",dlf,pname,strncmp(dlf,pname,strlen(pname))==0,dlBytes*100/expectBytes);
		//if(dlf[0] && strncmp(dlf,pname,strlen(pname))==0) {
		if(Util::strcmpMin(dlf,pname)==0) {
			int pct=dlBytes*100/expectBytes;
			if(pct>100) { pct=100; }
			util->setInt("pctDown",pct);
#define PCTOFS 7
			int slen=strlen(out);
			int pctofs=pct*((slen>PCTOFS?slen:PCTOFS)-PCTOFS)/100;
			screenWrite(row,isWide?1:0,out);
			attrOn(A_REVERSE);
			//syslog(LOG_ERR,"pctofs+PCTOFS=%d,pctofs=%d,PCTOFS=%d,slen=%d,pct=%d,dlBytes=%d,expectBytes=%d",pctofs+PCTOFS,pctofs,PCTOFS,slen,pct,dlBytes,expectBytes);
			out[pctofs+PCTOFS]=0;
			char msg[1024];
			snprintf(msg,sizeof(msg),"%d of expected %d bytes of '%s' downloaded.",dlBytes,expectBytes,dlf);
			row=screenWriteUrl(row,isWide?1+PCTOFS:0+PCTOFS,&out[PCTOFS],msg,"");
			attrOff(A_REVERSE);
		}
		else { screenWrite(row++,isWide?1:0,out); }
		int detailStatus=getIT("detailStatus");
		unsigned int detailRow=getIT("detailRow");
		if(detailStatus==1 && (row-1)==detailRow) {
			int dc=0;
			while(truncated!="") {
				snprintf(out,sizeof(out),"       %s",truncated.c_str());
				truncated=fixline(out,sizeof(out)); // Space padded, truncated to width
				screenWrite(row++,isWide?1:0,out);
				dc++;
			}
			setIT("detailCount",dc);
		}
		attrOff(A_REVERSE);
		if(row>=maxy-(isWide?1:0)) { break; }
	}
MARK
	return row;
}

void Out::initColors() {
	if(has_colors()) {
		start_color();
		/* Enables basic 8 colors on black background. */
		init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_WHITE);
		init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_WHITE);
		init_pair(COLOR_RED, COLOR_RED, COLOR_WHITE);
		init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_WHITE);
		init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_WHITE);
		init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_WHITE);
		init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_WHITE);
		init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_WHITE);
		attrOn(/*A_REVERSE|*/COLOR_PAIR(1));
		// 0=black,1=red,2=green,3=yellow,4=blue,5=purple,6=cyan
	}
}

// Returns truncated portion of line as a C++ string
// Also saves marked menu items and removes the marking braces {}
string Out::fixline(char *out, int size) {
	if(!out) { return ""; }
	/** Save marked menu items **/
	string str1(out);
	int nbraces=0;
	string::size_type loc=-1;
	curlineMenus.clear();
	do {
		loc=str1.find("{",0);
		if(loc != string::npos) {
			str1.erase(loc,1); // Remove opening brace
			string::size_type loc2=str1.find("}",loc);
			if(loc2 != string::npos) {
				str1.erase(loc2,1); // Remove closing brace
				curlineMenus[loc-nbraces]=str1.substr(loc,loc2-loc); // Save text between braces for this screen column
				//syslog(LOG_ERR,"menus[%d][%d]=%s",yy,xx+loc-nbraces,menus[yy][xx+loc-nbraces].c_str());
				nbraces+=2;
			}
		}
	} while(loc!=string::npos);
	strncpy(out,str1.c_str(),size);
	/** Trucate portion of line as a C++ string **/
  int xwid=maxx;
	if(xwid<0) { xwid=0; }
  if(xwid>size) { xwid=size; }
  int olen=strlen(out);
	for(int xx=olen; xx<=xwid; xx++) { out[xx]=' '; }
  int xa=xwid-(isWide?2:0); // Was -1
	string str=olen>xa?string(&out[xa]):"";
  out[xa]=0;
	return str;
}

void Out::setProgressBar(char *bar, int barLen, int progress, int nearEnd, char ch) {
  bar[barLen]=0;
  for(int xa=0; xa<barLen; xa++) {
    if(xa<progress) {
      if(xa>=barLen-nearEnd) { bar[xa]='+'; }
      else { bar[xa]=ch; }
    }
    else { bar[xa]=' '; }
  }
}

