#ifndef MOVIAN_HWDEC_H
#define MOVIAN_HWDEC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hardware decoder interface for the sandboxed test title. */
int hwdec_probe(void);
int hwdec_ready(void);
const char *hwdec_status(void);
void hwdec_force_software(int on);

typedef struct hwdec hwdec_t;

int hwdec_open(hwdec_t **out, int hevc, int tenbit, int width, int height);
void hwdec_headers(hwdec_t *h, const uint8_t *annexb, int n);
void hwdec_bind(hwdec_t *h, uint8_t *rgba, int width, int height);
void hwdec_close(hwdec_t *h);
void hwdec_reset(hwdec_t *h);
int hwdec_flush(hwdec_t *h);
int hwdec_shown_width(const hwdec_t *h);
int hwdec_shown_height(const hwdec_t *h);
unsigned hwdec_mean_conversion_us(const hwdec_t *h);
/* Display-order PTS of the last copied picture, microseconds, or -1. */
int64_t hwdec_shown_pts(const hwdec_t *h);
/* Visible size from SPS / VUI crop in Annex-B headers (VLC packetizer). */
int hwdec_visible_from_headers(int hevc, const uint8_t *annexb, int n, int *w, int *h);

/* Annex-B access unit. pts_us is the packet time (VLC pts_data), or -1.
 * 1 = a picture was written, 0 = none yet, -1 = give up. */
int hwdec_feed(hwdec_t *h, const uint8_t *au, int au_size, int64_t pts_us);

int hwdec_extradata_annexb(int hevc, const uint8_t *extra, int extra_n,
                           uint8_t *dst, int dst_cap, int *nal_size);
int hwdec_packet_annexb(const uint8_t *src, int src_n, int nal_size,
                        uint8_t *dst, int dst_cap);

#ifdef __cplusplus
}
#endif

#endif
