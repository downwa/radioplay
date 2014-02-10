#include "src-3abn.hh"

const char* Src3ABN::cachewaitflag="/tmp/play3abn/tmp/.cachewait.flag";
Src3ABN::Src3ABN(bool testing) {
MARK
	lastRefill=0;
	util=new Util("src-3abn");
	if(!testing) {
		decoder=new Decoder(NULL,util); // Used to check SecLen() for each file (and cache it)
		InitCodeCategories();
		//Start(NULL); // Cache the length (in seconds) of each file
		Scheduler();
	}
}

char *Src3ABN::getBaseUrl() {
MARK
	for(int xa=0; xa<60; xa++) {
		util->get("baseurl",baseurl,sizeof(baseurl));
		if(baseurl[0]) { break; }
		sleep(1);
	}
	if(!baseurl[0]) { strncpy(baseurl,"http://files.3abn.com/Radio/Share/Alaska/",sizeof(baseurl)-1); }
	return baseurl;
}

/** PURPOSE: Cache the date, length (in seconds) of each file (sort by date, len, name) **/
/** ALSO   : Cache the name of each file with a symlink to date, len attribute info (in dstflagsdir) **/
/** NOTE   : File Meta-info must be cached to avoid accessing slow CF media (cached in ramdisk for speed) **/
void Src3ABN::cacheLRU_Name(const char *srcdir, const char *dstdir, const char *filldir, const char *dstflagsdir) {
	struct stat statbuf1;
	struct stat statbuf2;
	bool waitmsg=false;
MARK
	if(strstr(srcdir,"/schedules") || strstr(srcdir,"/logs")) { return; } // Don't try to cache audio in schedules/logs folder
	do {
		if(stat(srcdir, &statbuf1)==-1) {
			if(!waitmsg) { waitmsg=true; syslog(LOG_ERR,"Src3ABN::cacheLRU_Name:Waiting for creation of %s.",srcdir); }
			sleep(1); continue;
		}
		if(stat(dstdir, &statbuf2)==-1) {
			if(mkdir(dstdir,0777)<0) {
				syslog(LOG_ERR,"Src3ABN::cacheLRU_Name:mkdir(%s): %s; ABORTING in 5 minutes.",dstdir,strerror(errno)); util->longsleep(300); abort();
			}
			//break;
		}
	MARK
		//syslog(LOG_ERR,"Src3ABN::cacheLRU_Name:stat %s: %d",dstflagsdir,stat(dstflagsdir, &statbuf2));
		if(stat(dstflagsdir, &statbuf2)==-1) {
			sleep(1); // Give time for parent dir to be created if needed...
			if(mkdir(dstflagsdir,0777)<0) { syslog(LOG_ERR,"Src3ABN::cacheLRU_Name:mkdir#2(%s): %s",dstflagsdir,strerror(errno)); continue; }
		}
		break;
	} while(true);
	syslog(LOG_ERR,"Src3ABN::cacheLRU_Name:Filling %s from %s",dstdir,srcdir);

	DIR *dir=opendir(srcdir);
	if(!dir) { syslog(LOG_ERR,"Src3ABN::cacheLRU_Name:opendir(%s): %s",srcdir,strerror(errno)); sleep(1); return; }
	struct dirent *entry;
MARK
	int count=0;
	time_t prevTime=0;
	while((entry=readdir(dir))) {
		if(entry->d_name[0]=='.') { continue; } // Skip hidden files
		/** AVOID PEGGING CPU **/
		count++;
		time_t now=Time();
		if(now-prevTime < 2 && count>10) {  // NOTE: SLOW DOWN TO AVOID CPU OVERLOAD
			//if(usleep(NOPEGCPU)==-1) { sleep(1); }
			sleep(1); // Sleep one second to avoid pegging the CPU
			count=0;
		}
		prevTime=now;
		usleep(NOPEGCPU); // Sleep a while to avoid pegging the CPU
		char fullpath[256];
		snprintf(fullpath,sizeof(fullpath),"%s/%s",srcdir,entry->d_name);
		if(entry->d_type==DT_DIR || entry->d_type==DT_LNK) {
			cacheLRU_Name(fullpath,dstdir,filldir,dstflagsdir);
		}
		//syslog(LOG_ERR,"type=%d,file=%s",entry->d_type,entry->d_name);
		if(entry->d_type!=DT_REG) { continue; } // Skip non-regular files
		if(strstr(entry->d_name,".sh")) { continue; } // Skip hidden files
		/** Check whether already cached **/
		char cachedflag[256];
		snprintf(cachedflag,sizeof(cachedflag),"%s/%s",dstflagsdir,entry->d_name);
		struct stat statbuf;
		if(stat(cachedflag, &statbuf)!=-1 /*&& stat(cdpath, &statbuf)!=-1*/) { continue; } // Skip cached files
		if(stat(fullpath, &statbuf)==-1) { continue; } // Skip bad file
		/** Cache if not done yet **/
		float seclen=decoder->SecLen(fullpath); // Get seconds length from source path
		if(seclen<0) { continue; }
		char centry[256]; // Use dummy time to avoid duplicate entries
		if(seclen>7*60) {
			snprintf(centry,sizeof(centry),"%s/%s %04d %s-%s",dstdir,Util::fmtTime(statbuf.st_mtime).c_str(),(int)seclen,
				"GENCODE"/*genCode(srcdir,entry->d_name).c_str()*/,entry->d_name);
		}
		else { // FILLER MATERIAL, e.g.  2010-04-13 04:09:10 0634
			snprintf(centry,sizeof(centry),"%s/%s %04d %s",filldir,Util::fmtTime(0).c_str(),(int)seclen,entry->d_name);
		}
		if(symlink(fullpath, centry)<0 && errno!=EEXIST) { syslog(LOG_ERR,"Src3ABN::cacheLRU_Name#1:symlink(%s): %s",centry,strerror(errno)); }
		/** Mark as cached **/
		if(symlink(fullpath, cachedflag)<0 && errno!=EEXIST) { syslog(LOG_ERR,"Src3ABN::cacheLRU_Name#2:symlink(%s): %s",cachedflag,strerror(errno)); }
	}
MARK
	closedir(dir);
MARK
}

const char *Src3ABN::genCode(const char *codeSrc) {
	char *qq=tmpcode;
	const char *pp=codeSrc;
	// Stop at first digit (unless it is a '3' in first position, as in 3ABN)
	while(*pp && qq<&tmpcode[60] && (!isdigit(*pp) || (*pp=='3' && pp==codeSrc))) {
		if(isalpha(*pp)) { *qq=toupper(*pp); qq++; }
		pp++;
	}
	*qq=0;
	return tmpcode;
}
const char *Src3ABN::genCode(const char *dirName, const char *fileName) {
	const char *code="";
	if(strstr(dirName,"/download/")) { // From 3ABN Source
		code=genCode(fileName); /** DETERMINE CODE FROM FILENAME **/
	}
	
 	if(!code[0] && strstr(dirName,"Music")) { code="MUSIC"; }
 	if(!code[0] && strstr(dirName,"tories")) { code="STORY"; }
 	if(!code[0] && strstr(dirName,"Story")) { code="STORY"; }
 	if(!code[0] && strstr(dirName,"Christmas In My Heart")) { code="STORY"; }
 	if(!code[0] && strstr(dirName,"Nature in a Nutshell")) { code="STORY"; }
 	if(!code[0] && strstr(dirName,"Packey Pokey and sally")) { code="STORY"; }
 	if(!code[0] && strstr(dirName,"E G White")) { code="INSP"; }
 	if(!code[0] && strstr(dirName,"Sermon")) { code="SERMON"; }
 	if(!code[0] && strstr(dirName,"SCHOOL CLIPS")) { code="FILL"; }
 	if(!code[0] && strstr(dirName,"Signs adds and Quotes")) { code="FILL"; } 	
 	if(!code[0] && strstr(dirName,"Health tips")) { code="FILL"; }
 	if(!code[0] && strstr(dirName,"Bible Promises")) { code="FILL"; }
 	if(!code[0] && strstr(dirName,"KidZone")) { code="FILL"; }
 	if(!code[0] && strstr(dirName,"Headers")) { code="FILL"; }
 	if(!code[0] && strstr(dirName,"Egypt to Canaan")) { code="BIBLE"; }
 	if(!code[0] && strstr(dirName,"Health for a Lifetime")) { code="HEALTH"; }
 	if(!code[0] && strstr(dirName,"Smoking")) { code="HEALTH"; }
 	if(!code[0] && strstr(dirName,"Joe Crews")) { code="SERMON"; }
	if(!code[0] && !isdigit(*fileName)) { code=genCode(fileName); } /** DETERMINE CODE FROM FILENAME **/	
	if(!code[0]) {
		char tmpdir[1024];
		strncpy(tmpdir,dirName,sizeof(tmpdir)-1); tmpdir[sizeof(tmpdir)-1]=0;
		char *pp=NULL;
		do {
			pp=strrchr(tmpdir,'/'); /** DETERMINE CODE FROM DIRECTORY not having leading digits **/
			if(pp) { code=genCode(&pp[1]); *pp=0; }
		} while(pp && !code[0]);
	}
	/** FIXME: Use data from CodeCategories.txt, e.g.
	 **
AC Creation15
ADBP Prophecy60
BA BibleAnswers30
BAL BibleAnswersLive60
BLS BibleStories30
BMS Sermon30
	**
	***/
	
	if(strcmp(code,"AMAZINGFACTS")==0) { code="SERMON"; }
	if(strcmp(code,"AF")==0) { code="SERMON"; }
	if(strcmp(code,"BAL")==0) { code="SERMON"; }
	if(strcmp(code,"BA")==0) { code="SERMON"; }
	if(strcmp(code,"BIBLEFORRADIO")==0) { code="BIBLE"; }
	if(strcmp(code,"ALBRECHT")==0) { code="SERMON"; }
// 
	if(code[0]) {
		const char *category=codeCategories[code].c_str();
		if(!category[0]) { syslog(LOG_ERR,"UNKNOWN CODE: %s",code); category=code; }
		return category;
	}

	syslog(LOG_ERR,"code=%s;dirName=%s;fileName=%s",code,dirName,fileName);
	return "";
}

void Src3ABN::InitCodeCategories() {
	syslog(LOG_ERR,"InitCodeCategories");
	FILE *fp=fopen("CodeCategories.txt","rb");
	if(!fp) { syslog(LOG_ERR,"Unable to read CodeCategories.txt: %s",strerror(errno)); return; }
	unsigned int idx=0;
	while(!feof(fp) && fgets(tmpcats[idx],sizeof(tmpcats[idx]),fp)) {
		char *code=tmpcats[idx];
		if(idx<sizeof(tmpcats)/sizeof(tmpcats[0])) { idx++; }
		char *category=strchr(code,' ');
		if(category) {
			*category=0; category++;
			char *pp=strchr(category,'\r');
			if(pp) { *pp=0; }
			pp=strchr(category,'\n');
			if(pp) { *pp=0; }
		}
		codeCategories[string(code)]=string(category);
		//syslog(LOG_ERR,"InitCodeCategories[%d]=%s***%s***%s;VOP=%s",idx,code,category,codeCategories[code].c_str(),codeCategories["VOP"].c_str());
	}
	fclose(fp);
	//syslog(LOG_ERR,"InitCodeCategories:codeCategories[0]=%s",codeCategories[0]); // NOTE: Indexes are not numeric
//	for( map<string,string>::iterator iter = codeCategories.begin(); iter != codeCategories.end(); ++iter) {
//      syslog(LOG_ERR,"%s IS A %s",(*iter).first.c_str(),(*iter).second.c_str());
//  }
}
void Src3ABN::Execute(void* arg) {
MARK
MARK
	syslog(LOG_ERR,"Src3ABN:Execute:Caching LRU...");
	//time_t prevTime=0;
	string cachedir="/tmp/play3abn/cache";
	string lrucache=string(Util::TEMPPATH)+"/leastrecentcache";
	string lrucached=string(Util::TEMPPATH)+"/leastrecentcache/cached";
	string fillcache=string(Util::TEMPPATH)+"/fillcache";
	syslog(LOG_ERR,"Src3ABN::Execute:Maintaining list at %s from %s",lrucache.c_str(),cachedir.c_str());
	const char *srccache=cachedir.c_str();
	const char *dstdir=lrucache.c_str();
	const char *filldir=fillcache.c_str();
	const char *dstflagsdir=lrucached.c_str();
MARK
	if(mkdir(filldir,0755)<0) { syslog(LOG_ERR,"Src3ABN::Execute:mkdir(%s): %s",filldir,strerror(errno)); }

	//util->longsleep(3600); // DELAY to see if following is the problem
	//bool firstTime=true;
	while(!Util::checkStop()) {
		//struct stat statbuf2; // IF dstdir does not exist (or we remounted), cache it and all subdirectories (recursively)
		//if(!firstTime || stat(dstdir, &statbuf2)==-1) { 
		cacheLRU_Name(srccache,dstdir,filldir,dstflagsdir); //firstTime=false; }
		syslog(LOG_ERR,"remove1(%s)",cachewaitflag);
		remove(cachewaitflag);
		util->longsleep(1800);
MARK
		//while(!Util::checkStop() && getId("denymount",0)==0) { sleep(2); } // While mounted
		int fd=creat(cachewaitflag,0755);
		if(fd!=-1) { close(fd); }	
MARK
		//while(!Util::checkStop() && getId("denymount",0)==1) { sleep(2); } // While unmounted
MARK
	}
MARK
}


/** PURPOSE: Cache the date, length (in seconds) of each download and filler file (sort by date, len, name) **/
/** ALSO   : Cache the name of each download and filler file with a symlink to date, len attribute info (in dstflagsdir) **/
/** NOTE   : File Meta-info must be cached to avoid accessing slow CF media (cached in ramdisk for speed) **/
/*void Src3ABN::cacheLRU_Name2(string dirsrc, const char *srcdir, string dirdst, const char *dstdir, const char *dstflagsdir, bool& firstTime) {
	struct stat statbuf1;
	struct stat statbuf2;
	bool waitmsg=false;
	bool chgmsg=false;
MARK
	do {
		if(stat(srcdir, &statbuf1)==-1) {
			if(!waitmsg) { waitmsg=true; syslog(LOG_ERR,"Src3ABN::cacheLRU_Name:Waiting for creation of "+dirsrc+"."); }
			sleep(1); continue;
		}
		if(stat(dstdir, &statbuf2)==-1) {
			if(mkdir(dstdir,0777)<0) { syslog(LOG_ERR,"Src3ABN::cacheLRU_Name:mkdir(%s): %s",dstdir,strerror(errno)); abort(); }
			//break;
		}
		else if(statbuf1.st_mtime <= statbuf2.st_mtime) {
			if(firstTime) { firstTime=false; }
			else {
				if(!chgmsg) { chgmsg=true; syslog(LOG_ERR,"Src3ABN::cacheLRU_Name:Waiting for changes in "+dirsrc+"."); }
				sleep(1);
			}
			//continue;
		}
	MARK
		//syslog(LOG_INFO,"Src3ABN::cacheLRU_Name:stat %s: %d",dstflagsdir,stat(dstflagsdir, &statbuf2));
		if(stat(dstflagsdir, &statbuf2)==-1) {
			sleep(1); // Give time for parent dir to be created if needed...
			if(mkdir(dstflagsdir,0777)<0) { syslog(LOG_ERR,"Src3ABN::cacheLRU_Name:mkdir#2(%s): %s",dstflagsdir,strerror(errno)); continue; }
		}
		break;
	} while(true);
	syslog(LOG_INFO,"Src3ABN::cacheLRU_Name:Filling %s from %s",dstdir,srcdir);

	DIR *dir=opendir(srcdir);
	if(!dir) { syslog(LOG_ERR,"Src3ABN::cacheLRU_Name:opendir(%s): %s",dirsrc.c_str(),strerror(errno)); sleep(1); return; }
	struct dirent *entry;
MARK
	while((entry=readdir(dir))) {
		if(entry->d_name[0]=='.') { continue; } // Skip hidden files
		if(entry->d_type!=DT_REG) { continue; } // Skip non-regular files
		if(strstr(entry->d_name,".sh")) { continue; } // Skip hidden files
		// Check whether already cached
		char cachedflag[1024];
		snprintf(cachedflag,sizeof(cachedflag),"%s/%s",dstflagsdir,entry->d_name);
		struct stat statbuf;
		string spath=dirsrc+"/"+string(entry->d_name);
		string dpath=dirdst+"/"+string(entry->d_name);
		const char *cspath=spath.c_str();
		const char *cdpath=dpath.c_str();
		if(stat(cachedflag, &statbuf)!=-1 && stat(cdpath, &statbuf)!=-1) { continue; } // Skip cached files
		// Cache if not done yet
		float seclen=decoder->SecLen(cspath); // Get seconds length from source path
		if(seclen<0) { continue; }
		if(stat(cspath, &statbuf)==-1) { continue; } // Skip bad file
		char centry[1024]; // Use dummy time to avoid duplicate entries
		snprintf(centry,sizeof(centry),"%s/%s %04d %s",dstdir,Util::fmtTime(statbuf.st_mtime).c_str(),(int)seclen,entry->d_name);
		if(symlink(cspath, centry)<0 && errno!=EEXIST) { syslog(LOG_ERR,"Src3ABN::cacheLRU_Name#1:symlink(%s): %s",centry,strerror(errno)); }
		// Mark as cached AND store date, len for use by playq
		char datelen[25]; // e.g. 2010-04-13 04:09:10 0634
		snprintf(datelen,sizeof(datelen),"%s %04d",Util::fmtTime(statbuf.st_mtime).c_str(),(int)seclen);
		if(symlink(cspath, cachedflag)<0 && errno!=EEXIST) { syslog(LOG_ERR,"Src3ABN::cacheLRU_Name#2:symlink(%s): %s",cachedflag,strerror(errno)); }
		//usleep(100000); // Sleep 1/10 second to avoid pegging the CPU
		sleep(1); // Sleep one second to avoid pegging the CPU
	}
MARK
	closedir(dir);
MARK
}
void Src3ABN::Execute(void* arg) {
MARK
	//time_t prevTime=0;
	string downdir="/tmp/play3abn/cache/download";
	string fillerdir="/tmp/play3abn/cache/filler";
	string lrucache=string(Util::TEMPPATH)+"/leastrecentcache";
	string lrucached=string(Util::TEMPPATH)+"/leastrecentcache/cached";
	syslog(LOG_ERR,"Src3ABN::Execute:Maintaining list at "+lrucache+" from "+downdir);
	const char *srcdir=downdir.c_str();
	const char *srcfiller=fillerdir.c_str();
	const char *dstdir=lrucache.c_str();
	const char *dstflagsdir=lrucached.c_str();
	bool firstTime=true;
MARK
	while(!Util::checkStop()) {
		cacheLRU_Name(downdir,srcdir,lrucache,dstdir,dstflagsdir,firstTime);
		cacheLRU_Name(fillerdir,srcfiller,lrucache,dstdir,dstflagsdir,firstTime);
		//syslog(LOG_ERR,"Src3ABN::Execute:Waiting for 1800 seconds...");
		util->longsleep(1800);
MARK
	}
MARK
}
*/
void Src3ABN::Scheduler() {
MARK
MARK
	syslog(LOG_ERR,"Src3ABN:Scheduler:Maintaining list...");
	time_t prevTime=0;
	while(!Util::checkStop()) {
/*		char radiotype[32];
		getST(radiotype);
		if(strncmp(radiotype,"3abn",4)!=0) { sleep(5); continue; }*/
		//unsigned int audiosource=util->getInt("audiosource",0);
		//syslog(LOG_ERR,"Src3ABN::Scheduler.");
		//if(audiosource!=0) { initialized=true; sleep(1); continue; }
		syslog(LOG_ERR,"Src3ABN::Scheduler: Scheduling now...");
		int debug=util->getInt("debug",0);
		int bufferSeconds=Util::getBufferSeconds();
		time_t now=Time();
		time_t today=now-bufferSeconds;
		time_t tomorrow=today+86400;
		if(today-prevTime > 3600) {
			//NOTE: curler->enqueue(today+86400,"http://radioalaska.3abn.org/url.txt","vars/baseurl.txt",true,0);
			prevTime=today;
		}
		syslog(LOG_ERR,"Src3ABN:Scheduler:getBaseUrl");
		getBaseUrl();

		vector<TSchedLine> sched;

		// NOTE: FETCH TODAY ------------------------------
		syslog(LOG_ERR,"Src3ABN:Scheduler:getSchedule1");
		string path=getSchedule(today,true);
		bool usingLiveSchedule=false;
		syslog(LOG_ERR,"Src3ABN:Scheduler:path=%s",path.c_str());
		int scheduleAgeDays=0;
		if(path!="") {
			if(debug) { syslog(LOG_ERR,"loadFromFile(today): path=%s",path.c_str()); }
			loadFromFile(path, sched, today, today);

			// NOTE: FETCH TOMORROW (if possible) -------------
			path=getSchedule(tomorrow,true);
			if(path!="") {
				if(debug) { syslog(LOG_ERR,"loadFromFile(tomorrow): path=%s",path.c_str()); }
				loadFromFile(path, sched, today, tomorrow);
			}
			usingLiveSchedule=true;
		}
		else { // If today was not found, don't try tomorrow.  Instead, go back until an old schedule is found.
syslog(LOG_ERR,"Today not found.");
util->longsleep(9999);
				time_t older=today-86400;
				time_t monthOlder=today-(86400*31);
				do {
					scheduleAgeDays++;
					path=getSchedule(older,false);
					if(path!="" || older<=monthOlder) break;
					older-=86400;
					//usleep(100000); // Sleep 1/10 second to avoid pegging the CPU
					sleep(1); // Sleep one second to avoid pegging the CPU
				} while(true);
				if(path=="") { path="FillerPlaylist.txt"; scheduleAgeDays=365; }
				if(debug) { syslog(LOG_ERR,"loadFromFile(old today): path=%s",path.c_str()); }
				loadFromFile(path, sched, today, today);
				if(debug) { syslog(LOG_ERR,"loadFromFile(old tomorrow): path=%s",path.c_str()); }
				loadFromFile(path, sched, today, tomorrow);
		}
		syslog(LOG_WARNING,"Src3ABN:Scheduler:Creating symlinks.  usingLiveSchedule=%d",usingLiveSchedule);
		int usecache=getId("cache",1);
		for(unsigned int xa=0; xa<sched.size(); xa++) { // Create schedule entries in directory
			/** WRITE OUT with schedule play date **/
			string sName=sched[xa].sName;
			string dispname="";
			unsigned int at=sName.rfind('/',string::npos);
			if(at != string::npos) { dispname=sName.substr(at+1); }
			time_t plNow=sched[xa].plNow+bufferSeconds; //+(usingLiveSchedule?0:1); // Play some time after scheduled time // time+1
			const char *linked=sName.c_str();
			time_t diff=plNow-now;
			struct stat statbuf;
// 			if(strstr(linked,"HOH023")) {
// 				syslog(LOG_INFO,"HOH023: %s,stat=%d,size=%d,diff=%d",linked,stat(linked, &statbuf),statbuf.st_size,diff);
// 			}
			time_t prgEnd=sched[xa].len+diff;
			time_t endsIn=0; // Was 3600 (one hour)
			bool doDownload=false;
			if(usingLiveSchedule && prgEnd>=endsIn) { // If live and ends in future...

// 				if(strstr(linked,"HYTH141")) {
// 					syslog(LOG_INFO,"HYTH141b: %s,stat=%d,size=%d",linked,stat(linked, &statbuf),statbuf.st_size);
// 				}
				
				if(usecache==0 || stat(linked,&statbuf)==-1 || statbuf.st_size==0) { // DOWNLOAD if file doesn't exist, or not using cache
					char dir[2];
					dir[0]=tolower(dispname[0]);
					dir[1]=0;
					// string baseurl="ftp://RadioAlaska:%s@ftp.3abn.org/OGG/"; or http://
					char linkedbuf[1024];
					snprintf(linkedbuf,sizeof(linkedbuf),"%s/%s/%s",util->mergePassword(string(baseurl)).c_str(),dir,urlencode(dispname).c_str());
					linked=linkedbuf;
					syslog(LOG_INFO,"Src3ABN:Scheduler:linked=%s",linked);
					doDownload=true;
				}
			}
			else { // If not live, or too near to download, SUBSTITUTE LRU file matching CODE
				//getCode
			}
			if(getId("customsched",0)==0 || doDownload) {
				// NOTE: false parameter to enqueue means we won't re-enqueue over existing timeslots.  
				// NOTE: If there should be exceptions to this practice, implement them here.
				util->enqueue(plNow,dispname,string(linked), scheduleAgeDays, sched[xa].len, false); // #1: Enqueue file if not existing
			}
			//usleep(100000); // Sleep 1/10 second to avoid pegging the CPU
			sleep(1); // Sleep one second to avoid pegging the CPU
		}
		initialized=true;
		syslog(LOG_NOTICE,"Src3ABN:Scheduler:Sleeping...");
		util->longsleep(60);
	}
	syslog(LOG_ERR,"PlayList3ABN:Scheduler:Done.");
}
string Src3ABN::dateString(time_t today) {
MARK
	char todaysDate[11];
	struct tm tms;
	struct tm* nptr=localtime_r(&today,&tms);
	snprintf(todaysDate,sizeof(todaysDate),"%04d-%02d-%02d",1900+nptr->tm_year,nptr->tm_mon+1,nptr->tm_mday);
	//syslog(LOG_ERR,"dateString:todaysDate=%s,today=%ld",todaysDate,today);
MARK
	return string(todaysDate);
}

//based on javascript encodeURIComponent()
string Src3ABN::urlencode(const string &c) {
MARK
	string escaped="";
	int max = c.length();
	for(int i=0; i<max; i++) {
		if( (48 <= c[i] && c[i] <= 57) ||//0-9
				(65 <= c[i] && c[i] <= 90) ||//abc...xyz
				(97 <= c[i] && c[i] <= 122) || //ABC...XYZ
				(c[i]=='~' || c[i]=='!' || c[i]=='*' || c[i]=='(' || c[i]==')' || c[i]=='\'' || c[i]=='.' || c[i]=='-' || c[i]=='_')
		)
		{ escaped.append( &c[i], 1); }
		else {
				escaped.append("%");
				escaped.append( char2hex(c[i]) );//converts char 255 to string "ff"
		}
	}
	return escaped;
}

string Src3ABN::char2hex( char dec ) {
MARK
    char dig1 = (dec&0xF0)>>4;
    char dig2 = (dec&0x0F);
    if ( 0<= dig1 && dig1<= 9) dig1+=48;    //0,48inascii
    if (10<= dig1 && dig1<=15) dig1+=97-10; //a,97inascii
    if ( 0<= dig2 && dig2<= 9) dig2+=48;
    if (10<= dig2 && dig2<=15) dig2+=97-10;

    string r;
    r.append( &dig1, 1);
    r.append( &dig2, 1);
    return r;
}
//---------------------------------------------------------------------------------------------------------------------
/** EXAMPLE NEED FOR SEGMENT MATCHING:
 *
 * 2011-05-18 14:00:00 0756 MMJ08047-1~MUSICAL_MEDITATIONS_Segment_One~Christian_Music_For_Today~.ogg
 * 2011-05-18 14:15:36 0662 MMJ08047-2~MUSICAL_MEDITATIONS_Segment_Two~Christian_Music_For_Today~.ogg
 *
 * 2011-05-18 20:00:00 0720 CR020-1~CROSSROADS_ON_3ABN_RADIO_Segment_One~The_Refiner's_Fire~.ogg
 * 2011-05-18 20:15:00 0720 CR020-2~CROSSROADS_ON_3ABN_RADIO_Segment_Two~The_Refiner's_Fire~.ogg
*/
void Src3ABN::loadFromFile(string path, vector<TSchedLine>& sched, time_t startDay, time_t curDay) {
MARK
//void PlayList3ABN::loadFromGeneric(vector<TSchedLine>& sched, time_t startDay, time_t curDay) {
	string sDate=dateString(curDay);
	filebuf fb;

MARK
	struct stat statbuf;
	statbuf.st_size=-1;
	if(stat(path.c_str(), &statbuf)==-1 || statbuf.st_size<2048) {
MARK
		char *cwd=get_current_dir_name();
		syslog(LOG_ERR,"loadFromFile: Invalid %s (cwd=%s,size=%ld)",path.c_str(),cwd,statbuf.st_size);
		if(cwd) free(cwd);
MARK
		return;	//abort();
	}
MARK

	fb.open(path.c_str(),ios::in);
	istream is(&fb);
	string realDate,tmp,sTime,sCode,sName;
	int len,retlen=-1;
	bool doLessHour=false;
	bool lessHour=false;
	time_t timeOfLastLine=0;
/*	syslog(LOG_ERR,"loadFromFile: START: path=%s,len=%ld,downloadBasedir=%s,fillerBasedir=%s",
				path.c_str(),statbuf.st_size,downloadBasedir.c_str(),fillerBasedir.c_str());*/
	int hourLeft=-1;
	//int expectHourLeft=hourLeft;
	int segnum=0;
	string prevUsedPrefix="";
	time_t plNow=0;
	time_t newPlNow=0;
	string fillName="";
	time_t now=Time();
	Util::getBufferSeconds();
MARK
	//syslog(LOG_ERR,"loadFromFile: "+path);
	while(!is.eof()) {
MARK
 		getline(is,realDate,' '); if(is.eof()) break;
		getline(is,sTime,' '); if(is.eof()) break;
		getline(is,tmp,' '); if(is.eof()) break;
		len=atoi(tmp.c_str());
		if(len>3600) { len-=3600; doLessHour=true; }
 		getline(is,sName,'\n');
		//syslog(LOG_ERR,"len=%d,tmp=%s,name=%s",len,tmp.c_str(),sName.c_str());
		int tmpsegnum=-1;
		sCode=getCode(sName.c_str(), tmpsegnum);
MARK
		std::transform(sCode.begin(), sCode.end(),sCode.begin(), ::toupper);			
MARK
		//syslog(LOG_ERR,"sCode="+sCode+"***");
MARK
		//syslog(LOG_ERR,"codeCategories[%s]=%s",sCode.c_str(),codeCategories[sCode.c_str()].c_str());
MARK
		string category=codeCategories[sCode.c_str()];
MARK
		if(category!="") { sCode=category; }
		else { syslog(LOG_ERR,"UNKNOWN CODE: %s",sCode.c_str()); }
		// each line e.g. 22:00:00 1800 YSH
MARK
		if(sTime.size()==5) { sTime+=":00"; } // Add seconds if schedule file had none
MARK
		string sDateTime=sDate+" "+sTime;
		newPlNow=TSchedLine::TimeOf(util,sDateTime);
		if(plNow<newPlNow) { plNow=newPlNow; } // FIXME: May leave large gaps if much shorter file is selected as filler
		// FIXME: above problem could be fixed by adding filler (but this is done automatically at end of hour anyway)
MARK

		string playqBasedir="/tmp/play3abn/tmp/playq";
		string downloadBasedir="/tmp/play3abn/cache/download";
		string sPath=downloadBasedir+"/"+sName;
		bool isFillerDay=(path=="FillerPlaylist.txt");
		bool isMissing=(stat(sPath.c_str(), &statbuf)==-1 || statbuf.st_size<2048);
		 /** NOTE: hasScheduledProgram should check the entire time slot, not just the specific time **/
		string playPath="";
		bool hasScheduledProgram=false;
		int plOfs=0;
		while(!hasScheduledProgram && plOfs<60) { /** NOTE: Check for matching programs within the minute.  Skip if any. **/
			string checkDateTime=util->fmtTime(plNow+plOfs);
			playPath=playqBasedir+"/"+checkDateTime+" -1";
			hasScheduledProgram=(stat(playPath.c_str(), &statbuf)!=-1);
			plOfs++;
		}
		bool isToday=(abs(curDay-now)<86400);
		time_t startItem=newPlNow; //+Util::bufferSeconds;
		time_t endItem=startItem+len;
		time_t diff=startItem-now;
		bool soonToPlay=(diff>=0 && diff<3600);
		if(startDay == curDay) { // If today, only keep last part of day
			if(endItem < startDay) { continue; }
			// NOTE: If previous scheduled item shorter than actual item, startItem < plNow
			// NOTE: In that case, skip scheduled item
			// FIXME: SKIPPING WHEN startItem < plNow CAUSES VALID ITEMS TO BE SKIPPED EVEN ON LIVE SCHEDULE!
			//if(startItem < plNow) {
			//	syslog(LOG_ERR,"WOULD SKIP %s < %s (newPlNow=%s): %s",
			//					Util::fmtTime(startItem).c_str(),Util::fmtTime(plNow).c_str(),Util::fmtTime(newPlNow).c_str(),sPath.c_str());
			//}
			if(startItem < plNow) { continue; }
//syslog(LOG_ERR,"%s %s",sTime.c_str(),sPath.c_str());
			//if(endItem < startDay || startItem < plNow) { continue; }
		}
		else { // If tomorrow, only keep first part of day
			if(endItem >= curDay) { /*syslog(LOG_ERR,"SKIP TOMORROW: %s",sLine.sPath.c_str());*/ continue; }
		}
		syslog(LOG_INFO,"%s sCode=%-10s,isToday=%d,isMissing=%d,hasScheduledProgram=%d,soonToPlay=%d,isFillerDay=%d",
			Util::fmtTime(plNow).c_str(),sCode.c_str(),isToday,isMissing,hasScheduledProgram,soonToPlay,isFillerDay);
		if(hourLeft<=0) { hourLeft=3600-(plNow%3600); }
		int maxLen=hourLeft;
		/** Get filler if not existing and will be playing soon (if on live schedule) **/
		if((isToday && isMissing && !hasScheduledProgram && soonToPlay) || (!isToday && isMissing && !hasScheduledProgram) || (isFillerDay && !hasScheduledProgram)) {
			string seg="";
			char segment[1024]={0};
			bool usesSegments=(sCode == "MMJ" || sCode == "CR");
			if(usesSegments) {
				if(tmpsegnum==-1) { segnum++; }
				else { segnum=tmpsegnum; }
				if(sCode == "MMJ") { snprintf(segment,sizeof(segment),"-%d~MUSICAL_MEDITATIONS_Segment",segnum); }
				if(sCode == "CR") { snprintf(segment,sizeof(segment),"-%d~CROSSROADS_ON_3ABN_RADIO_Segment",segnum); }
				if(segnum==4) { segnum=0; }
				seg=string(segment);
				if(sCode == "MMJ" && prevUsedPrefix.substr(0,3)=="MMJ" ) { sCode=prevUsedPrefix;  } // Try for previous (for continuity)
				if(sCode == "CR" && prevUsedPrefix.substr(0,2)=="CR" ) { sCode=prevUsedPrefix;  } // Try for previous (for continuity)
	//			syslog(LOG_ERR,"seg=%s,prevUsedPrefix=%s.",segment,prevUsedPrefix.c_str());
			}
			else { seg=""; }
			bool result=false;
			time_t secsLeft=3600-(plNow%3600);
			if(sCode=="Network") { sPath=downloadBasedir+"/Network_ID_with_DTMF_#4.ogg"; maxLen=15; segnum=0; prevUsedPrefix=""; result=true; }
			else { result=getLeastRecentlyPlayedFile(plNow,sPath,10,secsLeft-15,retlen, sCode, seg); }
			// Try for any code except MMJ
			if(!result) { result=getLeastRecentlyPlayedFile(plNow,sPath,len-600,min(secsLeft-15,len+600),retlen, "", seg,"MMJ"); }
			if(!result) { result=getLeastRecentlyPlayedFile(plNow,sPath,10,secsLeft-15,retlen, "", seg,sCode); } // Okay, use MMJ if no alternative
			//if(!result) { syslog(LOG_ERR,"NO LRU MATCH: len=%4d +/- 600: %s sPath=%s",len,sTime.c_str(),sPath.c_str()); continue; }
			if(sCode == "MMJ" || sCode == "CR") {
				prevUsedPrefix=sName.substr(0,prevUsedPrefix.find('-'));
			}
			fillName="Saved";
			len=maxLen;
		}
		else { fillName="Live"; }
		if(sCode=="Network") { plNow=((plNow+3600)/3600*3600)-15; }
		TSchedLine sLine(util,sDate,sDate,sTime,len,sPath,plNow,lessHour);
		lessHour=false;
		if(doLessHour) { doLessHour=false; lessHour=true; } // Next hour should be one less than supposed
		if(sCode=="Network") { hourLeft=0; plNow+=15; }
		else { hourLeft-=maxLen; plNow+=retlen; }
		if(sLine.plNow==timeOfLastLine) { sched.pop_back(); } // Remove file playing at duplicate time
		sLine.fillName=fillName;
		sched.push_back(sLine); // Schedule this file
		//syslog(LOG_INFO,"loadFromFile: scheduled(%s: %s)",Util::fmtTime(sLine.plNow).c_str(),sLine.sPath.c_str());
		timeOfLastLine=sLine.plNow;
MARK
	}
	syslog(LOG_NOTICE,"loadFromFile: END: sched.size=%d",sched.size());
	fb.close();
MARK
}

/** NOTE: Get recent filler, or refill from fillcache **/
/** Returns name of file used in filler dir (if any) so it can be removed when we're done **/
/** NOTE: getFiller should not return the -Filler files if there is anything else that can be used instead. **/
bool Src3ABN::getLeastRecentlyPlayedFile(time_t toPlayAt, string& lrpFilename, int minLen, int maxLen, int& retlen, string match, string midmatch, string dontmatch) {
MARK
	syslog(LOG_INFO,"getLeastRecentlyPlayedFile#1: toPlayAt=%s; maxLen=%4d,match=%5s;midmatch=%s;dontmatch=%s",
							Util::fmtTime(toPlayAt).c_str(),maxLen,match.c_str(),midmatch.c_str(),dontmatch.c_str());
	if(maxLen<=0) { maxLen=0; lrpFilename=""; return false; }
//strings Player::getFiller(int maxLen, string& fillname) {
MARK
	string lruq=string(Util::TEMPPATH)+"/leastrecentq";
	string lrucache=string(Util::TEMPPATH)+"/leastrecentcache";
	vector<strings> playlist=util->getPlaylist(lruq.c_str(),0/*nolimit*/,1/*lru sort*/,maxLen); // Get top entry
MARK
	//syslog(LOG_ERR,"getLeastRecentlyPlayedFile#1: playlist.size=%d",playlist.size());
	time_t now=Time();
	if(playlist.size()==0 && now-lastRefill>60) { // No short-enough files remain
		syslog(LOG_INFO,"getLeastRecentlyPlayedFile#2:   Filling %s from %s",lruq.c_str(),lrucache.c_str());
		util->cacheFiller(lrucache,lruq,1); // Refill the list (LRU sorted)
		playlist=util->getPlaylist(lruq.c_str(),0/*nolimit*/,0/*nosort*/,maxLen); // Get top entries (excluding too-long files)
MARK
		lastRefill=now;
	}
	//syslog(LOG_INFO,"Src3ABN::getLeastRecentlyPlayedFile: LRUQ size=%d",playlist.size());
	for(int retry=0; retry<2; retry++) {
		for(unsigned int xa=0; xa<playlist.size(); xa++) {
			strings ent=playlist[xa];
			/** CHECK LIMITS **/
			time_t dt=0;
			int flag=0;
			int seclen=0;
			string name="";
			string catcode="";
			string dispname="";
			string decodedUrl="";
			int result=util->itemDecode(ent, dt, flag, seclen, catcode, name, decodedUrl);
			if(result<0) {
			  syslog(LOG_ERR,"Src3ABN::getLeastRecentlyPlayedFile: Invalid format (result=%d): %s => %s",result,ent.one.c_str(),ent.two.c_str());
			  syslog(LOG_ERR,"remove2(%s)",decodedUrl.c_str());
			  remove(decodedUrl.c_str()); continue;
			}
			
			//syslog(LOG_INFO,"Src3ABN::getLeastRecentlyPlayedFile: name=%s",name.c_str());
MARK
			if(!dontmatch.empty()) { // Ignore files starting with dontmatch
				int mlen=dontmatch.length();
				if(name.compare(0, mlen, dontmatch) == 0) {
					if(!isalpha(name[mlen])) { 
						//syslog(LOG_INFO,"Src3ABN::getLeastRecentlyPlayedFile: DONTMATCH=%s,name=%s",dontmatch.c_str(),name.c_str());
						continue; } // There is an exact match of codes; skip this one
				}
			}
			if(!match.empty()) { // Consider only files starting with match
				int mlen=match.length();
				if(name.compare(0, mlen, match) != 0) { continue; }
				if(isalpha(name[mlen])) { continue; } // There was more to the code in this name than the match we passed
			}
			else if(name.find("Network_ID_with_DTMF",0) != string::npos) { 
				/*syslog(LOG_INFO,"SKIPPED matching Network_ID:generic match");*/ continue; } // If generic match, don't find Network_ID
			if(!midmatch.empty()) { // Consider only files containing midmatch
				if(name.find(midmatch,0) == string::npos) { 
					syslog(LOG_INFO,"Src3ABN::getLeastRecentlyPlayedFile:   MIDMATCH=%s,name=%s",midmatch.c_str(),name.c_str());
					continue; }
			}
			if(seclen<minLen || seclen<=0) { 
				syslog(LOG_INFO,"Src3ABN::getLeastRecentlyPlayedFile:   seclen(%4d)<minLen(%4d) or 0,name=%s",seclen,minLen,name.c_str());
				continue; } // File is shorter than we can use: skip it!
			retlen=seclen; // Output actual length of selected media file (in seconds)
			lrpFilename=decodedUrl;
			string linkpath=lruq+"/"+ent.one;
			// Remove link to avoid using this file in the near future (unless Network_ID)
			if(name.find("Network_ID_with_DTMF",0) == string::npos) { 
				/*syslog(LOG_INFO,"UNLINK THIS: %s",linkpath.c_str());*/ unlink(linkpath.c_str()); }
			//syslog(LOG_INFO,"getLeastRecentlyPlayedFile#2: lrpFilename="+lrpFilename);
			syslog(LOG_INFO,"Src3ABN::getLeastRecentlyPlayedFile: match=%s,name=%s",match.c_str(),name.c_str());
			return true;
		}
		/** NO MATCH FOUND.  If we tried a generic match=="", then refill the list **/
		if(retry==0 && match==""/*&& now-lastRefill>60*/) {
			syslog(LOG_WARNING,"getLeastRecentlyPlayedFile#3: LRUQ size was %d; Filling %s from %s",playlist.size(),lruq.c_str(),lrucache.c_str());
			util->cacheFiller(lrucache,lruq,1); // Refill the list (LRU sorted)
			playlist=util->getPlaylist(lruq.c_str(),0/*nolimit*/,0/*nosort*/,maxLen); // Get top entries (excluding too-long files)
MARK
			lastRefill=now;
		}
	}
	retlen=0;
	syslog(LOG_INFO,"getLeastRecentlyPlayedFile: NO MATCH FOUND");
	return false;
	// NOTE: "2011-06-09 12:40:00 0066 IT'S A WONDERFUL DAY-Heritage Singers.ogg" (datetime=0; len=20; name=25)
/*time_t now=Time();
 	ent.one=Util::fmtTime(toPlayAt)+name; // Reschedule for now
	return ent;*/
}
//---------------------------------------------------------------------------------------------------------------------
/** NOTE: Returns full path of schedule if found, "" if not */
string Src3ABN::getSchedule(time_t day, bool download) {
MARK
	string schedulesBasedir="/tmp/play3abn/cache/schedules";
	char scheduleName[1024];
	struct tm tms;
	struct tm* nptr=localtime_r(&day,&tms);
	snprintf(scheduleName,sizeof(scheduleName),"rec%04d-%02d-%02d.txt",1900+nptr->tm_year,nptr->tm_mon+1,nptr->tm_mday);
	string sbase=string(scheduleName);
	string spath=string(Util::TEMPPATH)+"/"+sbase;

	// -- RETURN FROM Util::TEMPPATH if already there
	char tmppath[1024];
	snprintf(tmppath,sizeof(tmppath),"%s/%s",Util::TEMPPATH,scheduleName);
	struct stat statbuf;
	if(stat(tmppath, &statbuf)!=-1 && statbuf.st_size>4096) {
		syslog(LOG_WARNING,"Src3ABN::getSchedule: spath=%s",spath.c_str());
		string opath=schedulesBasedir+"/"+sbase; // Save copy if not saved yet
		if(stat(opath.c_str(), &statbuf)==-1 || statbuf.st_size<=4096) { util->copyFile(tmppath,opath.c_str()); }
		return spath;
	}

	// -- COPY TO mytmp if available
	string dpath=spath;
	spath=schedulesBasedir+"/"+sbase;
	bool mounted=util->isMounted();
	//syslog(LOG_ERR,"getSchedule(%s): mounted=%d,download=%d",spath.c_str(),mounted,download);
	if(!mounted) { return ""; } // SD not available right now: Don't even try to download it.
	string sapath=spath;
	for(int xa=0; xa<10; xa++) { // Try with appended index
		if(xa>0) { sapath+=xa; }
		if(stat(sapath.c_str(), &statbuf)!=-1 && statbuf.st_size>4096) {
			util->copyFile(sapath.c_str(),dpath.c_str());
			return dpath;
		}
	}
	
	if(!download) { return ""; }
	// -- DOWNLOAD if unavailable
	//if(!curler) { syslog(LOG_ERR,"curler not initialized!"); sleep(1); abort(); }
	syslog(LOG_NOTICE,"Src3ABN::getSchedule: enqueue: sbase=%s.",sbase.c_str());
	// string baseurl="ftp://RadioAlaska:%s@ftp.3abn.org/OGG/";
	string url=util->mergePassword(string(baseurl)+"/r/")+urlencode(sbase);
	int scheduleAgeDays=(time(NULL)-day)/86400;
	util->enqueue((day/86400*86400)+86400,spath,url,scheduleAgeDays,0); // #2: Enqueue download of schedule
	return ""; // Schedule not found
}

/*void Src3ABN::getFiles() {
	vector<TSchedLine>::iterator cii;
	//int debug=util->getInt("debug",0);
	time_t notRecordedYet=chooser->Time(); //playTime+bufferSeconds;			//syslog(LOG_ERR,"scheduleFileIfPlayingSoon: sLine.plNow (%ld) - notRecordedYet (%ld) : %d",sLine.plNow,notRecordedYet,sLine.plNow-notRecordedYet);
	time_t playTime=notRecordedYet-bufferSeconds;
	struct tm tms;
	struct tm* ftptr=localtime_r(&playTime,&tms);
	int debug=util->getInt("debug",0);
	if(debug) {
		syslog(LOG_ERR,"PlayList3ABN::getFiles(%d): playTime=%04d-%02d-%02d %02d:%02d:%02d",curPList.size(),
				1900+ftptr->tm_year,ftptr->tm_mon+1,ftptr->tm_mday,ftptr->tm_hour,ftptr->tm_min,ftptr->tm_sec);
	}
	int xa=0;
	string peer=peerlist->getPeer();
	syslog(LOG_ERR,"peer="+peer);
	MutexLock(&mSchedule);
	for(cii=curPList.begin(); cii!=curPList.end(); cii++,xa++) {
		const TSchedLine& sLine=(TSchedLine)(*cii);
		string filename="";
		if(playTime <= sLine.plNow+sLine.len && sLine.plNow < notRecordedYet-3600) { // If file is near enough to being played, download it
			// NOTE: If playing this hour and we have no local peer to download from, probably not enough time to download
			if(sLine.plNow < playTime+3600 && peer=="") {
				// NOTE: Do nothing but check existance
				filename=getFile(sLine.plNow,sLine.sName,sLine.len, false);
				if(filename=="") {
					if(debug) { syslog(LOG_ERR,"@curlq@TOO NEAR: %s",TSchedLine::printString(sLine).c_str()); }
					continue;
				}
			}
			else {
				filename=getFile(sLine.plNow,sLine.sName,sLine.len);
				if(filename=="" || filename=="<QUEUED>") {
					if(debug) { syslog(LOG_ERR,"@curlq@DOWNLOAD: %s",TSchedLine::printString(sLine).c_str()); }
					continue;
				}
			}
			if(debug) { syslog(LOG_ERR,"@curlq@EXISTING: %s; filename=%s",TSchedLine::printString(sLine).c_str(),filename.c_str()); }
		} // END IF(sLine)
	}	// END FOR
	MutexUnlock(&mSchedule);
}*/
// ------------------------------------------------------------------------------------------------------------------
string Src3ABN::getCode(const char *fileBase, int& segnum) {
MARK
	//syslog(LOG_ERR,"getCode: fileBase=%s.",fileBase);
	code[0]=0;
	if(fileBase) {
		const char *pp=strrchr(fileBase,'/');
		if(pp) { fileBase=&pp[1]; }
		strncpy(code,fileBase,sizeof(code)); // MMJ07014-4
		code[sizeof(code)-1]=0;
		if     (strncmp(code,"3ABN_TODAY",10)==0) { code[10]=0; }
		else if(strncmp(code,"TODAY",5)==0) { code[5]=0; }
		else if(strncmp(code,"NEWSBREAK",9)==0) { code[0]=0; } // Don't fill with NEWSBREAK (it may be dated)
		else {
			char *pp;
			if(strncmp(code,"MMJ",3)==0 || strncmp(code,"CR",2)==0) {
				pp=strchr(code,'-');
				if(pp) {
					int tmpsegnum=atoi(&pp[1]);
					if(tmpsegnum>0) { segnum=tmpsegnum; }
				}
			}
			pp=code;
			while(*pp && isalpha(*pp)) { pp++; }
			*pp=0;
		}
	}
	//syslog(LOG_ERR,"getCode: code=%s.",code);
MARK
	return string(code);
}

int main(int argc, char **argv) {
	if(argc==2 && strcmp(argv[1],"test")==0) {
		syslog(LOG_ERR,"src-3abn: testing #2");
		new Util("src-3abn(testing)");
		int bufferSeconds=Util::getBufferSeconds();
		syslog(LOG_ERR,"src-3abn: bufferSeconds=%d",bufferSeconds);
		time_t today=Time()-bufferSeconds;
		syslog(LOG_ERR,"today=%s",Util::fmtTime(today).c_str());
		vector<TSchedLine> sched;
		syslog(LOG_ERR,"new Src3ABN(testing)");
		Src3ABN *src=new Src3ABN(true);
		syslog(LOG_ERR,"src-3abn: Calling getSchedule");
		string path=src->getSchedule(today,true);
		syslog(LOG_ERR,"src-3abn: Calling loadFromFile(%s)",path.c_str());
		src->loadFromFile(path, sched, today, today);
		syslog(LOG_ERR,"src-3abn: Resulting Schedule:");
		for(unsigned int xa=0; xa<sched.size(); xa++) { // Create schedule entries in directory
			/** WRITE OUT with schedule play date **/
			string sName=sched[xa].sName;
			string dispname="";
			unsigned int at=sName.rfind('/',string::npos);
			if(at != string::npos) { dispname=sName.substr(at+1); }
			time_t plNow=sched[xa].plNow+bufferSeconds; // Play some time after scheduled time
			syslog(LOG_ERR,"  %s %04d %s",Util::fmtTime(plNow).c_str(),sched[xa].len,dispname.c_str());
		}
		syslog(LOG_ERR,"src-3abn: DONE");
	}
	else {
MARK
		new Src3ABN();
	}
}
