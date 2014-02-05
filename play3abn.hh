#ifndef __PLAY3ABNHH__
#define __PLAY3ABNHH__

#include <sys/time.h>
#include <vector>
#include <unistd.h>
#include "thread.hh"
#include "util.hh"
#include "utildefs.hh"

#define HOGPCT 90.0

class Play3ABN: Thread {
	pid_t mypid;
	//static char rxadiodir[256];
	static char tty[32];
	//static char runprocs[1024]; //6][128];
	int rpStart;
	char runprocs[1024]; //6][128];
	static time_t procDates[6];
	char basedir[256];
	char mntPoint[256];
	Util* util;
	int mountCount;
	map<pid_t,int> total_jiffies_1,time1;
	
	void unmountTarget(const char *mnt);
	int getMatchingLine(const char *filename, int bufsiz, char *buffer, const char *match);
	int sumLine(char *linebuf, int startPos, int endPos);
	int sumJiffies(pid_t pid);
	float percentCpu(pid_t pid);
	time_t tzofs;
	time_t checkedTime;
protected:
	void Execute(void *arg);
public:
	Play3ABN(int argc, char **argv);
	void logline(const char* msg);
	void tzsetup();
	void updateDate(bool isBoot=false);
	void MainLoop();
	int SendSignal(const char *progname, int signum);
	pid_t pidof(const char *pname);
	vector<pid_t> pidsof(const char *pname, int limit=0);
	//void initDir(string src, string tgt);
	//void initDirs(char *mnt);
	void RunBackground(const char *cmdname);
	void findDevices();
	bool tryMount(char *trydev, dev_t dev, char*& mnt);
};

#endif