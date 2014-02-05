#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <pulse/pulseaudio.h>
#include <pulse/sample.h>

using namespace std;

#define BUFSIZE 1024

int main(int argc, char *argv[]) {
  /* The Sample format to use */
  /*static const pa_sample_spec ss = {
  .format = PA_SAMPLE_S16LE,
  .rate = 44100,
  .channels = 2
  };*/
  static pa_sample_spec ss;//= {
  ss.format = PA_SAMPLE_S16LE;
  ss.rate = 16000;
  ss.channels = 1;// ss;

  pa_simple *sa = NULL;
  int ret = 1;
  int error;

  /* replace STDIN with the specified file if needed */
  if (argc >= 1) {
    int fd;
    //argv[1]="abc.wav";

    if ((fd = open(argv[1], O_RDONLY)) < 0) {
      syslog(LOG_ERR, __FILE__": open() failed: %s", strerror(errno));
      goto finish;
    }

    if (dup2(fd, STDIN_FILENO) < 0) {
      syslog(LOG_ERR, __FILE__": dup2() failed: %s", strerror(errno));
      goto finish;
    }
    close(fd);
  }

  /* Create a new playback stream */
  if (!(sa = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "play3abn", &ss, NULL, NULL, &error))) {
    syslog(LOG_ERR, __FILE__": pa_simple_new() failed: %s", pa_strerror(error));
    goto finish;
  }

  for (;;) {
    uint8_t buf[BUFSIZE];
    ssize_t r;
    #if 0
      pa_usec_t latency;

      if ((latency = pa_simple_get_latency(s, &error)) == (pa_usec_t) -1) {
      syslog(LOG_ERR, __FILE__": pa_simple_get_latency() failed: %s", pa_strerror(error));
      goto finish;
      }

      syslog(LOG_ERR, "%0.0f usec \r", (float)latency);
    #endif

    /* Read some data ... */
    if ((r = read(STDIN_FILENO, buf, sizeof(buf))) <= 0) {
      if (r == 0) /* EOF */ break;
      syslog(LOG_ERR, __FILE__": read() failed: %s", strerror(errno));
      goto finish;
    }

    /* ... and play it */
    if (pa_simple_write(sa, buf, (size_t) r, &error) < 0) {
      syslog(LOG_ERR, __FILE__": pa_simple_write() failed: %s", pa_strerror(error));
      goto finish;
    }
    //s1=(uint8_t)*s;
  }

  /* Make sure that every single sample was played */
  if (pa_simple_drain(sa, &error) < 0) {
    syslog(LOG_ERR, __FILE__": pa_simple_drain() failed: %s", pa_strerror(error));
    goto finish;
  }

  ret = 0;

  finish:

  if (sa)
  pa_simple_free(sa);
  //return ret;
  return 0;
}
