//#include "taudioq.hh"
#include <errno.h>
#include "mutex.hh"
#include "maindec.hh"

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
			if(aq.empty()) {
MARK
 MutexUnlock(&qmut);
MARK
 syslog(LOG_NOTICE,"Taudioq: underflow.\r");
MARK
 sleep(1);
MARK
 continue; }
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

int main(int argc, char **argv) {
	Taudioq* audioq=new Taudioq(new Util("mplay#2"));
	Decoder* decoder=new Decoder(audioq);
	printf("#1\n");
	printf("#2\n");
		char enqbuf[1024];
		char buf[1024];
		int fileindex=0;
	printf("#3\n");
MARK
		while(true) {
MARK
			audioq->enq(enqbuf,sizeof(buf),fileindex++,decoder->blockindex++);
MARK
			printf("#4:fileindex=%d\n",fileindex);
MARK
			int left=1024;
MARK
			TQNode node=audioq->deq(left);
MARK
			printf("#5\n");
MARK
			sleep(3);
MARK
			printf("#6\n");
MARK
		}
MARK
		return 0;
}

