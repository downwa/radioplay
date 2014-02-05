#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
  char cmd[1024];
  snprintf(cmd,sizeof(cmd),"%s.sh",argv[0]);
  //snprintf(cmd,sizeof(cmd),"%s.sh",argv[0]);
  fprintf(stderr,"Running %s as %s\n",cmd,getenv("SUDO_USER"));
  if(execlp("su","mplay",getenv("SUDO_USER"),"-c",cmd,NULL)<0) { perror(cmd); }
}

