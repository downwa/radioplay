#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#ifndef PLAY3ABN
#include "netcat.c"
// #define min(a,b) ((a<b)?(a):(b))
#endif

class CurlPart {
	public:
		bool inited;
		const char *name; // Overrides filename if set
		const char *filename;
		FILE *stream;
		off_t fileSize;
		off_t maxlen;
		char errmsg[256];
		char endbuf[65536]; // "\nEND\n\0x00"
		long endbufLen;

		FILE *errlog;
		FILE *err;
		bool endWatch;
		int state;
		long endFoundAt;
		long totalBytesWritten;
		double totalArchiveSize;
		time_t lastProgress;
		char lastFilename[1024];
		
		CurlPart(long bytesWrittenSoFar) {
			inited=false;
			endbuf[0]=0;
			endbufLen=0;
			errlog=err=NULL;
			endWatch=false;
			state=0;
			endFoundAt=0;
			totalBytesWritten=bytesWrittenSoFar;
			totalArchiveSize=0;
			name=filename=NULL;
			stream=NULL;
			fileSize=0;
			maxlen=-1;
			errmsg[0]=0;
			lastFilename[0]=0;
			lastProgress=0;
		}
	static size_t my_fwrite(void *buffer, size_t size, size_t nmemb, void *pthis);
	size_t my_fwrite2(void *buffer, size_t memberSize, size_t nmemb, void *xferstream);
	void finishFile(const char *tmpfile, const char *dstfile);
	static int progress_callback(void *pthis, double dltotal, double dlnow, double ultotal, double ulnow);
	int progress_callback2(double dltotal, double dlnow, double ultotal, double ulnow);
	int curlpart(CURL *curl, const char *srcurl, const char *dstfile, int seekofs, int maxlen, const char *name);
};

size_t CurlPart::my_fwrite(void *buffer, size_t size, size_t nmemb, void *pthis) {
	CurlPart* cp=(CurlPart*)pthis;
	cp->my_fwrite2(buffer,size,nmemb,cp);
}
size_t CurlPart::my_fwrite2(void *buffer, size_t memberSize, size_t nmemb, void *xferstream) {
  if(!stream) {
		errno=0; stream=fopen(filename,"ab"); }
  if(!stream) { snprintf(errmsg,sizeof(errmsg),"curl: %s",strerror(errno)); 
		syslog(LOG_ERR,"curlpart: %s: %s",errmsg,name?name:filename);
		return 0; }
	bool fini=false;
	//syslog(LOG_ERR,"maxlen=%d,fileSize=%d,nmemb=%d",maxlen,fileSize,nmemb);
	if(this->maxlen>0 && fileSize+nmemb > this->maxlen) {
		nmemb=this->maxlen-fileSize; fini=true;
	}
	if(endWatch) {
		if(endbufLen+(memberSize*nmemb)<65536) {
			memcpy(&endbuf[endbufLen],buffer,nmemb*memberSize);
		}
		else {
			syslog(LOG_ERR,"BUFFER OVERFLOW: %ld > 65536",endbufLen+(memberSize*nmemb));
		}
		char *end=strstr(endbuf,"\nEND");
		if(end) {
			long endofs=(end-((char *)endbuf))+5;
			endFoundAt=fileSize+endofs-endbufLen;
#ifndef XARCHIVE
			syslog(LOG_INFO,"%ld",endFoundAt);
#endif
			nmemb=endofs-endbufLen; fini=true;
		}
	}
	size_t written=0;
	size_t bytesToWrite=memberSize*nmemb;
	size_t retval=0;
	char *buf=(char *)buffer;
	size_t membersLeft=nmemb;
	do {
		//syslog(LOG_ERR,"fwrite: written=%d,memberSize=%d,membersLeft=%d",written,memberSize,membersLeft);
		retval=fwrite(&buf[written], memberSize, membersLeft, stream);
		//syslog(LOG_ERR,"fwrite: retval=%d",retval);
		if(retval<=0) { break; }
		size_t bytesWritten=(retval*memberSize);
		membersLeft-=retval; written+=bytesWritten;
	} while(membersLeft>0);
	if(written>0) {
		this->fileSize+=written;
		errmsg[0]=0; 
		if(endbufLen>0) { memmove(endbuf,&endbuf[endbufLen],written); }
		endbufLen=written;
		if(name) { totalBytesWritten+=written; } // Only record total against files in archive (not header information)
	}
	return fini?retval:written;
}

void CurlPart::finishFile(const char *tmpfile, const char *dstfile) {
  remove(dstfile);
	if(rename(tmpfile,dstfile)<0) {
		char cmd[1024];
		snprintf(cmd,sizeof(cmd),"rename(%s,%s)",tmpfile,dstfile);
		syslog(LOG_ERR,"finishFile: %s: %s",cmd,strerror(errno));
		snprintf(cmd,sizeof(cmd),"mv %s %s >>%s/curl.log.txt",tmpfile,dstfile,LOGSPATH); // Handles cross-device move vs. rename(tmpfile,dstfile)
		if(system(cmd)<0) { syslog(LOG_ERR,"finishFile: %s: %s",cmd,strerror(errno)); }
	}
}

int CurlPart::progress_callback(void *pthis, double dltotal, double dlnow, double ultotal, double ulnow) {
	CurlPart* cp=(CurlPart*)pthis;
	cp->progress_callback2(dltotal,dlnow,ultotal,ulnow);
}
int CurlPart::progress_callback2(double dltotal, double dlnow, double ultotal, double ulnow) {
	bool abort=false;
	if(dltotal>totalArchiveSize) { totalArchiveSize=dltotal; }
	if(name && totalArchiveSize>0) { // Record progress
		char val[1024];
		snprintf(val,sizeof(val),"%5.2f%%%% %s",totalBytesWritten*100/totalArchiveSize,name?name:filename);
		set("update", val);
		//syslog(LOG_ERR,"%ld of %1.0f: %s",totalBytesWritten,dltotal,name);
		time_t now=time(NULL);
		if(strcmp(lastFilename,name)==0 && (now-lastProgress)>60) { abort=true; }
		strncpy(lastFilename,name,sizeof(lastFilename));
		lastProgress=now;
	}
	if(abort) { return -1; }
	return 0; // NOTE: Non-zero return will abort
}

// NOTE: Returns -1 if error, otherwise returns offset
int CurlPart::curlpart(CURL *curl, const char *srcurl, const char *dstfile, int seekofs, int maxlen, const char *name) {
	endFoundAt=0;
	endWatch=(maxlen==-2);
	int errlen=256;
	char errmsg[errlen+1];
	//if(!errlog) { errlog=stderr; }
	err=errlog;
  const char *basename=strrchr(dstfile,'/');
  if(basename) { basename++; }
  else { basename=dstfile; }
	fileSize=0;
	this->maxlen=maxlen;
	char tmpfile[1024]={0};
  if(this->filename==NULL) { // Running stand-alone, need to set this
		snprintf(tmpfile,sizeof(tmpfile),"%s.tmp",dstfile);
		this->filename=tmpfile;
		struct stat file_info;
		if(stat(tmpfile, &file_info)==0) { // Get local file size
			fileSize=file_info.st_size;
		}
	}
	if(this->stream==NULL && strcmp(dstfile,"-")==0) { this->stream=stdout; }
	if(name && name[0]) { this->name=name; }
	else { this->name=NULL; }
	bool done=false;
	int delay=0;
	FILE *log=errlog;
	if(log && strcmp(dstfile,"-")!=0) { fprintf(log,"%ld: %s",fileSize,basename); }
	curl_easy_setopt(curl, CURLOPT_URL,srcurl);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlPart::my_fwrite); // write callback
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, this); // callback data
	curl_easy_setopt(curl, CURLOPT_RESUME_FROM, seekofs+fileSize);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, this->errmsg);
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, &CurlPart::progress_callback);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 3600);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);	
	
	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Debug mode
	//if(stream==NULL) { syslog(LOG_ERR,"ERROR: NULL stream"); exit(-1); }
	CURLcode res=curl_easy_perform(curl);
	if(this->stream) { fclose(this->stream); this->stream=NULL; }
	time_t acttime=time(NULL);
	struct tm tms;
	struct tm* aptr=localtime_r(&acttime,&tms);
	char timestr[32];
	snprintf(timestr,sizeof(timestr),"%02d:%02d:%02d",aptr->tm_hour,aptr->tm_min,aptr->tm_sec);
	// /**/fprintf(errlog,"curl message: %s: downloading %s",ftpfile.errmsg,tmpfile); fflush(errlog);
	if(res != CURLE_OK) { // e.g. RETR response: 550: in
		if(strcmp(this->errmsg,"RETR response: 550")==0 || // FILE NOT FOUND: GO on to next file (or we'll get stuck)
			 strcmp(this->errmsg,"The requested URL returned error: 404")==0) { 
			if(tmpfile[0]) {
				syslog(LOG_ERR,"%s: curl failed: %s: Marking bad %s",timestr,this->errmsg,tmpfile);
				remove(tmpfile);
				FILE *bad=fopen(dstfile,"wb");
				if(bad) { fclose(bad); }
			}
			delay=60; done=false; // Go on to next file in one minute
		}
		else { // e.g. transfer closed with 2883584 bytes remaining to read
			if(strncmp(this->errmsg,"transfer closed with ",21)==0) { delay=900; }
			else {
				if(strstr(this->errmsg,"Failed writing body")==NULL) {
					syslog(LOG_ERR,"%s: curl failed: %s: in %s",timestr,this->errmsg,tmpfile);
				}
				else { done=true; }
			}
		}
		if(delay==0) { delay=10; } //sleep(10); // Retry in 10 seconds
	}
	else { done=true; }
	//}
	if(done) {
		if(errlog) { fprintf(errlog,"%s: curl fetched: %s",timestr,basename); fflush(errlog); }
		if(strcmp(dstfile,"-")!=0) { finishFile(tmpfile,dstfile); }
		delay=0; // successful download
	}
	if(res != CURLE_OK && strstr(this->errmsg,"Failed writing body")==NULL) { syslog(LOG_ERR,"CURL:%s",this->errmsg); }
	
	return (int)endFoundAt;
}

#ifndef XARCHIVE
int main(int argc, char **argv) {
	if(argc<3) {
		syslog(LOG_ERR,"Usage: curlpart [srcurl] [dstfile|-] {seekofs} {maxlen | -1=no limit | -2=endwatch}");
		exit(1);
	}
	const char *srcurl=argv[1];
	const char *dstfile=argv[2];
	int seekofs=0;
	if(argc>3) { seekofs=atoi(argv[3]); }
	int maxlen=-1;
	if(argc>4) { maxlen=atoi(argv[4]); }
	CurlPart* cp=new CurlPart(0);
	cp->errlog=stderr;
	if(!cp->inited) { curl_global_init(CURL_GLOBAL_DEFAULT); cp->inited=true; }
  CURL *curl = curl_easy_init();
  if(!curl) {
		if(cp->errlog) { fprintf(cp->errlog,"curl_easy_init failed"); }
		return -1;
	}
	int ret=cp->curlpart(curl,srcurl,dstfile,seekofs,maxlen,"");
	if(cp->inited) { curl_easy_cleanup(curl); }
	curl_global_cleanup(); cp->inited=false;
	return ret;
}
#endif
