#ifndef SPLICER_HWDEC_H
#define SPLICER_HWDEC_H

#include <stdint.h>

/* Hardware H.264 / HEVC via libSceVideodec2.
 * The module is loaded at runtime with sceKernelLoadStartModule and
 * sceSysmoduleLoadModule(207). Those are the calls the running HEN
 * already uses. If they do not resolve, this stays closed and the
 * caller keeps the software decoder.
 */
int hwdec_probe(void);
int hwdec_ready(void);
const char *hwdec_status(void);

typedef struct hwdec hwdec_t;

int hwdec_open(hwdec_t **out, int hevc, int tenbit, int width, int height);
void hwdec_bind(hwdec_t *h, uint8_t *rgba, int width, int height);
void hwdec_close(hwdec_t *h);
void hwdec_reset(hwdec_t *h);

/* Annex-B access unit. 1 = a picture was written, 0 = none yet, -1 = give up. */
int hwdec_feed(hwdec_t *h, const uint8_t *au, int au_size);

/* Convert avcC / hvcC (or Annex-B) extradata into Annex-B parameter sets.
 * *nal_size is 0 when packets are already Annex-B, else the length field width.
 * Returns bytes written, or -1.
 */
int hwdec_extradata_annexb(int hevc, const uint8_t *extra, int extra_n,
			   uint8_t *dst, int dst_cap, int *nal_size);

/* One packet to Annex-B. nal_size 0 copies start-code packets through. */
int hwdec_packet_annexb(const uint8_t *src, int src_n, int nal_size,
			uint8_t *dst, int dst_cap);

#endif
