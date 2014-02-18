
#include "getsched.hh"

//#define dolog syslog(LOG_ERR

#define EXECDELAY 100000 /* 0.1 seconds */

Scheduled::Scheduled() {
	prevFill=0;
	util=new Util("getsched#1"); // Need separate util instance for each thread (to avoid locking issues)
	fprintf(stderr,"        line=%d Scheduled: CLOCKSKEW=%d\n",__LINE__,CLOCKSKEW);
MARK
	fprintf(stderr,"        line=%d tzofs=%ld,timezone=%ld,zone=%s\n",__LINE__,Util::tzofs,timezone,Util::zone);
MARK
  snprintf(playqdir,sizeof(playqdir),"%s/playq",Util::TEMPPATH);
	fillsRpy=fillsReq=NULL;
	
	Execute(NULL);
MARK
}

/*******************************************************************/
void Scheduled::initFiller() {
	syslog(LOG_INFO,"Scheduled::initFiller");
	if(fillsRpy) { fclose(fillsRpy); }
	fillsRpy=popen("./getfills.sh","r");
	if(!fillsRpy) { syslog(LOG_ERR,"./getfills failed: %s",strerror(errno)); exit(1); }
	sleep(1); // Allow getfills to remove and recreate named pipe
	if(fillsReq) { fclose(fillsReq); }
	fillsReq=fopen("/tmp/play3abn/fills","wb");
	if(!fillsReq) { syslog(LOG_ERR,"fopen /tmp/play3abn/fills failed: %s",strerror(errno)); exit(1); }	
}

strings Scheduled::getFiller(int maxLen) {
  syslog(LOG_INFO,"getFiller(maxLen=%d)",maxLen);
  if(!fillsReq || !fillsRpy) { initFiller(); }
  fprintf(fillsReq,"%d\n",maxLen); fflush(fillsReq);
  char buf[1024];
  char *rpy=fgets(buf, sizeof(buf), fillsRpy);
  if(!rpy) {
	  syslog(LOG_ERR,"fillsRpy: %s",strerror(errno));
	  initFiller();
  }
  int len=strlen(rpy);
  if(rpy[len-1]=='\n' || rpy[len-1]=='\r') { rpy[len-1]=0; }
  errno = 0;
  char *info=NULL;
  char *playAt=buf;
  char *pp=strchr(buf,'|');
  if(pp) { *pp=0; pp++; info=pp; }
  pp=strchr(info,'\n');
  if(pp) { *pp=0; }
  syslog(LOG_INFO,"getFiller: playAt=%s,info=%s",playAt,info);
  // NOTE:    infs.one=2014-01-22 19:00:00 -1
  // NOTE:    infs.two=BIBLESTORIES 1377 /tmp/play3abn/cache/Radio/Bible stories/Vol 01/V1-06a  The Disappearing Idols.ogg
  strings ent=strings(playAt,info);
  return ent;
}
/*******************************************************************/

/** NOTE: Get current file to play, or filler if there is none **/
strings Scheduled::getScheduled(bool& isFiller, time_t now) {
MARK
 // /**/fprintf(stderr,"        line=%d Scheduled::getScheduled#0\n",__LINE__);
	mkdir(playqdir,0777);
	vector<strings> playlist=util->getPlaylist(playqdir); // Fetch sorted list of queued files (may have more than one for same time slot)

	int hourElapsed=now%3600;
	int hourRemaining=(3600/*-15*/)-hourElapsed;

	strings ent=(playlist.size() > 0) ? playlist.front() : strings("","");
	bool hitNetworkID=(ent.two.substr(0,9) == "StationID");
	/** IS this EARLY or LATE? **/
	time_t playAt=0;
	int seclen=0;
	string dispname="";
	/** Parse info string **/
	//strings saveEnt=ent;
	//saveEnt.two=string(saveEnt.two.c_str());
//syslog(LOG_INFO,"FIRST one=%s,two=%s",ent.one.c_str(),ent.two.c_str());
	string catcode="";
	string decodedUrl="";
	int flag;
	int result=util->itemDecode(ent, playAt, flag, seclen, catcode, dispname, decodedUrl);
	if(result<0) {
	  syslog(LOG_ERR,"        line=%d Player::Execute: Invalid format (result=%d): %s => %s",__LINE__,result,ent.one.c_str(),ent.two.c_str());
	  return ent;
	}
//syslog(LOG_INFO,"SECND one=%s,two=%s,scheduleAgeDays=%d,seclen=%d",saveEnt.one.c_str(),saveEnt.two.c_str(), scheduleAgeDays, seclen);
	bool tooLate=(now > playAt+seclen);
	if(tooLate) { hitNetworkID=false; } // Skip it
	bool tooEarly=(now < playAt);

//fprintf(stdout,"hourRemaining=%d,nid=%d\n",hourRemaining,hitNetworkID);

	//bool hasProg=false;
	int silentFill=getITd("silentfill",0);
	char curplaytgt[1024];
  getST(curplaytgt);

	if(tooEarly) {
		int fillSecs=playAt-now;
		syslog(LOG_INFO,"TOO EARLY FOR SCHEDULE: FILLING (fillSecs=%d)",fillSecs);
		ent=getFiller(fillSecs); isFiller=true;
	}
	else if(playlist.size()==0 || (hitNetworkID && hourRemaining>20)) {
		syslog(LOG_INFO,"TOO EARLY FOR NETWORK ID: FILLING (hourRemaining=%d)",hourRemaining);
		ent=getFiller(hourRemaining-15); isFiller=true;
	}
	else if(silentFill>0) {
					syslog(LOG_ERR,"SILENCE FILL(%d): previous file=%s.",silentFill,curplaytgt);
					if(strstr(curplaytgt,"/download/")) { remove(curplaytgt); } /** DON'T KEEP SILENT FILES THAT WERE DOWNLOADED **/
					ent=getFiller(silentFill); isFiller=true;
	}
	else { syslog(LOG_INFO,"getScheduled: playqdir=%s: one=%s; two=%s",playqdir,ent.one.c_str(),ent.two.c_str());
		fprintf(stderr,"getScheduled: playqdir=%s: one=%s; two=%s\n",playqdir,ent.one.c_str(),ent.two.c_str());
	  isFiller=false;
	}
	return ent;
}

/** NOTE: Read a directory of links of the following form:
 * "2011-06-09 12:40:00 0066 IT'S A WONDERFUL DAY-Heritage Singers.ogg"
 * Each link specifies the date, time, length, and display name of a file to play.
 * The target of the link is the actual file to play.
 */
void Scheduled::Execute(void *arg) {
	int count=0;
        time_t prevTime=0;
//        time_t prevPlayAt=0;

MARK
	strings ent("","");
	fprintf(stderr,"        line=%d getsched:Scheduled::Execute: Starting filler loop\n",__LINE__);
	while(!Util::checkStop()) {
MARK
		time_t now=Time();
		if(now-prevTime == 0 && count>10) {  // NOTE: SLOW DOWN TO AVOID CPU OVERLOAD
			if(usleep(EXECDELAY)==-1) { sleep(1); }
			count=0;
		}
		prevTime=now;
		time_t schedTime;
		if(scanf("%ld",&schedTime) < 1) { break; }
		fprintf(stderr,"getsched: Requesting schedule for time %ld\n",schedTime);
		char playlink[1024]={0};
MARK
		bool isFiller=false;
		time_t playAt=0;
		int seclen=0;
		string dispname="";
		int elapsed=0;
		int remaining=0;
		string catcode="";
		string decodedUrl="";
		int flag;
		bool tooLate=false;
		do {
			ent=getScheduled(isFiller,now);
			/** Parse info string **/
			int result=util->itemDecode(ent, playAt, flag, seclen, catcode, dispname, decodedUrl);
			if(result<0) {
			  fprintf(stderr,"        line=%d getsched::Execute: Invalid format1 (result=%d): %s => %s\n",__LINE__,result,ent.one.c_str(),ent.two.c_str());
			  continue;
			}
			//const char *pname=dispname.c_str();
			if(!isFiller) { snprintf(playlink,sizeof(playlink),"%s/%s",playqdir,ent.one.c_str()); }
			else { playlink[0]=0; }		
			const char* ppath=ent.two.c_str();
			elapsed=(now-playAt);
			remaining=seclen-elapsed;
/**
Jan 23 16:41:38 warrenlap getsched: Execute: playqdir=/tmp/play3abn/tmp/playq: one=2014-01-22 22:59:45 -1; two=StationID 0006 /tmp/play3abn/cache/Radio/Headers/1header.ogg
Jan 23 16:41:38 warrenlap getsched: Scheduled::Execute: CATCH UP (remaining=-63707,seclen=6,elapsed=63713,now=2014-01-23 16:41:38,playAt=2014-01-22 22:59:45): /tmp/play3abn/cache/R
**/
			tooLate=(remaining<10 && !isFiller); // || (remaining<1 && isFiller);
//fprintf(stdout,"remaining=%d,isFiller=%d,now=%ld,playAt=%ld,seclen=%d,elapsed=%d\n",remaining,isFiller,now,playAt,seclen,elapsed);
			if(tooLate) {
				syslog(LOG_ERR,"Scheduled::Execute: CATCH UP (remaining=%d,seclen=%d,elapsed=%d,now=%s,playAt=%s): %s; playlink=%s",
					remaining,seclen,elapsed,util->fmtTime(now).c_str(),util->fmtTime(playAt).c_str(),ppath,playlink);
				if(remove(playlink) < 0) { syslog(LOG_ERR,"Execute: remove(%s): %s",playlink,strerror(errno)); sleep(1); }
				continue;
			}
		} while(tooLate);
//fprintf(stderr,"isFiller=%d,tooLate=%d\n",isFiller,tooLate);
		printf("%s|%s\n",ent.one.c_str(),ent.two.c_str());
		fflush(stdout);
		sleep(1);
	}
MARK
}

int main(int argc, char **argv) {
	new Scheduled();
}
