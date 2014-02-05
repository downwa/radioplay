#include "maindec.hh"
#include "util.hh"

int main(int argc, char **argv) {
	if(argc<2) { syslog(LOG_ERR,"Usage: ogglen [filename.ogg]"); exit(1); }
	Util* util=new Util("src-3abn");
	Decoder* decoder=new Decoder(NULL,util); // Used to check SecLen() for each file (and cache it)
  float seclen=decoder->SecLen(argv[1])+0.5;
	syslog(LOG_INFO,"%04d %s",(int)seclen,argv[1]);
}