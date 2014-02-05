#ifndef __NETCATC__
#define __NETCATC__

#include <stdarg.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
//#include <curses.h>
#include <string.h>
#include <errno.h>
#include <time.h>

char VARSPATH[64]="/tmp";
char LOGSPATH[64]="/tmp/play3abn/logs";
volatile bool varsPathSet=false;

int bufferSeconds=0;

volatile int sigat0=0;
volatile int sigat1=0;
volatile int sigat2=0;
volatile int sigat3=0;
volatile int sigat4=0;
volatile int sigat5=0;
volatile int sigat6=0;
volatile int sigat7=0;
volatile int sigat8=0;
volatile int sigat9=0;

#include "util.hh"

Util* util=NULL;

#define dolog if(!util) util=new Util("xarchive"); util->dolog

#ifdef PLAY3ABN
#include "chooser.hh"
#endif

#define myFree free
#define myMalloc malloc
/*void myFree(void *ptr) {
	free(ptr);
}
void *myMalloc(size_t size) {
	void *ptr=malloc(size);
	return ptr;
}*/
void setVarsPath() {
	if(varsPathSet) { return; } // Already set
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
	varsPathSet=true;
}
void set(const char *varName, const char *varVal) {
  char fname[1024];
  char fnameFinal[1024];
	if(!varsPathSet) { setVarsPath(); }
  snprintf(fname,sizeof(fname)-1,"%s/%s.txt.tmp",VARSPATH,varName);
  snprintf(fnameFinal,sizeof(fnameFinal)-1,"%s/%s.txt",VARSPATH,varName);
  FILE *fp=fopen(fname,"wb");
  if(!fp) { dolog("set: %s: %s",fname,strerror(errno)); return; }
  fprintf(fp,"%s",varVal);
  fclose(fp);
	rename(fname,fnameFinal);
}
void setInt(const char *varName, int varVal) {
  char valstr[64];
  snprintf(valstr,sizeof(valstr)-1,"%d\n",varVal);
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
	if(!varsPathSet) { setVarsPath(); }
  snprintf(fname,sizeof(fname)-1,"%s/%s.txt",VARSPATH,varName);
	return getline(fname,varVal,valSize);
}
int getInt(const char *varName) {
  char valstr[64];
  get(varName,valstr,sizeof(valstr));
  return atoi(valstr);
}
int getInt(const char *varName, int dft) {
  char valstr[64];
  get(varName,valstr,sizeof(valstr));
	if(!valstr[0]) { return dft; }
  return atoi(valstr);
}
//#ifdef PLAY3ABN
int netcatSocket(const char *addr, int port) { // Read data from specified addr and port
	//dolog( "play3abn:netcatSocket to %s:%d", addr,port);
	int sock;
	struct sockaddr_in server;
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		dolog("play3abn:netcatSocket: %s",strerror(errno)); sleep(1); return -1;
	}

	struct hostent *host;     /* host information */
	struct in_addr h_addr;    /* internet address */
	if ((host=gethostbyname(addr)) == NULL) {
		dolog("play3abn:netcatSocket:gethostbyname '%s' failed", addr); sleep(1); return -1;
	}
	h_addr.s_addr = *((unsigned long *) host->h_addr_list[0]);
	//dolog("%s", inet_ntoa(h_addr));
	/* Construct the server sockaddr_in structure */
	memset(&server, 0, sizeof(server));       /* Clear struct */
	server.sin_family = AF_INET;                  /* Internet/IP */
	server.sin_addr.s_addr = *((unsigned long *) host->h_addr_list[0]); /* First IP address returned */
	server.sin_port = htons(port);       /* server port */
	//dolog("play3abn:netcatSocket to %s:%d", inet_ntoa(server.sin_addr),port);
	/* Establish connection */
	if (connect(sock,(struct sockaddr *) &server,sizeof(server)) < 0) {
		dolog("play3abn:netcatSocket:connect failed: %s",strerror(errno)); sleep(1); return -1;
	}
	return sock;
}
FILE *netcat(const char *addr, int port) { // Read data from specified addr and port
	return fdopen(netcatSocket(addr,port),"r+");
}
//#endif

/** NOTE:  Run command, scanning output lines for 'grepfor' string.
           Return first matching data from 'column' into 'buflen' bytes of 'outbuf'
           Alternately, if output list is supplied, matching data from 'column' is returned for all lines.
**/
void rungrep(const char *cmd, const char *grepfor, int column, int buflen, char *outbuf, list<string>* output=NULL) {
	FILE *fp=popen(cmd,"r"); // e=close on exec, useful for multithreaded programs
	if(!fp) { dolog("rungrep: %s: %s",cmd,strerror(errno)); }
	char buf[1024];
	bool matchStart=false;
	if(grepfor[0]=='^') { matchStart=true; grepfor++; }
	while(fp && !feof(fp) && fgets(buf,sizeof(buf),fp)) {
		while(char *pp=strrchr(buf,'\r')) { *pp=0; }
		while(char *pp=strrchr(buf,'\n')) { *pp=0; }
//dolog("rungrep: buf=%s",buf);
		char *at=NULL;
		if((at=strstr(buf,grepfor)) != NULL) {
			if(matchStart && (at-buf)>0) { continue; } // Not matching at start
			//dolog("rungrep:buf=%s",buf);
			char *str=buf,*token=buf,*saveptr;
			for(int xa=0; xa<column; xa++,str=NULL) {
				token=strtok_r(str, "\t\r\n ", &saveptr);
				if(!token) { break; }
//dolog("rungrep: xa=%d,column-1=%d,token=%s",xa,column-1,token);
				if(output && xa==column-1) {
					output->push_back(string(token));
				}
				else if(xa==column-1) { strncpy(outbuf,token,buflen); pclose(fp); return; }
			}
		}
	}
	if(outbuf && buflen>0) { outbuf[0]=0; } // not found
	if(fp) { pclose(fp); }
}

#endif