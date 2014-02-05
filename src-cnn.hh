#ifndef __SRCCNNHH__
#define __SRCCNNHH__

#include <curl/curl.h>
// #include <curl/types.h>
#include <curl/easy.h>

#include "util.hh"
#include "thread.hh"

#include "utildefs.hh"

class SrcCNN: Thread {
	Util* util;
	static string newsweatherBasedir;

	static char newsrssbuf[10240];
	static int rssOfs;
	static char *rssUrl;

	char weatherurl[1024];
	char errbuf[CURL_ERROR_SIZE];
	time_t lastChosen;
	time_t prevHourEnd;

	void Execute(void* arg);
	int hourlyNews();
	static size_t news_fwrite(void *buffer, size_t size, size_t nmemb, void *stream);
	void hourlyWeather();

	public:
		SrcCNN();
};

#endif
