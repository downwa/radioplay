#ifndef __UTILCC__
#define __UTILCC__

#include <syslog.h>

#define LOGDIR "/tmp/play3abn"
#define LOGFIFO LOGDIR "/play3abnlog.fifo"
#define LOGFILE LOGDIR "/play3abn.log"
#define CURLOGS LOGDIR "/curlogs"

#define getS(name) util->get(#name,name,sizeof(name)-1)				/* get String */
#define getST(name) util->get("." #name,name,sizeof(name)-1)	/* get String from temporary (leading period) */
#define getI(name) util->getInt(name)												/* get Integer */
#define getId(name,dft) util->getInt(name,dft)							/* get Integer */
#define getIT(name) util->getInt("." name)									/* get Integer from temporary */
#define getITd(name,dft) util->getInt("." name,dft)					/* get Integer from temporary with default */
#define setS(name,val) util->set(name,val)									/* set String */
#define setST(name,val) util->set("." name,val)							/* set String to temporary (leading period) */
#define setI(name,val) util->setInt(name,val)								/* set Integer */
#define setIT(name,val) util->setInt("." name,val)					/* set Integer to temporary (leading period) */
#define togId(name,val) setI(name,1-getId(name,val))				/* toggle Integer value */
#define cycleId(name,max,val) setI(name,(getId(name,val)+1)%max)	/* cycle multiple Integer values */
#define togITd(name,val) setIT(name,1-getITd(name,val))			/* toggle temporary Integer value */
#define cycleITd(name,max,val) setIT(name,(getITd(name,val)+1)%max)	/* cycle multiple temporary Integer values */
#define revCycleITd(name,max,val) setIT(name,(getITd(name,val)-1)%max)	/* reverse cycle multiple temporary Integer values */
//#define doLog util->dolog
//#define doLog syslog
#define match(a,b) (strncmp(a,b,min(strlen(a),strlen(b)))==0)

#define ORIGVARS "/tmp/play3abn/vars"

#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>

#include <string>
#include <list>
#include <map>
#include <vector>
#include <algorithm> // sort, random_shuffle

#include "stacktrace.h"
#include "thread.hh"

#define min(a,b) (((a)<(b)?(a):(b)))
// e.g. __TIME__ "23:59:01"
// Debugging, we want to always be just before the end of an hour.  So subtract the current minute+4 from the time
// #define CLOCKSKEW ((atoi(&__TIME__[3])+4)*60)
#define CLOCKSKEW 0
#define Time() (time(NULL)-CLOCKSKEW)
// #define remove(file) { if(strcmp(file,"/tmp/play3abn/tmp/rec2011-05-30.txt")==0) { dolog("%s:%s:%d remove(%s)",__FILE__,__func__,__LINE__,file); }; unlink(file); }


using namespace std;

class strings {
public:
	string one;
	string two;
	strings(string one, string two) { this->one=one; this->two=two; }
	static bool cmpStrings(strings a, strings b);
};

/** NOTE: Utility class to handle common functions such as logging, network, stored variables, etc. **/
/** NOTE: Separate instances should be created for each thread that will be using the functions **/
class Util: Thread {
	char playpass[256];
	static bool firstWarningPlaypass;
	static bool initialized;
	char threadname[64];
	static pthread_mutex_t dologMutex;

	static void __MARKINIT__();
	string getlogprefix();
	string getlogpath(const char *plogname);
	static char *getline(const char *fname, char *varVal, int valSize);
public:
	static const char* stopflag;
	static bool noNet;
	static bool hadNoNet;
	static bool resetDate;
	static bool doExit;
	static const char *zone;
	static char TEMPPATH[256];
	static char VARSPATH[256];
	static char LOGSPATH[256];
	static volatile int  bufferSeconds;
	static time_t tzofs;
	static char myip[16];
	static char publicip[16];
	static char igloowareip[16];
	static char macaddr[18];
	static time_t lastIPcheck;
	static string cachedir;
	
	Util(const char *threadname, bool initlogs=false) {
		strncpy(this->threadname, threadname, sizeof(this->threadname)-1); this->threadname[sizeof(this->threadname)-1]=0;
		init(initlogs);
	}
	void Execute(void* arg);
	void init(bool initlogs);
	
	/** LOGGING **/
	FILE *getlog(const char *plogname);
	int dologX(string msg);
	int dologX(const char *format, ...);
	static void logRotate(bool rotate=false);

	/** VARIABLES **/
	void set(const char *varName, const char *varVal);
	void inc(const char *varName, int incVal=1);
	void setInt(const char *varName, int varVal);
	static char *get(const char *varName, char *varVal, int valSize);
	static int getInt(const char *varName, int dft=0);

	/** NETWORK **/
	void getPublicIP(); // Sets vars/publicip.txt and vars/igloowareip.txt to the appropriate IP addresses
	void setConn();
	void setIP(); // Sets myip and calls getPublicIP
	int netcatSocket(const char *addr, int port);
	FILE *netcat(const char *addr, int port);
	string mergePassword(string url);
	void enqueue(time_t playAt, string url, string catcode, int flag, int expectSecs, bool replaceExisting=true);
	strings itemEncode(time_t playAt, int flag, int expectSecs, string catcode, string url);
	int itemDecode(strings infs, time_t& otime, int& flag, int& expectSecs, string& catcode, string& dispname, string& decodedUrl);

	/** THREADS **/
	void savetid();
	void longsleep(int seconds);

	/** SIGNALS **/
	static void listThreadMarks(Util* util);
	static void signalexit(string signame, bool ex);
	static void bye();
	static void sig_segv(int signo, siginfo_t *si, void *p);
	static void sig_fpe(int signo, siginfo_t *si, void *p);
	static void sig_ill(int signo, siginfo_t *si, void *p);
	static void sig_pipe(int signo, siginfo_t *si, void *p);
	static void sig_term(int signo, siginfo_t *si, void *p);
	static void sig_int(int signo, siginfo_t *si, void *p);
	static void sig_abrt(int signo, siginfo_t *si, void *p);
	static void sig_hup(int signo, siginfo_t *si, void *p);
	static void sig_quit(int signo, siginfo_t *si, void *p);
	static void sig_usr1(int signo, siginfo_t *si, void *p);
	static void sig_usr2(int signo, siginfo_t *si, void *p);
	static void sig_init(int signal, void (*handler)(int, siginfo_t *, void *));
	void sigs_init();

	/** FILES **/
	bool copyFile(const char *src, const char *dst);
	bool moveFile(const char *src, const char *dst);
	bool isMounted(bool show=false);
	bool isMounted(const char *mnt, dev_t& dev);

	/** TIME **/
	static void tzsetup();
	static const char *fmtTime(time_t t, char* timeBuf);
	static string fmtTime(time_t t);
	static int getBufferSeconds();

	/** PROCESSES **/
	void rungrep(const char *cmd, const char *grepfor, int column, int buflen, char *outbuf, list<string>* output=NULL);
	static bool checkStop();
	static void Stop();

	/** PLAYLISTS **/
	bool removeDirFiles(const char *dirname);
	vector<strings> getPlaylist(const char *dirname, int limit=0, int sortMode=1/*1=LRU,0=unsorted,-1=random*/, int maxLen=0, bool txtAlso=false);
	vector<strings> getPlaylist(vector<string>& lensFiles, int limit, int sortMode/*1=LRU,0=unsorted,-1=random*/, int maxLen);
	void findTarget(char *path, int pathLen, const char *name);
	map<string,vector<string> > ListPrograms(const char *prefix);
	/*bool parseInfo(strings& infs, time_t& otime, int& flag, int& seclen, string& dispname, bool leaveUrlEncoded=false);*/
	string urlDecode(string src);
	string urlEncode(string src);
	void cacheFiller(string cachedir, string queuedir, int sortMode, bool clearCache=true);

	/** MISC **/
	static string fmtInt(int value, int width);
	static int strcmpMin(const char *s1, const char *s2);
};

#endif
