#include "getfill.hh"

//#define dolog syslog(LOG_ERR

#define EXECDELAY 100000 /* 0.1 seconds */

Filler::Filler() {
	prevFill=0;
	util=new Util("getfill#1"); // Need separate util instance for each thread (to avoid locking issues)
	fprintf(stderr,"        line=%d Filler: CLOCKSKEW=%d\n",__LINE__,CLOCKSKEW);
MARK
	fprintf(stderr,"        line=%d tzofs=%ld,timezone=%ld,zone=%s\n",__LINE__,Util::tzofs,timezone,Util::zone);
MARK
	Execute(NULL);
MARK
}

/*******************************************************************/
bool Filler::maybeSabbath(time_t playTime) {
	struct tm tms;
	localtime_r(&playTime,&tms);
	bool maybe=false;
	if(tms.tm_wday==6) { maybe=true; }
	else if(tms.tm_wday==5 && tms.tm_hour>=2) { maybe=true; }
	return maybe;
}

/** NOTE: Returns path of latest news (if any).
 **       News files other than the latest, are removed.
 **       News files older than one day, are removed.
 **       If no news is less than an hour old, set refresh flag to true.
 **       If no news is less than a day old, return "" **/
string Filler::latestNews(bool& refresh) {
	string newsweatherBasedir="/tmp/play3abn/cache/newsweather";
	if(!util->isMounted()) { return ""; }
	const char *ndir=newsweatherBasedir.c_str();
  DIR *dir=opendir(ndir);
	if(!dir) { fprintf(stderr,"        line=%d Player::latestNews: %s: opendir '%s'\n",__LINE__,strerror(errno),ndir); return ""; }
  struct dirent *entry;
  time_t latest=0;
	char latestPath[1024]={0};
	struct stat statbuf;
	// NOTE: Find the latest news
	while((entry=readdir(dir))) {
		if(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) { continue; }
		char pathname[1024];
		snprintf(pathname,sizeof(pathname),"%s/%s",ndir,entry->d_name);
		if(stat(pathname, &statbuf)==-1) { continue; } // Non-existent
		if(statbuf.st_size<500000) { continue; } 				 // Incomplete file
		if(statbuf.st_mtime>=latest) { latest=statbuf.st_mtime; strncpy(latestPath,pathname,sizeof(latestPath)); }
	}
	rewinddir(dir);
	// NOTE: Remove all but the latest news
	while((entry=readdir(dir))) {
		if(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) { continue; }
		char pathname[1024];
		snprintf(pathname,sizeof(pathname),"%s/%s",ndir,entry->d_name);
		if(stat(pathname, &statbuf)==-1) { continue; } // Non-existent
		if(statbuf.st_mtime!=latest) { remove(pathname); }
	}
	closedir(dir);
	if(stat(latestPath, &statbuf)!=-1) {
		time_t now=time(NULL);
		if(now-statbuf.st_mtime>86400) { refresh=true; remove(latestPath); latestPath[0]=0; } // News is more than a day old: can't use it.
		if(now-statbuf.st_mtime>3600) { refresh=true; } // May use it but please refresh: it's stale!
	}
	string news=latestPath[0]?string(latestPath):"";
	if(refresh || news=="") {
		const char *newsflag="/tmp/play3abn/tmp/.news.flag"; /** NOTIFY Downloader to update the news (if possible) **/
		int fd=creat(newsflag,0755);
		if(fd!=-1) { close(fd); }
	}
	return news;
}

/** NOTE: Get recent filler, or refill from fillcache **/
/** Returns name of file used in filler dir (if any) so it can be removed when we're done **/
/** NOTE: getFiller should not return the -Filler files if there is anything else that can be used instead. **/
strings Filler::getFiller(int maxLen, string& fillname) {
MARK
/**/fprintf(stderr,"        line=%d Player::getFiller#0: maxLen=%d\n",__LINE__,maxLen);
	time_t now=Time();
//	if(maxLen<=-100) {
//		string info=Util::fmtTime(now)+" "+Util::fmtInt(15,4)+" Local_ID.ogg";
//		fprintf(stderr,"        line=%d Player::getFiller#1: Local_ID\n",__LINE__);
//		return strings(info,"/tmp/play3abn/cache/filler/Local_ID.ogg");
//	}
	if(maxLen<=0) { maxLen=1; }
	//string fillq=string(Util::TEMPPATH)+"/fillq";
	//vector<strings> playlist=util->getPlaylist(fillq.c_str(),0/*nolimit*/,1/*LRU*/,maxLen); // Get top entry
	
	/** DECIDE WHICH SET OF FILLER TO USE **/
	/** >=370     use MUSIC
            93-369    use OTHERLONG
            80-92     use OTHER
            61-79     use ADS
            10-60     alternate between SHORT and ADS
            <=10      use SILENT
        **/
	const char *fillType="MUSIC";
	if(maxLen>=370) { } // default to MUSIC
	else if(maxLen>=93 && maxLen<=369) { fillType="OTHERLONG"; }
	else if(maxLen>=80 && maxLen<=92)  { fillType="OTHER";     }
	else if(maxLen>=61 && maxLen<=79)  { fillType="ADS";       }
	else if(maxLen>=10 && maxLen<=60)  {
		prevFill=1-prevFill; // Toggle fill type for this length
		if(prevFill==1) { fillType="SHORT"; }
		else { fillType="ADS"; }
	}
	else { fillType="SILENT"; }
/**/fprintf(stderr,"        line=%d Filler::getFiller#0a: fillType=%s,fillPlaylist.size=%d\n",__LINE__,fillType,fillPlaylist.size());
	/** FILL CACHE IF NEEDED **/
	MARK
	if(fillPlaylist.size()==0) { // No short-enough files remain
fprintf(stderr,"        line=%d getFiller#0b: lenFile:#1:refilling\n",__LINE__);
		filler=util->ListPrograms("ProgramList2-FILLER_"); // FILLER_ADS FILLER_MUSIC FILLER_OTHER FILLER_SHORT FILLER_SILENT		
		fillPlaylist=util->getPlaylist(filler[fillType],0/*nolimit*/,-1/*random*/,maxLen);
fprintf(stderr,"        line=%d getFiller#0c: lenFile:#1b sorted '%s' length=%d\n",__LINE__,fillType,filler[fillType].size());		
vector<string>::iterator iter;
int count=0;
//#error Verified problem: sorted filler[fillType] does not leave getPlaylist (need pass by val?) (we want sorted random)
//#error check out.log for details.
fprintf(stderr,"        line=%d getFiller#0d: ITEM count (showing first five): %d\n",__LINE__,filler[fillType].size());
for( iter = filler[fillType].begin(); iter != filler[fillType].end() && count<5; iter++ ) {
	fprintf(stderr,"        line=%d getFiller#0e:  ITEM: %s\n",__LINE__,(*iter).c_str()); count++;
}
// #fixme Keeping fillPlaylist around means the maxLen we specified when calling getPlaylist (above) does not
// #fixme stop later access to the fillPlaylist from getting items that are too long (or too short, for that matter)
// #fixme for the available slot.  Instead, we need to either refresh the list each time, filter the list, or keep multiple lists
// #fixme depending on the length (maybe using the above categories, >=370, <370, <80, <61, <10)
// #fixme However, if we do that, we then still have to sub-filter the list when extracting items from it
		//string fillcache=string(Util::TEMPPATH)+"/fillcache";
		//string fillq=string(Util::TEMPPATH)+"/fillq";
		//util->cacheFiller(fillcache,fillq,-1); // Refill the list (randomly sorted)
		//playlist=util->getPlaylist(fillq.c_str(),0/*nolimit*/,1/*LRU*/,maxLen); // Get top entry
MARK
	}
	else { 
		fprintf(stderr,"        line=%d getFiller#0f: lenFile:#2:loading\n",__LINE__);
		fillPlaylist=util->getPlaylist(filler[fillType],0/*nolimit*/,0/*unsorted*/,maxLen);	 // unsorted to keep previous sort
		fprintf(stderr,"        line=%d getFiller#0g: lenFile:#2b length=%d\n",__LINE__,fillPlaylist.size());		
	}
	if(fillPlaylist.size()==0) {
		/** Failsafe Filler **/
		int fillLen=maxLen;
		if(fillLen<=90) {
			if(fillLen>6) {
				fillLen=fillLen/10*10;
				if(fillLen==0) { fillLen=6; } // <10 round to 6
			}
			if(fillLen==0) { fillLen=1; }
			char buf[256];
			snprintf(buf,sizeof(buf),"/tmp/play3abn/cache/filler/songs/%03d-Filler.ogg",fillLen);
			string info=Util::fmtTime(now)+" "+Util::fmtInt(fillLen,4)+" EMPTY FILL-"+Util::fmtInt(fillLen,4)+"-Filler.ogg";
			fillname="";
fprintf(stderr,"        line=%d Filler::getFiller#2a: Filler(%d): maxLen=%d\n",__LINE__,fillLen,maxLen);
			return strings(info,buf);
		}
		else {
			string info=Util::fmtTime(now)+" "+Util::fmtInt(40,4)+" EMPTY FILL-Filler.ogg";
			fillname="";
fprintf(stderr,"        line=%d Filler::getFiller#2b: Filler40: maxLen=%d\n",__LINE__,maxLen);
			return strings(info,"/tmp/play3abn/cache/filler/Filler.ogg");
		}
	}
	else {
vector<strings>::iterator iter;
int count=0;
//#error Verified problem: sorted filler[fillType] does not leave getPlaylist (need pass by val?) (we want sorted random)
//#error check out.log for details.
fprintf(stderr,"        line=%d getFiller#3a: ITEM count (showing first five): %d\n",__LINE__,fillPlaylist.size());
for( iter = fillPlaylist.begin(); iter != fillPlaylist.end() && count<5; iter++ ) {
	strings myent=(*iter);
	fprintf(stderr,"        line=%d getFiller#3aa:  ITEM: two=%s\n",__LINE__,myent.two.c_str()); count++;
}

	  
	  
		strings ent("","");
		ent=fillPlaylist.front();
fprintf(stderr,"        line=%d Filler::getFiller#3: lenFile: one=%s,two=%s\n",__LINE__,ent.one.c_str(),ent.two.c_str());
// e.g. one=2014-02-10 14:00:00; two=catcode=BIBLEANSWERS;expectSecs=3601;flag=366;url=/tmp/play3abn/cache/Radio/Amazing%20facts/ba20070715.ogg
		/** Parse info string **/
		time_t playAt=0;
		int seclen=0;
		string dispname="";
		int flag=0;
		string catcode="",url="";
		int decResult=util->itemDecode(ent, playAt, flag, seclen, catcode, dispname, url);
		if(decResult<0) {
		  fprintf(stderr,"        line=%d Player::Execute: Invalid format1 (decResult=%d): %s => %s",__LINE__,decResult,ent.one.c_str(),ent.two.c_str());
		  strings eent("","");
		  return eent;
		}
		

/*		
		const char *pp=ent.two.c_str(); // e.g. "365 0900 patt " (flag length pattern)
		const char *sLen=strchr(pp,' '); // Skip flag/catcode // sLen = " 0900 patt "
		const char *sPath=sLen?&sLen[6]:pp; // sPath = "patt "
		int len=sLen?atoi(&sLen[1]):0; // sLen e.g. " 0106 /tmp/play3abn/cache/Radio/My Music/..."
		fprintf(stderr,"        line=%d *** sLen=%s,sPath=%s,seclen=%d\n",__LINE__,sLen,sPath,seclen);		
*/		
		
		
		const char *sPath=url.c_str();
		const char *relPath=sPath;
		fprintf(stderr,"        line=%d *** relPath=%s,seclen=%d\n",__LINE__,relPath,seclen);
		const char *qq="";
		
		/** Remove these prefixes **/
		const char sources[3][10]={{"/download"},{"/filler"},{"/Radio"}};
		for(unsigned int sx=0; sx<(sizeof(sources)/sizeof(sources[0])); sx++) {
			qq=strstr(sPath,sources[sx]);
			if(qq) { relPath=&qq[strlen(sources[sx])]; break; }
		}
			
		if(relPath[0]=='/') { relPath++; } // Skip leading slash
		
		vector<string>::iterator result;
		char sLenFile[256];
		
		snprintf(sLenFile,sizeof(sLenFile),"%04d %s",seclen,relPath);
//fprintf(stderr,"        line=%d DEBUG2: sLenFile=%s.",__LINE__,sLenFile);
// 		syslog(LOG_INFO,"lenFile=%s",sLenFile);
fprintf(stderr,"        line=%d lenFile=%s (qq=%s,sPath=%s,two=%s\n",__LINE__,sLenFile,qq,sPath,ent.two.c_str());
		string lenFile=string(sLenFile);
		result=find(filler[fillType].begin(), filler[fillType].end(), lenFile);
 		fprintf(stderr,"        line=%d lenFile:#3before filler[fillType].length=%d\n",__LINE__,filler[fillType].size());		
		if(result == filler[fillType].end()) { /*lenFile=0000 patt .*/
			fprintf(stderr,"        line=%d Did not find any lenFile=%s.\n",__LINE__,lenFile.c_str());
		}
		else { filler[fillType].erase(result); }
     fprintf(stderr,"        line=%d lenFile:#3b length=%d\n",__LINE__,fillPlaylist.size());		
 		fprintf(stderr,"        line=%d lenFile:#3:loading\n",__LINE__);
		fillPlaylist=util->getPlaylist(filler[fillType],0/*nolimit*/,0/*unsorted*/,maxLen);	 // unsorted to keep previous sort
 		fprintf(stderr,"        line=%d lenFile:#3c length=%d\n",__LINE__,fillPlaylist.size());		
// 		for( iter = filler[fillType].begin(); iter != filler[fillType].end(); iter++ ) {
// 			fprintf(stdout,"  ITEM2: %s\n",(*iter).c_str());
// 		}

		//#fixme Verify erase worked: we're repeating the same lenFile= meaning the list item was not removed
		
		// NOTE: "2011-06-09 12:40:00 0066 IT'S A WONDERFUL DAY-Heritage Singers.ogg" (datetime=0; seclen=20; name=25)
		//        0123456789 123456789
		fillname=ent.one;
		ent.one=Util::fmtTime(now)+ent.one.substr(19); // Reschedule for now
		fprintf(stderr,"        line=%d Filler::getFiller#99: fill count=%d; one=%s,two=%s\n",__LINE__,
					 fillPlaylist.size(),ent.one.c_str(),ent.two.c_str());
		//fprintf(stderr,"        line=%d Player::getFiller#3: "+ent.two,__LINE__);
		return ent;
	}
}

/** NOTE: Read a directory of links of the following form:
 * "2011-06-09 12:40:00 0066 IT'S A WONDERFUL DAY-Heritage Singers.ogg"
 * Each link specifies the date, time, length, and display name of a file to play.
 * The target of the link is the actual file to play.
 */
void Filler::Execute(void *arg) {
	int count=0;
        time_t prevTime=0;
//        time_t prevPlayAt=0;

MARK
	strings ent("","");
	fprintf(stderr,"        line=%d getfill:Filler::Execute: Starting filler loop\n",__LINE__);
	while(!Util::checkStop()) {
MARK
                time_t now=Time();
                if(now-prevTime == 0 && count>10) {  // NOTE: SLOW DOWN TO AVOID CPU OVERLOAD
                        if(usleep(EXECDELAY)==-1) { sleep(1); }
                        count=0;
                }
                prevTime=now;
                int hourElapsed=now%3600;
                int hourRemaining=(3600/*-15*/)-hourElapsed;

MARK
		string fillname="";
		int filllen=hourRemaining-120;
		if(scanf("%d",&filllen) < 1) { break; }
		ent=getFiller(filllen,fillname);
                /** Parse info string **/
//                 time_t playAt=0;
//                 int seclen=0;
//                 string dispname="";
// 		int flag=0;
// 		string catcode="",url="";
// 		int result=util->itemDecode(ent, playAt, flag, seclen, catcode, dispname, url);
// 		if(result<0) {
// 		  fprintf(stderr,"        line=%d Player::Execute: Invalid format1 (result=%d): %s => %s",__LINE__,result,ent.one.c_str(),ent.two.c_str());
// 		  continue;
// 		}
	  printf("%s|%s\n",ent.one.c_str(),ent.two.c_str());
	  fflush(stdout);
	}
MARK
}

int main(int argc, char **argv) {
	new Filler();
}
