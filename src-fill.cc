#include "src-fill.hh"

SrcFill::SrcFill(Util* util, Decoder* decoder) {
	this->util=util;
	this->decoder=decoder;
	Start(NULL);
}

/** PURPOSE: Cache the length (in seconds) of each filler file **/
void SrcFill::Execute(void* arg) {
	//time_t prevTime=0;
	string fillcache=string(Util::TEMPPATH)+"/fillcache";
	string filldir="/tmp/play3abn/cache/filler/songs"; // FIXME: lrucache should cache filler, songs as well
	syslog(LOG_ERR,"SrcFill::Execute:Maintaining list at %s from %s",fillcache.c_str(),filldir.c_str());
	const char *srcdir=filldir.c_str();
	const char *dstdir=fillcache.c_str();
	while(!Util::checkStop()) {
		struct stat statbuf1;
		struct stat statbuf2;
		bool waitmsg=false;
		bool chgmsg=false;
		do {
			if(stat(srcdir, &statbuf1)==-1) {
				if(!waitmsg) { waitmsg=true; syslog(LOG_ERR,"SrcFill::Execute:Waiting for creation of %s.",filldir.c_str()); }
				sleep(1); continue;
			}
			if(stat(dstdir, &statbuf2)==-1) { mkdir(dstdir,0777); break; }
			else if(statbuf1.st_mtime <= statbuf2.st_mtime) {
				if(!chgmsg) { chgmsg=true; syslog(LOG_ERR,"SrcFill::Execute:Waiting for changes in %s.",filldir.c_str()); }
				sleep(1);
				continue;
			}
			else { break; }
		} while(true);
		syslog(LOG_ERR,"SrcFill::Execute:Filling %s from %s",fillcache.c_str(),filldir.c_str());
		
		DIR *dir=opendir(srcdir);
		if(!dir) { syslog(LOG_ERR,"SrcFill: opendir(%s): %s",filldir.c_str(),strerror(errno)); sleep(1); continue; }
		struct dirent *entry;
		while((entry=readdir(dir))) {
			if(entry->d_name[0]=='.') { continue; } // Skip hidden files
			string path=filldir+"/"+string(entry->d_name);
			const char *cpath=path.c_str();
			float seclen=decoder->SecLen(cpath);
			if(seclen<0) { continue; }
			char centry[1024]; // Use dummy time to avoid duplicate entries
			snprintf(centry,sizeof(centry),"%s/%s %04d %s",dstdir,Util::fmtTime(0).c_str(),(int)seclen,entry->d_name);
			if(symlink(cpath, centry)<0 && errno!=EEXIST) { syslog(LOG_ERR,"SrcFill::Execute: %s: %s",strerror(errno),centry); }
		}
		closedir(dir);
		//syslog(LOG_ERR,"SrcFill::Execute:Waiting for 1800 seconds...");
		sleep(1);
	}
}

// int main(int argc, char **argv) {
// 	/*SrcFill *srcfill=*/new SrcFill();
// }
