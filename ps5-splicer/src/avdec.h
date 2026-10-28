#ifndef SPLICER_AVDEC_H
#define SPLICER_AVDEC_H

#include <stdint.h>

/* H.264 / HEVC. Hardware (libSceVideodec2) when the running HEN can load
 * that module, otherwise FFmpeg software. Not REA.
 */
typedef struct avdec avdec_t;

/* Interleaved stereo float, -1..1. count is frames. rate is the source rate. */
typedef void (*avdec_pcm_fn)(void *user, const float *interleaved, int count, int rate);

int avdec_open(avdec_t **out, const char *path, uint8_t **rgba, int *w, int *h,
	       double *duration, avdec_pcm_fn pcm, void *pcm_user);
void avdec_close(avdec_t *a);
/* Decode forward by dt seconds. Returns 1 if a picture was written. */
int avdec_tick(avdec_t *a, double dt);
double avdec_time(const avdec_t *a);
void avdec_seek(avdec_t *a, double seconds);
/* "HW" or "SW". */
const char *avdec_mode(const avdec_t *a);

#endif
