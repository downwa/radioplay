#include "maindec.hh"
#include "util.hh"

int main(int argc, char **argv) {
	if(argc<2) { fprintf(stderr,"Usage: ogglen [filename.ogg]\n"); exit(1); }
	Util* util=new Util("src-3abn");
	Decoder* decoder=new Decoder(NULL,util); // Used to check SecLen() for each file (and cache it)
  float seclen=decoder->SecLen(argv[1])+0.5;
	printf("%04d %s\n",(int)seclen,argv[1]);
}
