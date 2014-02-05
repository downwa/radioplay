#include "taudioq.hh"
#include "mutex.hh"

#include <queue>

Taudioq::Taudioq(Util* util) {
	this->util=util; overflowed=false;
	if(pthread_mutex_init(&qmut, NULL)<0) { syslog(LOG_ERR,"Taudioq: pthread_mutex_init: %s",strerror(errno)); abort(); }
}

void Taudioq::enq(char *data, size_t datalen, long fileindex, long blockindex, int samplerate, int nchannels, string fname, 
									long trackSampleCount, bool doBoost, SAMPLE maxsamp, SAMPLE amaxsamp) {
	while(true) {
		if(Util::checkStop()) { exit(0); }
		MutexLock(&qmut);
		if(aq.size()>10) { MutexUnlock(&qmut); sleep(1); continue; }
		MutexUnlock(&qmut);
		break;
	}
	MutexLock(&qmut);
	aq.push(TQNode(data,datalen,fileindex,blockindex++,samplerate,nchannels,fname,trackSampleCount,doBoost,maxsamp,amaxsamp));
	MutexUnlock(&qmut);
}

TQNode Taudioq::deq(int expectSize) {
	TQNode node;
	bool wasOver=false;
MARK
	if(overflowed) {
MARK
		MutexLock(&qmut);
		overflowed=false;
		node=overflow;
		wasOver=true;
		//MutexUnlock(&qmut);
		//return node;
MARK
	}
	else {
MARK
		while(true) {
MARK
			if(Util::checkStop()) { exit(0); }
MARK
			MutexLock(&qmut);
MARK
			if(aq.empty()) { MutexUnlock(&qmut); syslog(LOG_NOTICE,"Taudioq: underflow.\r"); sleep(1); continue; }
MARK
			MutexUnlock(&qmut);
MARK
			break;
		}
MARK
		MutexLock(&qmut);
		node=aq.front();
MARK
	}
MARK
	int moredata=node.datalen-expectSize;
	if(moredata > 0) { // If there is more data than we were expecting, split the block and don't pop
MARK
		//TQNode node=noderef; // Copy of node
		node.datalen=expectSize;
		char *data=new char[moredata];
MARK
		if(!data) { syslog(LOG_ERR,"TAudioq:deq:memory error: %s",strerror(errno)); node.datalen=-1; return node; }
MARK
		memcpy(data,&node.data[expectSize],moredata);
		overflowed=true;
		overflow=TQNode(data,moredata,node.fileindex,node.blockindex,node.samplerate,node.nchannels,node.fname,
										node.trackSampleCount,node.doBoost,node.maxsamp,node.amaxsamp);
MARK
	}
MARK
	if(!wasOver) { aq.pop(); }
MARK
	MutexUnlock(&qmut);
MARK
	return node;
}