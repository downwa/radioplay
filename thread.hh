#ifndef __THREADHH__
#define __THREADHH__

#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <syslog.h>

#include <map>
#include <string>
#include <typeinfo>


using namespace std;

#define __NR_gettid 224 /* From e.g. /usr/include/asm-i386/unistd.h */
inline pid_t gettid() { return (long int)syscall(__NR_gettid); }

class TMarker {
	public:
	pid_t tid;
	time_t when;
	string filename;
	string function;
	int line;
	string tag;
};

#define MARK { \
		pthread_mutex_lock(&Thread::__mMarkDebug__); \
		TMarker marker; \
		marker.when=time(NULL); \
		marker.filename=string(__FILE__); \
		marker.function=string(__func__); \
		marker.line=__LINE__; \
		Thread::__CALLS__[gettid()]=marker; \
\
			pthread_mutex_unlock(&Thread::__mMarkDebug__); \
	}
#define MARKEND MARK
/*		char buf0x[1024]; \
		snprintf(buf0x,sizeof(buf0x),"/tmp/play3abn/curlogs/mark-%d",gettid()); \
		FILE* mkfp0=fopen(buf0x,"wb"); \
		struct tm tms0; \
		struct tm* tptr0=localtime_r(&marker.when,&tms0); \
		if(mkfp0) { fprintf(mkfp0,"%04d-%02d-%02d %02d:%02d:%02d: %s:%s:%d", \
			1900+tptr0->tm_year,tptr0->tm_mon+1,tptr0->tm_mday,tptr0->tm_hour,tptr0->tm_min,tptr0->tm_sec, \
			__FILE__,__func__,__LINE__); fclose(mkfp0); } \
\
*/

class Thread {
	static bool initialized;
	//Util* _tutil_;
protected:
	char threadname[64];
	pthread_t ThreadId_;
	void* _arg;

	void savetid(const char* threadname);
	void MARKInit();
	int Start(void *arg=NULL, size_t stackSize=256);
	static void *EntryPoint(void *pthis);
	virtual void Setup(void *arg) {}
	virtual void Execute(void* arg) {}
	void Run(void *arg);
	void Stop();
public:
	static map<int,string> modules;
	static map<pid_t, TMarker> __CALLS__;
	static pthread_mutex_t __mMarkDebug__;

	Thread() { threadname[0]=0; }
	~Thread() { }
	void *Arg() { return _arg; }
	void Arg(void *arg) { _arg=arg; }
};
#endif // __THREADHH__
