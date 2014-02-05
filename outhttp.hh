#ifndef __OUTHTTPHH__
#define __OUTHTTPHH__

#include "regdata.hh"

class OutHttp: Thread {
	void Execute(void* arg);
	int listen_port;
	RegDataMap registered;
public:
	OutHttp();
	~OutHttp();
	int do_listen();
	void screenWrite(size_type yy, size_type xx, char *msg);
	int Update();
	int UpdatePlaying(int row);
	void UpdateRows(int startRow);
	void Shutdown();
	int do_accept(int sd_listen);
	RegDataMap listClients() {
		RegDataMap reg(registered.begin(),registered.end());
		return reg;
	}
};

#endif