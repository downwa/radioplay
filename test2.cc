#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

int main(int argc, char **argv) {
  syslog(LOG_ERR,"uid=%d,euid=%d",getuid(),geteuid());
  execv("/bin/bash",argv);
}

