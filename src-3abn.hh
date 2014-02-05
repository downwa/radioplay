#include <fstream>
#include <iostream>

#include "maindec.hh"

#include "thread.hh"
#include "util.hh"

#include "tschedline.hh"

#include <algorithm>
#include <vector>

#include "utildefs.hh"

#define NOPEGCPU 250000

using namespace std;

class Src3ABN: Thread {
	time_t lastRefill;
	Util* util;
	Decoder* decoder;
	char code[32];
	char tmpcode[64];
	char tmpcats[100][40];
	char baseurl[256];
	bool initialized;
	map<string,string> codeCategories; // Contain a map (set) of Codes and matching Categories for each code.
	const char *filldir;
protected:
	void InitCodeCategories();
	char *getBaseUrl();
	void cacheLRU_Name(const char *srcdir, const char *dstdir, const char *filldir, const char *dstflagsdir);
	const char *genCode(const char *codeSrc);
	const char *genCode(const char *dirName, const char *fileName);
	void Execute(void* arg);
	void Scheduler();
	static string dateString(time_t today);
	static string urlencode(const string &c);
	static string char2hex( char dec );
	bool getLeastRecentlyPlayedFile(time_t toPlayAt, string& lrpFilename, int minLen, int maxLen, int& retlen, string match, string midmatch, string dontmatch="");
	//string getFile(time_t playAt, string name, int expectSecs, bool down);
	string getCode(const char *fileBase, int& segnum);
public:
	static const char* cachewaitflag;
	Src3ABN(bool testing=false);
	string getSchedule(time_t day, bool download);
	void loadFromFile(string path, vector<TSchedLine>& sched, time_t startDay, time_t curDay);
};