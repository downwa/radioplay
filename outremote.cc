
class OutRemote: Thread {
	void Execute(void* arg);
	bool send_data(int fd, char *buffer, int len);
	public:
	OutRemote();
};

//--------------------------------------------------------
OutRemote::OutRemote() {
	Start(NULL);
}
//--------------------------------------------------------
void OutRemote::Execute(void* arg) {
	int ishub=getInt("ishub",0);
	syslog(LOG_ERR,"OutRemote:Execute: Waiting for igloowareip to be set...");
	while(true || igloowareip[0]==0) { sleep(1); } // FIXME: Wait forever because this seems to be exhausting resources
	if(ishub && strcmp((const char *)publicip,(const char *)igloowareip)==0) {
		syslog(LOG_ERR,"OutRemote:Execute:We are the Hub Server (%s)",igloowareip);
		return;
	}
	syslog(LOG_ERR,"OutRemote:Execute:Handling remote requests from Hub Server (%s) via publicip=%s.",igloowareip,publicip);
	while(!doStop) {
		int sock=netcatSocket((const char *)igloowareip,LISTEN_PORT); // Get a connection
		if(sock==-1) {
			syslog(LOG_ERR,"OutRemote:Execute:netcatSocket: %s (connecting to %s:%d).  Waiting...",strerror(errno),igloowareip,LISTEN_PORT);
			longsleep("outremote",60); continue;
		}
		char reqbuf[256];
		snprintf(reqbuf,sizeof(reqbuf),"GET /register?mac=%s&myip=%s&publicip=%s HTTP/1.0\r\n\r",macaddr,myip,publicip);
		send_data(sock,reqbuf,strlen(reqbuf));
		while(!doStop) {
			// Wait for data to arrive (an incoming request from the Hub Server)
			fd_set rfds;
			struct timeval tv;
			int retval;
			FD_ZERO(&rfds);
			FD_SET(sock, &rfds);
			tv.tv_sec=600; // Wait up to 600 seconds
			tv.tv_usec=0;
			retval = select(sock+1, &rfds, NULL, NULL, &tv);
			if(retval == -1) { syslog(LOG_ERR,"OutRemote:Execute:select error: %s",strerror(errno)); sleep(1); break; }
			else if(!retval) { break; } // timeout: Need to get a new socket connection to server
			if(FD_ISSET(sock, &rfds)) {
				syslog(LOG_ERR,"OutRemote: Handling request from hub server on %d...",sock);
				RegDataMap registered;
				try {
					new OutHttpRequest(sock,registered,"remote",string((const char *)macaddr)); // Handle the request for this socket
				}
				catch (bad_alloc&)
				{
					syslog(LOG_ERR,"outremote:Error allocating memory to handle request from hub server.");
					sleep(1);
					break;
				} // FIXME: After OutHttpRequest:send_data: ERROR sending message. we get lots of requests!
				//sleep(1); // FIXME: Slow down the requests!  Getting hundreds a second somehow!
				break; // FIXME: Try to reuse connection but need to know if it's a valid one!
			}
			else { syslog(LOG_ERR,"OutRemote:select returned but not for us."); break; }
		}
		if(sock!=-1) { OutHttpRequest::closeASocket(sock); sock=-1; }
	}
}
//--------------------------------------------------------
// Send given data buffer, of given length (len), to the specified file descriptor (fd)
bool OutRemote::send_data(int fd, char *buffer, int len) {
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
		syslog(LOG_ERR,"OutHttpRequest: ERROR sending message: %s",buffer);
		if(errno==ENOTSOCK) {
			syslog(LOG_ERR,"OutHttpRequest::send_data: FATAL ERROR: %s",strerror(errno));
			exit(1);
		}
		return false;
	}
	return true;
}
