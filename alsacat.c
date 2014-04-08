#include <sys/soundcard.h>
#include <alsa/asoundlib.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

static const char *device = "default";                        /* playback device */
snd_output_t *output = NULL;

#define BUFSIZE 16000
int main(int argc, char*argv[]) {
    unsigned char buf[BUFSIZE];
    int ret = 1;
    /* replace STDIN with the specified file if needed */
    if (argc > 1) {
        int fd;
        if ((fd = open(argv[1], O_RDONLY)) < 0) {
            fprintf(stderr, __FILE__": open() failed: %s\n", strerror(errno));
            goto finish;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            fprintf(stderr, __FILE__": dup2() failed: %s\n", strerror(errno));
            goto finish;
        }
        close(fd);
    }



        int err;
        snd_pcm_t *handle;
        snd_pcm_sframes_t frames;
        if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
                printf("Playback open error: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
        }
        if ((err = snd_pcm_set_params(handle,
                                      SND_PCM_FORMAT_S16_LE,
                                      SND_PCM_ACCESS_RW_INTERLEAVED,
                                      1, // channels
                                      16000, // rate
                                      1, // allow resampling
                                      500000)) < 0) {   /* 0.5sec latency */
                printf("Playback open error: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
        }



    for (;;) {
        ssize_t r;
        // Read some data ...
        if ((r = read(STDIN_FILENO, buf, sizeof(buf))) <= 0) {
            if (r == 0) // EOF
                break;
            fprintf(stderr, __FILE__": read() failed: %s\n", strerror(errno));
            goto closeit;
        }
        // ... and play it 
        frames = snd_pcm_writei(handle, buf, sizeof(buf));
                if (frames < 0)
                        frames = snd_pcm_recover(handle, frames, 0);
                if (frames < 0) {
                        printf("snd_pcm_writei failed: %s\n", snd_strerror(err));
                        break;
                }
                if (frames > 0 && frames < (long)(sizeof(buf)/2))
                        printf("Short write (expected %li, wrote %li)\n", (long)sizeof(buf), frames);

	//fprintf(stderr,"Wrote %d bytes of %d\n",frames*2,sizeof(buf));
    }



    ret = 0;
closeit:
    snd_pcm_close(handle);
finish:
    return ret;
}




















