/** NOTE: Download finishFile runs script e.g. ./update-programlist2.sh /tmp/play3abn/cache/download1/MMJ06058-4~MUSICAL_MEDITATIONS_Segment_Four~Christian_Music_For_Today~.ogg **/
/** 
 * Scheduler which uses a fixed list of programs.  Each program consists of a file in the schedules/programs 
 * folder of the USB flash or SD card used for playback files.  If there is more than one playback file drive
 * available, the set of programs will be merged, but the last version of a program to be loaded will be the
 * one actually used.  The program files are to be titled "Program-xxxx.txt" (where xxxx is the program name)
 * or Filler.txt, Filler-Music.txt, or Filler-Ads.txt (used for all non-program files), and contain a list of
 * files(*) to be played when that program is scheduled.  For non-filler programs, the list specifies the order
 * of playback.
 * 
 * [*] NOTE: The "list of files" for a program may include wildcards (using regular expressions), such as HOH.* 
 * (to play Homeschool Of Health programs which are downloaded later, without knowing what numbers they will 
 * be in advance).
 *
 * A matching set of ProgramList2-xxxx.txt files are to be pre-generated for each playback file drive's set of
 * Program-xxxx.txt files.  Each of these files contains the expanded (sorted or randomized), list of files,
 * prefixed with the length (in seconds) of each file, in the following form:
 *
 * PATT: Bible stories/Vol 01/.*
 * 1160 Radio/Bible stories/Vol 01/V1-01a  Adam and Eve.ogg
 * 1574 Radio/Bible stories/Vol 01/V1-01b  Cain and Able and The Flood.ogg
 * PATT: Bible stories/Vol 02/.*
 * 1494 Radio/Bible stories/Vol 02/V2-13a The Ten Commandments2.ogg
 * 1523 Radio/Bible stories/Vol 02/V2-13b Miriam The Great2.ogg
 *
 * NOTE: The first part of a line begins with "PATT" if it is a pattern comment indicating the matching pattern
 * the following files came from.  Otherwise, the line contains a 4 digit zero-padded length in seconds (up to 9999),
 * followed by a space, then the path of the file relative to the drive of origin.
 * 
 * 1. On each scheduling pass, find the CURRENT file's position in the list, and pick the *next*
 * file in sorted order (which will often but not always be the Least Recently Used) for each schedule slot
 * which needs to be filled.  The scheduler will continually regenerate the playq set of symlinks with links
 * containing the playback date/time (overwriting any that already exist for that date/time).
 * 
 * 2. If the schedules/programs folder does not contain a ProgramCurrent-xxxx.txt file for program xxxx, create
 * one and initialize it to the name of the first file in sorted list order (whether the list was sorted alphanumerically
 * or randomly).
 *  * 
 * Paths in the program/filler files will not be absolute but relative to all the available program sources
 * (e.g. download and Radio folders on each available SD card and USB flash drive).
 * 
 * Playback of programs will write their name to the ProgramCurrent-xxxx.txt file for each program, to keep track
 * of which was most recently used.
 * 
 * Programs are referred to by Playlist-[1-7].txt files (also in the schedules folder).  The playlist numbers
 * refer to the day of week for which that playlist is to be used.

 * Filler.txt audio will be used to fill out each program's scheduled slot, 
 * find the MRU item and choosing the next program after it which is short enough to fit in the remaining space, 
 * if not enough space remains in the scheduled slot to play another item from that program.  This feature can be
 * used (for instance) to allow multiple Bible chapters to play for a 15 minute time slot, but have a song for the
 * last bit of that slot if the next scheduled Bible chapter is too long.
 * 
 **/

#include "schedfixed.hh"

const char* SchedFixed::playq="/tmp/play3abn/tmp/playq";
SchedFixed::SchedFixed() {
	util=new Util("schedfixed");
	util->tzsetup();
	int debug=util->getInt("debug",0);
	
	syslog(LOG_INFO,"SchedFixed::SchedFixed: Loading schedule...");
	schedules=util->ListPrograms("Schedule-");
	if(debug) syslog(LOG_INFO,"SchedFixed::SchedFixed: Loading length cache...");
	programs=util->ListPrograms("ProgramList2-");
	/** NOTE: Wait until playq is nearly empty **/
	if(debug) syslog(LOG_INFO,"SchedFixed::SchedFixed: Checking playq length...");
	int playqlen=0;
	struct dirent *entry;
	do {
		playqlen=0;
		DIR* dir=opendir(playq);
		if(dir) {
			char linksrc[1024];
			char linktgt[1024];
			while((entry=readdir(dir)) && !Util::checkStop()) {
				if(entry->d_name[0]=='.') { continue; } // Skip hidden files
				snprintf(linksrc,sizeof(linksrc),"%s/%s",playq,entry->d_name);
				ssize_t ofs=readlink(linksrc, linktgt, sizeof(linktgt)-1);
				if(ofs>=0) {
					linktgt[ofs]=0; // contents of linktgt e.g. "TESTIMONY4 0060 http://radio.iglooware.com or /tmp/play3abn/cache/download/AfflBreak~20130314_152900~.ogg" (category, len, path)
					struct stat statbuf;
					char *path=strchr(linktgt,' ');
					if(path) { path=strchr(&path[1],' '); }
					else { syslog(LOG_ERR,"SchedFixed::SchedFixed#1: Missing size after flag in '%s'",linktgt); continue; }
					if(path) { path=&path[1]; }
					else { syslog(LOG_ERR,"SchedFixed::SchedFixed#2: Missing path after size in '%s'",linktgt); continue; }
					if(strncmp(path, "http://",7)==0) { continue; } // Skip download items
					if(strncmp(path, "ftp://",6)==0) { continue; } // Skip download items
					if(stat(path, &statbuf)==-1) {
syslog(LOG_ERR,"MISSING %s",path);
 continue; } // Skip non-existing items
				}
				playqlen++;
			}
			closedir(dir);
			syslog(LOG_INFO,"SchedFixed::SchedFixed: playq length=%d",playqlen);
		}
		else { syslog(LOG_ERR,"SchedFixed::SchedFixed: Error in opendir: %s",playq); }
		if(playqlen>25) { util->longsleep(300); }
		else { sleep(1); }
	} while(playqlen>25 && !Util::checkStop());
	//decoder=new Decoder(NULL,util); // Used to check SecLen() for each file (and cache it)
	//Start(NULL); // Cache the length (in seconds) of each file
	if(!Util::checkStop()) { Scheduler(); }
}

string SchedFixed::lencachepath="/tmp/play3abn/tmp/lencache";
void SchedFixed::Scheduler() {
	setIT("scheduling",1);
	syslog(LOG_INFO,"SchedFixed::Scheduler: Loading current programs cache...");
	codes=util->ListPrograms("Program-");
	map<string,vector<string> >::iterator iter;
	for(iter = codes.begin(); iter != codes.end(); ++iter ) {
		string code=iter->first;
		char sProgram[32];
		char cpCode[64];

		snprintf(cpCode,sizeof(cpCode),"curProgram-%s",code.c_str());
		util->get(cpCode,sProgram,sizeof(sProgram));
		curProgram[code]=string(sProgram);

		snprintf(cpCode,sizeof(cpCode),"curProgramIndex-%s",code.c_str());
		curProgramIndex[code]=getI(cpCode);
	}
	
	syslog(LOG_ERR,"SchedFixed::Scheduler: Scheduling now...");
	
	time_t now=time(NULL);
	syslog(LOG_ERR,"SCHEDULING TODAY");
	ScheduleDay(now,now);
	syslog(LOG_ERR,"SCHEDULING TOMORROW");
	ScheduleDay(now+(3600*24),now); // Tomorrow
	
	syslog(LOG_NOTICE,"SchedFixed:Scheduler:Done.");
	setIT("scheduling",0);
	//util->ListPrograms();
	//#error Stop using same program every time! 1709 BOL209~BREATH_OF_LIFE~Everybody_Cried_Pt_2~Walter_Pearson.ogg
	//map<string,vector<string> >::iterator iter;
//#error if power goes out after this runs, we'll have scheduled items which don't get to run.
//#error We should instead set curProgram and curProgramIndex as each item gets played
//#error To do that, we need to use prgcur (schedules/programs/ProgramCurrent-%s.txt)
//#error However, strangely those files are not being created by mplay as they should be.
//#error Why does schedfixed reschedule every few seconds?
//#error And why does it shuffle the order of e.g. CR91001-[1234]~A God Big Enough to Create-Floyd Courney.ogg?
// 	for(iter = codes.begin(); iter != codes.end(); ++iter ) {
// 		string code=iter->first;
// 		char cpCode[64];
// 		
// 		snprintf(cpCode,sizeof(cpCode),"curProgram-%s",code.c_str());
// 		util->set(cpCode,curProgram[code].c_str());
// 		
// 		snprintf(cpCode,sizeof(cpCode),"curProgramIndex-%s",code.c_str());
// 		util->setInt(cpCode,curProgramIndex[code]);
// 	}
	
}

void SchedFixed::ScheduleDay(time_t schedNow, time_t now) {
	struct tm tms;
	struct tm* nptr=localtime_r(&schedNow,&tms);
	char dayOfWeek[1024];
	snprintf(dayOfWeek,sizeof(dayOfWeek),"%d",nptr->tm_wday+1); // dayOfWeek
	vector<string> schedule=schedules[string(dayOfWeek)];
	const int scheduleAgeDays=366;
	
	/** DISPLAY SCHEDULE **/
	vector<string>::iterator iter;
	for(iter=schedule.begin(); iter!=schedule.end(); ++iter) {
		char buf[128];
		snprintf(buf,sizeof(buf),"%s",iter->c_str());
		
		
		snprintf(buf,11,"%04d-%02d-%02d",1900+nptr->tm_year,nptr->tm_mon+1,nptr->tm_mday);
		buf[10]=' '; buf[19]=0;
		string sDateTime(buf);
		//syslog(LOG_ERR,"sDateTime=%s",buf);
		time_t plNow=TSchedLine::TimeOf(util,sDateTime);
		buf[19]=' ';
		int mSec=atoi(&buf[20]);
		time_t plEnd=plNow+mSec;
		if(plEnd < schedNow && schedNow==now) { continue; } // If item ended before now, skip it
		
		// buf e.g. 2013-05-08 16:45:00 0885 BIBLE
		//                    11111111112222222222
		//          0123456789 123456789 123456789
		char *code=&buf[25];
		/* NOTE: programs[code] returns a vector of strings, the list of programs available for a given
		 * program code. */
		string program=curProgram[code];
//syslog(LOG_ERR,"DEBUG1:code=%s,program=%s",code,program.c_str());
		unsigned int programIndex=curProgramIndex[code];
		/** FIXME:   We need to iterate past the most recently used item (number and name) to get
		 * to the new stuff. **/
		vector<string> programList=programs[code];
		vector<string>::iterator iter2;
		string item="";
		unsigned int curItem=1;
//syslog(LOG_ERR,"DEBUG2:programList.size=%d,programIndex=%d",(int)programList.size(),programIndex);
//#error Keep matching below until sum of seclen > than scheduled slot length
		for(iter2 = programList.begin(); iter2 != programList.end(); iter2++) {
			//syslog(LOG_INFO,"  SCHED0: program=%s,curItem=%d",program.c_str(),curItem);
			//syslog(LOG_ERR,"    --> iter2=%s, program=%s, curItem=%d, programIndex=%d",(*iter2).c_str(),program.c_str(),curItem,programIndex);
			if(program=="" || programIndex<curItem) { 
				item=*iter2; 
				//syslog(LOG_INFO,"  SCHED1: item=%s (program=%s,programIndex=%d,curItem=%d)",item.c_str(),program.c_str(),programIndex,curItem); 
				break;
			}
			if(*iter2 == program) { 
				iter2++; item=*iter2; curItem++;
				//syslog(LOG_INFO,"  SCHED2: item=%s (program=%s,programIndex=%d,curItem=%d)",item.c_str(),program.c_str(),programIndex,curItem); 
				break;
			}
			curItem++;
		}
		curProgramIndex[code]=(curItem<programList.size())?curItem:0; 
		curProgram[code]=item;
//syslog(LOG_INFO,"DEBUG3: item=%s,index=%d",item.c_str(),curProgramIndex[code]); 
		const char *sItem=item.c_str(); // e.g. 1709 BOL209~BREATH_OF_LIFE~Everybody_Cried_Pt_2~Walter_Pearson.ogg
		int seclen=atoi(sItem);   //      012345
		if(seclen==0) { syslog(LOG_ERR,"    NOT: %s",buf); continue; }
		char path[256];
		util->findTarget(path,sizeof(path),&sItem[5]);
		if(path[0]) {
			//syslog(LOG_INFO,"  SCHED: %s [CODE: %s, seclen=%d, path=%s]",buf,code,seclen,path);
			util->enqueue(plNow,string(path), string(code), scheduleAgeDays, seclen);
		}
		else {
			syslog(LOG_ERR,"    NOT: %s [CODE: %s, sItem=%s]",buf,code,sItem);
		}
	}
}

// NOTE: Schedule multiple lines from this buffer
void SchedFixed::ScheduleBufferX(string outbuffer) {
// 	syslog(LOG_ERR,"SchedFixed::ScheduleBuffer: outbuffer: %s",outbuffer.c_str());
// 	const int scheduleAgeDays=365;
// 	const char *lines=outbuffer.c_str();
// 	/** NOTE: each line of outbuffer is of the format: **/
// 	 //                        sched actual
// 	 // "  2013-03-14 13:00:00 3585 3028 /tmp/play3abn/tmp/lencache/^GYC.*/len/3028 GYC001~GENERATION_OF_YOUTH_FOR_CHRIST~Be~.ogg"
// 	 // "          111111111122222222223333333333
// 	 // "0123456789 123456789 123456789 123456789
// 	 // Inputs per line:
// 	 //   - date [2]
// 	 //   - time [13]
// 	 //   - requested length [22]
// 	 //   - actual length [27]
// 	 //   - complete path [32]
// 	 // enqueue(plNow, complete path, display name, scheduleAgeDays, actual length);
// 	const char *startline=lines;
// 	int linelen=0;
// MARK
// 	do {
// 		const char *endline=strchr(startline,'\n');
// 		if(!endline) { break; }
// 		linelen=endline-startline;
// 		char buf[1024];
// 		snprintf(buf,sizeof(buf)-1,"%.*s", linelen, startline);
//     syslog(LOG_ERR,"SchedFixed:ScheduleBuffer:buf=%s",buf); // FIXME: Getting e.g. "  201" instead of "  2013-04-01" ...
// 		// NOTE: parse date/time to plNow
// 		struct tm timestruct={0};
// 		char *result=strptime(buf,"%Y-%m-%d %H:%M:%S",&timestruct);
// 		if(!result) { syslog(LOG_ERR,"SchedFixed::ChooseNextItem:ScheduleBuffer:strptime failed: %s",buf); break; }
// 		int tzHour=(timezone-Util::tzofs); // 0 or 3600 depending on whether DST is in effect
// 		time_t plNow=mktime(&timestruct)-tzHour;
// 		const char* linked=&buf[32];
// 		// NOTE: Extract display name from complete path
// 		const char *dispname=strrchr(linked,'/');
// //#FIXME 2013-03-14 20:50:42: schedfixed-24994: Util::enqueue: /tmp/play3abn/tmp/playq/2013-03-15 05:00:00 -1 ==> 365 0000 MONY
// //#FIXME 2013-03-14 20:50:59: schedfixed-24994: SchedFixed:ScheduleBuffer:buf=  2013-03-15 05:00:00 0900 TESTIMONY
// 
// 		if(dispname) { dispname+=6; } // FIXME: Skip slash and length
// 		else { dispname=linked; }
// 		// NOTE: atoi() actual length
// 		int actLen=atoi(&buf[27]);
// MARK
// 		util->enqueue(plNow,string(linked), dispname, scheduleAgeDays, actLen);
// MARK
// 		//usleep(100000); // Sleep 1/10 second to avoid pegging the CPU
// 		//sleep(1); // Sleep one second to avoid pegging the CPU
// 		startline=endline+1;
// 	} while(linelen>0 && !Util::checkStop());
// MARK
}

int main(int argc, char **argv) {
	/** Set idle IO priority **/
#ifndef ARM
#ifndef RPI
	if(ioprio_set(IOPRIO_WHO_PROCESS, getpid(), IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 7))<0) {
		syslog(LOG_ERR,"schedfixed:ioprio_set: %s",strerror(errno));
	}
#endif
#endif
	if(setpriority(PRIO_PROCESS, 0, 19)<0) {
		syslog(LOG_ERR,"schedfixed:setpriority: %s",strerror(errno));
	}
	new SchedFixed();
}
