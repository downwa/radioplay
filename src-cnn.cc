#include "src-cnn.hh"

string SrcCNN::newsweatherBasedir="/tmp/play3abn/cache/newsweather";

SrcCNN::SrcCNN() {
	util=new Util("src-cnn");
	rssOfs=0;
	rssUrl=NULL;
	weatherurl[0]=0;
	errbuf[0]=0;
	lastChosen=0;
	prevHourEnd=0;
	Start(NULL);
}

void SrcCNN::Execute(void* arg) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
	const char *newsflag="/tmp/play3abn/tmp/.news.flag";
	syslog(LOG_ERR,"SrcCNN:Execute:Maintaining list...");
	while(!Util::checkStop()) {
		if(!Util::publicip[0] || !util->isMounted() || newsweatherBasedir=="") { util->longsleep(60); continue; }
		struct stat statbuf;
		if(stat(newsflag,&statbuf)!=-1) { /** Update the news when notified (if possible) **/
			remove(newsflag);
			hourlyWeather();
			hourlyNews();
		}		
		util->longsleep(60);
	}
	syslog(LOG_ERR,"SrcCNN:Execute:Done.");
}

int SrcCNN::hourlyNews() {
	//syslog(LOG_ERR,"hourlyNews");
  CURL *hcurl = curl_easy_init();
  if(!hcurl) { syslog(LOG_ERR,"curl_easy_init failed"); return -1; }
  rssOfs=0;
	rssUrl=NULL;
  newsrssbuf[0]=0;
	curl_easy_setopt(hcurl, CURLOPT_URL,"http://rss.cnn.com/services/podcasting/newscast/rss.xml");
	curl_easy_setopt(hcurl, CURLOPT_WRITEFUNCTION, news_fwrite); // write callback
	curl_easy_setopt(hcurl, CURLOPT_ERRORBUFFER, errbuf);
	CURLcode res=curl_easy_perform(hcurl);
	if(res != CURLE_OK && strstr(errbuf,"Failed writing body")==NULL) { syslog(LOG_ERR,"curl failed: %s",errbuf); }
	curl_easy_cleanup(hcurl);
	if(rssUrl && util->isMounted()) {
		const char *ndir=newsweatherBasedir.c_str();
		mkdir(ndir,0755);
		const char *base=strrchr(rssUrl,'/');
		if(base) { base++; }
		else { base=rssUrl; }
		char path[1024];
		snprintf(path,sizeof(path),"%s/%s",ndir,base);
		util->enqueue(Time()+3600,string(path), string(rssUrl), 0, 120);
	}
	return 0;
}
char SrcCNN::newsrssbuf[10240]={0};
int SrcCNN::rssOfs=0;
char *SrcCNN::rssUrl=NULL;
size_t SrcCNN::news_fwrite(void *buffer, size_t size, size_t nmemb, void *stream) {
	size_t amt=size*nmemb;
	size_t left=sizeof(newsrssbuf)-rssOfs;
	if(amt>=left) { amt=left-1; }
	memcpy(&newsrssbuf[rssOfs],buffer,amt);
	rssOfs+=amt;
	newsrssbuf[rssOfs]=0;
	//syslog(LOG_ERR,"fetchNews: rss: %d bytes received\r",rssOfs);
	// grep "<media:content url=" rss.xml | head -n 1 | sed -e 's/<media:content url="//g' -e 's/".*//g'
	char *url=strstr(newsrssbuf,"<media:content url=");
	if(url) {
		char *fs=strstr(url,"fileSize=");
		//int fileSize=0;
		if(fs) {
			fs=strchr(url,'"');
			//fileSize=atoi(fs?&fs[1]:"0");
		}
		char *pp=strchr(url,'"');
		if(pp) { url=pp+1; }
		pp=strchr(url,'"');
		if(pp) { *pp=0; rssUrl=url; }
		//syslog(LOG_ERR,"fetchNews: rss: fileSize=%d,rssUrl=%s",fileSize,rssUrl);
		return 0;
	}
	return amt;
}
void SrcCNN::hourlyWeather() {
MARK
	char yfile[1024];
	snprintf(yfile,sizeof(yfile),"yahoo-%s",Util::publicip);
	// If we don't have the Yahoo URL yet for our IP
	util->get(yfile, (char *)weatherurl, sizeof(weatherurl));
MARK
	if(!weatherurl[0]) {
		FILE *ts=util->netcat("www.ip2location.com",80);
		if(ts) {
			fprintf(ts,"GET /%s HTTP/1.0\r\n\r",Util::publicip);
MARK
			char line[1024];
			bool readyForLocation=false;
			while(ts && !feof(ts) && fgets(line,sizeof(line),ts)) {
MARK
				//syslog(LOG_ERR,"hourlyNewsWeather: line=%s",line);
				if(line[0]=='\r' && line[1]=='\n') { readyForLocation=true; }
				else if(readyForLocation && line[0]) {
					char *wurl=strstr(line,"http://weather.yahoo.com");
					if(wurl) {
						char *pp=strchr(wurl,'"');
						if(pp) { *pp=0; }
						strncpy((char *)weatherurl,wurl,sizeof(weatherurl));
						syslog(LOG_ERR,"weatherurl=%s",weatherurl);
						util->set(yfile,(char *)weatherurl);
						break;
					}
				}
			}
			fclose(ts);
MARK
		}
	}
MARK
	char cmd[1024];
	snprintf(cmd,sizeof(cmd),"./weather \"%s\" 2>logs/weather.err.txt",weatherurl);
	//syslog(LOG_ERR,"hourlyWeather: %s",cmd);
MARKEND
}
