/* SPLICER MPEG-1 payload. FW 4.03.
 * Linear sceVideoOut, pl_mpeg software decode, no Videodec2, no fPKG.
 * Same loader contract as splicer-linear.elf: entry 0, libkernel_web, port 9021.
 */
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/event.h>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

typedef struct vout_buf {
	void *data;
	uint64_t junk0[3];
} vout_buf_t;
typedef struct vout_stat {
	uint32_t res;
	uint32_t junk0;
	uint64_t junk1[5];
} vout_stat_t;
typedef struct vout_attr {
	uint8_t junk0[80];
} vout_attr_t;
typedef struct notify_request {
	char useless1[45];
	char message[3075];
} notify_request_t;

int sceKernelAllocateMainDirectMemory(size_t, size_t, int, intptr_t *);
int sceKernelMapDirectMemory(void **, size_t, int, int, intptr_t, size_t);
int sceKernelCreateEqueue(struct kevent **, const char *);
int sceKernelWaitEqueue(struct kevent *, struct kevent *, int, int *, unsigned int *);
int sceKernelSendNotificationRequest(int, notify_request_t *, size_t, int);
int sceVideoOutOpen(int, int, int, const void *);
int sceVideoOutGetOutputStatus(int, vout_stat_t *);
int sceVideoOutAddFlipEvent(struct kevent *, int, void *);
int sceVideoOutSetFlipRate(int, int);
int sceVideoOutSubmitFlip(int, int, unsigned int, int64_t);
void sceVideoOutSetBufferAttribute2(vout_attr_t *, uint64_t, uint32_t, uint32_t,
				    uint32_t, uint64_t, uint32_t, uint64_t);
int sceVideoOutRegisterBuffers2(int, int, int, vout_buf_t *, int, vout_attr_t *,
				int, void *);
int sceSystemServiceHideSplashScreen(void);

static FILE *g_log;

static void
note(const char *msg)
{
	notify_request_t req;
	memset(&req, 0, sizeof req);
	snprintf(req.message, sizeof req.message, "%s", msg);
	sceKernelSendNotificationRequest(0, &req, sizeof req, 0);
	if (!g_log) {
		g_log = fopen("/data/splicer/mpeg1.log", "a");
		if (!g_log)
			g_log = fopen("/mnt/usb0/splicer-mpeg1.log", "a");
	}
	if (g_log) {
		fprintf(g_log, "%s\n", msg);
		fflush(g_log);
	}
}

static int
ends_with(const char *name, const char *ext)
{
	size_t n = strlen(name);
	size_t e = strlen(ext);
	size_t i;
	if (n < e)
		return 0;
	for (i = 0; i < e; i++) {
		char a = name[n - e + i];
		char b = ext[i];
		if (a >= 'A' && a <= 'Z')
			a = (char)(a + 32);
		if (a != b)
			return 0;
	}
	return 1;
}

static int
find_mpeg(char *dst, size_t n)
{
	static const char *dirs[] = {
		"/mnt/usb0", "/mnt/usb1", "/data/video", "/data/splicer", "/user/splicer", NULL
	};
	static const char *exts[] = {".mpg", ".mpeg", ".mpv", ".m1v", ".m2v", NULL};
	int d;
	for (d = 0; dirs[d]; d++) {
		DIR *dir = opendir(dirs[d]);
		struct dirent *de;
		if (!dir)
			continue;
		while ((de = readdir(dir)) != NULL) {
			int e;
			for (e = 0; exts[e]; e++) {
				if (!ends_with(de->d_name, exts[e]))
					continue;
				snprintf(dst, n, "%s/%s", dirs[d], de->d_name);
				closedir(dir);
				return 0;
			}
		}
		closedir(dir);
	}
	return -1;
}

static void
blit_nearest(uint8_t *dst, int stride, int dw, int dh, const uint8_t *src, int sw, int sh)
{
	int y, x;
	if (sw < 1 || sh < 1)
		return;
	for (y = 0; y < dh; y++) {
		int sy = (int)(((long long)y * sh) / dh);
		uint32_t *row = (uint32_t *)(dst + (size_t)y * (size_t)stride * 4u);
		if (sy >= sh)
			sy = sh - 1;
		for (x = 0; x < dw; x++) {
			int sx = (int)(((long long)x * sw) / dw);
			const uint8_t *p;
			if (sx >= sw)
				sx = sw - 1;
			p = src + ((size_t)sy * (size_t)sw + (size_t)sx) * 4u;
			row[x] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
				 ((uint32_t)p[2] << 16) | 0xFF000000u;
		}
	}
}

static int
alloc_direct(void **vaddr, intptr_t *paddr, int *memsize_out)
{
	static const int sizes[] = {0x08000000, 0x04000000, 0x02000000};
	int i;
	for (i = 0; i < 3; i++) {
		intptr_t pa = 0;
		void *va = NULL;
		if (sceKernelAllocateMainDirectMemory((size_t)sizes[i], 0x20000, 3, &pa))
			continue;
		if (sceKernelMapDirectMemory(&va, (size_t)sizes[i], 0x33, 0, pa, 0x20000))
			continue;
		*vaddr = va;
		*paddr = pa;
		*memsize_out = sizes[i];
		return 0;
	}
	return -1;
}

int
main(void)
{
	vout_buf_t vbuf[2];
	vout_attr_t vattr;
	vout_stat_t vstat;
	struct kevent *evt_queue = NULL;
	struct kevent evt;
	int vout, memsize = 0, junk = 0;
	intptr_t paddr = 0;
	void *vaddr = NULL;
	uint32_t frame_id = 0;
	int vis_w = 1920, vis_h = 1080, stride;
	char path[700];
	char b[200];
	plm_t *plm = NULL;
	uint8_t *rgba = NULL;
	int vw = 0, vh = 0;
	double tprev;

	memset(vbuf, 0, sizeof vbuf);
	memset(&vattr, 0, sizeof vattr);
	memset(&vstat, 0, sizeof vstat);

	note("M1 start mpeg1 payload");
	sceSystemServiceHideSplashScreen();
	if (alloc_direct(&vaddr, &paddr, &memsize)) {
		note("M1 FATAL direct mem");
		return 1;
	}
	vout = sceVideoOutOpen(0xff, 0, 0, NULL);
	snprintf(b, sizeof b, "M1 Open rc=%d", vout);
	note(b);
	if (vout < 0)
		return 1;
	if (sceVideoOutGetOutputStatus(vout, &vstat)) {
		note("M1 FATAL status");
		return 1;
	}
	if (sceKernelCreateEqueue(&evt_queue, "m1 flip"))
		return 1;
	if (sceVideoOutAddFlipEvent(evt_queue, vout, NULL))
		return 1;
	sceVideoOutSetFlipRate(vout, 0);
	if (vstat.res == 2) {
		vis_w *= 2;
		vis_h *= 2;
	}
	if ((size_t)vis_w * (size_t)vis_h * 4u * 2u + 0x200000u > (size_t)memsize) {
		vis_w = 1920;
		vis_h = 1080;
	}
	stride = (vis_w + 0x3f) & ~0x3f;
	sceVideoOutSetBufferAttribute2(&vattr, 0x8000000022000000UL, 0,
				       (uint32_t)vis_w, (uint32_t)vis_h, 0, 0, 0);
	vbuf[0].data = vaddr;
	vbuf[1].data = (uint8_t *)vaddr + (memsize / 2);
	if (sceVideoOutRegisterBuffers2(vout, 0, 0, vbuf, 2, &vattr, 0, NULL)) {
		note("M1 FATAL register");
		return 1;
	}
	note("M1 linear registered");

	if (find_mpeg(path, sizeof path) != 0) {
		note("M1 no mpeg on usb or /data/video");
	} else {
		snprintf(b, sizeof b, "M1 file %s", path);
		note(b);
		plm = plm_create_with_filename(path);
		if (!plm || (!plm_probe(plm, 1024 * 1024) && !plm_has_headers(plm)) ||
		    plm_get_num_video_streams(plm) < 1) {
			note("M1 file rejected");
			if (plm)
				plm_destroy(plm);
			plm = NULL;
		} else {
			vw = plm_get_width(plm);
			vh = plm_get_height(plm);
			if (vw < 1 || vh < 1 || vw > 1920 || vh > 1088) {
				snprintf(b, sizeof b, "M1 size %dx%d skipped", vw, vh);
				note(b);
				plm_destroy(plm);
				plm = NULL;
			} else {
				rgba = (uint8_t *)malloc((size_t)vw * (size_t)vh * 4u);
				if (!rgba) {
					note("M1 no frame mem");
					plm_destroy(plm);
					plm = NULL;
				} else {
					plm_set_audio_enabled(plm, 0);
					plm_set_loop(plm, 1);
					snprintf(b, sizeof b, "M1 play %dx%d", vw, vh);
					note(b);
				}
			}
		}
	}

	tprev = 0;
	for (;;) {
		struct timespec ts;
		double now, dt;
		uint8_t idx = (uint8_t)(frame_id & 1);
		uint8_t *fb = (uint8_t *)vbuf[idx].data;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		now = (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
		dt = (tprev == 0) ? 0.016 : (now - tprev);
		if (dt < 0)
			dt = 0;
		if (dt > 0.1)
			dt = 0.1;
		tprev = now;

		if (plm && rgba) {
			plm_frame_t *fr = plm_decode_video(plm);
			if (fr && (int)fr->width == vw && (int)fr->height == vh) {
				plm_frame_to_rgba(fr, rgba, vw * 4);
				blit_nearest(fb, stride, vis_w, vis_h, rgba, vw, vh);
			}
		} else if ((frame_id % 120u) < 60u) {
			int y;
			for (y = 0; y < vis_h; y++) {
				uint32_t *row = (uint32_t *)(fb + (size_t)y * (size_t)stride * 4u);
				int x;
				for (x = 0; x < vis_w; x++)
					row[x] = 0xFF0000FFu;
			}
		} else {
			int y;
			for (y = 0; y < vis_h; y++) {
				uint32_t *row = (uint32_t *)(fb + (size_t)y * (size_t)stride * 4u);
				int x;
				uint32_t c = (y < vis_h / 2) ? 0xFF5AA1E2u : 0xFF00FF00u;
				for (x = 0; x < vis_w; x++)
					row[x] = c;
			}
		}

		if (sceVideoOutSubmitFlip(vout, idx, 1, frame_id)) {
			note("M1 flip failed");
			return 1;
		}
		if (sceKernelWaitEqueue(evt_queue, &evt, 1, &junk, 0)) {
			note("M1 wait failed");
			return 1;
		}
		if (frame_id == 0)
			note("M1 first flip");
		frame_id++;
	}
}
