#include "getarc.hh"

FILE *logfp=NULL;
char VARSPATH[256]=ORIGVARS;
char timeBuf[21];
struct CurlFile {
  FILE *stream;
  off_t size;
  char errmsg[256];
};
struct myprogress {
  double lastruntime;
  CURL *curl;
};
/**************************/
const char *T() {
	time_t t=time(NULL);
	struct tm tms;
  struct tm* tptr=localtime_r(&t,&tms);
	snprintf(timeBuf,20,"%04d-%02d-%02d %02d:%02d:%02d",
		1900+tptr->tm_year,tptr->tm_mon+1,tptr->tm_mday,tptr->tm_hour,tptr->tm_min,tptr->tm_sec);
	return timeBuf;
}

int dolog(const char *format, ...) {
	va_list ap;
	va_start(ap, format);

	if(!logfp) {
		logfp=fopen(LOGFILE,"wb");
		if(!logfp) { syslog(LOG_ERR,"%s: fopen(%s): %s",T(),LOGFILE,strerror(errno)); abort(); }
	}
	char timebuf[22];
	snprintf(timebuf,sizeof(timebuf),"%19s: ",T());
	int tbLen=strlen(timebuf);
	int charsWritten=tbLen;
	fputs(timebuf,logfp);
	va_start(ap, format);
	charsWritten+=vfprintf(logfp,format,ap);
	fputs("",logfp);
	fflush(logfp);
	va_end(ap);
	int doLogStderr=getITd("doLogStderr",1);
	if(doLogStderr || true) { fputs(timebuf,stderr); vsyslog(LOG_ERR,format,ap); fputs("\r",stderr); }
	return charsWritten;
}
/**************************/
void init() {
	/** SET VARIABLES PATH **/
	struct stat statbuf;
	uid_t uid=geteuid();
	if(uid!=0) {
		if(stat("vars", &statbuf)!=-1) { strncpy(VARSPATH,"vars",sizeof(VARSPATH)); }
	}
	else {
		if(stat("/play3abn/vars", &statbuf)!=-1) { strncpy(VARSPATH,"/play3abn/vars",sizeof(VARSPATH)); }
		else if(stat("/recorder/vars", &statbuf)!=-1) { strncpy(VARSPATH,"/recorder/vars",sizeof(VARSPATH)); }
		else if(stat("vars", &statbuf)!=-1) { strncpy(VARSPATH,"vars",sizeof(VARSPATH)); }
	}
	mkdir(BASEDIR,0755);
	mkdir(VARSPATH,0755);
}
//bool firstWarn=false;
void set(const char *varName, const char *varVal) {
  char fname[1024];
  char fnameFinal[1024];
	if(varName[0]=='.') {
		snprintf(fname,sizeof(fname)-1,"%s/%s.txt.tmp",ORIGVARS,&varName[1]);
		snprintf(fnameFinal,sizeof(fnameFinal)-1,"%s/%s.txt",ORIGVARS,&varName[1]);
	}
	else {
		snprintf(fname,sizeof(fname)-1,"%s/%s.txt.tmp",VARSPATH,varName);
		snprintf(fnameFinal,sizeof(fnameFinal)-1,"%s/%s.txt",VARSPATH,varName);
	}
  FILE *fp=fopen(fname,"wb");
  if(!fp /*&& !firstWarn*/) { /*firstWarn=true; syslog(LOG_ERR,"set: %s: %s",fname,strerror(errno)); */ return; }
  fprintf(fp,"%s",varVal);
  fclose(fp);
	rename(fname,fnameFinal);
}
void setInt(const char *varName, int varVal) {
  char valstr[64];
  snprintf(valstr,sizeof(valstr)-1,"%d",varVal);
  set(varName,valstr);
}
char *getline(const char *fname, char *varVal, int valSize) {
  FILE *fp=fopen(fname,"rb");
  if(valSize>0) { varVal[0]=0; }
  if(!fp) { return varVal; }
  if(!fgets(varVal,valSize,fp)) { fclose(fp); return varVal; } // Truncate and return
  int len=strlen(varVal); // Strip any trailing newline
	while((len>0 && varVal[len-1]=='\n') || varVal[len-1]=='\r') { varVal[len-1]=0; len--; }
  fclose(fp);
	return varVal;
}
char *get(const char *varName, char *varVal, int valSize) {
  char fname[1024];
	if(varName[0]=='.') { snprintf(fname,sizeof(fname)-1,"%s/%s.txt",ORIGVARS,&varName[1]); }
	else { snprintf(fname,sizeof(fname)-1,"%s/%s.txt",VARSPATH,varName); }
	return getline(fname,varVal,valSize);
}
int getInt(const char *varName, int dft) {
  char valstr[64];
  get(varName,valstr,sizeof(valstr));
	if(!valstr[0]) { return dft; }
  return atoi(valstr);
}
/**************************/
static int progress(void *p, double dltotal, double dlnow, double ultotal, double ulnow) {
	struct myprogress *myp = (struct myprogress *)p;
	CURL *curl = myp->curl;
	double curtime = 0;
	curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curtime);
	// NOTE: LINES WRAP, e.g.:
	/* 2014-09-15 16:29:06: TOTAL TIME:    67: UP:     0 of     0  DOWN: 6562152 of 
	 * 112662543 */
	if((curtime - myp->lastruntime) >= MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL) {
		myp->lastruntime = curtime;
		syslog(LOG_ERR,"TOTAL TIME: %5.0f: UP: %5.0f of %5.0f  DOWN: %5.0f of %5.0f", curtime, ulnow, ultotal, dlnow, dltotal);
	}
	char buf[256];
	snprintf(buf,sizeof(buf), "TOTAL TIME: %5.0f: UP: %5.0f of %5.0f  DOWN: %5.0f of %5.0f\r", curtime, ulnow, ultotal, dlnow, dltotal);
	setST("release_progress",buf);
	if(dlnow > STOP_DOWNLOAD_AFTER_THIS_MANY_BYTES) { return 1; }
	return 0;
}

volatile bool needPost=false;
size_t my_fwrite(void *buffer, size_t size, size_t nmemb, void *stream) {
  struct CurlFile *out=(struct CurlFile *)stream;
	if(out->size==0 && memcmp(buffer,"OK",3)==0) {
		return 0; // Abort download
	}
	if(out->size==0 && strncasecmp((char *)buffer,"<html>",6)==0) {
		needPost=true;
		return 0; // Switch to POST
	}
  size_t retval=fwrite(buffer, size, nmemb, out->stream);
  if(retval > 0) {
		out->size+=(size*nmemb); out->errmsg[0]=0;
		//char val[1024];
		//snprintf(val,sizeof(val),"%d %s",(int)out->size,out->filename);
		//setST("ftpget", val);
	}
	char *b=(char *)buffer;
	b[256]=0;
	char *ofs=b;
	for(const char *pp=b; *pp && isprint(*pp); pp++,ofs++) { }
	*ofs=0;
	if(strlen(b)>10) { syslog(LOG_ERR,"my_fwrite: %s",(char *)buffer); }
	return retval;
}

struct CurlFile curlfile={
	NULL,	// Stream to write to
	//fopen("../release.tgz","wb"),
	0,
	""		// Error message
};

static size_t headers(void *ptr, size_t size, size_t nmemb, void *userdata) {
	char buf[256];
	memcpy(buf,ptr,size*nmemb);
	buf[size*nmemb]=0;
	if(strstr(buf,"Content-Type: text/html")) {
		needPost=true;
		return 0; // Switch to POST
	}
	//syslog(LOG_ERR,"header: %s",buf);
	return size*nmemb;
}
bool doget(CURL *easyhandle, const char *srcurl) {
	struct myprogress prog={
		0, /*lastruntime*/
		easyhandle /* CURL* */
	};
	curlfile.stream=popen(EXTRACTOR,"w");
	if(!curlfile.stream) { syslog(LOG_ERR,"popen: %s",strerror(errno)); abort(); }
	curlfile.size=0;
	curlfile.errmsg[0]=0;
	curl_easy_setopt(easyhandle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(easyhandle, CURLOPT_URL,srcurl);
	curl_easy_setopt(easyhandle, CURLOPT_VERBOSE, 2);
	
	//curl_easy_setopt(easyhandle, CURLOPT_WRITEHEADER, true);
	curl_easy_setopt(easyhandle, CURLOPT_HEADERFUNCTION, headers);
	
	curl_easy_setopt(easyhandle, CURLOPT_PROGRESSFUNCTION, progress);
	curl_easy_setopt(easyhandle, CURLOPT_PROGRESSDATA, &prog);
	curl_easy_setopt(easyhandle, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(easyhandle, CURLOPT_STDERR, logfp);
	curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, my_fwrite); // write callback
	curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &curlfile); // callback data
	curl_easy_setopt(easyhandle, CURLOPT_ERRORBUFFER, curlfile.errmsg);
	curl_easy_setopt(easyhandle, CURLOPT_FOLLOWLOCATION, true);
	curl_easy_setopt(easyhandle, CURLOPT_FAILONERROR, true);
	 /* post away! */
	curl_easy_perform(easyhandle);
	syslog(LOG_ERR,"doget finished.");
	if(curlfile.errmsg[0]) { syslog(LOG_ERR,"curl: %s: %s",curlfile.errmsg,srcurl); }
	return !needPost;
}

void dopost(CURL *easyhandle, const char *filename, const char *srcurl) {
	struct myprogress prog={
		0, /*lastruntime*/
		easyhandle /* CURL* */
	};
	curlfile.stream=popen(EXTRACTOR,"w");
	if(!curlfile.stream) { syslog(LOG_ERR,"popen: %s",strerror(errno)); abort(); }
	curlfile.size=0;
	curlfile.errmsg[0]=0;
	struct curl_httppost *post=NULL;
	struct curl_httppost *last=NULL;
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "file", CURLFORM_FILE, filename, /*CURLFORM_CONTENTHEADER, headers, */CURLFORM_END);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "submit", CURLFORM_COPYCONTENTS, "Submit", CURLFORM_END);
	/* Set options */
	curl_easy_setopt(easyhandle, CURLOPT_HTTPPOST, post);
	curl_easy_setopt(easyhandle, CURLOPT_URL,srcurl);
	curl_easy_setopt(easyhandle, CURLOPT_VERBOSE, 2);
	
	curl_easy_setopt(easyhandle, CURLOPT_PROGRESSFUNCTION, progress);
	curl_easy_setopt(easyhandle, CURLOPT_PROGRESSDATA, &prog);
	curl_easy_setopt(easyhandle, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(easyhandle, CURLOPT_STDERR, logfp);
	curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, my_fwrite); // write callback
	curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &curlfile); // callback data
	curl_easy_setopt(easyhandle, CURLOPT_ERRORBUFFER, curlfile.errmsg);
	curl_easy_setopt(easyhandle, CURLOPT_FOLLOWLOCATION, true);
	curl_easy_setopt(easyhandle, CURLOPT_FAILONERROR, true);
	 /* post away! */
	curl_easy_perform(easyhandle);
	syslog(LOG_ERR,"dopost finished.");
	curl_formfree(post);
	if(curlfile.errmsg[0]) { syslog(LOG_ERR,"curl: %s",curlfile.errmsg); }
}

int main(int argc, char **argv) {
	syslog(LOG_ERR,"getarc - (c) 2011 Warren E. Downs");
	if(argc<3) { syslog(LOG_ERR,"Missing [md5sums.txt] [POST url]"); abort(); }
	init();
	const char *filename=argv[1];
	const char *srcurl=argv[2];
	CURL *easyhandle=curl_easy_init();
  if(!easyhandle) { syslog(LOG_ERR,"curl_easy_init failed"); abort(); }
	
 	syslog(LOG_ERR,"GETTING %s",srcurl);
	doget(easyhandle, srcurl);
	syslog(LOG_ERR,"needPost=%d",needPost);
	if(needPost) {
		syslog(LOG_ERR,"POSTING: %s",srcurl);
		dopost(easyhandle, filename, srcurl);
	}
	
  curl_easy_cleanup(easyhandle);
	if(logfp) { fclose(logfp); logfp=NULL; }
	if(curlfile.stream) { fclose(curlfile.stream); curlfile.stream=NULL; }
}