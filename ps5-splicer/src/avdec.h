#ifndef SPLICER_AVDEC_H
#define SPLICER_AVDEC_H

#include <stdint.h>

/* Software H.264 / HEVC. Not a hardware decoder and not REA. */
typedef struct avdec avdec_t;

int avdec_open(avdec_t **out, const char *path, uint8_t **rgba, int *w, int *h,
	       double *duration);
void avdec_close(avdec_t *a);
/* Decode forward by dt seconds. Returns 1 if a picture was written. */
int avdec_tick(avdec_t *a, double dt);
double avdec_time(const avdec_t *a);
void avdec_seek(avdec_t *a, double seconds);

#endif
