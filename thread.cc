#include "thread.hh"

map<int,string> Thread::modules;
map<pid_t, TMarker> Thread::__CALLS__;
pthread_mutex_t Thread::__mMarkDebug__;
bool Thread::initialized=false;

void Thread::MARKInit() {
	__CALLS__.clear();
	if(pthread_mutex_init (&__mMarkDebug__, NULL)<0) { syslog(LOG_ERR,"MARKInit:pthread_mutex_init: %s",strerror(errno)); exit(1); }
}

/** Save thread id with corresponding module name (useful to track down which thread is hogging CPU) **/
void Thread::savetid(const char* threadname) {
	const char *mode="ab";
	if(strcmp(threadname,"play3abn")==0) { mode="wb"; } // Initially create file
	char logpath[1024];
	snprintf(logpath,sizeof(logpath),"/tmp/play3abn/curlogs/pids.txt");
	FILE *fp=fopen(logpath,mode);
	if(!fp) { syslog(LOG_ERR,"ERROR opening %s: %s",logpath,strerror(errno)); abort(); }
	pid_t tid=gettid();
	modules[tid]=string(threadname);
	fprintf(fp,"%6d %s\r",tid,threadname);
	fclose(fp);
}

int Thread::Start(void* arg, size_t stackSize) {
	if(!initialized) { MARKInit(); initialized=true; }
	_arg=arg;
	if(!threadname[0]) {
		const type_info& tt=typeid(*this);
		const char *pp=tt.name();
		while(isdigit(*pp)) { pp++; }
		strncpy(threadname,pp,sizeof(threadname));
	}
	else {
		for(char *pp=threadname; *pp; pp++) {
			if(isspace(*pp)) { *pp='_'; }
		}
	}

/*	int debug=_tutil_->getInt("debug",0);
	if(debug) { _tutil_->syslog(LOG_ERR,"START THREAD: %s",threadname); }*/
	
/** VmSize=3908 **/	
	pthread_attr_t attr;
	int rc=pthread_attr_init(&attr);
	if(rc!=0) { syslog(LOG_ERR,"ERROR:pthread_attr_init:%s",strerror(rc)); return -1; }
	rc=pthread_attr_setstacksize(&attr,1024*stackSize); // 100kb, also can use ulimit -s 100
	if(rc!=0) { syslog(LOG_ERR,"ERROR:pthread_attr_setstacksize(%d kb): %s",stackSize,strerror(rc)); return -1; }
	int ret=pthread_create(&ThreadId_, &attr, &EntryPoint, (void*)this);
	rc=pthread_attr_destroy(&attr);
	if(rc!=0) { syslog(LOG_ERR,"ERROR:pthread_attr_destroy:%s",strerror(rc)); return -1; }
	return ret;
}
void *Thread::EntryPoint(void *pthis) {
/** VmSize=3912, then later, 12100 **/
	//sleep(3600);	
	/** INIT SIGNALS **/
//  sigset_t mask;
//  sigfillset(&mask); /* Mask all allowed signals */
//	int rc=sigprocmask(SIG_SETMASK, &mask, NULL);
	//int rc=pthread_sigmask(SIG_BLOCK, &mask, NULL);
//  if(rc!=0) { /*__tutil_->syslog(LOG_ERR,"Thread: %s: pthread_sigmask: %d",threadname,rc);*/ abort(); }*/
	
	/** BEGIN MAIN **/
	Thread *pt=(Thread*)pthis;
	pt->Run(pt->Arg());
	return NULL;
}
void Thread::Run(void *arg) {
  savetid(threadname);
	Setup(arg);
	Execute(arg);
	Stop();
}
void Thread::Stop() {
	//doStop=true;
	pthread_join(ThreadId_, NULL);
}
