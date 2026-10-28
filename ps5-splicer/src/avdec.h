#ifndef SPLICER_AVDEC_H
#define SPLICER_AVDEC_H

#include <stdint.h>

/* H.264 / HEVC. Hardware (libSceVideodec2) when the running HEN can load
 * that module, otherwise FFmpeg software. Not REA.
 */
typedef struct avdec avdec_t;

/* Interleaved stereo float, -1..1. count is frames. rate is the source rate. */
typedef void (*avdec_pcm_fn)(void *user, const float *interleaved, int count, int rate);

/* whence 0 set, 1 cur, 2 end, AVDEC_SEEK_SIZE asks for the length. */
#define AVDEC_SEEK_SIZE 0x10000
typedef int (*avdec_read_fn)(void *user, uint8_t *buf, int n);
typedef int64_t (*avdec_seek_fn)(void *user, int64_t off, int whence);

int avdec_open(avdec_t **out, const char *path, uint8_t **rgba, int *w, int *h,
	       double *duration, avdec_pcm_fn pcm, void *pcm_user);
/* Same decoder, fed by a reader that may not have every byte yet.
 * The read callback returns -1 when that range is still downloading. */
int avdec_open_io(avdec_t **out, avdec_read_fn read, avdec_seek_fn seek, void *io,
		  uint8_t **rgba, int *w, int *h, double *duration,
		  avdec_pcm_fn pcm, void *pcm_user);
void avdec_close(avdec_t *a);
/* Decode forward by dt seconds. Returns 1 if a picture was written. */
int avdec_tick(avdec_t *a, double dt);
double avdec_time(const avdec_t *a);
void avdec_seek(avdec_t *a, double seconds);
/* "HW" or "SW". */
const char *avdec_mode(const avdec_t *a);

#endif
