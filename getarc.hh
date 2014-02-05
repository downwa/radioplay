#include <syslog.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <curl/curl.h>
// #include <curl/types.h>
#include <curl/easy.h>

#define BASEDIR "/tmp/play3abn"
#define ORIGVARS BASEDIR "/vars"
#define getS(name) get(#name,name,sizeof(name))				/* get String */
#define getST(name) get("." #name,name,sizeof(name))	/* get String from temporary (leading period) */
#define getI(name) getInt(name)												/* get Integer */
#define getId(name,dft) getInt(name,dft)							/* get Integer */
#define getIT(name) getInt("." name)									/* get Integer from temporary */
#define getITd(name,dft) getInt("." name,dft)					/* get Integer from temporary with default */
#define setS(name,val) set(name,val)									/* set String */
#define setST(name,val) set("." name,val)							/* set String to temporary (leading period) */
#define setI(name,val) setInt(name,val)								/* set Integer */
#define setIT(name,val) setInt("." name,val)					/* set Integer to temporary (leading period) */

#define LOGFILE "/tmp/xarchive.log"

#define EXTRACTOR "gunzip 2>" ORIGVARS "/errgunzip.txt | tar -xv 2>" ORIGVARS "/errtar.txt | tee installed.txt"

#define STOP_DOWNLOAD_AFTER_THIS_MANY_BYTES         100000000 /* 100 Mb max */
#define MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL     3

void init();
const char *T();
int dolog(const char *format, ...);
void set(const char *varName, const char *varVal);
void setInt(const char *varName, int varVal);
char *getline(const char *fname, char *varVal, int valSize);
char *get(const char *varName, char *varVal, int valSize);
int getInt(const char *varName, int dft);
size_t my_fwrite(void *buffer, size_t size, size_t nmemb, void *stream);
