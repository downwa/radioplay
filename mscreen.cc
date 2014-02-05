#include "outscreen.hh"
#include "util.hh"
 
int main(int argc, char **argv) {
	Util* util=new Util("mscreen");
	syslog(LOG_ERR,"mscreen starting");
	new OutScreen(argc>1?argv[1]:"/dev/tty",util);
	//util->longsleep(3600);
	syslog(LOG_ERR,"mscreen terminating...");
	//sleep(60);
}
