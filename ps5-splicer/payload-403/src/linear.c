/* SPLICER linear scanout. FW 4.03 payload.
 * Same open/register/flip sequence as the red_test2 ELF that already
 * started on this console. Pixels are linear, matching the buffer
 * attribute 0x8000000022000000 (tile mode 0). No tiled writes.
 *
 * Link: host ld + ps5-payload-sdk main.script, entry 0, libkernel_web.
 * Send to etaHEN port 9021. This is not an fPKG.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/event.h>

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
log_line(const char *msg)
{
	notify_request_t req;
	memset(&req, 0, sizeof req);
	snprintf(req.message, sizeof req.message, "%s", msg);
	sceKernelSendNotificationRequest(0, &req, sizeof req, 0);
	if (!g_log) {
		g_log = fopen("/data/splicer/linear.log", "a");
		if (!g_log)
			g_log = fopen("/mnt/usb0/splicer-linear.log", "a");
	}
	if (g_log) {
		fprintf(g_log, "%s\n", msg);
		fflush(g_log);
	}
}

static void
log_rc(const char *tag, int rc)
{
	char b[160];
	snprintf(b, sizeof b, "LIN %s rc=%d (0x%08x)", tag, rc, (unsigned)rc);
	log_line(b);
}

static int
alloc_direct(void **vaddr, intptr_t *paddr, int *memsize_out)
{
	static const int sizes[] = {0x08000000, 0x04000000, 0x20000000, 0x02000000};
	int i;
	for (i = 0; i < 4; i++) {
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

/* Linear RGBA8. 0xAABBGGRR in the uint32 is byte order R,G,B,A. */
static void
fill_rect(uint8_t *dst, int stride, int w, int h, int x0, int y0, int rw, int rh, uint32_t rgba)
{
	int y, x;
	if (x0 < 0) {
		rw += x0;
		x0 = 0;
	}
	if (y0 < 0) {
		rh += y0;
		y0 = 0;
	}
	if (x0 + rw > w)
		rw = w - x0;
	if (y0 + rh > h)
		rh = h - y0;
	for (y = 0; y < rh; y++) {
		uint32_t *row = (uint32_t *)(dst + ((size_t)(y0 + y) * (size_t)stride + (size_t)x0) * 4u);
		for (x = 0; x < rw; x++)
			row[x] = rgba;
	}
}

static void
fill_bars(uint8_t *dst, int stride, int w, int h)
{
	static const uint32_t cols[5] = {
		0xFF0000FFu, 0xFF00FF00u, 0xFFFF0000u, 0xFFFFFFFFu, 0xFF5AA1E2u
	};
	int i;
	int bw = w / 5;
	if (bw < 1)
		bw = 1;
	for (i = 0; i < 5; i++) {
		int x0 = i * bw;
		int rw = (i == 4) ? (w - x0) : bw;
		fill_rect(dst, stride, w, h, x0, 0, rw, h, cols[i]);
	}
}

static void
hang(const char *msg)
{
	for (;;) {
		struct timespec ts = {2, 0};
		log_line(msg);
		nanosleep(&ts, NULL);
	}
}

int
main(void)
{
	vout_buf_t vbuf[2];
	vout_attr_t vattr;
	vout_stat_t vstat;
	struct kevent *evt_queue = NULL;
	struct kevent evt;
	int vout, memsize = 0, junk = 0, rc;
	intptr_t paddr = 0;
	void *vaddr = NULL;
	uint32_t frame_id = 0;
	int vis_w = 1920, vis_h = 1080, stride;
	char b[180];

	memset(vbuf, 0, sizeof vbuf);
	memset(&vattr, 0, sizeof vattr);
	memset(&vstat, 0, sizeof vstat);

	log_line("LIN start linear payload");
	rc = sceSystemServiceHideSplashScreen();
	log_rc("HideSplash", rc);

	if (alloc_direct(&vaddr, &paddr, &memsize))
		hang("LIN FATAL direct mem");
	snprintf(b, sizeof b, "LIN mem=0x%x", memsize);
	log_line(b);

	vout = sceVideoOutOpen(0xff, 0, 0, NULL);
	log_rc("Open", vout);
	if (vout < 0)
		hang("LIN FATAL open");

	rc = sceVideoOutGetOutputStatus(vout, &vstat);
	log_rc("Status", rc);
	snprintf(b, sizeof b, "LIN vstat.res=%u", (unsigned)vstat.res);
	log_line(b);
	if (rc)
		hang("LIN FATAL status");

	rc = sceKernelCreateEqueue(&evt_queue, "lin flip");
	log_rc("Equeue", rc);
	if (rc)
		hang("LIN FATAL equeue");
	rc = sceVideoOutAddFlipEvent(evt_queue, vout, NULL);
	log_rc("AddFlip", rc);
	if (rc)
		hang("LIN FATAL addflip");
	rc = sceVideoOutSetFlipRate(vout, 0);
	log_rc("FlipRate", rc);

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
	rc = sceVideoOutRegisterBuffers2(vout, 0, 0, vbuf, 2, &vattr, 0, NULL);
	log_rc("Register", rc);
	if (rc)
		hang("LIN FATAL register");

	snprintf(b, sizeof b, "LIN linear %dx%d pitch %d", vis_w, vis_h, stride);
	log_line(b);

	fill_rect((uint8_t *)vbuf[0].data, stride, stride, vis_h, 0, 0, vis_w, vis_h, 0xFF0000FFu);
	fill_bars((uint8_t *)vbuf[1].data, stride, vis_w, vis_h);
	log_line("LIN buf0 red  buf1 bars  (linear)");

	for (;;) {
		uint8_t idx = (uint8_t)(frame_id & 1);
		int rc_flip = sceVideoOutSubmitFlip(vout, idx, 1, frame_id);
		int rc_wait = sceKernelWaitEqueue(evt_queue, &evt, 1, &junk, 0);
		if (frame_id == 0) {
			snprintf(b, sizeof b, "LIN firstFlip flip=%d wait=%d", rc_flip, rc_wait);
			log_line(b);
		} else if ((frame_id % 480u) == 0) {
			log_line("LIN flipping linear (shell may still cover this)");
		}
		if (rc_flip || rc_wait) {
			snprintf(b, sizeof b, "LIN flip stop flip=%d wait=%d", rc_flip, rc_wait);
			hang(b);
		}
		frame_id++;
	}
}
