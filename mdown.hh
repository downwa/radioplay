#include "thread.hh"
#include "util.hh"
#include <sys/types.h>
#include <utime.h>
#include <dirent.h>
#include <vector>

#include <curl/curl.h>
// #include <curl/types.h>
#include <curl/easy.h>

//#include "mutex.hh"

#include "utildefs.hh"

#define STOP_DOWNLOAD_AFTER_THIS_MANY_BYTES         20000000 /* 20 Mb max, enough for ~2.5 hours */
#define MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL     3

class TDirEntry {
public:
	time_t toPlayAt;
	time_t atime;
	time_t ctime;
	off_t  size;
	int    seclen;
	string basedir;
	string name;
	bool exists;

	//off_t fileLen;
	//int playLen;
	int expectLen;
	//time_t cachedAt;

	TDirEntry() { toPlayAt=atime=0; size=0; seclen=expectLen=0; basedir=name=""; exists=false; }
};

class Downloader /*: Thread*/ {
	char errbuf[256];
	static volatile bool curlInitialized;
	pthread_mutex_t mPeerList;
protected:
	map<string,string> peerList;
	void Execute(void *arg);
public:
	static Util* sutil;
	Util* util;

	Downloader();
	static int progress(void *p, double dltotal, double dlnow, double ultotal, double ulnow);
	void DownloadLoop();
	bool Download(CURL *curl, string url, string tgtname, char *dstfile, int dstlen);
	map<string,string> getPeers();
	
	int docurl(CURL *cHandle, int &dly, const char *srcurl, const char *destfile);
	int curl(CURL* curl, const char *srcurl, const char *dstfile, int errlen, char *errmsg);
	static size_t my_fwrite(void *buffer, size_t size, size_t nmemb, void *stream);
	void finishFile(char *tmpfile, const char *dstfile);

	static bool cmpFileAge(TDirEntry a, TDirEntry b );
	off_t checkFreeSD(const char *sdDev);
	off_t removeLeastRecentlyPlayedFiles(off_t neededSpace);
	bool checkSD(off_t& sdfree);
};
