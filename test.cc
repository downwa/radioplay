#include <string>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

int main(int argc, char **argv) {
	string src="ABC DEF";
        string ret;
        unsigned int i;
        for(i=0; i<src.length(); i++) {
                if(     src[i]==' ') { ret+="%20"; }
                else if(src[i]==':') { ret+="%58"; }
                else if(src[i]==';') { ret+="%59"; }
                else if(src[i]=='=') { ret+="%61"; }
                else { ret+=src[i]; }
        }
	printf("ret=%s***\n",ret.c_str());
}
