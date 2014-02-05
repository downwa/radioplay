//#define NOPEGCPU 100000 /* Sleep 1/10 second to avoid pegging the CPU */
#define NOPEGCPU 250000 /* Sleep 1/4 second to avoid pegging the CPU */

#include "outscreen_saver.hh"
#include "outscreen_input.hh"

#ifndef __OUTHH__
#define __OUTHH__

#define METERLEN 23

#define OutputRow() screenWrite(row,isWide?1:0,out)

#include <sys/time.h>
#include <sys/mount.h>

#ifndef MNT_DETACH
#define MNT_DETACH 2
#endif

#include "thread.hh"
//#include "play3abn.h"
#include <string>
#include "util.hh"
#include "tschedline.hh"

#define ACT(a) (match(actionItem,a))

//#include "utildefs.hh"

using namespace std;
typedef map<unsigned int, string> MenuMap;
class Out: Thread {
	bool pqWarn;
	Util* util;
	static char screenNames[5][32];

	/** CHANGING SIZE OF STRUCTURE CAUSES PROBLEMS (probably a compile dependency not handled properly) **/
	//int detailStatus;
	//int detailRow;
	//int detailCount;
	/** **/
	int currow,curcol;
	int rowaction;
	int scrnum;
	bool isWide;
	char geolocation[256];
	time_t viewtime2;
	string clickAction;
	char savePlaying[256];
	
	virtual void Execute(void* arg);
public:
	vector<MenuMap> menus;
	MenuMap curlineMenus;
	unsigned int maxx,maxy;			// Number of characters in the x,y directions
	char release[64];
	//char updateStatus[256];
	bool clickShown;
	time_t prevPlayAt;
	
	Out(Util *util) {
		maxx=maxy=0; isWide=false; rowaction=0; scrnum=0; pqWarn=true; 
		if(!util) { this->util=new Util("out"); }
		else { this->util=util; }
	}
	bool isFiller(const char* path);
	void initColors();
	void Activate();
	int Update(int row, const char *radiodir, bool denyMount);
	int UpdatePlaying(int row);
	int UpdateRows(int startRow);
	string fixline(char *out, int size);
	void setProgressBar(char *bar, int barLen, int progress, int nearEnd, char ch);
	void QuickExit();
	void rowSelect(int rownum, int colnum);
	void rowAction(int rownum, int colnum, bool isLocal);
	string geoLocate(string publicip);
	static int textlen(const char *str);

		//virtual int screenWrite(unsigned int yy, unsigned int xx, const char *msg);
	virtual int screenWrite(unsigned int yy, unsigned int xx, const char *msg) { return yy+1; }
	virtual int screenWriteUrl(unsigned int yy, unsigned int xx, const char *msg, string tip, string url);
	virtual void Shutdown();
	virtual void attrOn(unsigned int attr);
	virtual void attrOff(unsigned int attr);
	virtual void Output();

	friend class OutScreen;
	friend class OutScreenSaver;
	friend class OutHttpRequest;
	friend class OutRemote;
	friend class OutScreenInput;
};

#endif
