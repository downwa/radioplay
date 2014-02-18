//#include "outscreen.hh"
//#include "util.hh"

#include "play3abn.hh"

//char Play3ABN::procs[6][128]={"","mplay","mdown","src-3abn","mscreen",""};
/** LIBRARY DEPENDENCIES (need to restart binary if library changed):
   mplay:    libdecoder.so, libradioutil.so
   mdown:                   libradioutil.so
	 src-3abn: libdecoder.so, libradioutil.so
	 mscreen:                 libradioutil.so
*/
time_t Play3ABN::procDates[6]={0,0,0,0,0,0};
char Play3ABN::tty[32]="/dev/tty";
//char Play3ABN::rxadiodir[256]="";

char timeBuf[21];
const char *T() {
	time_t t=time(NULL);
	struct tm tms;
  struct tm* tptr=localtime_r(&t,&tms);
	snprintf(timeBuf,20,"%04d-%02d-%02d %02d:%02d:%02d",
		1900+tptr->tm_year,tptr->tm_mon+1,tptr->tm_mday,tptr->tm_hour,tptr->tm_min,tptr->tm_sec);
	return timeBuf;
}
Play3ABN::Play3ABN(int argc, char **argv) {
	tzofs=checkedTime=0;
	syslog(LOG_ERR,"play3abn: LOGDIR=%s",LOGDIR);
	if(mkdir(LOGDIR,0770)==-1 && errno!=EEXIST) { syslog(LOG_ERR,"mkdir(%s): %s",LOGDIR,strerror(errno)); abort(); }
	if(mkdir(CURLOGS,0770)==-1 && errno!=EEXIST) { syslog(LOG_ERR,"mkdir(%s): %s",CURLOGS,strerror(errno)); abort(); }
	if(mkfifo(LOGFIFO,0600)==-1 && errno!=EEXIST) { syslog(LOG_ERR,"%s: mkfifo(%s): %s",T(),LOGFIFO,strerror(errno)); abort(); }
	Start(NULL);
	util=new Util("play3abn",true);
	//checkedTime=getI("date"); // NOTE: Setting this up front prevents us from setting the time within first hour after boot (not what we want to do)
	setIT("doLogStderr",1);
	syslog(LOG_ERR,"________________________________________");
	syslog(LOG_ERR,"play3abn - (c) 2008-2012 Warren E. Downs");
	syslog(LOG_ERR,"========================================");
	/** IF current date is less than or equal to at last run, updateDate **/
	time_t now=time(NULL);
	time_t was=getId("date",0);
	if(was>now) { now=was; } /** TIME CANNOT MOVE BACKWARD **/
	setI("date",now);
  const char *qq=strrchr(argv[0],'/');
	qq=qq?&qq[1]:argv[0];
	getS(runprocs);
	if(!runprocs[0]) { setS("runprocs","mplay,mdown,src-3abn,lrucache,mscreen"); }
	//strncat(runprocs,",",sizeof(runprocs)-strlen(runprocs)-1);
	//strncat(runprocs,qq,sizeof(runprocs)-strlen(runprocs)-1);
	syslog(LOG_ERR,"runprocs: %s",runprocs);
	mypid=getpid();
	if(argc>1) {
		char *proc,*rp=runprocs;
		if(strcmp(argv[1],"--stop")==0) {
			syslog(LOG_ERR,"mypid=%d",mypid);
			syslog(LOG_ERR,"Stopping play3abn processes with SIGTERM...");
			while((proc=strtok(rp,",")) != NULL) { rp=NULL; SendSignal(proc,SIGTERM); }
			sleep(2);
			syslog(LOG_ERR,"Stopping play3abn processes with SIGKILL...");
			sleep(2);
			rp=runprocs;
			while((proc=strtok(rp,",")) != NULL) { rp=NULL; SendSignal(proc,SIGKILL); }
			exit(0);
		}
		else if(strcmp(argv[1],"--list")==0) {
			syslog(LOG_ERR,"play3abn processes:");
			int xa=0;
			while((proc=strtok(rp,",")) != NULL) {
				vector<pid_t> pids=pidsof(proc); rp=NULL;
				for(unsigned int xb=0; xb<pids.size(); xb++) {
					if(pids[xa]==mypid) { continue; }
					syslog(LOG_ERR,"  >>%s (%d)",proc,pids[xb]);
				}
				xa++;
			}
			syslog(LOG_ERR,"EXITING");
			exit(0);
		}
		else {
			strncpy(tty,argv[1],sizeof(tty));
		}
	}
	char *argv0=argv[0];
	syslog(LOG_ERR,"argv0: %s",argv0);
	pid_t pid=pidof(argv0);
	if(pid>-1 && pid!=mypid) {
		syslog(LOG_ERR,"Already running: pid=%d (mypid=%d)",pid,mypid); sleep(1); exit(1);
	}
	strncpy(basedir,argv0,sizeof(basedir));
	char *pp=strrchr(basedir,'/');
	if(pp) { *pp=0; }
	const char *tgt="/tmp/play3abn/basedir";
	char cmd[1024];
	snprintf(cmd,sizeof(cmd),"ln -snf $(realpath %s) %s",basedir,tgt);
	if(system(cmd)==-1) { syslog(LOG_ERR,"system: %s: %s",cmd,strerror(errno)); }
	else {
		ssize_t ofs=readlink(tgt, basedir, sizeof(basedir)-1);
		if(ofs==-1) { syslog(LOG_ERR,"readlink: %s: %s",tgt,strerror(errno)); abort(); }
		basedir[ofs]=0;
		syslog(LOG_ERR,"basedir=%s",basedir);
	}
	mountCount=0;
MARK
	const char *cachedir="/tmp/play3abn/cache";
  DIR *dir=opendir(cachedir);
	if(dir) {
		struct dirent *entry;
		while((entry=readdir(dir))) {
			if(entry->d_name[0]=='.') { continue; }
			string tgt=string(cachedir)+"/"+string(entry->d_name);
			remove(tgt.c_str());
		}
		closedir(dir);
	}
	if(remove(cachedir)==-1 && errno!=ENOENT) {
		syslog(LOG_ERR,"rmdir(%s): %s.",cachedir,strerror(errno));
	}		
	findDevices();
MARK
	//initDirs(mnt);
MARK
	MainLoop();
MARK
	sleep(2);
}

/** Returns: -1 if error (check errno), 1 if matching line, 0 if no matching line found. **/
int Play3ABN::getMatchingLine(const char *filename, int bufsiz, char *buffer, const char *match) {
MARK
	FILE *fp=fopen(filename,"rb");
	if(!fp) { return -1; }
MARK
	while(fgets(buffer, bufsiz, fp)) {
MARK
		if(strncmp(buffer,match,strlen(match))==0) { fclose(fp); return 1; }
	}
MARK
	fclose(fp);
	buffer[0]=0;
MARK
	return 0;
}

/** Returns the sum of space-separated values in specified line, starting and ending with given column numbers **/
int Play3ABN::sumLine(char *linebuf, int startPos, int endPos) {
	char *str=linebuf,*token=linebuf,*saveptr;
	int sum=0;
	for(int xa=1; xa<=endPos && token; xa++,str=NULL) {
		token=strtok_r(str, "\t\r\n ", &saveptr); //str=NULL;
		if(token && xa>=startPos && xa<=endPos) sum+=atoi(token);
	}
	return sum;
}

/** Given a pid, sums the elapsed jiffies used by the process
 *  If pid is -1, sums the elapsed jiffies used by all processes **/
int Play3ABN::sumJiffies(pid_t pid) {
	char procstat[256];
	char procname[256];
	const char *match="";
	int aa=14,bb=15;
	if(pid==-1) { snprintf(procname,sizeof(procname),"/proc/stat"); match="cpu "; aa=2; bb=20; }
	else { snprintf(procname,sizeof(procname),"/proc/%d/stat",pid); }
	if(getMatchingLine(procname,sizeof(procstat),procstat,match)!=1) { return 0; }
	return sumLine(procstat,aa,bb); // $(cat /proc/stat | grep "^cpu " | sed -e 's/cpu *//g' -e 's/ /+/g' | bc)
}

float Play3ABN::percentCpu(pid_t pid) {
	//int total_jiffies_1=sumJiffies(-1); // $(cat /proc/stat | grep "^cpu " | sed -e 's/cpu *//g' -e 's/ /+/g' | bc)
	//int time1=sumJiffies(pid); // $(cat /proc/$(pidof $watch)/stat | awk '{print $14"+"$15}' | bc)
	//sleep(1);
	int total_jiffies_2=sumJiffies(-1); // $(cat /proc/stat | grep "^cpu " | sed -e 's/cpu *//g' -e 's/ /+/g' | bc)
	int time2=sumJiffies(pid); // $(cat /proc/$(pidof $watch)/stat | awk '{print $14"+"$15}' | bc)
	float total_over_period=total_jiffies_2-total_jiffies_1[pid];
	float workproc=time2-time1[pid];
	total_jiffies_1[pid]=total_jiffies_2;
	time1[pid]=time2;
	return 100.0*workproc/total_over_period;
}

int Play3ABN::SendSignal(const char *progname, int signum) {
	vector<pid_t> pids=pidsof(progname);
	int ret=-2; // Below, ESRCH could indicate a zombie, in which case we want to continue the loop in case there are other non-zombies
	//syslog(LOG_ERR,"  %s (%d instances)",progname,pids.size());
	for(unsigned int xa=0; xa<pids.size() && (ret!=-1 || errno!=ESRCH); xa++) {
		if(pids[xa]==mypid) { continue; }
		syslog(LOG_ERR,"    Killing %s (%d)",progname,pids[xa]);
		ret=kill(pids[xa],signum); }
		return ret;
}

void Play3ABN::RunBackground(const char *cmdname) {
	char cmd[1024]={0};
	if(strcmp(cmdname,"mscreen")==0) {
	  char logpath[256];
	  snprintf(logpath,sizeof(logpath),"/tmp/play3abn/curlogs/out-%s.log.txt",cmdname); // log path
	  snprintf(cmd,sizeof(cmd),"mv -f %s.bak %s.bak2; mv -f %s %s.bak; %s/%s %s >%s 2>&1 >%s <%s &",
					    logpath,logpath,logpath,logpath, // Rename the previous two logs for this command
					    basedir,cmdname,tty,logpath,tty,tty); // Run command with logging
	}
	else {
	  if(!cmd[0]) { snprintf(cmd,sizeof(cmd),"%s/%s %s >/tmp/play3abn/curlogs/out-%s.log.txt 2>&1 &",basedir,cmdname,tty,cmdname); }
	}
	//syslog(LOG_NOTICE,"system(%s)",cmd);
	if(system(cmd)==-1) { syslog(LOG_ERR,"system: %s: %s",cmd,strerror(errno)); }
	else { syslog(LOG_ERR,"Started: %s/%s %s (Logged to /tmp/play3abn/curlogs/out-%s.log.txt)",basedir,cmdname,tty,cmdname); }
}

FILE* outlog=NULL;
volatile bool logOpen=false;
time_t logopenAt=0;
void Play3ABN::logline(const char* msg) {
	time_t now=time(NULL);
	// If FIFO caught up or last message was a while ago, close, rotate, and reopen log file
MARK
	if(!logOpen || outlog==NULL || now-logopenAt>10) {
MARK
		if(outlog!=NULL) { fclose(outlog); outlog=NULL; }

		/** Rotate now while closed **/
		off_t maxSize=1000000; //:100000); // 1 Mb debug, 100k non-debug
		struct stat statbuf;
		if(stat(LOGFILE, &statbuf)!=-1) {
			bool isLarge=(statbuf.st_size>maxSize); // Larger than this, we rotate
			if(isLarge) {
				char oldpath[1024];
				strncpy(oldpath,LOGFILE,sizeof(oldpath));
				char *pp=strstr(oldpath,".log");
				if(pp) { strncpy(pp,".old.txt",sizeof(oldpath)-(pp-oldpath)); }
				//syslog(LOG_ERR,"unlink(%s)",oldpath);
				unlink(oldpath);
				//syslog(LOG_ERR,"path=%s,oldpath=%s",path,oldpath);
				rename(LOGFILE,oldpath);
			}
		}

MARK
		/** Now open main log again **/
		outlog=fopen(LOGFILE,"ab");
		if(outlog==NULL) { syslog(LOG_ERR,"%s: fopen(%s): %s\r",T(),LOGFILE,strerror(errno)); exit(3); }
		logopenAt=now; logOpen=true;
MARK
	}
MARK
	int doLogStderr=getIT("doLogStderr");
MARK
	const char *t=T();
MARK
	if(doLogStderr) { syslog(LOG_ERR,"%s: %s\r",t,msg); } // Message includes LF (so we prefix CR)
MARK
	fprintf(outlog,"%s: %s",t,msg);
MARK
}

/** This thread is the logger.  It creates a fifo which it then opens for read and
 *  periodically flushes to a file (and optionally to the screen) **/
void Play3ABN::Execute(void *arg) {
	logline("Play3ABN Log Starting");
MARK
	char prevmsg[1024]={0};
	while(!Util::checkStop()) {
		FILE *inlog=fopen(LOGFIFO,"r+");
		if(!inlog) { perror("fopen r+ " LOGFIFO); exit(2); }
		char msg[1024]={0};
		while(!feof(inlog) && !ferror(inlog) && fgets(msg,sizeof(msg),inlog)) {
			if(msg[0] && msg[0]!='\n' && strcmp(msg,prevmsg)!=0) { logline(msg); }
		}
		strncpy(prevmsg,msg,sizeof(prevmsg));
		if(outlog) { fclose(outlog); outlog=NULL; }
		logOpen=false; // Close every time we've caught up with all the messages in the FIFO
		sleep(1);
MARK
		fclose(inlog);
MARK
		util->logRotate(false); // Rotate all the other logs if they are oversize.
MARK
	}
	logline("Play3ABN::Execute done");
}
void Play3ABN::tzsetup() {
  tzset();
	tzofs=timezone;
  time_t startTime=time(NULL);
	struct tm tms;
  struct tm* tptr=localtime_r(&startTime,&tms);
	if(strcmp(tptr->tm_zone,"AKDT")==0) { /*syslog(LOG_ERR,"DST is in effect.");*/ tzofs-=3600; } // DST in effect
}
void Play3ABN::updateDate(bool isBoot) {
	if(isBoot) { syslog(LOG_INFO,"\rSetting date...\r"); }
	syslog(LOG_ERR,"updateDate");
	char cmd[1024];
	snprintf(cmd,sizeof(cmd),"./ntpdate.sh %s 2>/tmp/play3abn/ntpdate.log",isBoot?"-b":"");
	if(system(cmd)<0) { syslog(LOG_ERR,"setDate failure: %s",cmd); }
}

void Play3ABN::MainLoop() {
	syslog(LOG_ERR,"Play3ABN::MainLoop");
	const char *flag="/tmp/play3abn/tmp/.mediachange.flag";
	int hogtries[10]={0,0,0,0,0,0,0,0,0,0};
	struct stat statbuf;
	bool isBoot=true;
	time_t updatedAt=time(NULL);
MARK
	//int count=0;
	while(!Util::checkStop()) {
MARK
		time_t now=time(NULL);
		time_t was=getId("date",0);
		if(was>now && now<86400/*Less than 1 day since power on */) { /** ONLY DO THIS ON COLD BOOT **/
				syslog(LOG_ERR,"Play3ABN::Execute: now=%ld,was=%ld",now,was);
				now=was; 
				struct timeval tv;
				tv.tv_sec=now;
				tv.tv_usec=0;
				if(settimeofday(&tv,NULL)<0) { syslog(LOG_ERR,"Play3ABN::Execute: Unable to initialize time: %s",strerror(errno)); }
		} /** TIME CANNOT MOVE BACKWARD **/
		checkedTime=getId("ntpdateat",0);
		if(abs(now-checkedTime) > 3600 || now<86400) { updateDate(isBoot); isBoot=false; }
		tzsetup(); // Re-update timezone settings in case the date changed as well as the time
		/** RECORD DATE in file and HWCLOCK **/
MARK
		setI("date",now=time(NULL));
#define HWCLOCK "hwclock --systohc --local "
		char cmd[1024];
		snprintf(cmd,sizeof(cmd),HWCLOCK ">%s/hwclock.log.txt 2>&1",CURLOGS);
		if(system(cmd)<0) { syslog(LOG_ERR,"Play3ABN::MainLoop: HWCLOCK failure: %s",cmd); }
		/** SET VARIABLES for display to process */
		util->setIP();
		setST("myip",Util::myip);
		util->setConn();
		//syslog(LOG_DEBUG,"Media Check");
MARK
		if(!util->isMounted() || stat(flag, &statbuf)!=-1) { // Check for media change flag (set by user interface action)
			syslog(LOG_INFO,"Media Check:isMounted=%d,flag=%d",util->isMounted(),stat(flag, &statbuf));
MARK
			remove(flag);
MARK
			findDevices();
MARK
			//initDirs(mnt);
MARK
		}
MARK
		getS(runprocs);
		if(!runprocs[0]) { setS("runprocs","mplay,mdown,src-3abn,mscreen"); }
		//syslog(LOG_DEBUG,"Process Watchdog: runprocs=%s",runprocs);
		//syslog(LOG_ERR,"Play3ABN::Execute: Process watchdog running...");
		//syslog(LOG_ERR,"-");
MARK
		char *proc,*rp=runprocs;
		int xa=-1;
		while((proc=strtok(rp,",")) != NULL) {
			rp=NULL; xa++;
MARK
			const char *realproc=proc;
//#ifndef ARM
//			if(strcmp(proc,"mplay")==0) { realproc="mplay.bin"; }
//#endif
			memset(&statbuf,0,sizeof(statbuf));
			if(stat(realproc, &statbuf)==-1) {
				syslog(LOG_ERR,"Play3ABN:Missing proc: %s",proc);
			}
MARK
			time_t procDate=statbuf.st_mtime;
			if(strncmp(realproc,"mplay",5)==0 || strcmp(realproc,"src-3abn")==0) {
				if(stat("libdecoder.so", &statbuf)==-1) { syslog(LOG_ERR,"Play3ABN:Missing libdecoder.so"); }
				else { procDate=statbuf.st_mtime>procDate?statbuf.st_mtime:procDate; } // Take newer time to check for restart
			}
MARK
			if(stat("libradioutil.so", &statbuf)==-1) { syslog(LOG_ERR,"Play3ABN:Missing libradioutil.so"); }
			else { procDate=statbuf.st_mtime>procDate?statbuf.st_mtime:procDate; } // Take newer time to check for restart
			pid_t pid=pidof(proc);
			//syslog(LOG_ERR,"pidof(%s)=%d",proc,pid);
			float pct=0.0;
			if(pid!=-1) {
				pct=percentCpu(pid);
				if(pct>HOGPCT) { syslog(LOG_ERR,"CPU hog: %-10s: %5.2f%%",proc,pct); }
			}
MARK
			if(pct>HOGPCT) { hogtries[xa]++; }
			if(pid==-1 || procDates[xa]<procDate || (pct>HOGPCT && hogtries[xa]>5)) {
MARK
				hogtries[xa]=0;
				procDates[xa]=procDate;
				if(pid!=-1) {
					syslog(LOG_ERR,"Stopping %s process %s with SIGTERM...",pct>HOGPCT?"CPU hog":"outdated",proc);
					SendSignal(proc,SIGTERM);
					for(int xb=0; xb<20; xb++) {
						pid=pidof(proc);
						if(pid!=-1) { break; }
						usleep(1000);
					}
					if(pid!=-1) {
						syslog(LOG_ERR,"Stopping %s process %s with SIGKILL...",pct>HOGPCT?"CPU hog":"outdated",proc);
						SendSignal(proc,SIGKILL);
					}
MARK
				}
MARK
				if(pct>HOGPCT) { sleep(1); } // Wait a bit before restarting a CPU hog
MARK
				if(!Util::checkStop()) { RunBackground(proc); sleep(1); } // Sleep to give time for proc to start
MARK
				pid=pidof(proc);
				total_jiffies_1[pid]=time1[pid]=0;
				percentCpu(pid);
MARK
			}
		}
MARK
		if(now-updatedAt > 3600) { // If it's been more than an hour since last update
			int mod=(now%3600);
			if(mod>300 && mod<1800) { // Update after first 5 min and before first half of hour
				syslog(LOG_ERR,"Starting update.sh");
				if(system("./update.sh </dev/null")==-1) { syslog(LOG_ERR,"rowAction:system(./update.sh): %s",strerror(errno)); }
				updatedAt=now;
			}
		}
		//syslog(LOG_DEBUG,"Sleeping 10...[%d]",count);
		sleep(10);
		//syslog(LOG_DEBUG,"DONE Sleeping 10.[%d]",count++);
	}
	syslog(LOG_ERR,"Play3ABN::Execute: ENDED");
}

pid_t Play3ABN::pidof(const char *pname) {
	vector<pid_t> pids=pidsof(pname,1);
	if(pids.size()==1) { return pids[0]; }
	return -1;
}

/** NOTE: Returns list of pids for given process name (path ignored).  If limit > 0, only the first [limit] pids are returned **/
vector<pid_t> Play3ABN::pidsof(const char *pname, int limit) {
	vector<pid_t> thepids;
	if(!pname || !pname[0]) { return thepids; } // Not waiting on any process
  //pid_t mypid=getpid();
  DIR *dir=opendir("/proc");
	if(!dir) { syslog(LOG_ERR,"pidof: /proc filesystem must be mounted: %s",strerror(errno)); sleep(1); return thepids; }
	struct dirent *entry;
	pid_t pid=-1;
	int count=0;
MARK
	while((entry=readdir(dir))) {
		pid=atoi(entry->d_name);
		if(pid==0) { continue; }
		char fname[256];
		char cmdline[1024];
		snprintf(fname,sizeof(fname)-1,"/proc/%d/cmdline",pid);
		FILE *fp=fopen(fname,"rb");
		if(!fp) { continue; }
		cmdline[0]=0;
		if(fread(cmdline, 1, sizeof(cmdline), fp)<1) { fclose(fp); continue; }
		char *pp=strrchr(cmdline,'/');
		if(!pp) { pp=cmdline; }
		else { pp++; }
		const char *qq=strrchr(pname,'/');
		if(!qq) { qq=pname; }
		else { qq++; }
		if(strcmp(pp,qq)==0 /*&& pid!=mypid*/) { // basename portion should match exactly
			thepids.push_back(pid);
			count++;
			if(limit>0 && count>=limit) { fclose(fp); break; }
		}
		fclose(fp);
	}
MARK
	closedir(dir);
MARK
	return thepids;
}
/*void Play3ABN::initDir(string src, string tgt) {
MARK
	const char *srcdir=src.c_str();
	if(!srcdir[0]) { return; }
	const char *tgtdir=("/tmp/pxlay3abn/"+tgt).c_str();
MARK
	if(remove(tgtdir)==-1 && errno!=ENOENT) {
		syslog(LOG_ERR,"rmdir(%s): %s.",tgtdir,strerror(errno));
	}
MARK
	if(symlink(srcdir, tgtdir)==-1) {
		syslog(LOG_ERR,"symlink(%s,%s): %s.",srcdir,tgtdir,strerror(errno));
	}
MARK
}
*/
/** NOTE: initDirs should be called at start and at each audio source change **/
/*void Play3ABN::initDirs(char *mnt) {
	if(!mnt) { syslog(LOG_ERR,"initDirs: NOTHING MOUNTED"); return; }
MARK
	if(strstr(mnt,"/Radio")) { fillerBasedir     =string(Play3ABN::rxadiodir)+"/Headers"; }
	else { fillerBasedir     =string(Play3ABN::rxadiodir)+"/filler"; }
MARK
	schedulesBasedir  =string(Play3ABN::rxadiodir)+"/schedules";
MARK
	downloadBasedir   =string(Play3ABN::rxadiodir)+"/download";
MARK
	newsweatherBasedir=string(Play3ABN::rxadiodir)+"/newsweather";
MARK
	cacheBasedir      =downloadBasedir.substr(0,downloadBasedir.rfind('/',string::npos)); // equivalent of dirname of download dir.
MARK
	logsBasedir       =(cacheBasedir!="")?(cacheBasedir+"/logs"):"/tmp";
MARK
	syslog(LOG_NOTICE,"initDirs: fillerBasedir     =%s.",fillerBasedir.c_str());           initDir(fillerBasedir,"filler");
MARK
	syslog(LOG_NOTICE,"initDirs: schedulesBasedir  =%s.",schedulesBasedir.c_str());     initDir(schedulesBasedir,"schedules");
MARK
	syslog(LOG_NOTICE,"initDirs: downloadBasedir   =%s.",downloadBasedir.c_str());       initDir(downloadBasedir,"download");
MARK
	syslog(LOG_NOTICE,"initDirs: newsweatherBasedir=%s.",newsweatherBasedir.c_str()); initDir(newsweatherBasedir,"newsweather");
MARK
	syslog(LOG_NOTICE,"initDirs: cacheBasedir      =%s.",cacheBasedir.c_str());             initDir(cacheBasedir,"cache");
MARK
	syslog(LOG_NOTICE,"initDirs: logsBasedir       =%s.",logsBasedir.c_str());               initDir(logsBasedir,"logs");
MARK
}*/

/*bool Play3ABN::tryMount(char *trydev) {
	char *mnt=(char *)"";
	return tryMount(trydev,mnt);
}
*/
/** NOTE: Tries to mount specified trydev.  Return true if success or already mounted **/
/** NOTE: Unmounts the device and returns if denyMount is set **/
/** NOTE: Ignores mount if device is already mounted (possibly at another mount point than we would have chosen) **/
bool Play3ABN::tryMount(char *trydev, dev_t dev, char*& mnt) {
	char buf[1024]={0};
	FILE *fp=fopen("/proc/mounts","rb");
	while(fp && !feof(fp) && !ferror(fp) && fgets(buf,sizeof(buf),fp)) {
		char *pp=strchr(buf,' ');
		if(pp) { *pp=0; }
		if(strcmp(buf,trydev)==0) { 
			pp++;
			char *qq=strchr(pp,' ');
			if(qq) { *qq=0; }
			strcpy(mntPoint,pp);
			mnt=mntPoint;
			return false; // Already mounted at some mountpoint for this device
		}
	}
	if(fp) { fclose(fp); }
	/** CHOOSE mount point based on trydev:
	 * /dev/sr*  --> /cdrom
	 * /dev/sd* --> /udisk
	 * /dev/mmc* --> /sdcard
	 */
	dev_t mDev;
	strcpy(mntPoint,"/sdcard");
	for(int mNum=0; mNum<256; mNum++) {
		if(strncmp(trydev,"/dev/sr",7)==0) { strcpy(mntPoint,"/cdrom"); }
		else if(strncmp(trydev,"/dev/sd",7)==0) { strcpy(mntPoint,"/udisk"); }
		else if(strncmp(trydev,"/dev/mmc",8)==0) { strcpy(mntPoint,"/sdcard"); }
		char postfix[4]={""};
		if(mNum>0) {
			snprintf(postfix,sizeof(postfix),"%d",mNum);
			strncat(mntPoint,postfix,sizeof(mntPoint)-5);
			mntPoint[sizeof(mntPoint)-1]=0;
		}
		if(!util->isMounted(mntPoint,mDev)) { break; } // This one's free (not mounted)
		if(mDev==dev) { mnt=mntPoint; return false; } // Already mounted this mountpoint for this device
		// Repeat: If mount point is mounted (but for another device), increment postfix number
	}
	int denyMount=util->getInt("denymount",0);
	if(!trydev[0] || denyMount) { umount(mntPoint); mnt=(char *)""; return false; }
	
	/** MOUNT PROCESS **/
	mkdir(mntPoint,755);
	const char *type="vfat";
	const char *cvt="codepage=437";
	//mount -t vfat -o codepage=437 /dev/sdc1 mntPoint
	if(mount(trydev,mntPoint,type,0,cvt)==0 && util->isMounted()) {
		char cmd[256];
		setIT("extracting",1);
		snprintf(cmd,sizeof(cmd),"./exlencache.sh %s",mntPoint);
		setIT("extracting",0);
		if(system(cmd)==-1) { syslog(LOG_WARNING,"Error running ./exlencache.sh %s",mntPoint); }
		mnt=mntPoint; return true;
	}
	int err=errno;
	if(err!=EBUSY) {
		syslog(LOG_WARNING,"Error mounting %-10s (type=%s,cvt=%s) on %s: %s: isMounted=%d",trydev,type,cvt,mntPoint,strerror(err),util->isMounted(mntPoint,mDev));
	}
	mnt=(char *)"";
	return false;
}
// Find and return path to first SD node with label RADIO, and mount it
/** NEW VERSION needs to mount all RADIO label drives (and first SD node if none found). **/
void Play3ABN::findDevices() {
MARK
	FILE *fp=fopen("/proc/partitions","rb");
	char buf[1024]={0};
	bool gotTitle=false;
MARK
	while(fp && !feof(fp) && !ferror(fp) && fgets(buf,sizeof(buf),fp)) {
		if(!gotTitle) { gotTitle=true; continue; }
		char *str=buf,*token=buf,*saveptr;
		int devMajor=0;
		int devMinor=0;
		//int devSize=0;
		for(int xa=0; xa<4 && token; xa++,str=NULL) {
			token=strtok_r(str, "\t\r\n ", &saveptr); str=NULL;
			if(token && xa==0) { devMajor=atoi(token); }
			if(token && xa==1) { devMinor=atoi(token); }
			if(token && xa==2) { /* devSize=atoi(token); */}
			if(token && xa==3) { // 3=device name
				char trydev[32]={0};
				snprintf(trydev,sizeof(trydev),"/dev/%s",token);
				dev_t dev=makedev(devMajor, devMinor);
				int ret=mknod(trydev, 0644|S_IFBLK, dev);
				if(ret==-1 && errno!=EEXIST) { syslog(LOG_ERR,"mknod: %s: %s",trydev,strerror(errno)); trydev[0]=0; break; }
				char *mnt=(char *)"";
				bool mountedTry=tryMount(trydev,dev,mnt);
				if(mountedTry) { syslog(LOG_ERR,"tryMount(ed) %s on %s",trydev,mnt); }
			}
		}
	}	
MARK
	if(fp) { fclose(fp); }
MARK
	util->isMounted(true);
}
//******************************************************************************

int main(int argc, char **argv) {
		new Play3ABN(argc,argv);
		if(system("/bin/stty sane")==-1) {};
}
