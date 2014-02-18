#include "util.hh"
//#include "mutex.hh"

const char* Util::stopflag="/tmp/play3abn/tmp/.stop.flag";
char Util::TEMPPATH[256]="/tmp/play3abn/tmp";
char Util::LOGSPATH[256]="/tmp/play3abn/tmp/logs";
char Util::VARSPATH[256]=ORIGVARS;
bool Util::firstWarningPlaypass=false;
bool Util::initialized=false;
volatile int  Util::bufferSeconds=0;
time_t Util::tzofs=0;
const char *Util::zone="";
bool Util::doExit=false;
bool Util::noNet=true;
bool Util::hadNoNet=false; // Network was down when checked
bool Util::resetDate=false;
pthread_mutex_t Util::dologMutex;
char Util::myip[16]={0};
char Util::publicip[16]={0};
char Util::igloowareip[16]={0};
char Util::macaddr[18]={0};

/** init should not be called by multiple simultaneous threads **/
void Util::init(bool initlogs) {
	if(initialized) { return; }
	remove(stopflag);
	srandom(Time()); srand(Time()); // Not sure which random function is used by random_shuffle
	if(mkdir("/tmp/play3abn",0755)==-1 && errno!=EEXIST) { syslog(LOG_ERR,"Util::init: mkdir: %s: /tmp/play3abn",strerror(errno)); }
	if(mkdir(ORIGVARS,0755)==-1 && errno!=EEXIST) { syslog(LOG_ERR,"Util::init: mkdir: %s: %s",strerror(errno),ORIGVARS); }
	if(mkdir(TEMPPATH,0755)==-1 && errno!=EEXIST) { syslog(LOG_ERR,"Util::init: mkdir: %s: %s",strerror(errno),TEMPPATH); }
	if(mkdir(LOGSPATH,0755)==-1 && errno!=EEXIST) { syslog(LOG_ERR,"Util::init: mkdir: %s: %s",strerror(errno),LOGSPATH); }
	char timebuf[32];
	time_t now=Time();
	fmtTime(now,timebuf);
	for(char *pp=timebuf; *pp; pp++) {
		if(isspace(*pp) || *pp==':') { *pp='_'; }
	}
	string curlogs="/tmp/play3abn/curlogs";
	struct stat statbuf; // If more than 10 seconds old, create new logs dir
	//bool existLogs=true;
	if(stat(curlogs.c_str(), &statbuf)==-1 || initlogs) {
		//snprintf(LOGSPATH,sizeof(LOGSPATH),"/tmp/play3abn/tmp/logs/%s",timebuf);
		snprintf(LOGSPATH,sizeof(LOGSPATH),"/tmp/play3abn/tmp/logs/%s",timebuf);
		mkdir(LOGSPATH,0755);
		unlink(curlogs.c_str());
		if(symlink(LOGSPATH, curlogs.c_str())) ;
		//existLogs=false;
	}
	else { strncpy(LOGSPATH,curlogs.c_str(),sizeof(LOGSPATH)); }
	//if(pthread_mutex_init(&dologMutex, NULL)<0) { syslog(LOG_ERR,"Util: pthread_mutex_init: %s",strerror(errno)); abort(); }
	/** SET VARIABLES PATH **/
	uid_t uid=geteuid();
	if(uid!=0) {
		if(stat("vars", &statbuf)!=-1) { strncpy(VARSPATH,"vars",sizeof(VARSPATH)); }
	}
	else {
		if(stat("/play3abn/vars", &statbuf)!=-1) { strncpy(VARSPATH,"/play3abn/vars",sizeof(VARSPATH)); }
		else if(stat("/recorder/vars", &statbuf)!=-1) { strncpy(VARSPATH,"/recorder/vars",sizeof(VARSPATH)); }
		else if(stat("vars", &statbuf)!=-1) { strncpy(VARSPATH,"vars",sizeof(VARSPATH)); }
	}
	mkdir(VARSPATH,0755);
	initialized=true;
	getBufferSeconds();
	sigs_init();
	//logRotate not needed since we create new dirs each time
	//if(!existLogs) { logRotate(true); } /** NOTE: logRotate MUST NOT be called until initialized, else recursion will occur **/
  tzsetup();
#define util this
	//syslog(LOG_INFO,"Util::init done");
#undef util
}
bool Util::checkStop() {
	struct stat statbuf;
	return (stat(stopflag, &statbuf)!=-1);
}
void Util::Stop() {
	int fd=creat(stopflag,0755);
	if(fd!=-1) { close(fd); }
}
int Util::getBufferSeconds() {
	bufferSeconds=getInt("bufsecs",3600*3);
	return bufferSeconds;
}
void Util::tzsetup() {
  tzset();
	tzofs=timezone;
  time_t startTime=Time();
	struct tm tms;
  struct tm* tptr=localtime_r(&startTime,&tms);
	if(strcmp(tptr->tm_zone,"AKDT")==0) { /*syslog(LOG_ERR,"DST is in effect.");*/ tzofs-=3600; } // DST in effect
	zone=tptr->tm_zone;
}
void Util::set(const char *varName, const char *varVal) {
  char fname[1024];
  char fnameFinal[1024];
	while(!initialized) { sleep(1); }
	if(varName[0]=='.') {
		snprintf(fname,sizeof(fname)-1,"%s/%s.txt.tmp",ORIGVARS,&varName[1]);
		snprintf(fnameFinal,sizeof(fnameFinal)-1,"%s/%s.txt",ORIGVARS,&varName[1]);
	}
	else {
		snprintf(fname,sizeof(fname)-1,"%s/%s.txt.tmp",VARSPATH,varName);
		snprintf(fnameFinal,sizeof(fnameFinal)-1,"%s/%s.txt",VARSPATH,varName);
	}
  FILE *fp=fopen(fname,"wb");
  if(!fp) { syslog(LOG_ERR,"set: %s: %s",fname,strerror(errno)); 
//		if(system("ls -ld /proc/$(pidof mplay.bin)/fd/* >/tmp/mplay.err")<0) { syslog(LOG_ERR,"ls(pid)#1: %s",strerror(errno)); }
	abort(); }
  fprintf(fp,"%s",varVal);
  fclose(fp);
	rename(fname,fnameFinal);
}
void Util::inc(const char *varName, int incVal) {
	setInt(varName,getInt(varName, 0)+incVal);
}
void Util::setInt(const char *varName, int varVal) {
  char valstr[64];
  snprintf(valstr,sizeof(valstr)-1,"%d",varVal);
  set(varName,valstr);
}
char *Util::getline(const char *fname, char *varVal, int valSize) {
  FILE *fp=fopen(fname,"rb");
  if(valSize>0) { varVal[0]=0; }
  if(!fp) { return varVal; }
  if(!fgets(varVal,valSize,fp)) { fclose(fp); return varVal; } // Truncate and return
  int len=strlen(varVal); // Strip any trailing newline
	while((len>0 && varVal[len-1]=='\n') || varVal[len-1]=='\r') { varVal[len-1]=0; len--; }
  fclose(fp);
	return varVal;
}
char *Util::get(const char *varName, char *varVal, int valSize) {
  char fname[1024];
	while(!initialized) { sleep(1); }
	//if(!initialized) { init(false); }
	if(varName[0]=='.') { snprintf(fname,sizeof(fname)-1,"%s/%s.txt",ORIGVARS,&varName[1]); }
	else { snprintf(fname,sizeof(fname)-1,"%s/%s.txt",VARSPATH,varName); }
	return getline(fname,varVal,valSize);
}
int Util::getInt(const char *varName, int dft) {
  char valstr[64];
  get(varName,valstr,sizeof(valstr));
	if(!valstr[0]) { return dft; }
  return atoi(valstr);
}
time_t Util::lastIPcheck=0;
void Util::getPublicIP() { // Sets vars/publicip.txt and vars/igloowareip.txt to the appropriate IP addresses
	time_t now=time(NULL);
	if(now-lastIPcheck < 3600) { return; }
	lastIPcheck=now;
#define util this
	getST(publicip);
	getST(igloowareip);
	getST(macaddr);
	if(!publicip[0] || !igloowareip[0]) {
		// .com instead of .org, and also handle nslookup of iglooware.ath.cx
		syslog(LOG_INFO,"Util::getPublicIP: ./igloowareip.sh");
		if(system("./igloowareip.sh >/dev/null 2>&1")==-1) { syslog(LOG_ERR,"getPublicIP:system:igloowareip.sh failed: %s",strerror(errno)); }
		getST(publicip);
		getST(igloowareip);
		getST(macaddr);
#undef util
	}
/*	FILE *ts=netcat("www.whatismyip.org",80);
	if(ts) {
		fprintf(ts,"GET / HTTP/1.0\r\n\r");
		char line[1024];
		bool readyForIP=false;
		while(ts && !feof(ts) && fgets(line,sizeof(line),ts)) {
			if(line[0]=='\r' && line[1]=='\n') { readyForIP=true; }
			else if(readyForIP && line[0] && !strstr(line,"Too frequent!")) {
				strncpy((char *)publicip,line,sizeof(publicip));
				syslog(LOG_ERR,"publicip=%s",publicip);
				set("publicip", (char *)publicip);
				break;
			}
		}
		fclose(ts);
	}*/
}

void Util::setConn() { /** ping -c 1 $(cat vars/baseurl.txt | cut -d '/' -f 3) | grep "transmitted.*received" **/
	if(system("ping -W 5 -c 1 $(cat vars/baseurl.txt | cut -d '/' -f 3) 2>/dev/null | grep 'transmitted.*1.*received' | "
		"sed -e 's/.*transmitted, //g' | awk '{print $1}' >/tmp/play3abn/vars/isconn.txt.tmp && "
		"mv /tmp/play3abn/vars/isconn.txt.tmp /tmp/play3abn/vars/isconn.txt 2>/tmp/setconn.txt &")<0) {}
/*#define util this
	char baseurl[256];
	get("baseurl",baseurl,sizeof(baseurl));
	char result[2];
	char cmd[256];
	char *pp=strstr(baseurl,"://");
	if(!pp) { syslog(LOG_ERR,"setConn: invalid baseurl: %s",baseurl); return; }
	else { pp+=3; }
	char *qq=strchr(pp,'/');
	if(qq) { *qq=0; }
	if(!pp[0]) { syslog(LOG_ERR,"setConn: empty baseurl!"); return; }
	snprintf(cmd,sizeof(cmd),"/bin/ping -c 1 %s 2>/tmp/play3abn/curlogs/ping.txt",pp);
	rungrep(cmd,"packet loss",1,sizeof(result),result);
	setIT("isconn",(result[0]=='1'));
#undef util
*/
}
void Util::setIP() {
	char interface[32];
	rungrep("/sbin/route -n","0.0.0.0",8,sizeof(interface),interface);
	// /sbin/ifconfig "$if" | grep "inet addr:" | awk '{print $2}' | cut -d ':' -f 2
	char cmd[32];
	char outbuf[32];
	char *pp;
	if(!interface[0]) { noNet=true; goto otherTasks; }
	snprintf(cmd,sizeof(cmd),"/sbin/ifconfig %s",interface);
	rungrep(cmd,"inet addr:",2,sizeof(outbuf),outbuf);
	if(!outbuf[0]) { noNet=true; goto otherTasks; }
	pp=strchr(outbuf,':');
	if(!pp) { pp=(char *)""; }
	else { pp=pp+1; }
	if(strcmp((char *)myip,pp)!=0) {
		strncpy((char *)myip,pp,sizeof(myip));
		syslog(LOG_ERR,"myip=%s,interface=%s",myip,interface);
		noNet=false;
	}
otherTasks:
	if(!noNet) { getPublicIP(); }
}
int Util::netcatSocket(const char *addr, int port) { // Read data from specified addr and port
	//dolog( "play3abn:netcatSocket to %s:%d", addr,port);
	int sock;
	struct sockaddr_in server;
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		syslog(LOG_ERR,"play3abn:netcatSocket: %s",strerror(errno)); sleep(1); return -1;
	}

	struct hostent *host;     /* host information */
	if ((host=gethostbyname(addr)) == NULL) {
		syslog(LOG_ERR,"play3abn:netcatSocket:gethostbyname '%s' failed", addr); sleep(1); return -1;
	}
	struct in_addr h_addr;    /* internet address */
	memset(&h_addr,0,0);
	h_addr.s_addr = *((unsigned long *) host->h_addr_list[0]);
	//syslog(LOG_ERR,"%s", inet_ntoa(h_addr));
	/* Construct the server sockaddr_in structure */
	memset(&server, 0, sizeof(server));       /* Clear struct */
	server.sin_family = AF_INET;                  /* Internet/IP */
	server.sin_addr.s_addr = *((unsigned long *) host->h_addr_list[0]); /* First IP address returned */
	server.sin_port = htons(port);       /* server port */
	//syslog(LOG_ERR,"play3abn:netcatSocket to %s:%d", inet_ntoa(server.sin_addr),port);
	/* Establish connection */
	if (connect(sock,(struct sockaddr *) &server,sizeof(server)) < 0) {
		syslog(LOG_ERR,"play3abn:netcatSocket:connect failed: %s",strerror(errno)); sleep(1); return -1;
	}
	return sock;
}
FILE *Util::netcat(const char *addr, int port) { // Read data from specified addr and port
	return fdopen(netcatSocket(addr,port),"r+");
}
string Util::mergePassword(string url) {
	if(!playpass[0]) { get("playpass",playpass,sizeof(playpass)); }
	if(!playpass[0] && !firstWarningPlaypass) { syslog(LOG_ERR,"Util::mergePassword: vars/playpass.txt not set."); firstWarningPlaypass=true; }
	if(!playpass[0]) { sleep(1); return ""; } // Don't merge empty password into url
	char curlUrl[1024];
	snprintf(curlUrl,sizeof(curlUrl),url.c_str(),playpass); // Plug in the password
	return string(curlUrl);
}
/** Given an info string in the form "2011-06-09 12:40:00 1 0066 IT'S A WONDERFUL DAY-Heritage Singers.ogg",
 *  return the time, flag, length in seconds, and display name for the file. */
/** NOTE: If flag is -1, actual flag,length, and display name should be instead decoded from link target **/
// NOTE: In that case, link target will be e.g. /tmp/play3abn/tmp/lencache/SERMON4/^.*/len/0315 The Good news about hell.ogg
// NOTE: As of 2014-01-21, format should be e.g.:
// NOTE:    infs.one=2014-01-22 19:00:00 -1
// NOTE:    infs.two=BIBLESTORIES 1377 /tmp/play3abn/cache/Radio/Bible stories/Vol 01/V1-06a  The Disappearing Idols.ogg
/*bool Util::parseInfo(strings& infs, time_t& otime, int& flag, int& seclen, string& dispname, bool leaveUrlEncoded) {
MARK
	string inf=infs.one;
	string tgt=infs.two;
MARK
	char info[512];
	const char *link=inf.c_str();
	strncpy(info,link,sizeof(info));
	struct tm timestruct={0};
	char *result=strptime(info,"%Y-%m-%d %H:%M:%S",&timestruct);
	if(!result) { return false; }
	int tzHour=(timezone-Util::tzofs); // 0 or 3600 depending on whether DST is in effect
	otime=mktime(&timestruct)-tzHour; //+Util::bufferSeconds; // Play some time after scheduled time
	const char *nameptr;
	flag=(int)strtol(&result[1], &result, 10);
	bool secondInfo=(flag==-1);
	string catcode="";
	if(secondInfo) {
		result=(char *)tgt.c_str(); /// WARNING: DO NOT CHANGE CONTENTS OF result, ONLY WHAT IT POINTS TO 
		if(isdigit(result[0])) {
			flag=(int)strtol(result, &result, 10); // Result points past SPACE after flag digits
		}
		else { // Next item should be category code rather than flag (coming from schedfixed)
			flag=366;
			char *pp=strchr(result,' ');
			if(pp) { // Skip space
				*pp=0;
				catcode=string(result);
				result=pp; //&pp[1];
			}
		}
	}
  if(isdigit(result[1])) { seclen=(int)strtol(&result[1], (char **)&nameptr, 10); }
  else { seclen=flag; flag=999; nameptr=result; }
	if(secondInfo) { 	
		const char *pathptr=nameptr;
		while(pathptr[0]==' ') { pathptr++; } // Past possible space in name to get full path
		infs.two=string(pathptr);	
	}
  const char *pp=strrchr(nameptr,'/');
	if(pp) { nameptr=&pp[1]; } // Past last slash in path 
	while(nameptr[0]==' ' || nameptr[0]=='-') { nameptr++; } // Past leading space or dash to get display name
// NOTE: If link target contains /lencache/ then include following segment as prefix to dispname, e.g. SERMON4 in this case (or just SERMON) 
	string dispPrefix="";
// 	pp=strstr(infs.two.c_str(),"/lencache/");
// 	if(pp) {
// 		pp+=10;
// 		const char *qq=strchr(pp,'/');
// 		if(qq) {
// 			//qq--;
// 			dispPrefix=string(pp,qq-pp)+"-";
// 		}
// 	}
	if(catcode != "") { dispPrefix=catcode+":"; }
	dispname=dispPrefix+urlDecode(string(nameptr));
MARK
	if(!leaveUrlEncoded) { infs.two=urlDecode(infs.two); }
	return true;
}
*/
string Util::urlDecode(string src) {
	string ret;
	unsigned int i, ii;
	for(i=0; i<src.length(); i++) {
		if(src[i]=='%') {
			sscanf(src.substr(i+1,2).c_str(), "%x", &ii);
			ret+=static_cast<char>(ii);
			i=i+2;
		}
		else { ret+=src[i]; }
	}
	return (ret);
}
string Util::urlEncode(string src) {
	string ret;
	unsigned int i;
	for(i=0; i<src.length(); i++) {
    if(     src[i]==' ') { ret+="%20"; }
		else if(src[i]==':') { ret+="%58"; }
		else if(src[i]==';') { ret+="%59"; }
		else if(src[i]=='=') { ret+="%61"; }
		else { ret+=src[i]; }
	}
	return (ret);
}
//---------------------------------------------------------------------------------------------------------------------


/** Enqueue file for download or playback (depending on whether url is an actual URL, or a local file path) **/
/** NOTE: MODIFIED version stores flag,expectSecs in link with url                                          **/
/** NOTE: (expectSecs and file Last Used date can ALSO be retrieved from leastrecentcache/cached entries)   **/
/** NOTE: dispname can be derived from url                                                                  **/
/** NOTE: If replaceExisting is false, we will instead create an entry for the next second for existing one **/
void Util::enqueue(time_t playAt, string url, string catcode, int flag, int expectSecs, bool replaceExisting) {
	strings item=itemEncode(playAt, flag, expectSecs, url, catcode);
	char tmpentry[1024];
	strncpy(tmpentry,item.one.c_str(),sizeof(tmpentry)-4);
	strcat(tmpentry,".tmp");
	/** TWO-STEP CREATION of symlink to ensure atomic creation/replacement of existing scheduled item **/
	if(replaceExisting) {
		if(symlink(item.two.c_str(), tmpentry)<0) { syslog(LOG_ERR,"Util::enqueue:symlink: %s: %s",strerror(errno),tmpentry); }
		else {
			if(rename(tmpentry,item.one.c_str())<0) { syslog(LOG_ERR,"Util::enqueue:rename: %s: %s",strerror(errno),item.one.c_str()); }
		}
	}
	else {
		if(symlink(item.two.c_str(), item.one.c_str())<0) { // If this exists, do not replace it
			syslog(LOG_INFO,"Util::enqueue:symlink: %s: %s",strerror(errno),item.one.c_str());
		}
	}
}
/** INPUT: scheduled time, flag, playback seconds, category code, URL or local file path to be encoded.
 * OUTPUT: a pair of strings containing encoded information **/
strings Util::itemEncode(time_t playAt, int flag, int expectSecs, string catcode, string url) {
	char centry[1024];
	const char *qdir="/tmp/play3abn/tmp/playq";
	mkdir(qdir,0777);
	snprintf(centry,sizeof(centry),"%s/%s",qdir,Util::fmtTime(playAt).c_str()); // NOTE: Leaving placeholders to avoid crashing old/transitional software
	char surl[1024];
	snprintf(surl,sizeof(surl),"catcode=%s;expectSecs=%04d;flag=%d;url=%s",catcode.c_str(),expectSecs,flag,urlEncode(url).c_str());
	return strings(centry,surl);
}
/** itemDecode replaces parseInfo
 *  INPUT: infs => a pair of strings
 * OUTPUT: scheduled time, flag, playback seconds, category code, decoded URL or local file path **/
int Util::itemDecode(strings infs, time_t& otime, int& flag, int& expectSecs, string& catcode, string& dispname, string& decodedUrl) {
	/** DECODE otime **/
	struct tm timestruct={0};
	char *result=strptime(infs.one.c_str(),"%Y-%m-%d %H:%M:%S",&timestruct);
	if(!result) { return -2; }
	int tzHour=(timezone-Util::tzofs); // 0 or 3600 depending on whether DST is in effect
	otime=mktime(&timestruct)-tzHour; //+Util::bufferSeconds; // Play some time after scheduled time
	if(otime==-1) { return -1; }
	int ret=(4+8+16+32); // NOTE: Which items did we fail to find for decoding?  flag,expectSecs,catcode,url
	/** DECODE remainder of items (in no particular order): catcode, expectSecs; flag; url (decoded) **/
	/** NOTE: Format var=val;var2=val2 **/
	char buf[1024];
	strncpy(buf,infs.two.c_str(),sizeof(buf)-1); buf[strlen(buf)]=0;
	char *pp=buf;
	do {
		char *qq=strchr(pp,';');
		if(qq) { *qq=0; qq++; } // NEXT
		char *rr=strchr(pp,'=');
		if(rr) { // VALUE
			*rr=0; rr++;
			if     (strcmp(pp,"flag") == 0)       { flag=atoi(rr);             ret-=4;  }
			else if(strcmp(pp,"expectSecs") == 0) { expectSecs=atoi(rr);       ret-=8;  }
			else if(strcmp(pp,"catcode") == 0)    { catcode=string(rr);        ret-=16; }
			else if(strcmp(pp,"url") == 0)        { decodedUrl=urlDecode(string(rr)); ret-=32; }
		}
		pp=qq; // Next
	} while(pp);	
	
	/** Decode display name **/
	const char *qq=decodedUrl.c_str();
	const char *rr=strrchr(qq,'/');
	if(rr) { qq=&rr[1]; } // Past last slash in path 
	while(qq[0]==' ' || qq[0]=='-') { qq++; } // Past leading space or dash to get display name
	string dispPrefix="";
	if(catcode != "") { dispPrefix=catcode+":"; }
	dispname=dispPrefix+string(qq);

	return -ret;
}		
	///////////// NOTE: IS ANY OF THE REST OF THIS RELEVANT?
/*	const char *nameptr;
	flag=(int)strtol(&result[1], &result, 10);
	bool secondInfo=(flag==-1);
	string catcode="";
	if(secondInfo) {
		result=(char *)tgt.c_str(); // WARNING: DO NOT CHANGE CONTENTS OF result, ONLY WHAT IT POINTS TO
		if(isdigit(result[0])) {
			flag=(int)strtol(result, &result, 10); // Result points past SPACE after flag digits
		}
		else { // Next item should be category code rather than flag (coming from schedfixed)
			flag=366;
			char *pp=strchr(result,' ');
			if(pp) { // Skip space
				*pp=0;
				catcode=string(result);
				result=pp; //&pp[1];
			}
		}
	}
  if(isdigit(result[1])) { expectSecs=(int)strtol(&result[1], (char **)&nameptr, 10); }
  else { expectSecs=flag; flag=999; nameptr=result; }
	if(secondInfo) { 	
		const char *pathptr=nameptr;
		while(pathptr[0]==' ') { pathptr++; } // Past possible space in name to get full path
		infs.two=string(pathptr);	
	}
  const char *pp=strrchr(nameptr,'/');
	if(pp) { nameptr=&pp[1]; } // Past last slash in path 
	while(nameptr[0]==' ' || nameptr[0]=='-') { nameptr++; } // Past leading space or dash to get display name
// NOTE: If link target contains /lencache/ then include following segment as prefix to dispname, e.g. SERMON4 in this case (or just SERMON)
	string dispPrefix="";
	if(catcode != "") { dispPrefix=catcode+":"; }
	dispname=dispPrefix+urlDecode(string(nameptr));
MARK
	if(!leaveUrlEncoded) { infs.two=urlDecode(infs.two); }

	return true;
}
*/

/********************************************************************************************/
//#endif

/** NOTE:  Run command, scanning output lines for 'grepfor' string.
           Return first matching data from 'column' into 'buflen' bytes of 'outbuf'
           (If column is -1, then return first matching entire line)
           Alternately, if output list is supplied, matching data from 'column' is returned for all lines.
           (If column is -1, then return all matching lines in output list)
**/
void Util::rungrep(const char *cmd, const char *grepfor, int column, int buflen, char *outbuf, list<string>* output) {
	FILE *fp=popen(cmd,"r"); // e=close on exec, useful for multithreaded programs
	if(!fp) { syslog(LOG_ERR,"rungrep: %s: %s",cmd,strerror(errno)); }
	char buf[1024];
	bool matchStart=false;
	if(grepfor[0]=='^') { matchStart=true; grepfor++; }
	while(fp && !feof(fp) && fgets(buf,sizeof(buf),fp)) {
		while(char *pp=strrchr(buf,'\r')) { *pp=0; }
		while(char *pp=strrchr(buf,'\n')) { *pp=0; }
//syslog(LOG_ERR,"rungrep: buf=%s",buf);
		char *at=NULL;
		if((at=strstr(buf,grepfor)) != NULL) {
			if(matchStart && (at-buf)>0) { continue; } // Not matching at start
			if(column==-1) { // Save entire line instead of specific column
				if(output) { output->push_back(string(buf)); continue; }
				else if(buflen>0) { strncpy(outbuf,buf,buflen); pclose(fp); return; }
			}
			//syslog(LOG_ERR,"rungrep:buf=%s",buf);
			char *str=buf,*token=buf,*saveptr;
			for(int xa=0; xa<column; xa++,str=NULL) {
				token=strtok_r(str, "\t\r\n ", &saveptr);
				if(!token) { break; }
//syslog(LOG_ERR,"rungrep: xa=%d,column-1=%d,token=%s",xa,column-1,token);
				if(xa==column-1) {
					if(output) { output->push_back(string(token)); break; }
					else if(buflen>0) { strncpy(outbuf,token,buflen); pclose(fp); return; }
				}
			}
		}
	}
	if(outbuf && buflen>0) { outbuf[0]=0; } // not found
	if(fp) { pclose(fp); }
}

string Util::getlogprefix() {
	pid_t tid=gettid();
	char sTid[32];
	snprintf(sTid,sizeof(sTid),"%s-%d",threadname,tid);
	return string(sTid);
}

string Util::getlogpath(const char *plogname) {
	char logpath[1024];
	if(plogname==NULL || !plogname[0]) { snprintf(logpath,sizeof(logpath),"%s",LOGFIFO); } // Default to standard log if not specified
	else { snprintf(logpath,sizeof(logpath),"%s/%s.log.txt",LOGSPATH,plogname/*,module.c_str()*/); }
	return string(logpath);
}
// Open a file handle for the given name
FILE *Util::getlog(const char *plogname) {
	char logpath[1024];
	snprintf(logpath,sizeof(logpath),"%s",getlogpath(plogname).c_str());
	FILE *logfp=fopen(logpath, "a");
	if(!logfp) {
		char *pp=strrchr(logpath,'/');
		if(pp) { *pp=0; mkdir(logpath,0755); *pp='/'; } // Make the missing log directory
		logfp=fopen(logpath, "a");
	}
	if(!logfp) { syslog(LOG_ERR,"getlog: %s: %s",strerror(errno),logpath); logfp=stderr; }
	return logfp;
}

int Util::dologX(string msg) { return dologX(msg.c_str()); }
//#define NOLOG 1
char timeBuf[21];
const char *T() {
	time_t t=time(NULL);
	struct tm tms;
  struct tm* tptr=localtime_r(&t,&tms);
	snprintf(timeBuf,20,"%04d-%02d-%02d %02d:%02d:%02d",
		1900+tptr->tm_year,tptr->tm_mon+1,tptr->tm_mday,tptr->tm_hour,tptr->tm_min,tptr->tm_sec);
	return timeBuf;
}
int Util::dologX(const char *format, ...) {
	va_list ap;
#ifdef NOLOG
	return 0;
#endif
	if(format[1]=='@') {
		int debug=getInt("debug",0);
		int loglevel=atoi(format);
		if(debug<loglevel) { return 0; }
		format=&format[2];
	}
	va_start(ap, format);

	char timebuf[32+27]; // e.g. "Filler22513: "
	//char timeBuf2[32];
	//snprintf(timebuf,sizeof(timebuf),"%s (+%-5d)::%-16s: ",fmtTime(Time(),timeBuf2),bufferSeconds,getlogprefix().c_str());
	snprintf(timebuf,sizeof(timebuf),"%-16s: ",getlogprefix().c_str());
	int tbLen=strlen(timebuf);
	int charsWritten=tbLen;
	FILE *logfp=fopen(LOGFIFO,"wb");
	if(!logfp) {
		if(mkfifo(LOGFIFO,0600)==-1 && errno!=EEXIST) { syslog(LOG_ERR,"%s: mkfifo(%s): %s",T(),LOGFIFO,strerror(errno)); abort(); }
		logfp=fopen(LOGFIFO,"wb");
		if(!logfp) { syslog(LOG_ERR,"%s: fopen(%s): %s",T(),LOGFIFO,strerror(errno)); abort(); }
	}
	fputs(timebuf,logfp);
	//va_start(ap, format);
	charsWritten+=vfprintf(logfp,format,ap);
	fputs("",logfp);
	fclose(logfp);
	va_end(ap);
	return charsWritten;
}

void Util::logRotate(bool rotate) { // By default, Only rotate if log file is getting large
	int debug=getInt("debug",0);
	off_t maxSize=(debug?1000000:100000); // 1 Mb debug, 100k non-debug
  DIR *dir=opendir(LOGSPATH);
	if(!dir) { /*perror("opendir " LOGSPATH); sleep(1);*/ return; } // No logs to rotate
  struct dirent *entry;
  while((entry=readdir(dir))) {
    if(entry->d_name[0]=='.') { continue; } // Ignore hidden files, current and parent dir
		if(strstr(entry->d_name,".old.txt")) { continue; } // Ignore old logs (they'll be unlinked later)
    struct stat statbuf;
		char path[1024];
		snprintf(path,sizeof(path),"%s/%s",LOGSPATH,entry->d_name);
		if(stat(path, &statbuf)!=-1) {
			bool isLarge=(statbuf.st_size>maxSize); // Larger than this, we rotate
			if(rotate || isLarge) {
				char oldpath[1024];
				strncpy(oldpath,path,sizeof(oldpath));
				char *pp=strstr(oldpath,".log");
				if(pp) { strncpy(pp,".old.txt",sizeof(oldpath)-(pp-oldpath)); }
				//syslog(LOG_ERR,"unlink(%s)",oldpath);
				unlink(oldpath);
				//syslog(LOG_ERR,"path=%s,oldpath=%s",path,oldpath);
				rename(path,oldpath);
			}
		}
	}
	closedir(dir);
}

/** Return 0 if two strings are equal up to the length of the shorter (but neither string is empty)
 * -1 if s1's last char is less, +1 if s1's last char is greater or equal **/
int Util::strcmpMin(const char *s1, const char *s2) {
	int count=0;
	while(*s1 && *s2 && *s1==*s2) { s1++; s2++; count++; }
	if(count>0 && (*s1==0 || *s2==0)) { return 0; }
	return ((*s1)>=(*s2))?1:-1;
}

string Util::fmtInt(int value, int width) {
	char fmt[16];
	char buf[16];
	snprintf(fmt,sizeof(fmt),"%%0%dd",width);
	snprintf(buf,sizeof(buf),fmt,value);
	return string(buf);
}

string Util::fmtTime(time_t t) {
	char timebuf[21];
	return string(fmtTime(t,timebuf));
}
const char *Util::fmtTime(time_t t, char *timeBuf) {
	struct tm tms;
  struct tm* tptr=localtime_r(&t,&tms);
	snprintf(timeBuf,20,"%04d-%02d-%02d %02d:%02d:%02d",
		1900+tptr->tm_year,tptr->tm_mon+1,tptr->tm_mday,tptr->tm_hour,tptr->tm_min,tptr->tm_sec);
	return timeBuf;
}

/** An inaccurate sleep but gives an opportunity to break out of the sleep in the event of exit. **/
void Util::longsleep(int seconds) {
	if(seconds==0) { return; }
	if(seconds<0) { // Don't sleep a negative amount
		usleep(250000); // Sleep 1/4 second to avoid busy waiting
		return; // The negative sleep happens when we are waiting for the next file to come
	}
	int count=seconds/10;
	int remainder=seconds%10;
	for(int xa=0; xa<count && !checkStop() && !doExit && !(resetDate && strcmp(threadname,"setdate")==0); xa++) {
		if((hadNoNet!=noNet) && !noNet) { syslog(LOG_ERR,"NET up."); return; } // NOTE: Early exit if network comes up
		sleep(10);
	}
	if(!doExit && !checkStop()) { sleep(remainder); }
}

/************************************************************************/
/** SIGNAL HANDLING *****************************************************/
/************************************************************************/
void Util::listThreadMarks(Util* util) {
	char timebuf[32];
	syslog(LOG_ERR,"listThreadMarks: __CALLS__.size()=%d",__CALLS__.size());
	for(map<pid_t, TMarker>::iterator iter = __CALLS__.begin(); iter != __CALLS__.end(); ++iter) {
		pid_t tid=(*iter).first;
		string module=Thread::modules[tid];
		char sTid[32];
		snprintf(sTid,sizeof(sTid),"%s%d",module.c_str(),tid);
		module=string(sTid);
		TMarker marker=(*iter).second;
		syslog(LOG_ERR,"  (at %s, thr %s) %s:%s:%d",
			fmtTime(marker.when, timebuf),module.c_str(),marker.filename.c_str(),marker.function.c_str(),marker.line);
	}
}
void Util::signalexit(string signame, bool ex) {
	//if(oscreen) { oscreen->Shutdown(); }
	Util* util=new Util("signalexit");
	//FILE *logfp=util->getlog(NULL);
	// FIXME: Check to see if mscreen is running.  If not, do this: setIT("doLogStderr",1);
	syslog(LOG_ERR,"%s in play3abn.",signame.c_str());
	//sleep(5);
	//if(system("reset")==-1) {}
	//syslog(LOG_ERR,"%s in play3abn.",signame.c_str());
	syslog(LOG_ERR,"Function calls:");
	listThreadMarks(util);
	print_stacktrace(stderr);
	//print_stacktrace(logfp);
	//fclose(logfp);
	exit(1);
}

void Util::bye() {
	//if(Curler::curlInitialized) { Curler::curlInitialized=false; curl_global_cleanup(); }
	//syslog(LOG_ERR,"BYE");
}

void Util::sig_segv(int signo, siginfo_t *si, void *p) { signalexit("SIGSEGV",true); }
void Util::sig_fpe(int signo, siginfo_t *si, void *p) { signalexit("SIGFPE",true); }
void Util::sig_ill(int signo, siginfo_t *si, void *p) { signalexit("SIGILL",true); }
void Util::sig_pipe(int signo, siginfo_t *si, void *p) { }
//void Util::sig_kill(int signo, siginfo_t *si, void *p) {signalexit("SIGKILL",true); }
void Util::sig_term(int signo, siginfo_t *si, void *p) { signalexit("SIGTERM",true); }
void Util::sig_int(int signo, siginfo_t *si, void *p) { signalexit("SIGINT",true); }
void Util::sig_abrt(int signo, siginfo_t *si, void *p) { signalexit("SIGABRT",true); }
void Util::sig_hup(int signo, siginfo_t *si, void *p) { signalexit("SIGHUP",true); }
void Util::sig_quit(int signo, siginfo_t *si, void *p) { signalexit("SIGQUIT",true); }
void Util::sig_usr1(int signo, siginfo_t *si, void *p) {}
void Util::sig_usr2(int signo, siginfo_t *si, void *p) {
	syslog(LOG_ERR,"SIGUSR2#1");
	resetDate=true;
}

#define addset(set,signo) { sigaddset(set,signo); signames[signo]=&(#signo)[1]; }

void Util::Execute(void* arg) {
/*	syslog(LOG_ERR,"Util::Execute: Maintaining IP addresses...");
	while(!doStop) {
		setIP();
		longsleep(60);
	}*/
	syslog(LOG_ERR,"Util::Execute: DONE");
}

void Util::sig_init(int signal, void (*handler)(int, siginfo_t *, void *)) {
  struct sigaction act,oldact;
  sigemptyset(&act.sa_mask);
  act.sa_flags=SA_SIGINFO;
  //act.sa_handler=handler;
  act.sa_sigaction=handler;
  sigaction(signal, NULL, &oldact); // Get old action value
  if(oldact.sa_handler!=SIG_IGN) { sigaction(signal, &act, NULL); } // Set new value
}
void Util::sigs_init() {
#define util this
	//syslog(LOG_INFO,"Initializing signals.");
//   SIGABRT, SIGALRM, SIGFPE, SIGHUP, SIGILL, SIGINT, SIGKILL, SIGPIPE,
//   SIGQUIT, SIGSEGV, SIGTERM, SIGUSR1, SIGUSR2, SIGCHLD, SIGCONT,
//   SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU, SIGVTALRM, SIGPROF
  //sig_init(SIGKILL,sig_kill);
  sig_init(SIGTERM,sig_term);
  sig_init(SIGINT,sig_int);
  sig_init(SIGABRT,sig_abrt);
  sig_init(SIGFPE,sig_fpe);
  sig_init(SIGHUP,sig_hup);
  sig_init(SIGILL,sig_ill);
  sig_init(SIGPIPE,sig_pipe);
  sig_init(SIGQUIT,sig_quit);
  sig_init(SIGSEGV,sig_segv);
  sig_init(SIGUSR1,sig_usr1);
  sig_init(SIGUSR2,sig_usr2);
  //sig_init(SIGCHLD,sig_chld);
	//util->Start(); // Wait for signals
	if(atexit(bye)!=0) { syslog(LOG_ERR,"Cannot set exit function."); }
#undef util
}
/************************************************************************/
bool Util::copyFile(const char *src, const char *dst) {
	FILE *srcfp=fopen(src,"rb");
	if(!srcfp) { syslog(LOG_ERR,"Util::copyFile: src1: %s: %s",strerror(errno),src); return false; }
	char dstfname[1024];
	if(dst[0] && dst[strlen(dst)-1]=='/') {
		const char* base=strrchr(src,'/');
		if(base) { base++; }
		else { base=src; }
		snprintf(dstfname,sizeof(dstfname),"%s%s",dst,base);
		dst=dstfname;
	}
	FILE *dstfp=fopen(dst,"wb");
	if(!dstfp) { syslog(LOG_ERR,"Util::copyFile: dst: %s: %s (copying %s)",strerror(errno),dst,src); fclose(srcfp); return false; }
	char buf[65536];
	bool err=false;
	while(!feof(srcfp) && !ferror(srcfp) && !ferror(dstfp)) {
		int ret1=fread(buf, 1, sizeof(buf), srcfp);
		if(ret1==-1) { err=true; break; }
		int ret2=fwrite(buf, 1, ret1, dstfp);
		if(ret2==-1) { err=true; break; }
		if((unsigned)ret1<sizeof(buf) && !ferror(srcfp)) { break; } // EOF
	}
	fclose(srcfp); fclose(dstfp);
	if(err) { syslog(LOG_ERR,"Util::copyFile: src2: %s: %s",strerror(errno),src); return false; }
	return true;
}

bool Util::moveFile(const char *src, const char *dst) {
	if(copyFile(src,dst)) {
		if(unlink(src)==0) { return true; }
		syslog(LOG_ERR,"Util::moveFile: %s: %s",strerror(errno),src);
	}
	return false; // Move failed along the way
}
/************************************************************************/
bool Util::isMounted(bool show) {
	map<string, bool> wasMounted;
	char buf[1024]={0};
	FILE *fp=fopen("/proc/mounts","rb");
	char *pp=NULL;
	bool anyMounted=false; // Are any RADIO3ABN, Radio drives mounted
	const char *tgtdir0="/tmp/play3abn/cache";
	if(mkdir(tgtdir0,0777)<0 && errno!=EEXIST) { syslog(LOG_ERR,"isMounted: %s: %s",tgtdir0,strerror(errno)); return false; }
	while(fp && !feof(fp) && !ferror(fp) && fgets(buf,sizeof(buf),fp)) {
		// buf e.g.: /dev/sdb1 /media/MULTIBOOT fuseblk rw,nosuid,nodev,relatime,user_id=0,group_id=0,default_permissions,allow_other,blksize=4096 0 0
		pp=strchr(buf,' '); 
		if(pp) {
			pp++; // Skip device
			char *qq=strchr(pp,' ');
			if(qq) { *qq=0; } // Isolate mount point
			/** Skip paths starting with / or /dev/shm, e.g.: //filler, //logs, /dev/shm/logs **/
			if(strcmp(pp,"/")==0 || strcmp(pp,"/dev/shm")==0) { continue; }
			char checkmnt[1024];
			//string dirs[3]={"RADIO3ABN","Radio","filler/songs"};
			string dirs[11]={"Radio","RADIO3ABN/filler","RADIO3ABN/download","RADIO3ABN/schedules","RADIO3ABN/logs","RADIO3ABN/newsweather","filler","download","schedules","logs","newsweather"};
			for(unsigned int xa=0; xa<(sizeof(dirs)/sizeof(dirs[0])); xa++) {
				snprintf(checkmnt,sizeof(checkmnt),"%s/%s",pp,dirs[xa].c_str());
				DIR* dir;
				if((dir=opendir(checkmnt))!=NULL) {
					closedir(dir); anyMounted=true;
					const char *srcdir=(dirs[xa]=="filler/songs"?pp:checkmnt);
					const char *rr=strrchr(srcdir,'/');
					const char *link=(rr?&rr[1]:(const char *)"");
					char tgtdir[256];
					wasMounted[string(srcdir)]=true;
					bool exists=false;
					for(int xb=0;xb<10;xb++) { // Loop until we find a free name to link to (or skip linking due to existing matching link)
						if(xb>0) { snprintf(tgtdir,sizeof(tgtdir),"%s/%s%d",tgtdir0,link,xb); }
						else { snprintf(tgtdir,sizeof(tgtdir),"%s/%s",tgtdir0,link); }
						char linkbuf[256];
						int res=readlink(tgtdir,linkbuf,sizeof(linkbuf)-1);
						if(res==-1) { // If link does not exist, break out of loop and continue with symlink.
							if(errno==ENOENT) { break; }
							syslog(LOG_ERR,"isMounted:readlink(%s): %s",tgtdir,strerror(errno));
						}
						else { // If link exists, if it is to the same target, break and do nothing.  Else, increment counter and try again.
							linkbuf[res]=0;
							if(strcmp(linkbuf,srcdir)==0) { exists=true; break; }
						}
					}
					if(show) { syslog(LOG_ERR,"isMounted: src=%s,tgt=%s",srcdir,tgtdir); }
					if(!exists && symlink(srcdir, tgtdir)==-1) {
						syslog(LOG_ERR,"isMounted:symlink(%s,%s): %s.",srcdir,tgtdir,strerror(errno));
					}
				}
			}
		}
	}
	if(fp) { fclose(fp); }
	/** CLEAN UP stale links **/
  DIR *dir=opendir(tgtdir0);
	if(dir) {
		struct dirent *entry;
		while((entry=readdir(dir))) {
			if(entry->d_name[0]=='.') { continue; }
			string link=string(entry->d_name);
			string tgt=string(tgtdir0)+"/"+link;
			char linkbuf[256];
			int res=readlink(tgt.c_str(),linkbuf,sizeof(linkbuf)-1);
			if(res==-1) { syslog(LOG_ERR,"isMounted:readlink#2(%s): %s",tgt.c_str(),strerror(errno)); }
			else {
				linkbuf[res]=0;
				if(wasMounted[string(linkbuf)]) { continue; } // Only remove links that were not mounted
			}
			remove(tgt.c_str());
		}
		closedir(dir);
	}
	
	/** END **/
	set(".radiodir",anyMounted?"/tmp/play3abn/cache":"");
	return anyMounted;
	//return (isMounted("/sdcard")  || isMounted("/udisk") || isMounted("/cdrom"));
}
/** NOTE: Returns true if mount point is already mounted (to a device differing from "/" */
bool Util::isMounted(const char *mnt, dev_t& dev) {
	struct stat statbuf1;
	struct stat statbuf2;
	if(stat(mnt, &statbuf1)==-1) { syslog(LOG_NOTICE,"Play3ABN::isMounted: %s: %s",strerror(errno),mnt); return false; }
	dev=statbuf1.st_dev;
	if(stat("/", &statbuf2)==-1) { syslog(LOG_NOTICE,"Play3ABN::isMounted: %s: /",strerror(errno)); return false; }
	if(statbuf1.st_dev != statbuf2.st_dev) { return true; }
	return false;
}
/*************************************************************************/
bool strings::cmpStrings(strings a, strings b) {
	return a.one < b.one;
}

/** Delete all files in directory **/
bool Util::removeDirFiles(const char *dirname) {
MARK
	DIR *dir=opendir(dirname);
	if(!dir) { syslog(LOG_ERR,"removeDirFiles: opendir(%s): %s",dirname,strerror(errno)); return false; }
	struct dirent *entry;
MARK
	while((entry=readdir(dir))) {
		if(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) { continue; } // Skip cur and parent dirs (but remove all other hidden)
		string filepath=string(dirname)+"/"+string(entry->d_name);
		remove(filepath.c_str());
	}
	closedir(dir);
MARK
	return true;
}

/** NOTE: Returns vector of string pairs.  Each string pair contains:
 * - info string e.g. : 2011-06-09 12:40:00 1 0066 IT'S A WONDERFUL DAY-Heritage Singers.ogg
 * - path string      : /tmp/play3abn/cache/filler/songs/0066 IT'S A WONDERFUL DAY-Heritage Singers.ogg
 **/
vector<strings> Util::getPlaylist(const char *dirname, int limit, int sortMode/*1=LRU,0=unsorted,-1=random*/, int maxLen, bool txtAlso) {
MARK
	vector<strings> files;
	int dirfd=open(dirname, O_RDONLY);
	if(dirfd==-1) { syslog(LOG_NOTICE,"getPlaylist1: open dir(%s): %s",dirname,strerror(errno)); return files; }
	DIR *dir=fdopendir(dirfd);
	if(!dir) { syslog(LOG_NOTICE,"getPlaylist2: opendir(%s): %s",dirname,strerror(errno)); return files; }
	struct dirent *entry;
	while((entry=readdir(dir))) {
		//sleep(1); // Sleep one second to avoid pegging the CPU
		if(entry->d_name[0]=='.') { continue; } // Skip hidden files
		if(strcmp(entry->d_name,"cached")==0) { continue; } // Skip cached directory
		if(!txtAlso) { // Skip .txt files
			int slen=strlen(entry->d_name);
			if(slen>4 && strcmp(&entry->d_name[slen-4],".txt")==0) { continue; }
		}
		char filepath[512];
		int ret=readlinkat(dirfd, entry->d_name, filepath, sizeof(filepath)-1);
		if(ret==-1) { syslog(LOG_ERR,"getPlaylist: readlinkat(%s,%s): %s",dirname,entry->d_name,strerror(errno)); continue; }
		filepath[ret]=0;
		strings ent(string(entry->d_name),string(filepath));
		/** CHECK LIMITS **/
		time_t dt=0;
		int flag=0;
		int seclen=0;
		string catcode="";
		string dispname="";
		string decodedUrl="";
		int result=itemDecode(ent, dt, flag, seclen, catcode, dispname, decodedUrl);
		if(result<0) {
			string playlink=string(dirname)+"/"+ent.one;
			syslog(LOG_ERR,"getPlaylist: Invalid format (result=%d): %s => %s",result,ent.one.c_str(),ent.two.c_str());
			remove(playlink.c_str()); continue;
		}
MARK
		if(maxLen>0 && seclen>maxLen) { 
MARK
			continue; } // Exclude too-long files
MARK
		/** SAVE IF OK **/
		files.push_back(ent);
MARK
		if(limit>0) {
			limit--;
			if(limit==0) { break; }
		}
MARK
	}
MARK
	closedir(dir);
MARK
	if(sortMode==1) { sort(files.begin(), files.end(), strings::cmpStrings); } /*1=LRU*/
	else if(sortMode==-1) { random_shuffle(files.begin(), files.end()); }
MARK	
	return files;
}


/** NOTE: lensFiles is a vector of strings each of the form "0061 Your Story Hour/Promo/Header01.ogg" **/
/** NOTE: Returns vector of string pairs.  Each string pair contains (unscheduled date):
 * - info string e.g. : 1970-01-01 00:00:00 1 0066 IT'S A WONDERFUL DAY-Heritage Singers.ogg
 * - path string      : /tmp/play3abn/cache/filler/songs/0066 IT'S A WONDERFUL DAY-Heritage Singers.ogg
 * NOTE: Sorting is performed on input vector rather than output, so you can make subsequent calls
 * using input vector but turn off sort mode to get the same sort as previous.
 **/
vector<strings> Util::getPlaylist(vector<string>& lensFiles, int limit, int sortMode/*1=LRU,0=unsorted,-1=random*/, int maxLen) {
fprintf(stderr,"        Util::getPlaylist: vector len=%d,limit=%d,sortMode=%d,maxLen=%d\n",lensFiles.size(),limit,sortMode,maxLen);
	if(sortMode==1) { sort(lensFiles.begin(), lensFiles.end()/*, string::cmpStrings*/); } /*1=LRU*/
	else if(sortMode==-1) { random_shuffle(lensFiles.begin(), lensFiles.end()); }
	vector<strings> files;
	//struct dirent entry;
	vector<string>::iterator iter;
	char filepath[512];
	for(iter=lensFiles.begin(); iter!=lensFiles.end(); ++iter) {
		string item=*iter;
		const char *sItem=item.c_str(); // e.g. 1709 Sermons/BOL209~BREATH_OF_LIFE~Everybody_Cried_Pt_2~Walter_Pearson.ogg
		int seclen=atoi(sItem);   //      012345
		const char *shortpath=&sItem[5];
		findTarget(filepath,sizeof(filepath),shortpath);
		const char *dispname=strrchr(shortpath,'/');
		if(dispname) { dispname=&dispname[1]; } // Skip slash
		else { dispname=shortpath; }
		//strncpy(entry.d_name,dispname,sizeof(entry.d_name)-1);
		//entry.d_name[sizeof(entry.d_name)-1]=0;
		//strings ent(Util::fmtTime(0)+" 365 "+string(dispname),string(filepath)); // Unscheduled, age one year, include length and
		strings ent(Util::fmtTime(0)+" -1","365 "+string(item,0,4)+" "+string(filepath)); // Unscheduled, age one year, include length and
if(!filepath[0]) { fprintf(stderr,"                ***Util::getPlaylist: filepath=%s,shortpath=%s\n",filepath,shortpath); }
		//strings ent(string(test/*dispname*/),string(filepath));
		if(maxLen>0 && seclen>maxLen) { continue; } // Exclude too-long files
		/** SAVE IF OK **/
		files.push_back(ent);
		if(limit>0) {
			limit--;
			if(limit==0) { break; }
		}
	}
// 	if(sortMode==1) { sort(files.begin(), files.end(), strings::cmpStrings); } /*1=LRU*/
// 	else if(sortMode==-1) { random_shuffle(files.begin(), files.end()); }
	return files;
		 
}

string Util::cachedir="/tmp/play3abn/cache";
void Util::findTarget(char *path, int pathLen, const char *name) {
	bool found=false;
	const char sources[3][10]={{"download"},{"filler"},{"Radio"}};
	for(unsigned int sx=0; !found && sx<(sizeof(sources)/sizeof(sources[0])); sx++) {
                char d[3]={0};
                if(sx==0) { d[0]=tolower(name[0]); d[1]='/'; d[2]=0; }
		for(int ix=0; ix<4; ix++) {
			char sIx[2]={0};
			if(ix>0) { sIx[0]=('0'+ix); sIx[1]=0; }
                        snprintf(path,pathLen,"%s/%s%s/%s%s",cachedir.c_str(),sources[sx],sIx,d,name);
			struct stat statbuf;
			if(stat(path, &statbuf)!=-1) { found=true; break; }
		}
	}
	if(!found) { path[0]=0; }
	//syslog(LOG_INFO,"SCHED findTarget(%s): %s",name,path);
}

/** Returns set of programs for given prefix, e.g. ProgramList2-, or Schedule- **/
map<string,vector<string> > Util::ListPrograms(const char *prefix) {	
	// NOTE: Returns a map of program names to program paths from multiple mounted media sources
	// NOTE: Last found version of a program "wins" (replacing previous path reference)
	map<string,vector<string> > programs;
	//syslog(LOG_NOTICE,"SchedFixed::ListPrograms(%s):Listing from %s",prefix,cachedir.c_str());
	const char *srccache=cachedir.c_str();
	DIR *dir=opendir(srccache);
	if(!dir) { syslog(LOG_ERR,"SchedFixed::ListPrograms:opendir1(%s): %s",srccache,strerror(errno)); sleep(1); return programs; }
	struct dirent *entry;
	while((entry=readdir(dir)) && !Util::checkStop()) {
		if(entry->d_name[0]=='.') { continue; } // Skip hidden files
		if(strncmp(entry->d_name,"schedules",9)!=0) { continue; } // Skip sources w/o schedules
		char prgdir[1024];
		snprintf(prgdir,sizeof(prgdir),"%s/%s/programs",srccache,entry->d_name);
		DIR *pdir=opendir(prgdir);
		if(!dir) { syslog(LOG_ERR,"SchedFixed::ListPrograms:opendir2(%s): %s",prgdir,strerror(errno)); sleep(1); return programs; }
		struct dirent *entry2;
		while((entry2=readdir(pdir)) && !Util::checkStop()) {
			if(entry2->d_name[0]=='.') { continue; } // Skip hidden files
			int plen=strlen(prefix);
			if(strncmp(entry2->d_name,prefix,plen)!=0) { continue; } // Skip non-matching prefixes
			int dlen=strlen(entry2->d_name);
			if(dlen<4 || strncmp(&entry2->d_name[dlen-4],".txt",4)!=0) { continue; } // Skip non-matching suffix
			char fullpath[128];
			snprintf(fullpath,sizeof(fullpath),"%s/%s",prgdir,entry2->d_name);
			//syslog(LOG_ERR,"ListPrograms: fullpath=%s",fullpath);
			char* sub=&entry2->d_name[plen];
			char* pp=strstr(sub,".txt");
			if(pp) { *pp=0; }
			/** NOTE: Build program vector **/
			vector<string> programContents;
			FILE *fp=fopen(fullpath,"rb");
			if(!fp) { syslog(LOG_ERR,"SchedFixed::ListPrograms:fopen(%s): %s",fullpath,strerror(errno)); sleep(1); return programs; }
			char line[128];
			while(fp && !feof(fp) && fgets(line,sizeof(line),fp)) {
				while(char *pp=strrchr(line,'\r')) { *pp=0; }
				while(char *pp=strrchr(line,'\n')) { *pp=0; }
				//syslog(LOG_ERR,"ListPrograms:     line=%s",line);
				if(strncmp(line,"PATT: ",6)==0) { continue; }
				programContents.push_back(string(line));
			}
			fclose(fp);
			programs[string(sub)]=programContents;
		}
	}
	//  	syslog(LOG_ERR,"ListPrograms(ProgramList2-): SHOW LIST");
	//  	map<string,vector<string> >::iterator iter;
	//  	for(iter=programs.begin(); iter!=programs.end(); ++iter) {
	//  		syslog(LOG_ERR,"  PROGRAM: %-17s= %s",iter->first.c_str(),iter->second.size()>0?iter->second[0].c_str():"");
	//  	}
	return programs;
}

void Util::cacheFiller(string cachedir, string queuedir, int sortMode, bool clearCache) {
MARK
	vector<strings> playlist=getPlaylist(cachedir.c_str(),0,sortMode,0); // Sort list ascending or randomly: Each entry: [secs] [mtime] [name]
MARK
	const char *qdir=queuedir.c_str();
	if(clearCache && !removeDirFiles(qdir)) { mkdir(qdir,0755); }
MARK
	for(unsigned int xa=0; xa<playlist.size(); xa++) {
		strings ent=playlist[xa];
		/** PARSE OUT: flag, seclen, mtime, dispname **/
		time_t dt=0;
		int flag=0;
		int seclen=0;
		string catcode="";
		string dispname="";
		string url="";
		string cachelink=cachedir+"/"+ent.one;
		int result=itemDecode(ent, dt, flag, seclen, catcode, dispname, url);
		if(result<0) {
			string playlink=queuedir+"/"+ent.one;
			syslog(LOG_ERR,"cacheFiller: Invalid format (result=%d): %s => %s",result,ent.one.c_str(),ent.two.c_str());
			remove(playlink.c_str());
			remove(cachelink.c_str());
			continue;
		}
		struct stat statbuf;
		if(stat(ent.two.c_str(), &statbuf)==-1) { // Remove links to non-existent cached files
			syslog(LOG_NOTICE,"Util::cacheFiller:remove(%s)",cachelink.c_str());
			remove(cachelink.c_str());
			continue;
		}

MARK
		char centry[1024];
		snprintf(centry,sizeof(centry),"%s/%s %04d %s",qdir,Util::fmtTime(xa).c_str(),seclen,dispname.c_str());
		if(symlink(ent.two.c_str(), centry)<0 && errno!=EEXIST) { syslog(LOG_ERR,"cacheFiller: %s: %s",strerror(errno),centry); }
	}
MARKEND
}
