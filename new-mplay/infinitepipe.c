#include <stdio.h>
#include <stdlib.h>

#define PLAY "/tmp/play3abn/play.fifo"

int main(int argc, char **argv) {
	while(true) {
		FILE *fpin=fopen(PLAY,"rb");
		while(!feof(fpin) && !ferror(fpin)) {
			unsigned char buf[4000];
			size_t ilen=fread(buf, sizeof(buf), 1, fpin);
			if(ilen<=0) { break; }
       			size_t olen=fwrite(buf, sizeof(buf), 1, stdout); fflush(stdout);
			if(olen<ilen) { fprintf(stderr,"Short write: %d\n",olen); }
		}
		fclose(fpin);
	}	
}
