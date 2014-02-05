#ifndef __TSCHEDLINEHH__
#define __TSCHEDLINEHH__

#include <string>
#include <list>

#include "thread.hh"
#include "util.hh"


using namespace std;

class TSchedLine {
	Util* util;
	
	public:
	string sPath;
	char path[256];
	string fillName;
	time_t realNow; // Actual date the schedule was intended for
	time_t plNow; // Calculated from strings sDate+sTime (Date we will play the schedule at)
	int len;
	string sName;
	string replacing;
	inline TSchedLine(Util* util, string realDate, string sDate, string sTime, int len, string sName, time_t plNow=0, bool lessHour=false, string replacing="") {
		this->util=util;
		this->replacing=replacing;
		this->fillName="";
		this->len=len; this->sName=sName;
		if(sDate!="" && sTime!="" && plNow==0) { plNow=TimeOf(util,sDate+" "+sTime); }
		if(lessHour) { plNow-=3600; }
		this->plNow=plNow;
		if(realDate!="" && sTime!="") { realNow=TimeOf(util,realDate+" "+sTime); }
		else { realNow=plNow; }
		//util->dolog("TSchedLine:plNow=%ld,sDate=%s,sTime=%s",plNow,sDate.c_str(),sTime.c_str());
	}
	inline string toString() {
		return printString(*this);
	}
	static inline time_t TimeOf(Util* util, string dateTime) {
		struct tm *tptr,timestruct={0};
		tptr=&timestruct;
		char *result=strptime(dateTime.c_str(),"%Y-%m-%d %H:%M:%S",tptr);
		if(result==NULL) { syslog(LOG_ERR,"TSchedLine::TimeOf(%s): invalid format",dateTime.c_str()); abort(); }
		else if(result[0]) { syslog(LOG_ERR,"TSchedLine:TimeOf(%s): Extra characters:%s.",dateTime.c_str(),result); }
		int tzHour=(timezone-Util::tzofs); // 0 or 3600 depending on whether DST is in effect
		return mktime(tptr)-tzHour;
	}
	static inline string printString(TSchedLine sLine) {
		char d1[20]; // YYYY-MM-DD HH:MM:SS
		char d2[20]; // YYYY-MM-DD HH:MM:SS
		char tmp[5];
		snprintf(tmp,sizeof(tmp),"%04d",sLine.len);		
		time_t playTime=sLine.plNow; //+bufferSeconds;
		time_t realTime=sLine.realNow; //+bufferSeconds;
		struct tm tms;
		struct tm* tptr=localtime_r(&realTime,&tms);
		snprintf(d1,sizeof(d1),"%04d-%02d-%02d %02d:%02d:%02d",
			1900+tptr->tm_year,tptr->tm_mon+1,tptr->tm_mday,tptr->tm_hour,tptr->tm_min,tptr->tm_sec);
		tptr=localtime_r(&playTime,&tms);
		snprintf(d2,sizeof(d2),"%04d-%02d-%02d %02d:%02d:%02d",
			1900+tptr->tm_year,tptr->tm_mon+1,tptr->tm_mday,tptr->tm_hour,tptr->tm_min,tptr->tm_sec);
		return string(d1)+","+string(d2)+","+string(tmp)+","+sLine.sName;
	}
};

#endif