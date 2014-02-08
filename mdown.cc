//#FIXME did not exit maybe need to check more often

// NOTE: new downloader won't have a target dir for enqueued files.  Files will be enqueued simply by their
// existance in either /tmp/play3abn/tmp/playq, or in /tmp/play3abn/tmp/downq
// (In either case, with a http:// or ftp:// as the target of the link)
// Files in downq will be removed when processed.  Files in playq will have the link replaced with a valid link
// to the actual location the file was downloaded to.
// Those locations will be determined by the file type:
//   .ogg				--> /tmp/play3abn/cache/download
//   .mp3				--> /tmp/play3abn/cache/newsweather
//   rec*.txt		--> /tmp/play3abn/cache/schedules
//   other .txt --> vars
// Links contain a "playAt" date, and if the date passes or is to close to passing, before we are able to download the file,
// the file link will be skipped (if in playq) or removed (if in downq).

#include "mdown.hh"
#include "mutex.hh"

Downloader::Downloader() {
	util=new Util("mdown#1"); // Need separate util instance for each thread (to avoid locking issues)
	if(pthread_mutex_init (&mPeerList, NULL)<0) { syslog(LOG_ERR,"pthread_mutex_init: %s",strerror(errno)); exit(2); }
	//Start(); // Get peer list in background
	int offline;
	do {
		offline=getId("offline",0);
		if(offline) { sleep(1); }
	} while(offline);
	DownloadLoop();
}

/** NOTE: Read a directory of links of the following form:
 * "2011-06-09 12:40:00 0066 IT'S A WONDERFUL DAY-Heritage Singers.ogg"
 * Each link specifies the date, time, length, and display name of a file to play.
 * The target of the link is the actual file to play.
 */
void Downloader::Execute(void *arg) {
	Util* util=new Util("mdown#2"); // Need separate util instance for each thread (to avoid locking issues)
	syslog(LOG_ERR,"Downloader:Execute:Maintaining peer list...");
	while(!Util::checkStop()) {
MARK
		//int debug=getInt("debug",0);
		char network[32];
		util->rungrep("/sbin/route -n","0.0.0.0",1,sizeof(network),network);
		//syslog(LOG_ERR,"network=%s",network);
		char cmd[1024];
		// e.g. nmap -oG - --open -T5 -p 1042 192.168.1.0-254 2>/dev/null | grep 1042/open/tcp | awk '{print $2}'
		snprintf(cmd,sizeof(cmd),"nmap -oG - --open -T5 -p 1042 %s-254 2>%s/peerlist.log.txt",network,Util::LOGSPATH);
		//syslog(LOG_ERR,"peerlist: cmd=%s",cmd);
		list<string> output;
		util->rungrep(cmd,"1042/open/tcp",2,0,NULL,&output);
		list<string>::iterator cii;
		map<string,string> newPeerList;
		//syslog(LOG_ERR,"PeerList#1: output.size=%d",output.size());
		string ipaddr=string((char *)Util::myip);
MARK
		for(cii=output.begin(); cii!=output.end(); cii++) {
			const string& cur=(string)(*cii);
			if(ipaddr==cur) { continue; }
			if(newPeerList[cur]!=cur) {
				newPeerList[cur]=cur;
				//syslog(LOG_ERR,"PeerList: peer="+cur);
			}
		}
MARK
		//syslog(LOG_ERR,"PeerList#3");
		MutexLock(&mPeerList);
		peerList=newPeerList;
		MutexUnlock(&mPeerList);
MARK
/*		for(cii=peerList.begin(); cii!=peerList.end(); cii++) {
			const string& cur=(string)(*cii);
			syslog(LOG_ERR,"  HOSTS: %s",cur.c_str());
		}*/
		//syslog(LOG_ERR,"PeerList: peer="+peerList.front());
		util->longsleep(60);
MARK
	}
	syslog(LOG_ERR,"PeerList:Execute:Done.");
MARKEND
}
map<string,string> Downloader::getPeers() {
	MutexLock(&mPeerList);
	map<string,string> retPeerList=peerList;
	MutexUnlock(&mPeerList);
	return retPeerList;
}
//#ERROR getting re-enqueued every minute!
void Downloader::DownloadLoop() {
	char downqdir[1024];
	snprintf(downqdir,sizeof(downqdir),"%s/playq",Util::TEMPPATH);
	syslog(LOG_ERR,"Execute");
	int count=0;
	time_t prevTime=0;
	CURL *curl=curl_easy_init();
	while(!Util::checkStop()) {
MARK
		count++;
		time_t now=Time();
		//syslog(LOG_ERR,"count=%d,now=%ld",count,now);
		if(now-prevTime == 0 && count>10) { sleep(1); count=0; } // NOTE: SLOW DOWN TO AVOID CPU OVERLOAD
		prevTime=now;

		mkdir(downqdir,0777);
		vector<strings> downlist=util->getPlaylist(downqdir,0/*limit*/,1/*sortMode*/,0/*maxLen*/,true); // Get .txt files also
MARK
		//syslog(LOG_ERR,"downlist size=%d",downlist.size());
		//int bufferSeconds=Util::getBufferSeconds();
		char downlink[1024];
		for(unsigned int xa=0; xa<downlist.size() && !Util::checkStop(); xa++) {
			strings ent=downlist[xa];
			const char *pname=ent.one.c_str();
			snprintf(downlink,sizeof(downlink),"%s/%s",downqdir,pname);
			/** Parse info string **/
			time_t playAt=0;
			int flag=0;
			int seclen=0;
			string dispname="";
			string catcode="";
			string decodedUrl="";
			int result=util->itemDecode(ent, playAt, flag, seclen, catcode, dispname, decodedUrl);
			if(result<0) {
			  syslog(LOG_ERR,"Downloader::DownloadLoop: Invalid format1 (result=%d): %s => %s",result,ent.one.c_str(),ent.two.c_str());
			  if(downlink[0]) { remove(downlink); }
			}
			if(ent.two.substr(0,7)!="http://" && ent.two.substr(0,6)!="ftp://") { continue; } // Skip all non-url links

MARK
			//playAt+=bufferSeconds;
			/** Calculate time to start playing this file **/
			int elapsed=(now-playAt);
			int early=-elapsed;

			bool dequeue=true;
			//syslog(LOG_ERR,"early=%d,dispname=%s",early,dispname.c_str());
			char dstfile[1024]={0};
MARK
			if(early>=0/*3600*/ || early<0) { // >3600 Has had time to record; <0 is a schedule
				dequeue=false;
				bool isSchedule=false;
				int slen=dispname.length();
				if(slen>4 && dispname.substr(slen-4)==".txt") {
					if(dispname.substr(0,3)=="rec") { isSchedule=true; }
				}
MARK
				/** File may not be available yet **/
				if(early<3600*3 || isSchedule) { dequeue=Download(curl, ent.two, dispname,dstfile,sizeof(dstfile)); }
MARK
			}
			else {
MARK
				dequeue=true; // Whether or not the file succeeds, we'll dequeue it
				Download(curl, ent.two, dispname,dstfile,sizeof(dstfile));
MARK
			}
MARK
			if(dequeue) {
				struct stat statbuf;
				/** NOTE: If file failed to download OR we're using a custom schedule, remove the download link without replacing it in the playq **/
				if(stat(dstfile, &statbuf)==-1 || getId("customsched",0)==1) { remove(downlink); }
				else { /** TWO-STEP REMOVAL/CREATION of symlink to ensure atomic replacement of existing scheduled item **/
					char tmpentry[1024];
					strncpy(tmpentry,downlink,sizeof(tmpentry)-4);
					strcat(tmpentry,".tmp");
					char dstlink[1024];
					snprintf(dstlink,sizeof(dstlink),"%d %04d %s",flag,seclen,dstfile);
					if(symlink(dstlink, tmpentry)<0) { syslog(LOG_ERR,"Downloader::DownloadLoop:symlink(%s): %s",tmpentry,strerror(errno)); }
					else { // Replace existing symlink in single operation
						if(rename(tmpentry,downlink)<0) { syslog(LOG_ERR,"Downloader::DownloadLoop:rename(%s): %s",downlink,strerror(errno)); }
					}
MARK
					/** ALSO, remove all others scheduled at same time slot **/
					for(unsigned int xb=0; xb<downlist.size(); xb++) {
						strings entry=downlist[xb];
						/** e.g. 2011-11-21 11:10:28 0108 106-HEAVEN'S REALLY GONNA SHINE-Chuck Wagon Gang.ogg **/
						/**                1111111111 222222222 3333 **/
						/**      0123456789 123456789 123456789 1234 **/
						const char *ename=entry.one.c_str();
						if(strncmp(ename,pname,20)==0 && strcmp(ename,pname)!=0) { /** AT SAME TIME, but NOT same file. **/
							char rmvlink[1024];
							snprintf(rmvlink,sizeof(rmvlink),"%s/%s",downqdir,ename);
							remove(rmvlink);
						}
					}
MARK
				}
MARK
			}
MARK
		}
MARK
		//syslog(LOG_ERR,"sleep(5)");
		sleep(5);
MARK
/*
		struct timespec req,rem;
		req.tv_sec=5; req.tv_nsec=0;
		rem.tv_sec=0; req.tv_nsec=0;
		int ret=nanosleep((const struct timespec *)&req, &rem);
		//syslog(LOG_ERR,"nanosleep:ret=%d",ret);
		if(ret==-1) { syslog(LOG_ERR,"nanosleep: %s",strerror(errno)); } // errno==EFAULT,EINTR,EINVAL
		//syslog(LOG_ERR,"done sleep(5)");
//#ERROR Fix sleep/nanosleep to avoid freezing until SIGTERM.
*/
	}
	curl_easy_cleanup(curl);
	syslog(LOG_ERR,"Downloader::DownloadLoop:Ending");
}

bool Downloader::Download(CURL *curl, string url, string tgtname, char *dstfile, int dstlen) {
	/** Determine destination file
	//   .ogg				--> /tmp/play3abn/cache/download
	//   .mp3				--> /tmp/play3abn/cache/newsweather
	//   rec*.txt		--> /tmp/play3abn/cache/schedules
	//   other .txt --> vars **/
	const char *dstpath="/tmp/play3abn/cache/download";
	const char *ctgtname=tgtname.c_str();
	int slen=tgtname.length();
	string tgtdir="download";
	if(slen>4 && tgtname.substr(slen-4)==".txt") {
		if(tgtname.substr(0,3)=="rec") { tgtdir="schedules"; dstpath="/tmp/play3abn/cache/schedules"; }
		else { tgtdir=""; dstpath="vars"; }
	}
	else if(tgtname.substr(0,9)=="CNN-News-") { tgtdir="newsweather"; dstpath="/tmp/play3abn/cache/newsweather"; }
	snprintf(dstfile,dstlen,"%s/%s",dstpath,ctgtname);
	char tmppath[1024];
	snprintf(tmppath,sizeof(tmppath),"%s/%s",Util::TEMPPATH,ctgtname);
	/** Try downloading from all peers first **/
	map<string,string> peers=getPeers();
	string saveUrl=url;
	int vetoReason=3;
	int dly=5;
	if(tgtdir!="") { // Don't try to download e.g. variables from peers
		for(map<string,string>::iterator cii=peers.begin(); cii!=peers.end(); cii++) {
			const string& peer=(string)(*cii).first;
			// Example URLS:
			// File    : ftp://RadioAlaska:Elephant@ftp.3abn.org/OGG/f/FullBreak~20110411_202830~.ogg --> download/
			// Schedule: ftp://RadioAlaska:Elephant@ftp.3abn.org/OGG/r/rec2011-04-11.txt             --> schedules/
			url="http://"+peer+"/"+tgtdir+"/"+tgtname;
			//if(debug) { syslog(LOG_ERR,"Curler::Execute:mod Download=%s",url.c_str()); }
			vetoReason=docurl(curl, dly, url.c_str(), tmppath);
			if(!vetoReason) { break; }
		}
	}
	if(vetoReason==3) { vetoReason=docurl(curl, dly, saveUrl.c_str(), tmppath); }
	if(!vetoReason) {
		while(!util->isMounted() && !Util::checkStop()) { util->setInt("mountwaiting",1); sleep(5); }
		util->setInt("mountwaiting",0);
		util->moveFile(tmppath,dstfile); // -------------- MOVE TO SD CARD STORAGE --------------
		struct utimbuf utb;
		utb.actime=utb.modtime=time(NULL)-(3600*3); // Mark as NOT recently used
		if(utime(dstfile,&utb)<0) {
			syslog(LOG_ERR,"mdown: utime: %s",strerror(errno));
		}
		// symlink here
	}
MARK
	util->longsleep(dly);
	bool dequeue=(vetoReason==3 || vetoReason==0); // If success, or failure due to absence, consider done
	return dequeue;
}

//******************************************************************************
off_t Downloader::checkFreeSD(const char *sdDev) {
	long sdfree=-1;
	char cmd[1024];
	snprintf(cmd,sizeof(cmd),"df -Pk %s 2>%s/checkfreesd.log.txt",sdDev,Util::LOGSPATH);
	FILE *df=popen(cmd,"r");
	if(!df) { return sdfree; }
	char buf[1024]={0};
	while(!feof(df) && !ferror(df) && fgets(buf,sizeof(buf),df)) {
		char *str=buf;
		char *token=buf;
		char *saveptr;
		if(buf[0] == 'F') { continue; } // First line starts with "Filesystem"
		for(int xa=0; xa<6 && token; xa++,str=NULL) {
			token=strtok_r(str, "\t ", &saveptr);
			if(token && xa==3) { sdfree=atol(token); break; }
		}
	}
	pclose(df);
	return sdfree;
}

bool Downloader::cmpFileAge(TDirEntry a, TDirEntry b ) {
	return a.atime < b.atime;
}

off_t Downloader::removeLeastRecentlyPlayedFiles(off_t neededSpace) { // *** Remove OLDEST files (even if .tmp)
	if(!util->isMounted()) { return 0; }
	const char *baseDir="/tmp/play3abn/cache/download";
	off_t sdfree=checkFreeSD(baseDir);
	if(sdfree>=neededSpace) { syslog(LOG_ERR,"Downloader: Already %ld Kb free (only need %ld)",sdfree,neededSpace); return sdfree; }
	syslog(LOG_ERR,"%ld Kb free (need %ld Kb)",sdfree,neededSpace);
	neededSpace-=sdfree;
  DIR *dir=opendir(baseDir);
	if(!dir) { syslog(LOG_ERR,"getLeastRecentlyPlayed: %s: opendir %s",strerror(errno),baseDir); sleep(1); return 0; }
  struct dirent *entry;
	vector<TDirEntry> v;
	syslog(LOG_ERR,"Downloader: Checking for files to remove (to free at least %ld Kb)...",neededSpace);
  char pathname[1024];
	bool stop=false;
  while((entry=readdir(dir)) && !stop) {
    struct stat statbuf;
    if(entry->d_name[0]=='.') { continue; } // Ignore hidden files, current and parent dir
    snprintf(pathname,sizeof(pathname),"%s/%s",baseDir,entry->d_name);
    if(stat(pathname, &statbuf)==-1) { continue; }
    TDirEntry ent;
		ent.atime=statbuf.st_atime;
		ent.ctime=statbuf.st_ctime;
		ent.size=statbuf.st_size;
		ent.name=string(entry->d_name);
		//strncpy(ent.name,entry->d_name,sizeof(ent.name)-1);
    v.push_back(ent);
		stop=Util::checkStop();
  }
  closedir(dir);
	if(stop) { return 0; }
	syslog(LOG_ERR,"Downloader: Sorting files to remove (to free at least %ld Kb)...",neededSpace);
	sort(v.begin(), v.end(), cmpFileAge);
 	syslog(LOG_ERR,"Downloader: Removing files to free at least %ld Kb...",neededSpace);
	off_t removedSize=0;
	for(unsigned int i = 0; i < v.size() && (removedSize/1024)<neededSpace; i++ ) {
		//if(strcmp(v[i].name,"Network_ID_with_DTMF_#4.ogg")==0) { continue; }
		if(v[i].name == "Network_ID_with_DTMF_#4.ogg") { continue; }
		const char *toremove=v[i].name.c_str();
		syslog(LOG_ERR,"Downloader: Removing (%s) %s",Util::fmtTime(v[i].ctime).c_str(),toremove);
    snprintf(pathname,sizeof(pathname),"%s/%s",baseDir,toremove);
		remove(pathname);
		removedSize+=v[i].size;
	}
	return checkFreeSD(baseDir);
}
//******************************************************************************
// Defines needed space in Kb
#define NEEDEDSDSPACE ((9+5)*1024)    // 9 Mb per 1 hour .ogg, 5=news .mp3 and .wav
#define FREESDSPACE (((9*2)+5)*1024) // 9*2=2 hours .ogg, 5=news .mp3 and .wav
// Returns true if vetoed due to missing RADIO volume or insufficient space.
bool Downloader::checkSD(off_t& sdfree) {
	const char *baseDir="/tmp/play3abn/cache/download";
	sdfree=-1;
	//syslog(LOG_ERR,"findSD: %s",sdDev);
	/*if(sdDev[0]) {*/ sdfree=checkFreeSD(baseDir); //}
	// NOTE: Unused: bool removeSome=false;
	if(sdfree < NEEDEDSDSPACE) { sdfree=Downloader::removeLeastRecentlyPlayedFiles(FREESDSPACE); }
	int debug=util->getInt("debug",0);
	if(debug) { syslog(LOG_NOTICE,"RADIO drive has %ldkb free for downloading.",sdfree); }
	return false;
}
//******************************************************************************
// NOTE: Returns vetoReason: 1=no password, 2=no download volume mounted, 3=error during download
int Downloader::docurl(CURL *cHandle, int &dly, const char *srcurl, const char *destfile) {
	errbuf[0]=0;
	//if(!passwordSet()) { dly=60; syslog(LOG_ERR,"docurl: No FTP password set"); return 1; }
	off_t sdfree=-1;
	bool notEnough=checkSD(sdfree);
	if(notEnough)   { syslog(LOG_ERR,"docurl: ERROR: RADIO drive has only %ldkb available.",sdfree); return 2; }
	// NOTE: curl=actual download (extern module)
	dly=curl(cHandle, srcurl,destfile,sizeof(errbuf),errbuf);
	if(errbuf[0]) {
		//syslog(LOG_ERR,"docurl: %s",errbuf);
		if(dly==-2) { dly=0; return 3; } // File did not exist.  Try next file immediately
		else { return 4; } // Some other error
	}
	return 0;
}
//******************************************************************************
//******************************************************************************
//******************************************************************************
//******************************************************************************
struct myprogress {
	const char *srcurl;
  const char *dstfile;
  double lastruntime;
	double dlnow;
	double lastchangetime; // Last time the dlnow changed
	off_t size;
  CURL *curl;
};

/**************************/
struct FtpFile {
  const char *filename;
  FILE *stream;
  off_t size;
  char errmsg[256];
};

/** STATIC METHODS ************************/
#define tmputil util
#undef util
#define util sutil
Util* Downloader::sutil=NULL;
int Downloader::progress(void *p, double dltotal, double dlnow, double ultotal, double ulnow) {
	struct myprogress *myp = (struct myprogress *)p;
	dlnow+=myp->size;
	dltotal+=myp->size;
	CURL *curl = myp->curl;
	double curtime = 0;
	curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curtime);
	// NOTE: LINES WRAP, e.g.:
	/* 2014-09-15 16:29:06: TOTAL TIME:    67: UP:     0 of     0  DOWN: 6562152 of 
	 * 112662543 */
	char buf[512];
	snprintf(buf,sizeof(buf), "%sTIME: %5.0f: DOWN: %5.0f of %5.0f: FROM %s",myp->size?"(RESUMED) ":"TOTAL ", curtime, dlnow, dltotal,myp->srcurl);
	char val[1024];
	snprintf(val,sizeof(val),"%d %d %s",(int)dlnow,(int)dltotal,myp->dstfile);
	setST("ftpget", val);
	if((curtime - myp->lastruntime) >= MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL) {
		myp->lastruntime = curtime;
		syslog(LOG_NOTICE,"%s",buf);
	}
	setST("progress",buf);
	if(dlnow > STOP_DOWNLOAD_AFTER_THIS_MANY_BYTES) { syslog(LOG_ERR,"CURL file too long: DOWN: %5.0f of %5.0f: FROM %s",dlnow,dltotal,myp->srcurl); return 1; }
	if(dlnow != myp->dlnow) {
		myp->dlnow=dlnow;
		myp->lastchangetime=curtime; // Reset elapsed time
	}
	else {
		double elapsed=curtime-myp->lastchangetime;
		if(elapsed > 30) { syslog(LOG_ERR,"CURL timeout: %5.0f: DOWN: %5.0f of %5.0f: FROM %s",elapsed,dlnow,dltotal,myp->srcurl); return 1; }
	}
	return 0;
}

size_t Downloader::my_fwrite(void *buffer, size_t size, size_t nmemb, void *stream) {
  struct FtpFile *out=(struct FtpFile *)stream;
  if(!out->stream) { errno=0; out->stream=fopen(out->filename,"ab"); }
  if(!out->stream) { snprintf(out->errmsg,sizeof(out->errmsg),"curl: %s",strerror(errno));
		syslog(LOG_ERR,"my_fwrite error: %s: %s",out->errmsg,out->filename);
		return 0;
	}
  size_t retval=fwrite(buffer, size, nmemb, out->stream);
  if(retval > 0) {
		out->size+=(size*nmemb); out->errmsg[0]=0;
/*		char val[1024];
		snprintf(val,sizeof(val),"%d %s",(int)out->size,out->filename);
		setST("ftpget", val); */
	}
	if(Util::checkStop()) { return 0; } // Terminate abruptly
	if(retval<=0) { syslog(LOG_ERR,"my_fwrite: fwrite returned %d: %s",retval,strerror(errno)); }
  return retval;
}
#undef util
#define util tmputil
/**.**********************.**/

void Downloader::finishFile(char *tmpfile, const char *dstfile) {
  remove(dstfile);
	int debug=util->getInt("debug",0);
	if(debug) { syslog(LOG_ERR,"finishFile:%s --> %s",tmpfile,dstfile); }
	if(rename(tmpfile,dstfile)<0) { util->moveFile(tmpfile,dstfile); }
	if(strstr(dstfile,"download")) {
		char cmd[256];
		snprintf(cmd,sizeof(cmd),"./update-programlist2.sh %s",dstfile);
		if(system(cmd)<0) { 	syslog(LOG_ERR,"Downloader::finishFile:system(%s): %s",cmd,strerror(errno)); }
	}
}

volatile bool Downloader::curlInitialized=false;
int Downloader::curl(CURL* curl, const char *srcurl, const char *dstfile, int errlen, char *errmsg) {
	//system("./mountsd.sh");
top:
	int debug=util->getInt("debug",0);
	if(debug) { syslog(LOG_ERR,"curl %s to %s",srcurl,dstfile); }
  if(!curl) { syslog(LOG_ERR,"curl_easy_init failed"); return -1; }
  const char *basename=strrchr(dstfile,'/');
  if(basename) { basename++; }
  else { basename=dstfile; }
  char tmpfile[1024];
  snprintf(tmpfile,sizeof(tmpfile),"%s.tmp",dstfile);
  struct FtpFile ftpfile={
    tmpfile, 	// Filename to write to
    NULL,	// Stream to write to
    0,		// Current size of file (amount to skip for FTP resume)
    ""		// Error message
  };
	struct myprogress prog={
		srcurl,
		dstfile,
		0, /*lastruntime*/
		0, /*dlnow*/
		0, /*lastchangetime*/
		0, /*size*/
		curl /* CURL* */
	};
	bool done=false;
	int delay=0;
	//while(!done) {
	ftpfile.size=0;
	prog.size=0;
	struct stat file_info;
	if(stat(tmpfile, &file_info)==0) { prog.size=ftpfile.size=file_info.st_size; } // Get local file size
	if(debug && ftpfile.size>0) { syslog(LOG_ERR,"RESUME from %ld (%s)",ftpfile.size,tmpfile); }
	char val[1024];
	snprintf(val,sizeof(val),"%ld %s",ftpfile.size,basename);
	setST("ftpget", val);
	curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 0);
	syslog(LOG_NOTICE,"Downloader::curl(%s)",srcurl);
	FILE *curlog=util->getlog("curl");
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(curl, CURLOPT_STDERR, curlog);
	curl_easy_setopt(curl, CURLOPT_URL,srcurl);
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &prog);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_fwrite); // write callback
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ftpfile); // callback data
	curl_easy_setopt(curl, CURLOPT_RESUME_FROM, ftpfile.size);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, ftpfile.errmsg);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);

	int connectTimeout=util->getInt("timeout_connect",30); // Five minutes should be generous enough
	int timeout=util->getInt("timeout_transfer", 2000); // If slower than an hour, too slow for us to use.
	int responseTimeout=util->getInt("timeout_response", 60); //240); // Four minutes should be enough (I hope)

  //	curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 3600); // 3ABN unlikely to change DNS all at once
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeout);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
	curl_easy_setopt(curl, CURLOPT_FTP_RESPONSE_TIMEOUT, responseTimeout);
	//curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1000); // Require at least 1000 bytes per second

	CURLcode res=curl_easy_perform(curl);
	if(res!=0 && debug) { syslog(LOG_ERR,"curl:res=%d,errmsg=%s,url=%s",res,ftpfile.errmsg,srcurl); }
	if(strcmp(ftpfile.errmsg,"The requested URL returned error: 404")==0) {
		strcpy(ftpfile.errmsg,"RETR response: 550"); // Geared toward FTP, other places look for this message
	}
	if(ftpfile.stream) { fclose(ftpfile.stream); ftpfile.stream=NULL; }
	if(curlog) { fclose(curlog); curlog=NULL; }
	// /**/fprintf(errlog,"curl message: %s: downloading %s",ftpfile.errmsg,tmpfile); fflush(errlog);
	if(res != CURLE_OK) { // e.g. RETR response: 550: in
		if(strcmp(ftpfile.errmsg,"RETR response: 550")==0 || // FILE NOT FOUND: GO on to next file (or we'll get stuck)
			 strcmp(ftpfile.errmsg,"The requested URL returned error: 404")==0) {
			//syslog(LOG_ERR,"curl failed: %s: %s",ftpfile.errmsg,srcurl);
			snprintf(ftpfile.errmsg,sizeof(ftpfile.errmsg),"File Not Found (550 FTP or 404 HTTP)");
			//remove(tmpfile); // NOTE: No reason to remove tmp file (may be resumable later from another source)
			delay=-2; done=false; // Go on to next file in one second
		}
		else { // e.g. transfer closed with 2883584 bytes remaining to read
			//if(strncmp(ftpfile.errmsg,"transfer closed with ",21)==0) { delay=30; }
			//else {
				syslog(LOG_ERR,"curl failed: %s: in %s",ftpfile.errmsg,tmpfile);
				//if(strstr(ftpfile.errmsg,"Couldn't resolve host")) {
					curl_global_cleanup(); // RETRY due to network failure
					sleep(5);
					curl_global_init(CURL_GLOBAL_DEFAULT);
					if(!Util::checkStop()) { goto top; }
			//		delay=30;
				//}
			//}
		}
		if(delay==0) { delay=10; } //sleep(10); // Retry in 10 seconds
	}
	else { done=true; }
	//}
	if(done) {
		if(debug) { syslog(LOG_NOTICE,"curl fetched: %s",basename); }
		finishFile(tmpfile,dstfile); delay=0; // successful download
	}
	setST("ftpget", "");
	if(ftpfile.errmsg[0]) { snprintf(errmsg,errlen,"CURL:%s",ftpfile.errmsg); }
	else { ftpfile.errmsg[0]=0; }
	return delay;
	//   curl_global_cleanup(); // NOTE: Leave this for the main program to do
}
// -------------------------------------------------------------------------------------------------------------


int main(int argc, char **argv) {
	Downloader::sutil=new Util("mdown#3");
	new Downloader();
}
