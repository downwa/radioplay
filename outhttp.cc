#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <map>

#include "outhttp.hh"

//--------------------------------------------------------
// Listen for incoming connections.  Returns listen socket descriptor
int OutHttp::do_listen() {
	struct sockaddr_in  addr_server;
	int                 sd_listen=-1;
	int                 option_on=1;
	struct linger		linger_on;

	linger_on.l_onoff  = 1;
	linger_on.l_linger = 30; /* Timeout value in seconds */

	/* Create a new socket to listen on */
	sd_listen = socket(PF_INET, SOCK_STREAM, 0);
	if(sd_listen == -1) { syslog(LOG_ERR,"OutHttp::do_listen: Error creating socket: %s",strerror(errno)); exit(1); }
			/* Set socket options */
	if(setsockopt(sd_listen,SOL_SOCKET,SO_REUSEADDR,(unsigned char *)&option_on,sizeof(option_on)) == -1) {
		syslog(LOG_ERR,"OutHttp::do_listen: ERROR setting socket options: %s",strerror(errno)); exit(2);
	}
	if(setsockopt(sd_listen,SOL_SOCKET,SO_LINGER,
		(unsigned char *)&linger_on,sizeof(linger_on)) == -1) {
		syslog(LOG_ERR,"OutHttp::do_listen: ERROR setting socket options(2): %s",strerror(errno)); exit(3);
	}
	// Set up server address details and bind them to socket sd_listen
	addr_server.sin_family=PF_INET;
	addr_server.sin_port=htons(listen_port);
	addr_server.sin_addr.s_addr = htonl(INADDR_ANY);
	//addr_server.sin_addr.s_addr = htonl(0x7F000001);	// localhost only
	int retval=bind(sd_listen,(struct sockaddr *)&addr_server,sizeof(addr_server));
	if(retval == -1) { syslog(LOG_ERR,"OutHttp::do_listen: ERROR binding socket: %s",strerror(errno)); exit(4); }
	// Listen with queue space for 5 incoming connections
	retval = listen(sd_listen,5);
	if(retval == -1) { syslog(LOG_ERR,"OutHttp::do_listen: ERROR listening on socket: %s",strerror(errno)); exit(5); }
	return sd_listen;
}
//--------------------------------------------------------
// Accept the incoming connection
int OutHttp::do_accept(int sd_listen) {
	struct sockaddr_in  incoming;
	socklen_t addr_size = sizeof(incoming);
	int	sd_accept=accept(sd_listen, (struct sockaddr *)&incoming, &addr_size);
  if(sd_accept == -1) { syslog(LOG_ERR,"OutHttp::do_accept: ERROR accepting connection: %s",strerror(errno)); sleep(1); }
	return sd_accept;
}
//--------------------------------------------------------
void OutHttp::Execute(void* arg) {
	fd_set rfds;
	int	retval,sd_listen;
	syslog(LOG_ERR,"OutHttp:Execute:Handling http requests on port %d...",listen_port);
  sd_listen=do_listen();
	while(!doStop && !doExit) {
		//syslog(LOG_ERR,"OutHttp:Execute:waiting...");
		FD_ZERO(&rfds);
		FD_SET(sd_listen, &rfds);
		retval=select(sd_listen+1, &rfds, NULL, NULL, NULL);
		if(retval==-1) { syslog(LOG_ERR,"OutHttp::Execute: ERROR in select: %s",strerror(errno)); break; }
		if(retval==0) { continue; }
		int sd_accept=do_accept(sd_listen);
		if(sd_accept==-1) { continue; }
		new OutHttpRequest(sd_accept,registered,"","");
	}
	syslog(LOG_ERR,"OutHttp::Execute:EXITED");
	Shutdown();
}

OutHttp::OutHttp() {
	/**/syslog(LOG_ERR,"OutHttp::OutHttp");
	listen_port=LISTEN_PORT;
	Start(NULL);
}

void OutHttp::Shutdown() { Stop(); }
OutHttp::~OutHttp() { Shutdown(); }
