#include <vector>
#include <map>
#include "libs/tremor/ivorbiscodec.h"
#include "libs/tremor/ivorbisfile.h"
#include <sys/sendfile.h>
#include <openssl/md2.h>
#include <openssl/evp.h>
#define _FILE_OFFSET_BITS 64

#define save_getmaxyx getmaxyx
#undef getmaxyx
#define getmaxyx(win, y, x) { y=20; x=80; }

extern OggVorbis_File vf;
extern char oggFname[1024];

#include "regdata.hh"

static void tohex(const unsigned char *digest, char *buf, int len) {
  for(int i=0; i<len; i++){ snprintf(&buf[i*2],3,"%02x", digest[i]); }
}

struct strCmp {
	bool operator()( const char* s1, const char* s2 ) const {
		return strcmp( s1, s2 ) < 0;
	}
};
typedef vector<unsigned int> AttrArray;
class OutHttpRequest: Out {
	unsigned int curAttr;
	vector<string> screen;
	vector<AttrArray> attrs;
	vector<string> urls;
	vector<string> tips;
	map<const char*, const char*, strCmp> parms;
	RegDataMap& registered;
	//map parms;
	
	char getpath[64];
	string suffix;
	string macAddr;
	int sd_accept;
	char reqbuf[1024];
	char bodyline[1024];
	char saveBodyline[1024];
	char *method;
	char *reqpath;
	char *httpver;
	static char cexts[][5];
	static char ctypes[][32];

	void attrOn(unsigned int attr);
	void attrOff(unsigned int attr);
	string AttrToHtml(unsigned int attr);
	int parmInt(const char *key);
	int parmInt(const char *key, int val);
	void setParmInt(const char *key, int val);
	void SendFile(string path);
	void Error(string errmsg);
	void Execute();
	void Execute(void* arg);
	void Stream();
	void DoLogin(const char *mac, const char *reqpath);
	bool checkLogin(char *sessionId, const char *password, const char *hash, bool debug);
	void processParms(char *parmLine, bool debug);
	void DoRemote(vector<string>& headers, bool debug);
	public:
	OutHttpRequest(int sd_accept, RegDataMap& reg, string suffix, string macAddr);
	~OutHttpRequest();
	bool send_data(char *buffer, int len);
	bool send_data(int fd, char *buffer, int len);
	bool readline(char *buffer, int size, int& len);
	int get_data(char *buffer, int size, int& len);
	void screenWrite(size_type yy, size_type xx, const char *msg);
	int screenWrite(size_type yy, size_type xx, const char *msg, string tip, string url);
	int screenWriteUrl(size_type yy, size_type xx, const char *msg, string tip, string url) { return screenWrite(yy,xx,msg,tip,url); }
	void Shutdown();
	void closeSocket(int sock);
	static void closeASocket(int sock);
	int screenLen();
	string urlParms();
	string screenVal();
	string screenText();
	void SendScreen();
	void sha256(const char *message, char *buf, int bufsiz);
	const char* ContentType(string path);
};

//--------------------------------------------------------
OutHttpRequest::OutHttpRequest(int sd_accept, RegDataMap& reg, string suffix, string macAddr): 
		registered(reg)
		{	
	//syslog(LOG_ERR,"OutHttpRequest::OutHttpRequest: sd_accept=%d",sd_accept);
	this->sd_accept=sd_accept;
	this->suffix=suffix;
	this->macAddr=macAddr;
	isWide=false;
	curAttr=0;
	Activate();
	Start(NULL);
}

//--------------------------------------------------------
void OutHttpRequest::sha256(const char *message, char *buf, int bufsiz) {
	if(bufsiz>0) { buf[0]=0; }
  EVP_MD_CTX mdctx;
  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;
  EVP_DigestInit(&mdctx, EVP_sha256());
	EVP_DigestUpdate(&mdctx, message, strlen(message)); //(size_t) sb.st_size);
  EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
  EVP_MD_CTX_cleanup(&mdctx);
	tohex(md_value,buf,min(md_len,bufsiz/2));
}

//--------------------------------------------------------
int OutHttpRequest::parmInt(const char *key, int val) {
	const char *pval=parms[key];
	return (pval && pval[0])?atoi(pval):val;
}
int OutHttpRequest::parmInt(const char *key) {
	return parmInt(key,0);
}
void OutHttpRequest::setParmInt(const char *key, int val) {
	char *valbuf=new char[10];
	snprintf(valbuf,10,"%d",val);
	const char *old=parms[key];
	if(old) {
		if(old<reqbuf || old>&reqbuf[sizeof(reqbuf)]) { delete old; } // Delete old value only if not in reqbuf
	}
	parms[key]=valbuf;
	//syslog(LOG_ERR,"1:setParmInt(%s,%s)",key,valbuf);
}
//--------------------------------------------------------
void OutHttpRequest::closeSocket(int sock) {
	if(suffix=="remote") { return; } // Keep connection alive for more requests from server
	closeASocket(sock);
}
void OutHttpRequest::closeASocket(int sock) {
	if(sock==-1) { return; } // Keep connection alive for more requests from server
	struct linger ling;
	ling.l_onoff=1;
	ling.l_linger=2;
	setsockopt(sock, SOL_SOCKET, SO_LINGER,&ling,sizeof(ling));
	shutdown(sock,SHUT_RDWR); 
	close(sock);
}
//--------------------------------------------------------
char OutHttpRequest::cexts[][5]={
	"html","htm",
	"txt","c","cc","h","hh",
	"css",
	"gif",
	"jpg","jpeg",
	"png",
	"bmp",
	"ico",
	"wav",
	"mp3",
	"ogg",
	"mpg",
	"mov",
	"avi",
	"ps",
	"rtf",
	"pdf",
	"doc",
	"tar",
	"zip",
	"js",
	""
};
	
char OutHttpRequest::ctypes[][32]={
	"text/html","text/html",
	"text/plain","text/plain","text/plain","text/plain","text/plain",
	"text/css",
	"image/gif",
	"image/jpeg","image/jpeg",
	"image/x-png",
	"image/x-ms-bmp",
	"image/x-icon",
	"audio/x-wav",
	"audio/x-mpeg", // NOT SURE this is right for .mp3
	"application/ogg; charset=binary",
	"video/mpeg",
	"video/quicktime",
	"video/x-msvideo",
	"application/postscript",
	"application/rtf",
	"application/pdf",
	"application/msword",
	"application/x-tar",
	"application/zip",
	"text/javascript",
	""
};

const char* OutHttpRequest::ContentType(string path) {
	const char *ppath=path.c_str();
	const char *ext=strrchr(ppath,'.');
	if(!ext) { return "text/plain"; }
	ext=&ext[1];
	for(int xa=0; cexts[xa][0]; xa++) { // Check extension
		if(strcmp(cexts[xa],ext)==0) { return ctypes[xa]; }
	}
	return "text/plain";
}
//--------------------------------------------------------
void OutHttpRequest::SendFile(string path) {
	int in_fd=open(path.c_str(),O_RDONLY);
	if(in_fd==-1) { Error(string("Error: ")+string(strerror(errno))); return; }
	struct stat statbuf;
	if(fstat(in_fd, &statbuf)==-1) { close(in_fd); Error("SendFile Error: "+string(reqpath)+": "+string(strerror(errno))); return; }
	char headers[1024];
	const char *ctype=ContentType(path);
	snprintf(headers,sizeof(headers),"HTTP/1.1 200 Ok\r\nContent-type: %s\r\nContent-Length: %d\r\n\r",
		ctype,(int)statbuf.st_size);
	send_data(headers,strlen(headers));
	ssize_t ret=sendfile(sd_accept, in_fd, NULL, statbuf.st_size);
	if(ret==-1) { syslog(LOG_ERR,"SendFile Error: %s",strerror(errno)); }
	close(in_fd);
	closeSocket(sd_accept);
}
//--------------------------------------------------------
void OutHttpRequest::Execute() {
MARK
	scrnum=parmInt("scrnum");
	rowaction=parmInt("row",9999);
	setParmInt("row",9999); // Avoid repeating the action we just took
	//syslog(LOG_ERR,"1:rowaction=%d,scrnum=%d",rowaction,scrnum);
	rowAction(rowaction,0,false);
	setParmInt("scrnum",scrnum);
	//syslog(LOG_ERR,"1:parmInt(scrnum)=%d,parms[scrnum]=%s",parmInt("scrnum"),parms["scrnum"]);
	//syslog(LOG_ERR,"2:rowaction=%d,scrnum=%d",rowaction,scrnum);
	screen.clear();
	getmaxyx(screen,maxy,maxx);
	maxx=parmInt("maxx",maxx);
	maxy=parmInt("maxy",maxy);
	chooser->timeFactor=parmInt("timeFactor",1);
MARK
	Output();
MARK
	SendScreen();
MARK
	closeSocket(sd_accept);
MARK
}
void OutHttpRequest::Stream() {
	bool done=false;
	char headers[1024];
	snprintf(headers,sizeof(headers),"HTTP/1.1 200 Ok\r\nContent-type: application/ogg; charset=binary\r\n\r");
	if(!send_data(headers,strlen(headers))) { done=true; }
	char curogg[1024]={0};
	bool firstFile=true;
	while(!done && !doStop) {
		// NOTE: Wait until a new file starts playing (in case client is faster than real time)
		while(strcmp(curogg,oggFname)==0) { sleep(1); }
		strncpy(curogg,oggFname,sizeof(curogg));
		char buffer[4096];
		char cmd[1024];
		int seekSecs=playSec;
		// NOTE: If client is slower than server (e.g. FriendlyARM playback is faster than PC),
		//       client will miss part of each successive file (the part that had already played on server)
		//       I can't think of any solution but to set seekSecs to 0 if it's a small number.
		//       But, this will cause client to get farther and farther behind, until it has to catch up 
		//       all at once.  However, that may be preferable to other alternatives
		if(!firstFile && seekSecs>0 && seekSecs<300) { seekSecs=0; }
		// ***************
		snprintf(cmd,sizeof(cmd),"./oggcut \"%s\" %d",oggFname,seekSecs);
		syslog(LOG_ERR,"Stream: %s",cmd);
		FILE *fp=popen(cmd,"r");
		while(fp && !feof(fp) && !ferror(fp)) {
			size_t retval=fread(buffer, 1, sizeof(buffer), fp);
			if(retval>0) {
				if(!send_data(buffer,retval)) { done=true; break; }
			}
		} // WHILE not to end of file
		if(fp) { pclose(fp); }
		else { sleep(1); } // Failure to open: avoid hard loop
		firstFile=false;
	} // WHILE !done
	closeSocket(sd_accept);
}
void OutHttpRequest::Error(string errmsg) {
	syslog(LOG_ERR,"HTTP Error Response: %s",errmsg.c_str());
	char headers[1024];
	char body[1024];
	const char *errmsgS=errmsg.c_str();
	snprintf(body,sizeof(body),"<html><head><title>Not Found: %s</title>"
		"<link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"/favicon.ico\"></head><body><h1>Not Found</h1>%s"
		"<a href=\"/\">Continue&gt;</a>\n</body><!-- %s --></html>",
		errmsgS,errmsgS,macaddr);
	int clen=strlen(body);
	snprintf(headers,sizeof(headers),"HTTP/1.1 200 Ok\r\nContent-type: text/html\r\nContent-Length: %d\r"
		"Cache-Control: no-cache\r\nPragma: no-cache\r\nExpires: -1\r\n\r",clen);
	send_data(headers,strlen(headers));
	send_data(body,clen);
	closeSocket(sd_accept);
}
void OutHttpRequest::DoLogin(const char *mac, const char *reqpath) {
	syslog(LOG_ERR,"DoLogin: %s",mac);
	char headers[1024];
	char body[1024];
	snprintf(body,sizeof(body),"<html><head><title>Authentication Required (%s)</title>"
		"<script src=\"/web/sha256.js\">/* SHA-256 JavaScript implementation */</script>"
		"<link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"/favicon.ico\"></head><body><h1>Authentication Required (%s)</h1>"
		"<form name=\"login\" method=\"POST\" action=\"%s\">"
		"  <label for=\"password\">Password:</label><input name=\"password\" id=\"password\" type=\"password\" /><br />"
		"  <input type=\"hidden\" name=\"hash\" id=\"hash\" />"
		"  <input type=\"submit\" name=\"submit\" id=\"submit\" onClick=\""
				"login.hash.value = Sha256.hash(login.password.value);"
				"login.password.value=login.password.value.replace(/./g,'*'); forms['login'].submit();\" value=\"Login\" />"
		//"  <input name=\"submit\" type=\"submit\" value=\"Submit\">"
		"</form>"
		"</body><!-- %s --></html>",mac,mac,reqpath,mac);
	int clen=strlen(body);
	char sessionKey[64];
	snprintf(sessionKey,sizeof(sessionKey),"%08X%08X%08X%08X",
		(unsigned int)random(),(unsigned int)random(),(unsigned int)random(),(unsigned int)random());
	snprintf(headers,sizeof(headers),"HTTP/1.1 200 Ok\r\nContent-type: text/html\r"
		"Set-Cookie: Session%s=%s\r\nCache-Control: no-cache\r\nPragma: no-cache\r\nExpires: -1\r"
		"Content-Length: %d\r\n\r",mac,sessionKey,clen);
	send_data(headers,strlen(headers));
	send_data(body,clen);
	closeSocket(sd_accept);
}
bool OutHttpRequest::checkLogin(char *sessionId, const char *password, const char *hash, bool debug) {
	if(!sessionId) { syslog(LOG_ERR,"checkLogin: No sessionId"); return false; }
	if(!password) { password=""; }
	if(!hash) { hash=""; }
	if(debug) { syslog(LOG_ERR,"checkLogin: sessionId=%s,password=%s,hash=%s",sessionId,password,hash); }
	char keyName[32];
	snprintf(keyName,sizeof(keyName),"session%s",sessionId);
	bool auth=(getInt(keyName,0)==1);
	if(auth) { return true; }
	char actualPassword[64];
	get("password",actualPassword,sizeof(actualPassword));
	if(!actualPassword[0]) {
		strncpy(actualPassword,"revelation1467",sizeof(actualPassword));
		set("password",actualPassword);
	}
	if(password && password[0]!='*' && strcmp(password,actualPassword)==0) {
		setInt(keyName,1);
		return true;
	}
	if(hash && hash[0]) {
		char hashActualPassword[64+1]; // Space for hex 256 bits (plus Null byte)
		sha256(actualPassword, hashActualPassword, sizeof(hashActualPassword));
		syslog(LOG_ERR,"hashActualPassword=%s.",hashActualPassword);
		if(strcmp(hash,hashActualPassword)==0) {
			setInt(keyName,1);
			return true;
		}
	}
	return false;
}
void OutHttpRequest::processParms(char *parmLine, bool debug) {
	char *pp=parmLine;
	while(pp && *pp) {
		char *key=pp;
		char *qq=strchr(key,'&');
		if(qq) { *qq=0; pp=&qq[1]; }
		else { pp=0; }
		char *val=strchr(key,'=');
		if(!val) { val=(char *)"1"; }
		else { *val=0; val=&val[1]; }
		if(debug) { syslog(LOG_ERR,"key=%s,val=%s",key,val); }
		parms[key]=val;
		//syslog(LOG_ERR,"refresh=%s,%s",parms["refresh"],parms[key]);
	}
}
void OutHttpRequest::DoRemote(vector<string>& headers, bool debug) {
		const char* newreqpath=strchr(&reqpath[1],'/'); // Find new path
		if(!newreqpath) { newreqpath="/"; }
		const char *pmac=parms["mac"];
		if(pmac==NULL) { Error(string(reqpath)+" (mac=[blank])"); return; }
		string mac=string(pmac);
		RegData rd=registered[mac];
		if(debug) { syslog(LOG_ERR,"OutHttpRequest:/remote?mac=%s: publicip=%s,internal ip=%s",pmac,rd.publicip.c_str(),rd.myip.c_str()); }
		if(rd.publicip=="" || rd.sock==-1) { registered.erase(mac); Error(string(reqpath)+" #1 (mac="+mac+")"); return; }
		char reqbuf[1024];
		parms.erase("mac");
		// ------------- SEND REQUEST -------------
		string headersStr="";
		for(size_type i = 0; i < headers.size(); i++) { headersStr+=(headers.at(i)+"\r"); }
		snprintf(reqbuf,sizeof(reqbuf),"%s %s%s HTTP/1.1\r\n%s\r\n%s",method,newreqpath,urlParms().c_str(),headersStr.c_str(),bodyline);
		if(!send_data(rd.sock,reqbuf,strlen(reqbuf))) { registered.erase(mac); Error(string(reqpath)+" #2 (mac="+mac+")"); return; }
		int ofs=0;
		int remainder=0;
		ssize_t retval=0;
		// Following loop proxies data from rd.sock to sd_accept (in a read/write loop)
		if(debug) { syslog(LOG_ERR,"Sent %d bytes: %s",strlen(reqbuf),reqbuf); }
		int recdBytes=0;
		do {
			retval=recv(rd.sock, reqbuf, sizeof(reqbuf),0);
			if(retval<1) { break; }
			ofs=0; remainder=retval; recdBytes+=retval;
			do {
				retval=send(sd_accept, &reqbuf[ofs], remainder, 0);
				if(retval == -1) { break; }
				remainder-=retval; ofs+=retval;
			} while(remainder > 0);
			if(retval == -1) {
				reqbuf[sizeof(reqbuf)-1]=0;
				syslog(LOG_ERR,"OutHttpRequest: ERROR sending message: %s",reqbuf);
				if(errno==ENOTSOCK) { syslog(LOG_ERR,"OutHttpRequest::send_data: FATAL ERROR: %s",strerror(errno)); }
				break;
			}
		} while(retval>0);
		if(recdBytes==0) { registered.erase(mac); Error(string(reqpath)+" #3 (mac="+mac+")"); return; }
		closeSocket(sd_accept);
}
void OutHttpRequest::Execute(void* arg) {
MARK
	//syslog(LOG_ERR,"OutHttpRequest:Execute:Handling http request...");
	int actlen;
	int debug=getInt("debug",0);
	char sessionKey[64]={0};
MARK
	if(readline(reqbuf,sizeof(reqbuf),actlen)) {
		if(!reqbuf[0]) { return; } // No request sent
		if(debug) { syslog(LOG_ERR,"HTTP REQUEST: %s.",reqbuf); }
		method=reqbuf;
		reqpath=strchr(reqbuf,' ');
		if(reqpath) { reqpath[0]=0; reqpath++; }
		httpver=strchr(reqpath,' ');
		if(httpver) { httpver[0]=0; httpver++; }
		char *pp=strchr(reqpath,'?');
		if(pp) {
			*pp=0;
			processParms(&pp[1],debug==1);
		}
	}
	else { return; }
MARK
	bool isAuthenticated=false;
	char headline[1024];
	int clen=-1;
	char mySessionName[32];
	snprintf(mySessionName,sizeof(mySessionName),"Session%s",macaddr);
	if(debug) { syslog(LOG_ERR,"mySessionName=%s",mySessionName); }
	vector<string> headers;
MARK
	while(readline(headline,sizeof(headline),actlen)) {
MARK
		if(!headline[0]) { break; } // Terminate at first blank header
			headers.push_back(string(headline));
		// Scan for e.g. Cookie: Session=OPAQUE_STRING1; NAME2=OPAQUE_STRING2
		/*	Host: 192.168.1.105:1042
				Connection: keep-alive
				X-Purpose: : preview
				Referer: http://192.168.1.105:1042/
				Content-Length: 23
				Cache-Control: max-age=0
				Origin: http://192.168.1.105:1042
				Content-Type: application/x-www-form-urlencoded
				*/
		//syslog(LOG_ERR,"Header: %s",headline);
		char *key=headline;
		char *val=strchr(key,':');
		if(!val) { val=(char *)""; }
		else { *val=0; val=&val[2]; }
		if(debug) { syslog(LOG_ERR,"key=%s,val=%s",key,val); }
		if(strcmp(key,"Cookie")==0) {
			char *pp=val;
			while(pp && *pp) {
				char *key2=pp;
				char *qq=strchr(key2,';');
				if(qq) { *qq=0; pp=&qq[2]; }
				else { pp=0; }
				char *val2=strchr(key2,'=');
				if(!val2) { val2=(char *)""; }
				else { *val2=0; val2=&val2[1]; }
				if(debug) { syslog(LOG_ERR,"key2=%s,val2=%s",key2,val2); }
				if(strcmp(key2,mySessionName)==0) { // Watch for my specific Session key (may be multiple cookies)
					strncpy(sessionKey,val2,sizeof(sessionKey));
				}
				//parms[key]=val;
			}
		}
		else if(strcasecmp(key,"Content-length")==0) {
			clen=atoi(val);
		}
	}
	// FIXME: Only handles Content-Type: application/x-www-form-urlencoded, and doesn't check it
	bodyline[0]=0;
MARK
	if(strcmp(method,"POST")==0 && clen>0) {		
MARK
		int ofs=0,len=min(sizeof(bodyline),clen),retval;
		do {
			retval=read(sd_accept,&bodyline[ofs],len);
			if(retval==-1) { break; }
			ofs+=retval; len-=retval;
		} while(len>0);
		bodyline[ofs]=0;
		if(debug) { syslog(LOG_ERR,"bodyline=%s",bodyline); }
		strncpy(saveBodyline,bodyline,sizeof(saveBodyline));
		saveBodyline[ofs]=0;
		processParms(saveBodyline,debug==1);
	}
	bool isDown=(strncmp(reqpath,"/download/",10)==0);
	bool isWeb=(strncmp(reqpath,"/web/",5)==0 || strcmp(reqpath,"/favicon.ico")==0);
	bool isReg=(strcmp(reqpath,"/register")==0);
	bool isStream=(strcmp(reqpath,"/stream")==0);
MARK
	if(!isDown && !isWeb && !isReg && !isStream) {
		isAuthenticated=checkLogin(sessionKey,parms["password"],parms["hash"],debug==1);
		if(!isAuthenticated) {
			// Set a session cookie, and expect that cookie to return, along with valid credentials
			// If invalid credentials are sent, checkLogin will fail and we'll be here again... 
			DoLogin((const char *)macaddr,reqpath); return;
		}
		else { parms["password"]=NULL; }
	}
	if(debug) { syslog(LOG_ERR,"method=%s,reqpath=%s,httpver=%s",method,reqpath,httpver); }
MARK
	if(strcmp(reqpath,"/")==0) {
MARK
		if(macAddr!="") { parms["mac"]=macAddr.c_str(); }
		snprintf(getpath,sizeof(getpath),"%s%s",reqpath,suffix.c_str());
		reqpath=getpath;
		//if(suffix=="") {
			Execute();
		//}
		//else { Error(string(reqpath)+" MERELY TESTING"); return; }
MARK
	}
	else if(strcmp(reqpath,"/execute")==0) {
MARK
		const char *pcmd=parms["cmd"];
		const char *pqueue=parms["queue"];
		if(pcmd==NULL) { Error(string(reqpath)+" (missing cmd or queue)"); return; }
		if(debug) { syslog(LOG_ERR,"OutHttpRequest:/execute?cmd=%s",pcmd); }
		string cmd=string(pcmd);
		string::size_type ofs=0;
		do {
MARK
			string::size_type loc = cmd.find("%",ofs);
			if(loc != string::npos) {
				string hex1=cmd.substr(loc+1,1);
				string hex2=cmd.substr(loc+2,1);
				int ch=atoi(hex1.c_str())*16+atoi(hex2.c_str());
				char chs[2];
				chs[0]=ch;
				chs[1]=0;
				cmd=cmd.substr(0,loc)+string(chs)+cmd.substr(loc+3);
				//syslog(LOG_ERR,"cmd: %s",cmd.c_str());
				ofs=loc+1;
			}
			else { break; }
		} while(true);
MARK
		char cmdbuf[1024];
		char qpath[32];
		snprintf(qpath,sizeof(qpath),"/tmp/out-%s.txt",pqueue);
		snprintf(cmdbuf,sizeof(cmdbuf),"%s >>%s 2>&1 &",cmd.c_str(),qpath);
		if(debug) { syslog(LOG_ERR,"OutHttpRequest:system(%s)",cmdbuf); }
		if(system(cmdbuf)==-1) { syslog(LOG_ERR,"performAction:system:%s failed: %s",cmdbuf,strerror(errno)); }
		sleep(2);
		if(debug) { syslog(LOG_ERR,"OutHttpRequest:SendFile(%s)",qpath); }
		SendFile(string(qpath)); // Closes the connection
MARK
	}
	else if(strcmp(reqpath,"/register")==0) {
MARK
		const char *pmac=parms["mac"];
		const char *pmyip=parms["myip"];
		const char *ppublicip=parms["publicip"];
		if(pmac==NULL || pmyip==NULL || ppublicip==NULL) { Error(string(reqpath)+" (missing mac,myip, or publicip)"); return; }
		string mac=string(pmac);
		string smyip=string(pmyip);
		string spublicip=string(ppublicip);
		if(debug) { syslog(LOG_ERR,"OutHttpRequest:/register?mac=%s&myip=%s&publicip=%s",pmac,pmyip,ppublicip); }
		time_t reqtime=time(NULL);
		string village=geoLocate(spublicip);
		registered[mac]=(RegData){spublicip,smyip,reqtime,village,sd_accept};
		// NOTE: Leaves connection open for later /remote requests
MARK
	}
	else if(strcmp(reqpath,"/remote")==0 || strncmp(reqpath,"/remote/",8)==0) {
MARK
		DoRemote(headers,debug);
MARK
	}
	else if(isStream) {
MARK
		Stream(); // Never closes connection
MARK
	}
	else if(isDown || isWeb) {
MARK
		if(strstr(reqpath,"/..")) { Error(string(reqpath)+": Navigation to parent directory not allowed."); return; }
		if(isWeb) {
			const char *pp=reqpath;
			if(strncmp(reqpath,"/web",4)==0) { pp=&pp[4]; }
			SendFile("web"+string(pp));
		}
		else {
			SendFile(cacheBasedir+string(reqpath));
		}
MARK
	}
	else { Error(string(reqpath)); }
MARKEND
}
void OutHttpRequest::SendScreen() {
	char headers[1024];
	//int clen=screenLen();
	int plaintext=parmInt("plaintext");
	string val=plaintext?screenText():screenVal();
	int clen=val.length();
	snprintf(headers,sizeof(headers),"HTTP/1.1 200 Ok\r\nContent-type: text/html\r\nContent-Length: %d\r"
	"Cache-Control: no-cache\r\nPragma: no-cache\r\nExpires: -1\r\n\r",clen);
	send_data(headers,strlen(headers));
	send_data((char *)val.c_str(),clen);
}
//--------------------------------------------------------
int OutHttpRequest::screenLen() {
	vector<string>::iterator cii;
	int slen=0;
	for(cii=screen.begin(); cii!=screen.end(); cii++) { slen+=((string)(*cii)).length(); }
	return slen;
}
//--------------------------------------------------------
// For attrOff, if curAttr was binary 0011 (3) and attr is binary 0010 (2), inversion of attr
// is binary 1101, producing 0011 AND 1101 = 0001 (Thus clearing the specified attribute).
// e.g. 00240600; CYAN=06,GREEN=02,RED=01,MAGENTA=05,BLUE=04,BOLD=200000,REVERSE=40000
void OutHttpRequest::attrOn(unsigned int attr) {
	unsigned int aHi=attr>>16;
	unsigned int aLo=attr&0xFFFF;
	if(aLo==0) { aLo=curAttr&0xFFFF; } // Don't change color values if no color was passed
	unsigned int curAHi=(curAttr>>16);
	curAHi|=aHi;
//	unsigned int ca=curAttr;
	curAHi<<=16;
	curAHi+=aLo;
	curAttr=curAHi;
	//syslog(LOG_ERR,"attrOn:curAttr was %08X,attr=%08X,aHi=%04X,aLo=%04X,curAttr=%08X",ca,attr,aHi,aLo,curAttr);
}
void OutHttpRequest::attrOff(unsigned int attr) {
	unsigned int aHi=attr>>16;
	unsigned int aLo=attr&0xFFFF;
	if(aLo==0) { aLo=curAttr&0xFFFF; } // Don't change color values if no color was passed
	unsigned int curAHi=(curAttr>>16);
	curAHi&=(~aHi);
//	unsigned int ca=curAttr;
//	curAttr=(curAHi<<16);
	curAHi<<=16;
	curAHi+=aLo;
	curAttr=curAHi;	
	//curAttr+=aLo;
	//syslog(LOG_ERR,"attrOff:curAttr was %08X,attr=%08X,aHi=%04X,aLo=%04X,curAttr=%08X",ca,attr,aHi,aLo,curAttr);
}
// #define attrOff(attr)			curAttr&=(~attr);syslog(LOG_ERR,"attrOff:attr=%08X",attr)
// #define attrOn(attr)				curAttr|=attr;syslog(LOG_ERR,"attrOn:attr=%08X",attr)
/*#define _A_BOLD					(0x0000001)
#define _A_REVERSE			(0x0000002)*/
unsigned int colors[]={COLOR_CYAN,COLOR_GREEN,COLOR_RED,COLOR_MAGENTA,COLOR_BLUE};
string colorNames[]={"cyan","green","red","magenta","blue"};
//char val[256];
string OutHttpRequest::AttrToHtml(unsigned int attr) {
	bool isReverse=(attr&A_REVERSE);
	bool isBold=(attr&A_BOLD);
	unsigned short f=(attr&0xFF00)>>8;
	// NOTE: Unused: unsigned short b=attr&0xFF;
/*	syslog(LOG_ERR,"attr=%08X,f=%02X,b=%02X,CYAN=%02X,GREEN=%02X,RED=%02X,MAGENTA=%02X,BLUE=%02d,BOLD=%02X,REVERSE=%02X",
				attr,f,b,
				COLOR_CYAN,COLOR_GREEN,COLOR_RED,COLOR_MAGENTA,COLOR_BLUE,A_BOLD,A_REVERSE);*/
	string ins="";
	string colorName="";
	for(size_type xa=0; xa<sizeof(colors)/sizeof(colors[0]); xa++) {
		if(f==colors[xa]) { colorName=colorNames[xa]; }
	}
	if(colorName!="") {
		if(isBold) { colorName="bold_"+colorName; }
		if(isReverse) { colorName="reverse_"+colorName; }
		ins+=string("</div><divXclass=\""+colorName+"\">");
	}
/*	else { 
		snprintf(val,sizeof(val),"attr=%08X",attr);
		ins+=string(val);
	}*/
	return ins;
}
string OutHttpRequest::urlParms() { // Returns string of form ?var=value&var2=value2
	string parmVal="";
	int count=0;
	for(map<const char*, const char*, strCmp>::iterator iter = parms.begin(); iter != parms.end(); ++iter,count++) {
		const char *key=(*iter).first;
		const char *val=(*iter).second;
		//syslog(LOG_ERR,"key=%s,val=%s",key,val);
		if(count==0) { parmVal="?"; }
		else { parmVal+="&"; }
		if(!val) { val=""; }
		parmVal+=string(key)+"="+string(val);
	}
	return parmVal;
}
string OutHttpRequest::screenVal() {
	vector<string>::iterator cii;
	string val="<html><head><link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"/favicon.ico\">";
	const char *ref=parms["refresh"];
	int refresh=ref?atoi(ref):0;
	if(refresh>0) { val+=string("<meta http-equiv=\"refresh\" content=\""+string(ref)+";url=/"+suffix+urlParms()+"\" />"); }
	val+=string("<style type=\"text/css\">"
		"  BODY     { background-color: B2B2B2; }"
		"  DIV      { display: inline; font-family: monospace; font-size: 9pt; margin: 0 0 0 0;padding: 0 0 0 0; }" // line-height: 0; float: left; 
		"  A        { text-decoration: none; }"
		"  .cyan    { background-color: B2B2B2; color:#18B2B2; }"
		"  .green   { background-color: B2B2B2; color:#18B218; }"
		"  .red     { background-color: B2B2B2; color:#B21818; }"
		"  .magenta { background-color: B2B2B2; color:#B218B2; }"
		"  .blue    { background-color: B2B2B2; color:#1818B2; }"
		"  .reverse_cyan    { background-color:#18B2B2; color:#FFFFFF; }"
		"  .reverse_green   { background-color:#18B218; color:#FFFFFF; }"
		"  .reverse_red     { background-color:#B21818; color:#FFFFFF; }"
		"  .reverse_magenta { background-color:#B218B2; color:#FFFFFF; }"
		"  .reverse_blue    { background-color:#1818B2; color:#FFFFFF; }"
		"  .reverse_bold_cyan    { background-color:#18B2B2; color:#FFFFFF; font-weight: bold; }"
		"  .reverse_bold_green   { background-color:#18B218; color:#FFFFFF; font-weight: bold; }"
		"  .reverse_bold_red     { background-color:#B21818; color:#FFFFFF; font-weight: bold; }"
		"  .reverse_bold_magenta { background-color:#B218B2; color:#FFFFFF; font-weight: bold; }"
		"  .reverse_bold_blue    { background-color:#1818B2; color:#FFFFFF; font-weight: bold; }"
		"</style>"
		"<title>Play3ABN - By Warren E. Downs</title>\n</head><body>");
		//body {background-image:url("images/back40.gif");}
	//val.reserve(65536);
	int yy=0;
	for(cii=screen.begin(); cii!=screen.end(); cii++) {
		string origline=((string)(*cii));
		int len=origline.length();
		string url="";
		if(yy>=scheduleRow) {
			if(len>=8) {
				string myurl="download/"+origline.substr(8,origline.find_last_not_of(" ")-7);
				string path=cacheBasedir+"/"+myurl;
				struct stat statbuf;
				if(stat(path.c_str(), &statbuf)!=-1 && statbuf.st_size>0) { url=myurl; }
				//else { syslog(LOG_ERR,"Missing: %s.",path.c_str()); }
			}
		}
		string baseline=origline;
		string pre="<div>";
		string post="</div>";
		/** INSERT Attributes into line (as HTML DIV tags **/
		int ofs=0;
		unsigned int cur=0;
		for(size_type xx=0; xx<attrs[yy].size(); xx++) {
			unsigned int attr=attrs[yy][xx];
			if(attr!=cur) {
				string ins=AttrToHtml(attr);
				//syslog(LOG_ERR,"baseline.length=%d,yy=%d,xx=%d.insert(%d,%s)",baseline.length(),yy,xx,xx+ofs,ins.c_str());
				baseline.insert(xx+ofs,ins);
				ofs+=ins.length();
				cur=attr;
			}
		}
		string::size_type loc=0;
		// Replace spaces
		do {
			loc = baseline.find(" ", loc);
			if(loc == string::npos) { break; }
			baseline.erase(loc,1);
			baseline.insert(loc,"&nbsp;");
		} while(true);
		// Fix DIVs
		loc=0;
		do {
			loc = baseline.find("divXclass", loc);
			if(loc == string::npos) { break; }
			baseline.replace(loc,9,"div class");
		} while(true);
		// Generate output line
		string line=pre+baseline+post;
		if(line.substr(0,11)=="<div></div>") { line=line.substr(11); }
		if(line.substr(0,12)=="<div> </div>") { line=line.substr(12); }
		len=line.length();
		if(len>=11 && line.substr(len-11,11)=="<div></div>") { line=line.substr(0,len-11); }
		char row[4];
		snprintf(row,sizeof(row),"%02d",yy);
		parms["row"]=row;
		string sTip=tips[yy];
		string sUrl=urls[yy];
		char msg[256];
		if(sTip=="") {
			if(detailRow>-1) { snprintf(msg,sizeof(msg),"Row %02d, Detail Row %02d",yy,detailRow); }
			else {snprintf(msg,sizeof(msg),"Row %02d",yy); }
			sTip=string(msg);
		}
		if(sUrl=="") {
			if(url=="") { url=urlParms(); }
			else { url=url+urlParms(); }
			sUrl="/"+suffix+(suffix!=""?"/":"")+url;
		}
		line="<a title=\""+sTip+"\" href=\""+sUrl+"\">"+line+"</a><br />";
		parms["row"]=NULL;
		val+=line;
		yy++;
	}
	val+="</body><!-- "+string((const char *)macaddr)+" --></html>";
	return val;
}
//--------------------------------------------------------
string OutHttpRequest::screenText() {
	vector<string>::iterator cii;
	string val="<html><head><link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"/favicon.ico\">";
	const char *ref=parms["refresh"];
	int refresh=ref?atoi(ref):0;
	if(refresh>0) { val+=string("<meta http-equiv=\"refresh\" content=\""+string(ref)+";url=/"+suffix+urlParms()+"\" />"); }
	val+=string("<style type=\"text/css\">"
		"  BODY     { background-color: B2B2B2; }"
		"  A        { text-decoration: none; }"
		"</style>"
		"<title>Play3ABN - By Warren E. Downs</title>\n</head><body><pre>");
	int yy=0;
	int realplain=parmInt("realplain");
	for(cii=screen.begin(); cii!=screen.end(); cii++) {
		string origline=((string)(*cii));
		string line="";
		if(!realplain) {
			char row[4];
			snprintf(row,sizeof(row),"%02d",yy);
			parms["row"]=row;
			string sUrl="/"+suffix+(suffix!=""?"/":"")+urlParms();
			string line="<a href=\""+sUrl+"\">"+origline+"</a>";
			parms["row"]=NULL;
			val+=line;
		}
		else { val+=origline+""; }
		yy++;
	}
	val+="</pre></body><!-- "+string((const char *)macaddr)+" --></html>";
	return val;
}
//--------------------------------------------------------

void OutHttpRequest::screenWrite(size_type yy, size_type xx, const char *msg) {
	screen.resize(yy+1,"");
	AttrArray aa;
	attrs.resize(yy+1,aa);
	string line=string(msg);
	int llen=line.length();
	if(xx < screen[yy].length()) { screen[yy].replace(xx, llen, line); }
	else { screen[yy].resize(xx,' '); screen[yy]+=line; }
	attrs[yy].resize(xx+llen+1,0);
	for(size_type xa=xx; xa<xx+llen; xa++) { attrs[yy][xa]=curAttr; }
	tips.resize(yy+1,"");
	urls.resize(yy+1,"");
	tips[yy]="";
	urls[yy]="";
}

int OutHttpRequest::screenWrite(size_type yy, size_type xx, const char *msg, string tip, string url) {
	screenWrite(yy, xx, msg);
	if(tip!="") { tips[yy]=tip; }
	if(url!="") { urls[yy]=url; }
	yy++;
	return yy;
}
// int OutHttpRequest::Update() {
// 	// Streaming file information
// 	ogg_int64_t ofs=ov_raw_tell(&vf);
//   snprintf(out,sizeof(out),"offset=%lld,reqpath=%s,oggFname=%s",ofs,reqpath,oggFname);
// 	fixline(out,sizeof(out));
// 	OutputRow(); row++;
// }

//--------------------------------------------------------
// Get data into buffer, up to given size.  Actual length
// is returned in len.  Return false if connection closes.
int OutHttpRequest::get_data(char *buffer, int size, int& len) {
	int				retval,fd=sd_accept;

	if(fd == -1) return -1;  /* Don't work with bad sockets */
	retval=read(fd,buffer,size);
	if(retval < 1) {   /* -1 = error, 0 = graceful close */
	if(retval < 0) { syslog(LOG_ERR,"OutHttpRequest::get_data: ERROR receiving message from %d: %s",fd,strerror(errno)); return -1; }
			closeSocket(fd);
			len=0;
			return 0;
			}
	if(len) { len=retval; }
	return 1;
}
//--------------------------------------------------------
/** Read line into buffer, up to given size.  Actual length
    is returned in len.  Return false if connection closes
    or provided buffer is too small.
**/
bool OutHttpRequest::readline(char *buffer, int size, int& len) {
	// NOTE: Unused: int fd=sd_accept;
	char *ch=buffer;
	int gotchr,gotCR=false;
	int count=0;

	if(size<1) { return false; }
	do {
		int len=0;
		gotchr=get_data(ch,1,len);
		if(gotchr<0) { break; }
		if(*ch=='\r') { *ch=0; gotCR=true; }
		if(*ch=='\n') { *ch=0; break; }
		if(gotchr && !gotCR) { count++; ch++; }
	} while(gotchr>0);
	*ch=0;
	len=count;
	if(gotchr>=0) { return true; }
	return false;
}
//--------------------------------------------------------
// Send given data buffer, of given length (len)
bool OutHttpRequest::send_data(char *buffer, int len) {
	return send_data(sd_accept,buffer,len);
}
bool OutHttpRequest::send_data(int fd, char *buffer, int len) {
  int ofs=0;
	int remainder=len; /* Offset into outgoing data to send */
	int	retval;

	if(fd == -1) { return false; }  /* Don't work with bad sockets */
	do {
		retval=send(fd, buffer+ofs, remainder, 0);
		if(retval == -1) { break; }
		remainder-=retval; ofs+=retval;
	} while(remainder > 0);
	if(retval == -1) {
		syslog(LOG_ERR,"OutHttpRequest:send_data: ERROR sending message.");
		if(errno==ENOTSOCK) {
			syslog(LOG_ERR,"OutHttpRequest::send_data: FATAL ERROR: %s",strerror(errno));
			exit(1);
		}
		return false;
	}
	return true;
}
//--------------------------------------------------------
void OutHttpRequest::Shutdown() { Stop(); }

#undef getmaxyx
#define getmaxyx save_getmaxyx
