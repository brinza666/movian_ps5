/* SPLICER — two-bus video combiner for a PS5 ELF loader.
 *
 * Native x86-64 payload. Scanout is sceVideoOut (direct memory, A8B8G8R8).
 * MPEG-1 is pl_mpeg. MP4, MKV and MPEG-TS that carry H.264 or HEVC are
 * demuxed with FFmpeg. Decode tries libSceVideodec2 (sysmodule 207),
 * loaded with sceKernelLoadStartModule / sceSysmoduleLoadModule, the
 * same module calls the running HEN already makes. If that module does
 * not open, FFmpeg decodes in software. AAC, AC-3, E-AC-3, MP3, DTS,
 * TrueHD, Opus, FLAC and PCM in those files are decoded with FFmpeg
 * and mixed to stereo on sceAudioOut, the same output MPEG-1 uses.
 *
 * The scanout sequence follows the public hbldr sample (GPL-3.0):
 *   Copyright (C) 2023 John Törnblom
 * This file is licensed under GPL-3.0-or-later. pl_mpeg is MIT.
 * FFmpeg libraries linked here are LGPL-2.1-or-later.
 * font8x8 is public domain.
 */

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <sys/event.h>

#include "pl_mpeg.h"
#include "avdec.h"
#include "hwdec.h"
#include "plugin_host.h"
#include "bt.h"
#include "font8x8_basic.h"

#define MAX_FILES 48
#define QMAX 8192
#define LIVE_W 480
#define LIVE_H 272

enum {
	LAY_SBS = 0,
	LAY_TB,
	LAY_PIP,
	LAY_WIPE,
	LAY_DIFF,
	LAY_SCREEN,
	LAY_COUNT
};

static const char *LAY_NAME[LAY_COUNT] = {
	"SIDE BY SIDE",
	"TOP / BOTTOM",
	"PICTURE IN PICTURE",
	"WIPE",
	"DIFFERENCE",
	"SCREEN"
};

enum {
	BTN_L3 = 0x00000002,
	BTN_R3 = 0x00000004,
	BTN_OPTIONS = 0x00000008,
	BTN_UP = 0x00000010,
	BTN_RIGHT = 0x00000020,
	BTN_DOWN = 0x00000040,
	BTN_LEFT = 0x00000080,
	BTN_L2 = 0x00000100,
	BTN_R2 = 0x00000200,
	BTN_L1 = 0x00000400,
	BTN_R1 = 0x00000800,
	BTN_TRI = 0x00001000,
	BTN_CIR = 0x00002000,
	BTN_CROSS = 0x00004000,
	BTN_SQR = 0x00008000
};

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

typedef struct aq {
	float l[QMAX];
	float r[QMAX];
	int n;
	double credit;
	int rate;
} aq_t;

typedef struct bus {
	char name[192];
	char path[768];
	plm_t *plm;
	avdec_t *av;
	uint8_t *rgba;
	int w, h;
	int has;
	int live;
	int kind;
	double duration;
	aq_t aq;
} bus_t;

int sceKernelAllocateMainDirectMemory(size_t, size_t, int, intptr_t *);
int sceKernelMapDirectMemory(void **, size_t, int, int, intptr_t, size_t);
int sceKernelCreateEqueue(struct kevent **, const char *);
int sceKernelWaitEqueue(struct kevent *, struct kevent *, int, int *, uint *);
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

static int (*p_sceAudioOutInit)(void);
static int (*p_sceAudioOutOpen)(int, int, int, unsigned int, unsigned int, unsigned int);
static int (*p_sceAudioOutOutput)(int, const void *);
static int (*p_sceAudioOutSetVolume)(int, int, int *);
static int (*p_sceAudioOutClose)(int);
static int (*p_sceUserServiceInitialize)(void *);
static int (*p_sceUserServiceGetInitialUser)(int *);
static int (*p_scePadInit)(void);
static int (*p_scePadOpen)(int, int, int, const void *);
static int (*p_scePadReadState)(int, void *);
static int g_opt_loaded;

static bus_t g_bus[2];
static char g_files[MAX_FILES][768];
static int g_nfiles;
static int g_idx[2];
static int g_layout;
static int g_paused;
static int g_audio_mode; /* 0 both, 1 A, 2 B */
static int g_focus;
static int g_split = -1;
static volatile int g_run = 1;
static int g_audio = -1;
static char g_status[96] = "booting";
static int g_bt_opened;
static int g_bt_cool;
static void bt_release_if_unused(void);
static int g_plug;
static int g_plug_i;
static int g_plug_view;
static int g_plug_wait;
static int g_plug_job;
static char g_stream[768];
static char g_stream_name[180];

static const uint8_t COL_AMBER[3] = {0xe2, 0xa1, 0x5a};
static const uint8_t COL_PAPER[3] = {0xeb, 0xe6, 0xdb};
static const uint8_t COL_SAGE[3] = {0xb7, 0xc4, 0xa4};
static const uint8_t COL_DIM[3] = {0x8a, 0x84, 0x78};

static void
notify(const char *msg)
{
	notify_request_t req;
	memset(&req, 0, sizeof req);
	snprintf(req.message, sizeof req.message, "%s", msg);
	sceKernelSendNotificationRequest(0, &req, sizeof req, 0);
}

static double
now_sec(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int
ends_with_ci(const char *name, const char *ext)
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
			a = (char)(a - 'A' + 'a');
		if (a != b)
			return 0;
	}
	return 1;
}

static int
is_video_name(const char *name)
{
	return ends_with_ci(name, ".mpg") || ends_with_ci(name, ".mpeg") ||
	       ends_with_ci(name, ".mpv") || ends_with_ci(name, ".m1v") ||
	       ends_with_ci(name, ".m2v") || ends_with_ci(name, ".mp4") ||
	       ends_with_ci(name, ".m4v") || ends_with_ci(name, ".mov") ||
	       ends_with_ci(name, ".mkv") || ends_with_ci(name, ".ts") ||
	       ends_with_ci(name, ".m2ts") || ends_with_ci(name, ".mts");
}

static int
is_bt_name(const char *name)
{
	return ends_with_ci(name, ".torrent") || ends_with_ci(name, ".magnet");
}

static int
is_bt_src(const char *s)
{
	return s && (strncmp(s, "magnet:?", 8) == 0 || is_bt_name(s));
}

static void
basename_copy(char *dst, size_t n, const char *path)
{
	const char *s = strrchr(path, '/');
	s = s ? s + 1 : path;
	snprintf(dst, n, "%s", s);
}

static void
aq_init(aq_t *q)
{
	memset(q, 0, sizeof *q);
	q->rate = 48000;
}

static void
aq_reset(aq_t *q)
{
	q->n = 0;
	q->credit = 0;
}

static void
aq_push(aq_t *q, const float *interleaved, int count, int rate)
{
	int i;
	if (rate > 0)
		q->rate = rate;
	if (count > QMAX / 2)
		count = QMAX / 2;
	if (q->n + count > QMAX) {
		int drop = q->n + count - QMAX;
		if (drop > q->n)
			drop = q->n;
		memmove(q->l, q->l + drop, (size_t)(q->n - drop) * sizeof(float));
		memmove(q->r, q->r + drop, (size_t)(q->n - drop) * sizeof(float));
		q->n -= drop;
	}
	for (i = 0; i < count; i++) {
		q->l[q->n] = interleaved[i * 2];
		q->r[q->n] = interleaved[i * 2 + 1];
		q->n++;
	}
}

static int16_t
ftoi(float s, float gain)
{
	int v = (int)(s * gain * 30000.0f);
	if (v > 32767)
		v = 32767;
	if (v < -32768)
		v = -32768;
	return (int16_t)v;
}

static void
aq_pull(aq_t *q, int16_t *dst, int frames, float gain)
{
	int i;
	double step = (q->rate > 0 ? (double)q->rate : 48000.0) / 48000.0;
	for (i = 0; i < frames; i++) {
		float l = 0, r = 0;
		if (q->n >= 2) {
			double t = q->credit;
			int drop;
			if (t < 0)
				t = 0;
			if (t > 0.999)
				t = 0.999;
			l = (float)(q->l[0] * (1.0 - t) + q->l[1] * t);
			r = (float)(q->r[0] * (1.0 - t) + q->r[1] * t);
			q->credit += step;
			drop = (int)q->credit;
			if (drop > 0) {
				if (drop >= q->n) {
					q->n = 0;
					q->credit = 0;
				} else {
					memmove(q->l, q->l + drop,
						(size_t)(q->n - drop) * sizeof(float));
					memmove(q->r, q->r + drop,
						(size_t)(q->n - drop) * sizeof(float));
					q->n -= drop;
					q->credit -= drop;
				}
			}
		}
		dst[i * 2] = ftoi(l, gain);
		dst[i * 2 + 1] = ftoi(r, gain);
	}
}

static void
on_video(plm_t *plm, plm_frame_t *frame, void *user)
{
	bus_t *b = user;
	(void)plm;
	if (!b->rgba)
		return;
	if ((int)frame->width != b->w || (int)frame->height != b->h)
		return;
	plm_frame_to_rgba(frame, b->rgba, b->w * 4);
	b->has = 1;
}

static void
on_audio(plm_t *plm, plm_samples_t *samples, void *user)
{
	bus_t *b = user;
	int rate = plm_get_samplerate(plm);
	aq_push(&b->aq, samples->interleaved, (int)samples->count, rate);
}

static void
bus_init(bus_t *b)
{
	memset(b, 0, sizeof *b);
	aq_init(&b->aq);
}

static void
fill_rgba(uint8_t *p, int n, uint32_t pix)
{
	int i;
	uint32_t *d = (uint32_t *)p;
	for (i = 0; i < n; i++)
		d[i] = pix;
}

static void
bus_live(bus_t *b, int kind, const char *name)
{
	if (b->plm) {
		plm_destroy(b->plm);
		b->plm = NULL;
	}
	if (b->av) {
		avdec_close(b->av);
		b->av = NULL;
	}
	free(b->rgba);
	b->rgba = (uint8_t *)malloc((size_t)LIVE_W * LIVE_H * 4);
	b->w = LIVE_W;
	b->h = LIVE_H;
	b->has = 1;
	b->live = 1;
	b->kind = kind;
	b->duration = 0;
	b->path[0] = 0;
	snprintf(b->name, sizeof b->name, "%s", name);
	if (b->rgba)
		fill_rgba(b->rgba, LIVE_W * LIVE_H, 0xff0f1312u);
	aq_reset(&b->aq);
}

static void
on_av_pcm(void *user, const float *interleaved, int count, int rate)
{
	bus_t *b = user;
	aq_push(&b->aq, interleaved, count, rate);
}

static int
bus_open_av(bus_t *b, const char *path, int half_seek)
{
	avdec_t *av = NULL;
	uint8_t *rgba = NULL;
	int w = 0, h = 0;
	double duration = 0;
	aq_reset(&b->aq);
	if (avdec_open(&av, path, &rgba, &w, &h, &duration, on_av_pcm, b) != 0)
		return -1;
	if (b->plm) {
		plm_destroy(b->plm);
		b->plm = NULL;
	}
	if (b->av)
		avdec_close(b->av);
	free(b->rgba);
	b->av = av;
	b->rgba = rgba;
	b->w = w;
	b->h = h;
	b->has = 1;
	b->live = 0;
	b->duration = duration;
	snprintf(b->path, sizeof b->path, "%s", path);
	basename_copy(b->name, sizeof b->name, path);
	if (half_seek && b->duration > 1.0) {
		aq_reset(&b->aq);
		avdec_seek(av, b->duration * 0.5);
	}
	return 0;
}

static int
bt_io_read(void *user, uint8_t *buf, int n)
{
	(void)user;
	return bt_read(buf, n);
}

static int64_t
bt_io_seek(void *user, int64_t off, int whence)
{
	(void)user;
	return bt_seek(off, whence);
}

static int
bus_open_bt(bus_t *b)
{
	avdec_t *av = NULL;
	uint8_t *rgba = NULL;
	int w = 0, h = 0;
	double duration = 0;
	aq_reset(&b->aq);
	if (avdec_open_io(&av, bt_io_read, bt_io_seek, NULL, &rgba, &w, &h, &duration,
			  on_av_pcm, b) != 0)
		return -1;
	if (b->plm) {
		plm_destroy(b->plm);
		b->plm = NULL;
	}
	if (b->av)
		avdec_close(b->av);
	free(b->rgba);
	b->av = av;
	b->rgba = rgba;
	b->w = w;
	b->h = h;
	b->has = 1;
	b->live = 0;
	b->kind = 0;
	b->duration = duration;
	snprintf(b->path, sizeof b->path, "%s", bt_name());
	snprintf(b->name, sizeof b->name, "%.60s", bt_name());
	return 0;
}

static int
bus_open_plm(bus_t *b, const char *path, int half_seek)
{
	plm_t *plm;
	int w, h;
	uint8_t *rgba;

	plm = plm_create_with_filename(path);
	if (!plm)
		return -1;
	if (!plm_probe(plm, 1024 * 1024) && !plm_has_headers(plm)) {
		plm_destroy(plm);
		return -1;
	}
	if (plm_get_num_video_streams(plm) < 1) {
		plm_destroy(plm);
		return -1;
	}
	w = plm_get_width(plm);
	h = plm_get_height(plm);
	if (w < 16 || h < 16 || w > 1920 || h > 1088) {
		plm_destroy(plm);
		return -1;
	}
	rgba = (uint8_t *)malloc((size_t)w * (size_t)h * 4);
	if (!rgba) {
		plm_destroy(plm);
		return -1;
	}
	fill_rgba(rgba, w * h, 0xff0f1312u);

	if (b->plm)
		plm_destroy(b->plm);
	if (b->av) {
		avdec_close(b->av);
		b->av = NULL;
	}
	free(b->rgba);
	b->plm = plm;
	b->rgba = rgba;
	b->w = w;
	b->h = h;
	b->has = 0;
	b->live = 0;
	b->duration = plm_get_duration(plm);
	snprintf(b->path, sizeof b->path, "%s", path);
	basename_copy(b->name, sizeof b->name, path);
	aq_reset(&b->aq);
	plm_set_loop(plm, 1);
	plm_set_audio_enabled(plm, 1);
	plm_set_audio_lead_time(plm, 0.08);
	plm_set_video_decode_callback(plm, on_video, b);
	plm_set_audio_decode_callback(plm, on_audio, b);
	if (half_seek && b->duration > 1.0)
		plm_seek(plm, b->duration * 0.5, 0);
	return 0;
}

static int
is_mpeg_name(const char *name)
{
	return ends_with_ci(name, ".mpg") || ends_with_ci(name, ".mpeg") ||
	       ends_with_ci(name, ".mpv") || ends_with_ci(name, ".m1v") ||
	       ends_with_ci(name, ".m2v");
}

static int
bus_open_file(bus_t *b, const char *path, int half_seek)
{
	if (is_bt_src(path)) {
		if (bt_start(path) != 0)
			return -1;
		bus_live(b, 2, bt_name());
		g_bt_opened = 0;
		return 0;
	}
	if (is_mpeg_name(path) && bus_open_plm(b, path, half_seek) == 0)
		return 0;
	if (bus_open_av(b, path, half_seek) == 0)
		return 0;
	if (!is_mpeg_name(path) && bus_open_plm(b, path, half_seek) == 0)
		return 0;
	return -1;
}

static void
render_live(bus_t *b, double t)
{
	int x, y;
	uint8_t *p;
	if (!b->rgba)
		return;
	p = b->rgba;
	if (b->kind == 0) {
		int band = (int)(t * 80.0) % b->h;
		for (y = 0; y < b->h; y++) {
			int dy = y - band;
			if (dy < 0)
				dy = -dy;
			for (x = 0; x < b->w; x++) {
				uint8_t r = 0xc4, g = 0x72, bcol = 0x22;
				if (dy < 10) {
					r = 0xff;
					g = 0xf2;
					bcol = 0xd8;
				}
				p[0] = r;
				p[1] = g;
				p[2] = bcol;
				p[3] = 255;
				p += 4;
			}
		}
	} else {
		int shift = (int)(t * 50.0);
		for (y = 0; y < b->h; y++) {
			for (x = 0; x < b->w; x++) {
				int on = ((x + shift) / 24) & 1;
				p[0] = on ? 0x7d : 0x14;
				p[1] = on ? 0xe0 : 0x3a;
				p[2] = on ? 0x8a : 0x2c;
				p[3] = 255;
				p += 4;
			}
		}
	}
}

static void
scan_dir(const char *base, int depth)
{
	DIR *dir;
	struct dirent *dp;
	if (depth < 0 || g_nfiles >= MAX_FILES)
		return;
	dir = opendir(base);
	if (!dir)
		return;
	while ((dp = readdir(dir))) {
		char path[1024];
		struct stat st;
		size_t bl, nl;
		if (dp->d_name[0] == '.')
			continue;
		bl = strlen(base);
		nl = strlen(dp->d_name);
		if (bl + 1 + nl >= sizeof path)
			continue;
		memcpy(path, base, bl);
		path[bl] = '/';
		memcpy(path + bl + 1, dp->d_name, nl + 1);
		if (stat(path, &st))
			continue;
		if (S_ISDIR(st.st_mode)) {
			if (depth > 0)
				scan_dir(path, depth - 1);
			continue;
		}
		if (!S_ISREG(st.st_mode))
			continue;
		if (!is_video_name(dp->d_name) && !is_bt_name(dp->d_name))
			continue;
		if (g_nfiles >= MAX_FILES)
			break;
		snprintf(g_files[g_nfiles], sizeof g_files[0], "%s", path);
		g_nfiles++;
	}
	closedir(dir);
}

static int
file_exists(const char *path)
{
	struct stat st;
	return path && path[0] && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void
read_playlist(const char *path, char *a, char *b, size_t n)
{
	FILE *f = fopen(path, "r");
	char line[1024];
	if (!f)
		return;
	while (fgets(line, sizeof line, f)) {
		char *eq;
		size_t L = strlen(line);
		while (L && (line[L - 1] == '\n' || line[L - 1] == '\r'))
			line[--L] = 0;
		if (line[0] == '#' || line[0] == 0)
			continue;
		eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = 0;
		if (strcmp(line, "a") == 0 || strcmp(line, "A") == 0) {
			if (strncmp(eq + 1, "magnet:?", 8) == 0)
				snprintf(a, n, "%s", eq + 1);
			else if (file_exists(eq + 1))
				snprintf(a, n, "%s", eq + 1);
		}
		if (strcmp(line, "b") == 0 || strcmp(line, "B") == 0) {
			if (strncmp(eq + 1, "magnet:?", 8) == 0)
				snprintf(b, n, "%s", eq + 1);
			else if (file_exists(eq + 1))
				snprintf(b, n, "%s", eq + 1);
		}
	}
	fclose(f);
}

static void
remember(const char *path)
{
	int i;
	if (!path || !path[0] || g_nfiles >= MAX_FILES)
		return;
	for (i = 0; i < g_nfiles; i++) {
		if (strcmp(g_files[i], path) == 0)
			return;
	}
	snprintf(g_files[g_nfiles], sizeof g_files[0], "%s", path);
	g_nfiles++;
}

static void
scan_media(void)
{
	char force_a[768];
	char force_b[768];
	static const char *plays[] = {
		"/mnt/usb0/splicer.txt",
		"/mnt/usb1/splicer.txt",
		"/data/splicer.txt",
		"/user/splicer.txt",
		NULL
	};
	int i;
	force_a[0] = force_b[0] = 0;
	g_nfiles = 0;
	for (i = 0; plays[i]; i++)
		read_playlist(plays[i], force_a, force_b, sizeof force_a);
	scan_dir("/mnt/usb0", 2);
	scan_dir("/mnt/usb1", 2);
	scan_dir("/data/video", 2);
	scan_dir("/data/splicer", 2);
	scan_dir("/user/splicer", 2);
	scan_dir("/data", 0);
	remember(force_a);
	remember(force_b);
	/* Playlist paths win the first two slots when both exist. */
	if (force_a[0] && force_b[0]) {
		char tmp[768];
		int ia = -1, ib = -1;
		for (i = 0; i < g_nfiles; i++) {
			if (strcmp(g_files[i], force_a) == 0)
				ia = i;
			if (strcmp(g_files[i], force_b) == 0)
				ib = i;
		}
		if (ia > 0) {
			memcpy(tmp, g_files[0], sizeof tmp);
			memcpy(g_files[0], g_files[ia], sizeof tmp);
			memcpy(g_files[ia], tmp, sizeof tmp);
		}
		if (ib >= 0 && ib != 1 && g_nfiles > 1) {
			int ib2 = -1;
			for (i = 0; i < g_nfiles; i++)
				if (strcmp(g_files[i], force_b) == 0)
					ib2 = i;
			if (ib2 > 1) {
				memcpy(tmp, g_files[1], sizeof tmp);
				memcpy(g_files[1], g_files[ib2], sizeof tmp);
				memcpy(g_files[ib2], tmp, sizeof tmp);
			}
		}
	}
}

static void
load_buses(void)
{
	int same;
	if (g_nfiles <= 0) {
		bus_live(&g_bus[0], 0, "REEL A  (no mpeg)");
		bus_live(&g_bus[1], 1, "REEL B  (no mpeg)");
		return;
	}
	if (g_idx[0] < 0 || g_idx[0] >= g_nfiles)
		g_idx[0] = 0;
	if (g_idx[1] < 0 || g_idx[1] >= g_nfiles)
		g_idx[1] = g_nfiles > 1 ? 1 : 0;
	same = g_idx[0] == g_idx[1];
	if (bus_open_file(&g_bus[0], g_files[g_idx[0]], 0) != 0)
		bus_live(&g_bus[0], 0, "REEL A  unreadable");
	if (bus_open_file(&g_bus[1], g_files[g_idx[1]], same) != 0)
		bus_live(&g_bus[1], 1, "REEL B  unreadable");
	bt_release_if_unused();
}

static void
shift_file(int bus, int dir)
{
	if (g_nfiles <= 0) {
		load_buses();
		return;
	}
	g_idx[bus] = (g_idx[bus] + dir) % g_nfiles;
	if (g_idx[bus] < 0)
		g_idx[bus] += g_nfiles;
	if (bus_open_file(&g_bus[bus], g_files[g_idx[bus]], 0) != 0)
		bus_live(&g_bus[bus], bus, "unreadable");
	bt_release_if_unused();
}

static void
bt_release_if_unused(void)
{
	int i, keep = 0;
	if (!bt_active())
		return;
	for (i = 0; i < 2; i++) {
		if (g_bus[i].live && g_bus[i].kind == 2)
			keep = 1;
		if (g_idx[i] >= 0 && g_idx[i] < g_nfiles && bt_same(g_files[g_idx[i]]))
			keep = 1;
	}
	if (!keep) {
		bt_stop();
		g_bt_opened = 0;
	}
}

static void
seek_bus(bus_t *b, double delta)
{
	double t;
	if (b->plm) {
		t = plm_get_time(b->plm) + delta;
		if (t < 0)
			t = 0;
		plm_seek(b->plm, t, 0);
		return;
	}
	if (b->av) {
		t = avdec_time(b->av) + delta;
		if (t < 0)
			t = 0;
		aq_reset(&b->aq);
		avdec_seek(b->av, t);
	}
}

static void
pump_audio(void)
{
	int16_t a[256 * 2];
	int16_t b[256 * 2];
	int16_t mix[256 * 2];
	float ga = 0, gb = 0;
	int i;
	if (g_audio < 0 || !p_sceAudioOutOutput)
		return;
	if (g_audio_mode != 2)
		ga = (g_audio_mode == 0) ? 0.55f : 1.0f;
	if (g_audio_mode != 1)
		gb = (g_audio_mode == 0) ? 0.55f : 1.0f;
	aq_pull(&g_bus[0].aq, a, 256, ga);
	aq_pull(&g_bus[1].aq, b, 256, gb);
	for (i = 0; i < 256 * 2; i++) {
		int s = (int)a[i] + (int)b[i];
		if (s > 32767)
			s = 32767;
		if (s < -32768)
			s = -32768;
		mix[i] = (int16_t)s;
	}
	p_sceAudioOutOutput(g_audio, mix);
}

static void
sample_px(const uint8_t *src, int sw, int sh, int x, int y, int dw, int dh,
	  uint8_t *o, int bilinear)
{
	int sx, sy, r, g, b;
	if (!src || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
		o[0] = 0x12;
		o[1] = 0x13;
		o[2] = 0x0f;
		o[3] = 255;
		return;
	}
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x >= dw)
		x = dw - 1;
	if (y >= dh)
		y = dh - 1;
	if (!bilinear) {
		sx = (int)(((long)x * sw) / dw);
		sy = (int)(((long)y * sh) / dh);
		if (sx >= sw)
			sx = sw - 1;
		if (sy >= sh)
			sy = sh - 1;
		{
			const uint8_t *p = src + ((size_t)sy * (size_t)sw + (size_t)sx) * 4;
			o[0] = p[0];
			o[1] = p[1];
			o[2] = p[2];
			o[3] = 255;
		}
		return;
	}
	{
		int fx = (int)(((long long)x * sw << 16) / dw);
		int fy = (int)(((long long)y * sh << 16) / dh);
		int tx = (fx >> 8) & 255;
		int ty = (fy >> 8) & 255;
		int r00, g00, b00, r10, g10, b10, r01, g01, b01, r11, g11, b11;
		int r0, g0, b0, r1, g1, b1;
		const uint8_t *p;
		sx = fx >> 16;
		sy = fy >> 16;
#define FETCH(DX, DY, RR, GG, BB)                                              \
	do {                                                                   \
		int xx = sx + (DX);                                            \
		int yy = sy + (DY);                                            \
		if (xx < 0)                                                    \
			xx = 0;                                                \
		if (yy < 0)                                                    \
			yy = 0;                                                \
		if (xx >= sw)                                                  \
			xx = sw - 1;                                           \
		if (yy >= sh)                                                  \
			yy = sh - 1;                                           \
		p = src + ((size_t)yy * (size_t)sw + (size_t)xx) * 4;          \
		RR = p[0];                                                     \
		GG = p[1];                                                     \
		BB = p[2];                                                     \
	} while (0)
		FETCH(0, 0, r00, g00, b00);
		FETCH(1, 0, r10, g10, b10);
		FETCH(0, 1, r01, g01, b01);
		FETCH(1, 1, r11, g11, b11);
#undef FETCH
		r0 = (r00 * (256 - tx) + r10 * tx) >> 8;
		g0 = (g00 * (256 - tx) + g10 * tx) >> 8;
		b0 = (b00 * (256 - tx) + b10 * tx) >> 8;
		r1 = (r01 * (256 - tx) + r11 * tx) >> 8;
		g1 = (g01 * (256 - tx) + g11 * tx) >> 8;
		b1 = (b01 * (256 - tx) + b11 * tx) >> 8;
		r = (r0 * (256 - ty) + r1 * ty) >> 8;
		g = (g0 * (256 - ty) + g1 * ty) >> 8;
		b = (b0 * (256 - ty) + b1 * ty) >> 8;
		o[0] = (uint8_t)r;
		o[1] = (uint8_t)g;
		o[2] = (uint8_t)b;
		o[3] = 255;
	}
}

static void
put_px(uint8_t *dst, int stride, int vis_w, int vis_h, int x, int y,
       const uint8_t *rgb)
{
	uint8_t *p;
	if ((unsigned)x >= (unsigned)vis_w || (unsigned)y >= (unsigned)vis_h)
		return;
	p = dst + ((size_t)y * (size_t)stride + (size_t)x) * 4;
	p[0] = rgb[0];
	p[1] = rgb[1];
	p[2] = rgb[2];
	p[3] = 255;
}

static void
blit_pane(uint8_t *dst, int stride, int vis_w, int vis_h, int x0, int y0,
	  int pw, int ph, const uint8_t *src, int sw, int sh, int bilinear)
{
	int x, y;
	for (y = 0; y < ph; y++) {
		int dy = y0 + y;
		uint8_t *row;
		if (dy < 0 || dy >= vis_h)
			continue;
		row = dst + ((size_t)dy * (size_t)stride) * 4;
		for (x = 0; x < pw; x++) {
			int dx = x0 + x;
			uint8_t o[4];
			if ((unsigned)dx >= (unsigned)vis_w)
				continue;
			sample_px(src, sw, sh, x, y, pw, ph, o, bilinear);
			row[(size_t)dx * 4] = o[0];
			row[(size_t)dx * 4 + 1] = o[1];
			row[(size_t)dx * 4 + 2] = o[2];
			row[(size_t)dx * 4 + 3] = 255;
		}
	}
}

static void
composite(uint8_t *dst, int stride, int vis_w, int vis_h, int bilinear)
{
	bus_t *A = &g_bus[0];
	bus_t *B = &g_bus[1];
	const uint8_t *sa = A->has ? A->rgba : NULL;
	const uint8_t *sb = B->has ? B->rgba : NULL;
	int split = g_split;
	if (split < 0 || split > vis_w)
		split = vis_w / 2;

	if (g_layout == LAY_SBS) {
		int mid = vis_w / 2;
		blit_pane(dst, stride, vis_w, vis_h, 0, 0, mid, vis_h, sa, A->w, A->h, bilinear);
		blit_pane(dst, stride, vis_w, vis_h, mid, 0, vis_w - mid, vis_h, sb, B->w, B->h, bilinear);
	} else if (g_layout == LAY_TB) {
		int mid = vis_h / 2;
		blit_pane(dst, stride, vis_w, vis_h, 0, 0, vis_w, mid, sa, A->w, A->h, bilinear);
		blit_pane(dst, stride, vis_w, vis_h, 0, mid, vis_w, vis_h - mid, sb, B->w, B->h, bilinear);
	} else if (g_layout == LAY_PIP) {
		int pw = vis_w / 3;
		int ph = vis_h / 3;
		int x0 = vis_w - pw - vis_w / 24;
		int y0 = vis_h - ph - vis_h / 8;
		blit_pane(dst, stride, vis_w, vis_h, 0, 0, vis_w, vis_h, sa, A->w, A->h, bilinear);
		blit_pane(dst, stride, vis_w, vis_h, x0, y0, pw, ph, sb, B->w, B->h, bilinear);
	} else if (g_layout == LAY_WIPE) {
		blit_pane(dst, stride, vis_w, vis_h, 0, 0, split, vis_h, sa, A->w, A->h, bilinear);
		if (split < vis_w)
			blit_pane(dst, stride, vis_w, vis_h, split, 0, vis_w - split, vis_h, sb, B->w, B->h, bilinear);
	} else {
		/* DIFF or SCREEN over the full frame. Two samples per pixel. */
		int x, y;
		for (y = 0; y < vis_h; y++) {
			uint8_t *row = dst + ((size_t)y * (size_t)stride) * 4;
			for (x = 0; x < vis_w; x++) {
				uint8_t a[4], b[4];
				sample_px(sa, A->w, A->h, x, y, vis_w, vis_h, a, bilinear);
				sample_px(sb, B->w, B->h, x, y, vis_w, vis_h, b, 0);
				if (g_layout == LAY_DIFF) {
					row[x * 4] = (uint8_t)(a[0] > b[0] ? a[0] - b[0] : b[0] - a[0]);
					row[x * 4 + 1] = (uint8_t)(a[1] > b[1] ? a[1] - b[1] : b[1] - a[1]);
					row[x * 4 + 2] = (uint8_t)(a[2] > b[2] ? a[2] - b[2] : b[2] - a[2]);
				} else {
					row[x * 4] = (uint8_t)(255 - ((255 - a[0]) * (255 - b[0])) / 255);
					row[x * 4 + 1] = (uint8_t)(255 - ((255 - a[1]) * (255 - b[1])) / 255);
					row[x * 4 + 2] = (uint8_t)(255 - ((255 - a[2]) * (255 - b[2])) / 255);
				}
				row[x * 4 + 3] = 255;
			}
		}
	}

	if (g_layout == LAY_SBS || g_layout == LAY_WIPE) {
		int x = (g_layout == LAY_SBS) ? vis_w / 2 : split;
		int y;
		for (y = 0; y < vis_h; y++) {
			put_px(dst, stride, vis_w, vis_h, x, y, COL_AMBER);
			put_px(dst, stride, vis_w, vis_h, x + 1, y, COL_AMBER);
		}
	} else if (g_layout == LAY_TB) {
		int y = vis_h / 2;
		int x;
		for (x = 0; x < vis_w; x++) {
			put_px(dst, stride, vis_w, vis_h, x, y, COL_AMBER);
			put_px(dst, stride, vis_w, vis_h, x, y + 1, COL_AMBER);
		}
	}
}

static void
draw_glyph(uint8_t *dst, int stride, int vis_w, int vis_h, int x, int y,
	   int ch, const uint8_t col[3], int scale)
{
	int row, colb, sy, sx;
	unsigned char bits;
	if (ch < 0 || ch > 127)
		ch = '?';
	for (row = 0; row < 8; row++) {
		bits = (unsigned char)font8x8_basic[ch][row];
		for (colb = 0; colb < 8; colb++) {
			if ((bits & (1u << colb)) == 0)
				continue;
			for (sy = 0; sy < scale; sy++) {
				for (sx = 0; sx < scale; sx++)
					put_px(dst, stride, vis_w, vis_h,
					       x + colb * scale + sx,
					       y + row * scale + sy, col);
			}
		}
	}
}

static void
draw_text(uint8_t *dst, int stride, int vis_w, int vis_h, int x, int y,
	  const char *s, const uint8_t col[3], int scale)
{
	int cx = x;
	for (; s && *s; s++) {
		if (*s == '\n') {
			y += 8 * scale + scale;
			cx = x;
			continue;
		}
		draw_glyph(dst, stride, vis_w, vis_h, cx, y, (unsigned char)*s, col, scale);
		cx += 8 * scale;
		if (cx > vis_w)
			break;
	}
}

static void
shade(uint8_t *dst, int stride, int vis_w, int vis_h, int x0, int y0, int w, int h)
{
	int x, y;
	for (y = 0; y < h; y++) {
		int dy = y0 + y;
		uint8_t *row;
		if (dy < 0 || dy >= vis_h)
			continue;
		row = dst + ((size_t)dy * (size_t)stride) * 4;
		for (x = 0; x < w; x++) {
			int dx = x0 + x;
			uint8_t *p;
			if ((unsigned)dx >= (unsigned)vis_w)
				continue;
			p = row + (size_t)dx * 4;
			p[0] = (uint8_t)(p[0] / 4);
			p[1] = (uint8_t)(p[1] / 4);
			p[2] = (uint8_t)(p[2] / 4);
		}
	}
}

static void
fmt_tc(char *out, size_t n, double t)
{
	int s, m, h;
	if (t < 0)
		t = 0;
	s = (int)t;
	h = s / 3600;
	m = (s / 60) % 60;
	s = s % 60;
	snprintf(out, n, "%02d:%02d:%02d", h, m, s);
}

static void
draw_hud(uint8_t *dst, int stride, int vis_w, int vis_h, int scale)
{
	char line1[160];
	char line2[160];
	char line3[180];
	char ta[16], tb[16];
	const char *am = g_audio_mode == 0 ? "A+B" : (g_audio_mode == 1 ? "A" : "B");
	const char *focus = g_focus == 0 ? "A" : "B";
	int bar_h = (8 * scale + scale) * 4 + 8 * scale;
	int y0 = vis_h - bar_h;
	double ta_s = 0, tb_s = 0;
	if (y0 < 0)
		y0 = 0;
	if (g_bus[0].plm)
		ta_s = plm_get_time(g_bus[0].plm);
	else if (g_bus[0].av)
		ta_s = avdec_time(g_bus[0].av);
	if (g_bus[1].plm)
		tb_s = plm_get_time(g_bus[1].plm);
	else if (g_bus[1].av)
		tb_s = avdec_time(g_bus[1].av);
	fmt_tc(ta, sizeof ta, ta_s);
	fmt_tc(tb, sizeof tb, tb_s);
	shade(dst, stride, vis_w, vis_h, 0, y0, vis_w, vis_h - y0);
	snprintf(line1, sizeof line1, "SPLICER  %s  audio %s  focus %s  %s%s",
		 LAY_NAME[g_layout], am, focus, g_status, g_paused ? "  PAUSED" : "");
	snprintf(line2, sizeof line2, "%c A  %-22.22s %s %s",
		 g_focus == 0 ? '>' : ' ', g_bus[0].name,
		 (g_bus[0].plm || g_bus[0].av) ? ta : "live",
		 g_bus[0].av ? avdec_mode(g_bus[0].av) : (g_bus[0].plm ? "M1" : ""));
	snprintf(line3, sizeof line3, "%c B  %-22.22s %s %s   %d reel%s",
		 g_focus == 1 ? '>' : ' ', g_bus[1].name,
		 (g_bus[1].plm || g_bus[1].av) ? tb : "live",
		 g_bus[1].av ? avdec_mode(g_bus[1].av) : (g_bus[1].plm ? "M1" : ""),
		 g_nfiles, g_nfiles == 1 ? "" : "s");
	draw_text(dst, stride, vis_w, vis_h, 12 * scale, y0 + 4 * scale, line1, COL_AMBER, scale);
	draw_text(dst, stride, vis_w, vis_h, 12 * scale, y0 + (12 * scale), line2, COL_PAPER, scale);
	draw_text(dst, stride, vis_w, vis_h, 12 * scale, y0 + (20 * scale), line3, COL_PAPER, scale);
	draw_text(dst, stride, vis_w, vis_h, 12 * scale, y0 + (28 * scale),
		  "L2 plugins  Cross pause  Up/Down layout  L1/R1 clip  Sqr focus  Cir audio  Tri swap  Opt rescan",
		  COL_SAGE, scale > 1 ? scale - 1 : 1);
	if (g_nfiles == 0) {
		int cx = vis_w / 2 - 18 * 8 * scale;
		int cy = vis_h / 2 - 20 * scale;
		if (cx < 16)
			cx = 16;
		shade(dst, stride, vis_w, vis_h, cx - 12, cy - 12, 40 * 8 * scale, 8 * scale * 5);
		draw_text(dst, stride, vis_w, vis_h, cx, cy, "NO REELS ON USB", COL_AMBER, scale);
		draw_text(dst, stride, vis_w, vis_h, cx, cy + 10 * scale,
			  "Put a video on /mnt/usb0", COL_PAPER, scale);
		draw_text(dst, stride, vis_w, vis_h, cx, cy + 20 * scale,
			  "MPEG-1, or H.264/HEVC in MP4 MKV TS", COL_PAPER, scale);
	}
	draw_text(dst, stride, vis_w, vis_h, 16, 16, "A", COL_AMBER, scale);
	draw_text(dst, stride, vis_w, vis_h, g_layout == LAY_SBS ? vis_w / 2 + 16 : 16,
		  g_layout == LAY_TB ? vis_h / 2 + 16 : 16, "B", COL_SAGE, scale);
}

static void
load_optional(void)
{
	void *a;
	void *u;
	void *p;
	if (g_opt_loaded)
		return;
	g_opt_loaded = 1;
	a = dlopen("libSceAudioOut.sprx", RTLD_LAZY);
	if (a) {
		p_sceAudioOutInit = dlsym(a, "sceAudioOutInit");
		p_sceAudioOutOpen = dlsym(a, "sceAudioOutOpen");
		p_sceAudioOutOutput = dlsym(a, "sceAudioOutOutput");
		p_sceAudioOutSetVolume = dlsym(a, "sceAudioOutSetVolume");
		p_sceAudioOutClose = dlsym(a, "sceAudioOutClose");
	}
	u = dlopen("libSceUserService.sprx", RTLD_LAZY);
	if (u) {
		p_sceUserServiceInitialize = dlsym(u, "sceUserServiceInitialize");
		p_sceUserServiceGetInitialUser = dlsym(u, "sceUserServiceGetInitialUser");
	}
	p = dlopen("libScePad.sprx", RTLD_LAZY);
	if (p) {
		p_scePadInit = dlsym(p, "scePadInit");
		p_scePadOpen = dlsym(p, "scePadOpen");
		p_scePadReadState = dlsym(p, "scePadReadState");
	}
}

static int
open_audio(int user)
{
	static const unsigned grains[] = {256, 512};
	int gi, ui;
	int users[2];
	if (!p_sceAudioOutOpen)
		return -1;
	users[0] = user;
	users[1] = 0xFF;
	if (p_sceAudioOutInit)
		p_sceAudioOutInit();
	for (gi = 0; gi < 2; gi++) {
		for (ui = 0; ui < 2; ui++) {
			int h = p_sceAudioOutOpen(users[ui], 0, 0, grains[gi], 48000, 1);
			if (h >= 0) {
				int vol[2];
				vol[0] = vol[1] = 26000;
				if (p_sceAudioOutSetVolume)
					p_sceAudioOutSetVolume(h, 3, vol);
				return h;
			}
		}
	}
	return -1;
}

static int
open_pad(int *user_out)
{
	int user = 0xFF;
	int u = 0;
	int h;
	*user_out = user;
	if (!p_scePadOpen || !p_scePadInit)
		return -1;
	if (p_sceUserServiceInitialize)
		p_sceUserServiceInitialize(NULL);
	if (p_sceUserServiceGetInitialUser && p_sceUserServiceGetInitialUser(&u) == 0 && u != 0)
		user = u;
	*user_out = user;
	p_scePadInit();
	h = p_scePadOpen(user, 0, 0, NULL);
	if (h < 0)
		h = p_scePadOpen(0xFF, 0, 0, NULL);
	return h;
}

static void
fill_rect_px(uint8_t *dst, int stride, int w, int h, int x0, int y0, int rw, int rh, uint32_t pix)
{
	int y, x;
	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x0 + rw > w)
		rw = w - x0;
	if (y0 + rh > h)
		rh = h - y0;
	for (y = 0; y < rh; y++) {
		uint32_t *row = (uint32_t *)(dst + ((size_t)(y0 + y) * (size_t)stride + (size_t)x0) * 4);
		for (x = 0; x < rw; x++)
			row[x] = pix;
	}
}

static void
proof_pattern(uint8_t *dst, int stride, int w, int h, int frame, int scale)
{
	static const uint32_t cols[5] = {
		0xFF0000FFu, /* red   R */
		0xFF00FF00u, /* green G */
		0xFFFF0000u, /* blue  B */
		0xFFFFFFFFu, /* white */
		0xFF5AA1E2u  /* amber */
	};
	int i;
	int bw = w / 5;
	char line[64];
	for (i = 0; i < 5; i++) {
		int x0 = i * bw;
		int rw = (i == 4) ? (w - x0) : bw;
		fill_rect_px(dst, stride, w, h, x0, 0, rw, h, cols[i]);
	}
	snprintf(line, sizeof line, "SPLICER  %d", frame);
	draw_text(dst, stride, w, h, w / 8, h / 2 - 16 * scale, "SPLICER", (const uint8_t *)"\x10\x10\x10", scale * 3);
	draw_text(dst, stride, w, h, w / 8, h / 2 + 20 * scale, line, (const uint8_t *)"\x10\x10\x10", scale);
	draw_text(dst, stride, w, h, w / 8, h / 2 + 36 * scale, "PAYLOAD RUNNING", (const uint8_t *)"\x10\x10\x10", scale);
}

static int
stream_url(const char *url)
{
	return url && (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0 ||
		       strncmp(url, "hls:", 4) == 0);
}

static void
plug_job(void)
{
	if (g_plug_job == 1) {
		if (plug_activate(g_plug_i) == 0) {
			g_plug_view = 1;
			g_plug_i = 0;
			notify("SPLICER plugin page");
		} else {
			notify(plug_status());
		}
	} else if (g_plug_job == 2) {
		const char *url = plug_item_url(g_plug_i);
		const char *title = plug_item_title(g_plug_i);
		struct stat st;
		if (is_bt_src(url)) {
			if (bt_start(url) == 0) {
				bus_live(&g_bus[g_focus], 2, bt_name());
				g_bt_opened = 0;
				g_plug = 0;
				notify("SPLICER torrent");
			} else {
				notify("SPLICER torrent rejected");
			}
		} else if (stream_url(url)) {
			snprintf(g_stream, sizeof g_stream, "%s", url);
			snprintf(g_stream_name, sizeof g_stream_name, "%s", title);
			g_plug_view = 2;
			g_plug_i = 0;
		} else if (url && url[0] == '/' && stat(url, &st) == 0 && S_ISREG(st.st_mode)) {
			bus_open_file(&g_bus[g_focus], url, 0);
			g_plug = 0;
			notify("SPLICER opened file");
		} else if (url && url[0] && plug_open(url) == 0) {
			g_plug_view = 1;
			g_plug_i = 0;
		} else if (url && url[0]) {
			notify(plug_status());
		}
	} else if (g_plug_job == 3) {
		if (plug_back() != 0)
			g_plug_view = 0;
		g_plug_i = 0;
	}
	g_plug_wait = 0;
}

static void clear_frame(uint8_t *dst, int stride, int vis_w, int vis_h);

static void
draw_plugins(uint8_t *dst, int stride, int vis_w, int vis_h, int scale)
{
	char ascii[200];
	char line[220];
	int n = 0;
	int rows;
	int start;
	int i;
	int pitch = 10 * scale;
	int y;
	int chars;
	if (pitch < 8)
		pitch = 8;
	rows = (vis_h - 36 * scale) / pitch;
	if (rows < 3)
		rows = 3;
	if (rows > 22)
		rows = 22;
	chars = vis_w / (8 * scale) - 4;
	if (chars < 16)
		chars = 16;
	if (chars > 180)
		chars = 180;
	clear_frame(dst, stride, vis_w, vis_h);
	draw_text(dst, stride, vis_w, vis_h, 16 * scale, 12 * scale, "PLUGINS", COL_AMBER, scale);
	if (g_plug_wait) {
		draw_text(dst, stride, vis_w, vis_h, 16 * scale, 28 * scale, "LOADING", COL_PAPER, scale);
		return;
	}
	if (g_plug_view == 2) {
		plug_ascii(g_stream_name, ascii, sizeof ascii);
		draw_text(dst, stride, vis_w, vis_h, 16 * scale, 28 * scale, ascii, COL_PAPER, scale);
		draw_text(dst, stride, vis_w, vis_h, 16 * scale, 40 * scale,
			  "Stream address. This build does not decode HTTP yet.", COL_SAGE, scale);
		{
			const char *p = g_stream;
			int row = 0;
			while (*p && row < rows) {
				int k = 0;
				while (p[k] && k < chars)
					k++;
				snprintf(line, sizeof line, "%.*s", k, p);
				draw_text(dst, stride, vis_w, vis_h, 16 * scale, (52 + row * 10) * scale, line, COL_PAPER, scale);
				p += k;
				row++;
			}
		}
		draw_text(dst, stride, vis_w, vis_h, 16 * scale, vis_h - 16 * scale, "Circle back   L2 close", COL_SAGE, scale);
		return;
	}
	if (g_plug_view == 0) {
		n = plug_count();
		snprintf(line, sizeof line, "%s", plug_status());
		draw_text(dst, stride, vis_w, vis_h, 16 * scale, 28 * scale, line, COL_SAGE, scale);
		if (n <= 0) {
			draw_text(dst, stride, vis_w, vis_h, 16 * scale, 48 * scale, "No plugins on USB", COL_PAPER, scale);
			draw_text(dst, stride, vis_w, vis_h, 16 * scale, 60 * scale, "Put packages in /mnt/usb0/plugins", COL_PAPER, scale);
		}
	} else {
		n = plug_item_count();
		plug_ascii(plug_page_title(), ascii, sizeof ascii);
		draw_text(dst, stride, vis_w, vis_h, 16 * scale, 28 * scale, ascii[0] ? ascii : "Page", COL_PAPER, scale);
		if (plug_page_error()[0]) {
			plug_ascii(plug_page_error(), ascii, sizeof ascii);
			draw_text(dst, stride, vis_w, vis_h, 16 * scale, 40 * scale, ascii, COL_AMBER, scale);
		}
	}
	if (n > 0) {
		if (g_plug_i >= n)
			g_plug_i = n - 1;
		if (g_plug_i < 0)
			g_plug_i = 0;
		start = g_plug_i - rows / 2;
		if (start < 0)
			start = 0;
		if (start + rows > n)
			start = n > rows ? n - rows : 0;
		for (i = 0; i < rows && start + i < n; i++) {
			int idx = start + i;
			int yy = (g_plug_view == 0 ? 44 : 52) * scale + i * pitch;
			const char *label = g_plug_view == 0 ? plug_title(idx) : plug_item_title(idx);
			plug_ascii(label, ascii, sizeof ascii);
			if (idx == g_plug_i)
				fill_rect_px(dst, stride, vis_w, vis_h, 8 * scale, yy - scale, vis_w - 16 * scale, 8 * scale, 0xFF18405Au);
			snprintf(line, sizeof line, "%c %s", idx == g_plug_i ? '>' : ' ', ascii);
			draw_text(dst, stride, vis_w, vis_h, 12 * scale, yy, line,
				  idx == g_plug_i ? COL_AMBER : COL_PAPER, scale);
		}
	}
	y = vis_h - 16 * scale;
	draw_text(dst, stride, vis_w, vis_h, 16 * scale, y,
		  g_plug_view == 0 ? "Up/Down  Cross open  L2 close" : "Up/Down  Cross open  Circle back  L2 close",
		  COL_SAGE, scale);
}

static void
border(uint8_t *dst, int stride, int w, int h, int t, uint32_t pix)
{
	fill_rect_px(dst, stride, w, h, 0, 0, w, t, pix);
	fill_rect_px(dst, stride, w, h, 0, h - t, w, t, pix);
	fill_rect_px(dst, stride, w, h, 0, 0, t, h, pix);
	fill_rect_px(dst, stride, w, h, w - t, 0, t, h, pix);
}

static void
clear_frame(uint8_t *dst, int stride, int vis_w, int vis_h)
{
	fill_rect_px(dst, stride, vis_w, vis_h, 0, 0, vis_w, vis_h, 0xff2a2824u);
}

static void
fail_loop(const char *msg)
{
	int n = 0;
	for (;;) {
		char b[160];
		struct timespec ts;
		snprintf(b, sizeof b, "SPLICER %s #%d", msg, n++);
		notify(b);
		ts.tv_sec = 2;
		ts.tv_nsec = 0;
		nanosleep(&ts, NULL);
	}
}

static int
alloc_direct(void **vaddr, intptr_t *paddr, int *memsize_out)
{
	static const int sizes[] = {0x08000000, 0x04000000, 0x20000000, 0x02000000};
	int i;
	int align = 0x20000;
	for (i = 0; i < 4; i++) {
		intptr_t pa = 0;
		void *va = NULL;
		if (sceKernelAllocateMainDirectMemory((size_t)sizes[i], (size_t)align, 3, &pa))
			continue;
		if (sceKernelMapDirectMemory(&va, (size_t)sizes[i], 0x33, 0, pa, (size_t)align))
			continue;
		*vaddr = va;
		*paddr = pa;
		*memsize_out = sizes[i];
		return 0;
	}
	return -1;
}

static int
render_loop(void)
{
	vout_buf_t vbuf[2];
	vout_attr_t vattr;
	vout_stat_t vstat;
	struct kevent *evt_queue = NULL;
	struct kevent evt;
	int vout;
	int memsize = 0;
	intptr_t paddr = 0;
	void *vaddr = NULL;
	uint32_t frame_id = 0;
	int vis_w = 1920;
	int vis_h = 1080;
	int stride;
	int bilinear;
	int scale;
	int pad = -1;
	int user = 0xFF;
	uint32_t prev_btn = 0;
	int opt_hold = 0;
	int extras = 0;
	double tprev;
	uint8_t padbuf[256];

	memset(vbuf, 0, sizeof vbuf);
	memset(&vattr, 0, sizeof vattr);
	memset(&vstat, 0, sizeof vstat);

	if (alloc_direct(&vaddr, &paddr, &memsize))
		fail_loop("direct memory failed");

	vout = sceVideoOutOpen(0xff, 0, 0, NULL);
	if (vout < 0)
		fail_loop("sceVideoOutOpen failed");
	if (sceVideoOutGetOutputStatus(vout, &vstat))
		fail_loop("output status failed");
	if (sceKernelCreateEqueue(&evt_queue, "splicer flip"))
		fail_loop("equeue failed");
	if (sceVideoOutAddFlipEvent(evt_queue, vout, NULL))
		fail_loop("flip event failed");
	sceVideoOutSetFlipRate(vout, 0);
	if (vstat.res == 2) {
		vis_w *= 2;
		vis_h *= 2;
	}
	/* A 4K frame pair does not fit in the smaller fallback heaps. */
	if ((size_t)vis_w * (size_t)vis_h * 4u * 2u + 0x200000u > (size_t)memsize) {
		vis_w = 1920;
		vis_h = 1080;
	}
	sceVideoOutSetBufferAttribute2(&vattr, 0x8000000022000000UL, 0,
				       (uint32_t)vis_w, (uint32_t)vis_h, 0, 0, 0);
	vbuf[0].data = vaddr;
	vbuf[1].data = (uint8_t *)vaddr + (memsize / 2);
	if (sceVideoOutRegisterBuffers2(vout, 0, 0, vbuf, 2, &vattr, 0, NULL))
		fail_loop("register buffers failed");

	stride = (vis_w + 0x3f) & ~0x3f;
	bilinear = (vis_w * vis_h) <= (1920 * 1200);
	scale = vis_h >= 2000 ? 3 : (vis_h >= 1000 ? 2 : 1);
	snprintf(g_status, sizeof g_status, "flip %dx%d", vis_w, vis_h);
	notify("SPLICER on screen");

	tprev = now_sec();
	while (g_run) {
		double now = now_sec();
		double dt = now - tprev;
		uint8_t idx;
		uint32_t btn = 0;
		uint32_t down;
		int junk = 0;
		tprev = now;
		if (dt < 0)
			dt = 0;
		if (dt > 0.1)
			dt = 0.1;

		idx = (uint8_t)(frame_id & 1);

		/* Nothing else runs until color bars have actually flipped. */
		if (frame_id < 180) {
			proof_pattern((uint8_t *)vbuf[idx].data, stride, vis_w, vis_h,
				      (int)frame_id, scale);
		} else {
			if (frame_id == 180) {
				notify("SPLICER scanning USB");
				hwdec_probe();
				g_nfiles = 0;
				scan_media();
				load_buses();
				snprintf(g_status, sizeof g_status, "%d reels %s", g_nfiles,
					 hwdec_status());
				notify(hwdec_ready() ? "SPLICER hardware decoder" :
						       "SPLICER software decoder");
			}
			if (!extras) {
				load_optional();
				pad = open_pad(&user);
				g_audio = open_audio(user);
				extras = 1;
				notify(pad >= 0 ? "SPLICER pad ok" : "SPLICER pad off, picture stays");
			}
			if (pad >= 0 && p_scePadReadState && p_scePadReadState(pad, padbuf) == 0) {
				uint8_t rx;
				btn = (uint32_t)padbuf[0] | ((uint32_t)padbuf[1] << 8) |
				      ((uint32_t)padbuf[2] << 16) | ((uint32_t)padbuf[3] << 24);
				rx = padbuf[6];
				if (rx < 108 || rx > 148) {
					g_split = ((int)rx * vis_w) / 255;
					if (g_layout != LAY_WIPE && (rx < 70 || rx > 186))
						g_layout = LAY_WIPE;
				}
			}
			down = btn & ~prev_btn;
			prev_btn = btn;
			if (btn & BTN_OPTIONS)
				opt_hold++;
			else
				opt_hold = 0;
			if (opt_hold > 100) {
				g_run = 0;
				break;
			}
			if (down & BTN_L2) {
				g_plug = !g_plug;
				g_plug_wait = 0;
				if (g_plug) {
					plug_scan();
					g_plug_i = 0;
					g_plug_view = 0;
					notify(plug_count() ? "SPLICER plugins" : "SPLICER no plugins on USB");
				}
			}
			if (g_plug) {
				int n = g_plug_view == 0 ? plug_count() :
					(g_plug_view == 1 ? plug_item_count() : 0);
				if (g_plug_wait == 1)
					plug_job();
				else if (g_plug_view != 2) {
					if ((down & BTN_UP) && n > 0) {
						g_plug_i--;
						if (g_plug_i < 0)
							g_plug_i = n - 1;
					}
					if ((down & BTN_DOWN) && n > 0) {
						g_plug_i++;
						if (g_plug_i >= n)
							g_plug_i = 0;
					}
					if ((down & BTN_CROSS) && n > 0) {
						g_plug_job = g_plug_view == 0 ? 1 : 2;
						g_plug_wait = 1;
					}
				}
				if (!g_plug_wait && (down & BTN_CIR)) {
					if (g_plug_view == 2) {
						g_plug_view = 1;
						g_plug_i = 0;
					} else if (g_plug_view == 1) {
						g_plug_job = 3;
						g_plug_wait = 1;
					} else {
						g_plug = 0;
					}
				}
			} else {
				if (opt_hold == 1) {
					g_nfiles = 0;
					scan_media();
					load_buses();
				}
				if (down & BTN_UP)
					g_layout = (g_layout + LAY_COUNT - 1) % LAY_COUNT;
				if (down & BTN_DOWN)
					g_layout = (g_layout + 1) % LAY_COUNT;
				if (down & BTN_CROSS)
					g_paused = !g_paused;
				if (down & BTN_SQR)
					g_focus ^= 1;
				if (down & BTN_CIR)
					g_audio_mode = (g_audio_mode + 1) % 3;
				if (down & BTN_TRI) {
					int tmp = g_idx[0];
					g_idx[0] = g_idx[1];
					g_idx[1] = tmp;
					load_buses();
				}
				if (down & BTN_L1)
					shift_file(g_focus, -1);
				if (down & BTN_R1)
					shift_file(g_focus, +1);
				if (down & BTN_LEFT)
					seek_bus(&g_bus[g_focus], -5.0);
				if (down & BTN_RIGHT)
					seek_bus(&g_bus[g_focus], 5.0);
			}

			if (g_bt_cool > 0)
				g_bt_cool--;
			bt_pump(0);
			if (bt_active()) {
				int tb;
				snprintf(g_status, sizeof g_status, "%s", bt_line());
				for (tb = 0; tb < 2; tb++) {
					if (g_bus[tb].live && g_bus[tb].kind == 2)
						snprintf(g_bus[tb].name, sizeof g_bus[tb].name, "%.60s",
							 bt_name());
				}
				if (!g_bt_opened && g_bt_cool <= 0 && bt_playable()) {
					int which = -1;
					for (tb = 0; tb < 2; tb++) {
						if (g_bus[tb].live && g_bus[tb].kind == 2)
							which = tb;
					}
					if (which >= 0) {
						if (bus_open_bt(&g_bus[which]) == 0) {
							g_bt_opened = 1;
							notify("SPLICER torrent");
						} else {
							g_bt_cool = 45;
						}
					}
				}
			}

			if (!g_paused) {
				int i;
				for (i = 0; i < 2; i++) {
					if (g_bus[i].plm)
						plm_decode(g_bus[i].plm, dt);
					else if (g_bus[i].av)
						avdec_tick(g_bus[i].av, dt);
					else if (g_bus[i].live)
						render_live(&g_bus[i], now);
				}
			}
			clear_frame((uint8_t *)vbuf[idx].data, stride, vis_w, vis_h);
			if (g_plug) {
				draw_plugins((uint8_t *)vbuf[idx].data, stride, vis_w, vis_h, scale);
			} else {
				composite((uint8_t *)vbuf[idx].data, stride, vis_w, vis_h, bilinear);
				draw_hud((uint8_t *)vbuf[idx].data, stride, vis_w, vis_h, scale);
			}
			border((uint8_t *)vbuf[idx].data, stride, vis_w, vis_h, 10, 0xFF5AA1E2u);
		}

		if (sceVideoOutSubmitFlip(vout, idx, 1, frame_id))
			fail_loop("flip failed");
		if (sceKernelWaitEqueue(evt_queue, &evt, 1, &junk, 0))
			fail_loop("wait flip failed");
		if (frame_id >= 180)
			pump_audio();
		frame_id++;
	}

	g_run = 0;
	if (g_audio >= 0 && p_sceAudioOutClose)
		p_sceAudioOutClose(g_audio);
	return 0;
}

int
main(void)
{
	sceSystemServiceHideSplashScreen();
	notify("SPLICER payload started");
	bus_init(&g_bus[0]);
	bus_init(&g_bus[1]);
	g_idx[0] = 0;
	g_idx[1] = 1;
	bus_live(&g_bus[0], 0, "REEL A");
	bus_live(&g_bus[1], 1, "REEL B");
	return render_loop();
}
