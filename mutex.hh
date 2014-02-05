#ifndef __MUTEXHH__
#define __MUTEXHH__

static const char *MutexError(int code) {
	switch(code) {
		case EINVAL : return "EINVAL";
		case EBUSY  : return "EBUSY";
		case EAGAIN : return "EAGAIN";
		case EDEADLK: return "EDEADLK";
		case EPERM  : return "EPERM";
	}
	return "NONE";
}

#define MutexLock(pMutex) { \
	int ret=pthread_mutex_lock(pMutex); \
	if(ret) { syslog(LOG_ERR,"pthread_mutex_lock: %d: %s",ret,MutexError(ret)); abort(); } \
}
#define MutexUnlock(pMutex) { \
	int ret=pthread_mutex_unlock(pMutex); \
	if(ret) { syslog(LOG_ERR,"pthread_mutex_unlock: %d: %s",ret,MutexError(ret)); abort(); } \
}

#endif