/* Options screen. M7 stores the alternate plugin feed in
 * pluginconf/alt_repo (see andoma/movian src/plugins.c). This file is
 * that setting, the three plugin settings beside it, and the playback
 * knobs the payload already applies.
 */
#include "options.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define OPT_REPO_MAX 80
#define OPT_URL 700
#define OPT_ID 64
#define OPT_TITLE 96
#define Z_OK 0
#define Z_STREAM_END 1
#define Z_FINISH 4

typedef struct z_stream_s {
	const unsigned char *next_in;
	unsigned int avail_in;
	unsigned long total_in;
	unsigned char *next_out;
	unsigned int avail_out;
	unsigned long total_out;
	const char *msg;
	void *state;
	void *(*zalloc)(void *, unsigned, unsigned);
	void (*zfree)(void *, void *);
	void *opaque;
	int data_type;
	unsigned long adler;
	unsigned long reserved;
} z_stream;

int inflateInit2_(z_stream *, int, const char *, int);
int inflate(z_stream *, int);
int inflateEnd(z_stream *);

enum { VIEW_MENU = 0, VIEW_KEYS, VIEW_REPO };
enum { JOB_NONE = 0, JOB_FETCH, JOB_INSTALL, JOB_UPGRADE };
enum { ROW_SEC = 0, ROW_STR, ROW_BOOL, ROW_CYCLE, ROW_ACT };

enum {
	ID_REPO = 1,
	ID_BETA,
	ID_UP,
	ID_DEMAND,
	ID_BROWSE,
	ID_UPGRADE,
	ID_SPEED,
	ID_SEEK,
	ID_AUDIO,
	ID_LAYOUT,
	ID_DEC,
	ID_RESCAN
};

typedef struct {
	int kind;
	int id;
	const char *title;
} row_def_t;

typedef struct {
	char id[OPT_ID];
	char title[OPT_TITLE];
	char version[32];
	char file[96];
	char url[OPT_URL];
	char category[32];
} repo_item_t;

typedef struct {
	int index;
	char text[96];
} pref_t;

#define PREF_MAX 160

static const row_def_t ROWS[] = {
	{ROW_SEC, 0, "GENERAL"},
	{ROW_STR, ID_REPO, "Alternate plugin Repository URL"},
	{ROW_STR, ID_BETA, "Beta testing passwords"},
	{ROW_BOOL, ID_UP, "Automatically upgrade plugins"},
	{ROW_BOOL, ID_DEMAND, "Auto install plugins on demand"},
	{ROW_ACT, ID_BROWSE, "Browse available plugins"},
	{ROW_ACT, ID_UPGRADE, "Upgrade installed plugins"},
	{ROW_SEC, 0, "PLAYBACK"},
	{ROW_CYCLE, ID_SPEED, "Playback speed"},
	{ROW_CYCLE, ID_SEEK, "Seek step"},
	{ROW_CYCLE, ID_AUDIO, "Audio"},
	{ROW_CYCLE, ID_LAYOUT, "Layout"},
	{ROW_CYCLE, ID_DEC, "Hardware decoding"},
	{ROW_ACT, ID_RESCAN, "Rescan USB"},
};
#define ROW_N ((int)(sizeof ROWS / sizeof ROWS[0]))

static const double SPEEDS[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
#define SPEED_N 6
static const int SEEKS[] = {5, 10, 30, 60};
#define SEEK_N 4
static const char *AUDIO_NAME[] = {"A+B", "A", "B"};
static const char *LAY_NAME[] = {
	"SIDE BY SIDE", "TOP / BOTTOM", "PICTURE IN PICTURE", "WIPE", "DIFFERENCE", "SCREEN"
};
#define LAY_N 6
static const char *DEC_NAME[] = {"Auto", "Software"};

static const char *PRESET[] = {
	"",
	"https://bit.ly/mprepo",
	"https://raw.githubusercontent.com/brinza666/movian_ps5/main/plugins/repo.json",
	"https://repo.movian.eu/plugins-v1.json",
};
#define PRESET_N 4

static const char *KEY_ROW[] = {
	"1234567890",
	"qwertyuiop",
	"asdfghjkl",
	"zxcvbnm.:/",
	"-_~?#@%+",
};
#define KEY_ROWS 5

static char g_settings_path[512];
static char g_install_root[512];
static char g_alt[OPT_URL];
static char g_beta[180];
static int g_upgrade = 1;
static int g_demand = 1;
static int g_speed = 2;
static int g_seek = 0;
static int g_audio = 0;
static int g_layout = 0;
static int g_dec = 0;
static int g_open;
static int g_view;
static int g_cursor;
static int g_kx, g_ky, g_shift;
static char g_edit[OPT_URL];
static int g_edit_id;
static int g_job;
static int g_armed;
static int g_rescan;
static int g_reopen;
static int g_upgrade_i;
static int g_then_upgrade;
static char g_status[240];
static repo_item_t g_repo[OPT_REPO_MAX];
static int g_repo_n;
static int g_repo_i;
static pref_t g_pref[PREF_MAX];
static int g_pref_n;

static void
copy_str(char *dst, size_t n, const char *src)
{
	size_t i;
	if (!dst || n == 0)
		return;
	if (!src)
		src = "";
	for (i = 0; i + 1 < n && src[i]; i++)
		dst[i] = src[i];
	dst[i] = 0;
}

static void
set_status(const char *s)
{
	copy_str(g_status, sizeof g_status, s);
}

static void
mkdirs(const char *path)
{
	char tmp[512];
	size_t len;
	char *p;
	if (!path || !path[0])
		return;
	len = strlen(path);
	if (len >= sizeof tmp)
		return;
	memcpy(tmp, path, len + 1);
	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = 0;
			mkdir(tmp, 0755);
			*p = '/';
		}
	}
	mkdir(tmp, 0755);
}

static int
writable_file(const char *path)
{
	FILE *f;
	char dir[512];
	const char *slash;
	if (!path || !path[0])
		return 0;
	slash = strrchr(path, '/');
	if (slash && slash != path) {
		size_t n = (size_t)(slash - path);
		if (n >= sizeof dir)
			return 0;
		memcpy(dir, path, n);
		dir[n] = 0;
		mkdirs(dir);
	}
	f = fopen(path, "ab");
	if (!f)
		return 0;
	fclose(f);
	return 1;
}

static const char *
settings_path(void)
{
	static const char *cands[] = {
		"/data/splicer/settings.txt",
		"/mnt/usb0/splicer-settings.txt",
		"/mnt/usb1/splicer-settings.txt",
		"/user/splicer/settings.txt",
		NULL
	};
	int i;
	if (g_settings_path[0])
		return g_settings_path;
	for (i = 0; cands[i]; i++) {
		FILE *f = fopen(cands[i], "rb");
		if (f) {
			fclose(f);
			return cands[i];
		}
	}
	for (i = 0; cands[i]; i++) {
		if (writable_file(cands[i]))
			return cands[i];
	}
	return cands[0];
}

static void
save_settings(void)
{
	const char *path = settings_path();
	char tmp[540];
	FILE *f;
	if (!path)
		return;
	snprintf(tmp, sizeof tmp, "%s.tmp", path);
	f = fopen(tmp, "wb");
	if (!f)
		return;
	fprintf(f, "alt_repo=%s\n", g_alt);
	fprintf(f, "beta=%s\n", g_beta);
	fprintf(f, "autoupgrade=%d\n", g_upgrade ? 1 : 0);
	fprintf(f, "autoinstall=%d\n", g_demand ? 1 : 0);
	fprintf(f, "speed=%d\n", g_speed);
	fprintf(f, "seek=%d\n", g_seek);
	fprintf(f, "audio=%d\n", g_audio);
	fprintf(f, "layout=%d\n", g_layout);
	fprintf(f, "decoder=%d\n", g_dec);
	fclose(f);
	rename(tmp, path);
}

static void
apply_kv(const char *key, const char *val)
{
	if (strcmp(key, "alt_repo") == 0)
		copy_str(g_alt, sizeof g_alt, val);
	else if (strcmp(key, "beta") == 0)
		copy_str(g_beta, sizeof g_beta, val);
	else if (strcmp(key, "autoupgrade") == 0)
		g_upgrade = atoi(val) ? 1 : 0;
	else if (strcmp(key, "autoinstall") == 0)
		g_demand = atoi(val) ? 1 : 0;
	else if (strcmp(key, "speed") == 0) {
		g_speed = atoi(val);
		if (g_speed < 0 || g_speed >= SPEED_N)
			g_speed = 2;
	} else if (strcmp(key, "seek") == 0) {
		g_seek = atoi(val);
		if (g_seek < 0 || g_seek >= SEEK_N)
			g_seek = 0;
	} else if (strcmp(key, "audio") == 0) {
		g_audio = atoi(val);
		if (g_audio < 0 || g_audio > 2)
			g_audio = 0;
	} else if (strcmp(key, "layout") == 0) {
		g_layout = atoi(val);
		if (g_layout < 0 || g_layout >= LAY_N)
			g_layout = 0;
	} else if (strcmp(key, "decoder") == 0)
		g_dec = atoi(val) ? 1 : 0;
}

static void
load_settings(void)
{
	const char *path = settings_path();
	FILE *f;
	char line[OPT_URL + 32];
	g_upgrade = 1;
	g_demand = 1;
	g_speed = 2;
	g_seek = 0;
	g_audio = 0;
	g_layout = 0;
	g_dec = 0;
	g_alt[0] = 0;
	g_beta[0] = 0;
	if (!path)
		return;
	f = fopen(path, "rb");
	if (!f)
		return;
	while (fgets(line, sizeof line, f)) {
		char *eq;
		size_t n = strlen(line);
		while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
			line[--n] = 0;
		if (line[0] == '#' || line[0] == 0)
			continue;
		eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = 0;
		apply_kv(line, eq + 1);
	}
	fclose(f);
}

void
opt_set_paths(const char *settings_file, const char *install_root)
{
	copy_str(g_settings_path, sizeof g_settings_path, settings_file);
	copy_str(g_install_root, sizeof g_install_root, install_root);
}

void
opt_boot(void)
{
	load_settings();
	set_status(g_alt[0] ? "alternate repository set" : "official repository");
}

static int
row_skips(int i)
{
	return i < 0 || i >= ROW_N || ROWS[i].kind == ROW_SEC;
}

static void
move_cursor(int dir)
{
	int i;
	if (g_view != VIEW_MENU)
		return;
	for (i = 0; i < ROW_N; i++) {
		g_cursor = (g_cursor + dir + ROW_N) % ROW_N;
		if (!row_skips(g_cursor))
			return;
	}
}

static const char *
active_repo(void)
{
	return g_alt[0] ? g_alt : PRESET[3];
}

static void
cycle_preset(int dir)
{
	int i, cur = -1;
	for (i = 0; i < PRESET_N; i++) {
		if (strcmp(g_alt, PRESET[i]) == 0)
			cur = i;
	}
	if (cur < 0)
		cur = dir > 0 ? -1 : PRESET_N;
	cur = (cur + dir + PRESET_N) % PRESET_N;
	copy_str(g_alt, sizeof g_alt, PRESET[cur]);
	save_settings();
	set_status(g_alt[0] ? g_alt : "official repository");
}

static void
cycle_id(int id, int dir)
{
	if (id == ID_SPEED) {
		g_speed = (g_speed + dir + SPEED_N) % SPEED_N;
	} else if (id == ID_SEEK) {
		g_seek = (g_seek + dir + SEEK_N) % SEEK_N;
	} else if (id == ID_AUDIO) {
		g_audio = (g_audio + dir + 3) % 3;
	} else if (id == ID_LAYOUT) {
		g_layout = (g_layout + dir + LAY_N) % LAY_N;
	} else if (id == ID_DEC) {
		g_dec = g_dec ? 0 : 1;
		g_reopen = 1;
	}
	save_settings();
}

static void
value_of(int id, char *dst, size_t n)
{
	if (id == ID_REPO)
		snprintf(dst, n, "%s", g_alt[0] ? g_alt : "(official)");
	else if (id == ID_BETA)
		snprintf(dst, n, "%s", g_beta[0] ? g_beta : "(none)");
	else if (id == ID_UP)
		snprintf(dst, n, "%s", g_upgrade ? "On" : "Off");
	else if (id == ID_DEMAND)
		snprintf(dst, n, "%s", g_demand ? "On" : "Off");
	else if (id == ID_SPEED)
		snprintf(dst, n, "%.2gx", SPEEDS[g_speed]);
	else if (id == ID_SEEK)
		snprintf(dst, n, "%d s", SEEKS[g_seek]);
	else if (id == ID_AUDIO)
		snprintf(dst, n, "%s", AUDIO_NAME[g_audio]);
	else if (id == ID_LAYOUT)
		snprintf(dst, n, "%s", LAY_NAME[g_layout]);
	else if (id == ID_DEC)
		snprintf(dst, n, "%s", DEC_NAME[g_dec]);
	else
		dst[0] = 0;
}

static void
keys_open(int id)
{
	g_edit_id = id;
	copy_str(g_edit, sizeof g_edit, id == ID_REPO ? g_alt : g_beta);
	g_kx = 0;
	g_ky = 0;
	g_shift = 0;
	g_view = VIEW_KEYS;
	set_status(g_edit[0] ? g_edit : "R1 saves, Circle cancels");
}

static void
keys_commit(void)
{
	if (g_edit_id == ID_REPO)
		copy_str(g_alt, sizeof g_alt, g_edit);
	else
		copy_str(g_beta, sizeof g_beta, g_edit);
	save_settings();
	g_view = VIEW_MENU;
	set_status("saved");
}

static char
key_at(int x, int y)
{
	char c;
	if (y < 0 || y >= KEY_ROWS)
		return 0;
	if (x < 0 || x >= (int)strlen(KEY_ROW[y]))
		return 0;
	c = KEY_ROW[y][x];
	if (g_shift && c >= 'a' && c <= 'z')
		c = (char)(c - 'a' + 'A');
	return c;
}

static void
edit_push(char c)
{
	size_t n = strlen(g_edit);
	if (n + 1 >= sizeof g_edit)
		return;
	g_edit[n] = c;
	g_edit[n + 1] = 0;
	set_status(g_edit);
}

static void
edit_pop(void)
{
	size_t n = strlen(g_edit);
	if (n)
		g_edit[n - 1] = 0;
	set_status(g_edit[0] ? g_edit : "empty");
}

static int
key_width(int y)
{
	if (y < 0)
		return 1;
	if (y >= KEY_ROWS)
		return 8;
	return (int)strlen(KEY_ROW[y]);
}

static int
keys_pad(unsigned bits)
{
	if (bits & OPT_UP) {
		g_ky--;
		if (g_ky < 0)
			g_ky = KEY_ROWS;
	}
	if (bits & OPT_DOWN) {
		g_ky++;
		if (g_ky > KEY_ROWS)
			g_ky = 0;
	}
	if (g_kx >= key_width(g_ky))
		g_kx = key_width(g_ky) - 1;
	if (g_kx < 0)
		g_kx = 0;
	if (bits & OPT_LEFT) {
		g_kx--;
		if (g_kx < 0)
			g_kx = key_width(g_ky) - 1;
	}
	if (bits & OPT_RIGHT) {
		g_kx++;
		if (g_kx >= key_width(g_ky))
			g_kx = 0;
	}
	if (bits & OPT_TRI)
		g_shift = !g_shift;
	if (bits & (OPT_SQUARE | OPT_L1))
		edit_pop();
	if (bits & OPT_R1)
		keys_commit();
	if (bits & OPT_CIRCLE) {
		g_view = VIEW_MENU;
		set_status("unchanged");
	}
	if (bits & OPT_CROSS) {
		if (g_ky == KEY_ROWS) {
			if (g_kx <= 2)
				edit_push(' ');
			else if (g_kx <= 5)
				edit_pop();
			else
				keys_commit();
		} else {
			char c = key_at(g_kx, g_ky);
			if (c)
				edit_push(c);
		}
	}
	return 1;
}

static int
utf8_put(char *dst, size_t n, size_t *i, unsigned cp)
{
	if (cp < 0x80) {
		if (*i + 1 >= n)
			return 0;
		dst[(*i)++] = (char)cp;
	} else if (cp < 0x800) {
		if (*i + 2 >= n)
			return 0;
		dst[(*i)++] = (char)(0xC0 | (cp >> 6));
		dst[(*i)++] = (char)(0x80 | (cp & 0x3F));
	} else {
		if (*i + 3 >= n)
			return 0;
		dst[(*i)++] = (char)(0xE0 | (cp >> 12));
		dst[(*i)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
		dst[(*i)++] = (char)(0x80 | (cp & 0x3F));
	}
	return 1;
}

static int
json_string_range(const char *start, const char *end, const char *key, char *out, size_t n)
{
	char pat[80];
	const char *p;
	size_t i = 0;
	if (!out || n == 0)
		return -1;
	int found = 0;
	out[0] = 0;
	snprintf(pat, sizeof pat, "\"%s\"", key);
	for (p = start; p && p + strlen(pat) <= end; p++) {
		if (memcmp(p, pat, strlen(pat)) == 0) {
			p += strlen(pat);
			found = 1;
			break;
		}
	}
	if (!found || p >= end)
		return -1;
	while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
		p++;
	if (p >= end || *p != ':')
		return -1;
	p++;
	while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
		p++;
	if (p >= end || *p != '"')
		return -1;
	p++;
	while (p < end && *p != '"') {
		unsigned char c = (unsigned char)*p++;
		if (c == '\\' && p < end) {
			char e = *p++;
			if (e == 'n')
				c = '\n';
			else if (e == 't')
				c = '\t';
			else if (e == 'r')
				c = '\r';
			else if (e == 'u' && p + 4 <= end) {
				unsigned cp = 0;
				int k;
				for (k = 0; k < 4; k++) {
					char h = *p++;
					cp <<= 4;
					if (h >= '0' && h <= '9')
						cp |= (unsigned)(h - '0');
					else if (h >= 'a' && h <= 'f')
						cp |= (unsigned)(h - 'a' + 10);
					else if (h >= 'A' && h <= 'F')
						cp |= (unsigned)(h - 'A' + 10);
				}
				utf8_put(out, n, &i, cp);
				continue;
			} else
				c = (unsigned char)e;
		}
		if (i + 1 < n)
			out[i++] = (char)c;
	}
	out[i] = 0;
	return i ? 0 : -1;
}

static int
span_object(const char *p, const char *end, const char **obj_end)
{
	int depth = 0;
	int str = 0;
	int esc = 0;
	if (p >= end || *p != '{')
		return -1;
	for (; p < end; p++) {
		char c = *p;
		if (str) {
			if (esc)
				esc = 0;
			else if (c == '\\')
				esc = 1;
			else if (c == '"')
				str = 0;
			continue;
		}
		if (c == '"')
			str = 1;
		else if (c == '{')
			depth++;
		else if (c == '}') {
			depth--;
			if (depth == 0) {
				*obj_end = p + 1;
				return 0;
			}
		}
	}
	return -1;
}

static int
json_u32_key(const char *start, const char *end, const char *key, int *out)
{
	char pat[80];
	const char *p;
	int found = 0;
	char sbuf[32];
	snprintf(pat, sizeof pat, "\"%s\"", key);
	for (p = start; p && p + strlen(pat) <= end; p++) {
		if (memcmp(p, pat, strlen(pat)) == 0) {
			p += strlen(pat);
			found = 1;
			break;
		}
	}
	if (!found || p >= end)
		return -1;
	while (p < end && isspace((unsigned char)*p))
		p++;
	if (p >= end || *p != ':')
		return -1;
	p++;
	while (p < end && isspace((unsigned char)*p))
		p++;
	if (p < end && *p == '"') {
		if (json_string_range(start, end, key, sbuf, sizeof sbuf) != 0)
			return -1;
		*out = atoi(sbuf);
		return 0;
	}
	if (p >= end || !isdigit((unsigned char)*p))
		return -1;
	*out = 0;
	while (p < end && isdigit((unsigned char)*p)) {
		*out = *out * 10 + (*p - '0');
		p++;
	}
	return 0;
}

static void
absolutize(char *url, size_t n)
{
	char rel[OPT_URL];
	const char *base;
	const char *slash;
	size_t blen;
	size_t rlen;
	if (!url || !url[0])
		return;
	if (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0)
		return;
	copy_str(rel, sizeof rel, url);
	base = active_repo();
	slash = strrchr(base, '/');
	rlen = strlen(rel);
	if (!slash) {
		url[0] = 0;
		return;
	}
	if (rel[0] == '/') {
		const char *scheme = strstr(base, "://");
		const char *host = scheme ? scheme + 3 : base;
		const char *path = host ? strchr(host, '/') : NULL;
		if (!scheme || !path) {
			url[0] = 0;
			return;
		}
		blen = (size_t)(path - base);
		if (blen + rlen + 1 > n) {
			url[0] = 0;
			return;
		}
		memmove(url, base, blen);
		memcpy(url + blen, rel, rlen + 1);
	} else {
		blen = (size_t)(slash - base);
		if (blen + 1 + rlen + 1 > n) {
			url[0] = 0;
			return;
		}
		memmove(url, base, blen);
		url[blen] = '/';
		memcpy(url + blen + 1, rel, rlen + 1);
	}
}

static void
take_prefixes(const char *obj, const char *end, int index)
{
	const char *key = "\"uriprefixes\"";
	const char *p = obj;
	size_t kn = strlen(key);
	int n = 0;
	while (p + kn <= end) {
		if (memcmp(p, key, kn) == 0) {
			p += kn;
			break;
		}
		p++;
	}
	if (p >= end)
		return;
	while (p < end && *p != '[' && *p != '}')
		p++;
	if (p >= end || *p != '[')
		return;
	p++;
	while (p < end && n < 4 && g_pref_n < PREF_MAX) {
		char text[96];
		size_t i = 0;
		while (p < end && *p != '"' && *p != ']')
			p++;
		if (p >= end || *p == ']')
			break;
		p++;
		while (p < end && *p != '"' && i + 1 < sizeof text) {
			char c = *p++;
			if (c == '\\' && p < end)
				c = *p++;
			text[i++] = c;
		}
		text[i] = 0;
		if (p < end && *p == '"')
			p++;
		if (text[0]) {
			g_pref[g_pref_n].index = index;
			copy_str(g_pref[g_pref_n].text, sizeof g_pref[g_pref_n].text, text);
			g_pref_n++;
			n++;
		}
	}
}

int
opt_ingest_repo(const char *json, char *err, size_t errn)
{
	const char *p;
	const char *end;
	const char *list;
	const char *header_end;
	int ver = 0;
	if (!json) {
		snprintf(err, errn, "empty repository");
		return -1;
	}
	end = json + strlen(json);
	if (json_u32_key(json, end, "version", &ver) != 0)
		ver = 0;
	if (ver != 1) {
		snprintf(err, errn, "unsupported repository version");
		return -1;
	}
	list = strstr(json, "\"plugins\"");
	header_end = list ? list : end;
	if (json_string_range(json, header_end, "message", g_status, sizeof g_status) == 0 &&
	    g_status[0]) {
		snprintf(err, errn, "%s", g_status);
		return -1;
	}
	if (!list) {
		snprintf(err, errn, "repository has no plugins");
		return -1;
	}
	g_repo_n = 0;
	g_pref_n = 0;
	p = strchr(list, '[');
	if (!p) {
		snprintf(err, errn, "repository has no plugins");
		return -1;
	}
	p++;
	while (p < end && g_repo_n < OPT_REPO_MAX) {
		const char *obj;
		const char *obj_end = NULL;
		repo_item_t *it;
		while (p < end && *p != '{' && *p != ']')
			p++;
		if (p >= end || *p == ']')
			break;
		obj = p;
		if (span_object(p, end, &obj_end) != 0)
			break;
		it = &g_repo[g_repo_n];
		memset(it, 0, sizeof *it);
		{
			char type[24];
			type[0] = 0;
			json_string_range(obj, obj_end, "type", type, sizeof type);
			if (type[0] && strcmp(type, "ecmascript") != 0) {
				p = obj_end;
				continue;
			}
		}
		json_string_range(obj, obj_end, "id", it->id, sizeof it->id);
		json_string_range(obj, obj_end, "title", it->title, sizeof it->title);
		json_string_range(obj, obj_end, "version", it->version, sizeof it->version);
		json_string_range(obj, obj_end, "file", it->file, sizeof it->file);
		json_string_range(obj, obj_end, "downloadURL", it->url, sizeof it->url);
		json_string_range(obj, obj_end, "category", it->category, sizeof it->category);
		absolutize(it->url, sizeof it->url);
		if (!it->title[0])
			copy_str(it->title, sizeof it->title, it->id[0] ? it->id : "plugin");
		if (it->id[0] && it->url[0]) {
			take_prefixes(obj, obj_end, g_repo_n);
			g_repo_n++;
		}
		p = obj_end;
	}
	if (g_repo_n <= 0) {
		snprintf(err, errn, "repository has no plugins");
		return -1;
	}
	g_repo_i = 0;
	snprintf(err, errn, "%d plugins", g_repo_n);
	return 0;
}

static int
id_ok(const char *id)
{
	const char *p;
	if (!id || !id[0] || strlen(id) > 48)
		return 0;
	for (p = id; *p; p++) {
		char c = *p;
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		      (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
			return 0;
	}
	return 1;
}

static int
path_safe(const char *name)
{
	const char *p;
	if (!name || !name[0] || name[0] == '/' || name[0] == '\\')
		return 0;
	if (strstr(name, ".."))
		return 0;
	for (p = name; *p; p++) {
		if (*p == '\\' || *p == ':')
			return 0;
	}
	return 1;
}

static uint16_t
ru16(const unsigned char *p)
{
	return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t
ru32(const unsigned char *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int
inflate_raw(const unsigned char *src, size_t src_n, unsigned char *dst, size_t dst_n, size_t *out)
{
	z_stream z;
	int rc;
	memset(&z, 0, sizeof z);
	z.next_in = src;
	z.avail_in = (unsigned int)src_n;
	z.next_out = dst;
	z.avail_out = (unsigned int)dst_n;
	rc = inflateInit2_(&z, -15, "1.2.13", (int)sizeof z);
	if (rc != Z_OK)
		return -1;
	rc = inflate(&z, Z_FINISH);
	*out = z.total_out;
	inflateEnd(&z);
	return (rc == Z_STREAM_END || rc == Z_OK) ? 0 : -1;
}

static int
write_whole(const char *path, const void *data, size_t n)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return -1;
	if (n && fwrite(data, 1, n, f) != n) {
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

static int
unzip_to(const uint8_t *zip, size_t n, const char *dir, int *files_out)
{
	const unsigned char *b = zip;
	size_t eocd = 0;
	int found = 0;
	uint16_t entries;
	uint32_t cd_off;
	size_t pos;
	int files = 0;
	size_t total = 0;
	int i;
	if (n < 22)
		return -1;
	for (i = (int)n - 22; i >= 0 && (size_t)(n - (size_t)i) < 66000; i--) {
		if (b[i] == 'P' && b[i + 1] == 'K' && b[i + 2] == 5 && b[i + 3] == 6) {
			eocd = (size_t)i;
			found = 1;
			break;
		}
	}
	if (!found)
		return -1;
	entries = ru16(b + eocd + 10);
	cd_off = ru32(b + eocd + 16);
	if ((size_t)cd_off >= n)
		return -1;
	pos = cd_off;
	for (i = 0; i < (int)entries && i < 64; i++) {
		uint16_t method, flag, namelen, extra, comment;
		uint32_t csize, usize, local;
		char name[256];
		const unsigned char *data;
		size_t data_off;
		char rel[256];
		char outp[700];
		unsigned char *plain = NULL;
		size_t plain_n = 0;
		if (pos + 46 > n || ru32(b + pos) != 0x02014b50u)
			return -1;
		flag = ru16(b + pos + 8);
		method = ru16(b + pos + 10);
		csize = ru32(b + pos + 20);
		usize = ru32(b + pos + 24);
		namelen = ru16(b + pos + 28);
		extra = ru16(b + pos + 30);
		comment = ru16(b + pos + 32);
		local = ru32(b + pos + 42);
		if (namelen >= sizeof name || pos + 46u + (size_t)namelen > n)
			return -1;
		memcpy(name, b + pos + 46, namelen);
		name[namelen] = 0;
		if (pos + 46u + (size_t)namelen + (size_t)extra + (size_t)comment > n)
			return -1;
		pos += 46u + (size_t)namelen + (size_t)extra + (size_t)comment;
		if (name[namelen > 0 ? namelen - 1 : 0] == '/')
			continue;
		if (!path_safe(name) || (flag & 1u) || (method != 0 && method != 8) ||
		    csize == 0xffffffffu || usize == 0xffffffffu || usize > 2 * 1024 * 1024 ||
		    csize > 4 * 1024 * 1024)
			return -1;
		if ((size_t)local + 30 > n || ru32(b + local) != 0x04034b50u)
			return -1;
		{
			uint16_t ln = ru16(b + local + 26);
			uint16_t en = ru16(b + local + 28);
			data_off = (size_t)local + 30u + ln + en;
		}
		if (data_off + csize > n)
			return -1;
		data = b + data_off;
		if (method == 0) {
			plain = (unsigned char *)malloc(csize ? csize : 1);
			if (!plain)
				return -1;
			if (csize)
				memcpy(plain, data, csize);
			plain_n = csize;
		} else if (usize == 0) {
			plain = (unsigned char *)malloc(1);
			if (!plain)
				return -1;
			plain_n = 0;
		} else {
			plain = (unsigned char *)malloc(usize ? usize : 1);
			if (!plain)
				return -1;
			if (inflate_raw(data, csize, plain, usize, &plain_n) != 0 || plain_n != usize) {
				free(plain);
				return -1;
			}
		}
		total += plain_n;
		if (total > 8 * 1024 * 1024) {
			free(plain);
			return -1;
		}
		copy_str(rel, sizeof rel, name);
		{
			size_t dl = strlen(dir);
			size_t rl = strlen(rel);
			char parent[700];
			char *sl;
			if (dl + 1 + rl + 1 > sizeof outp) {
				free(plain);
				return -1;
			}
			memcpy(outp, dir, dl);
			outp[dl] = '/';
			memcpy(outp + dl + 1, rel, rl + 1);
			sl = strrchr(outp, '/');
			if (sl) {
				size_t pn = (size_t)(sl - outp);
				if (pn < sizeof parent) {
					memcpy(parent, outp, pn);
					parent[pn] = 0;
					mkdirs(parent);
				}
			}
		}
		if (write_whole(outp, plain, plain_n) != 0) {
			free(plain);
			return -1;
		}
		free(plain);
		files++;
	}
	if (files_out)
		*files_out = files;
	return files > 0 ? 0 : -1;
}

static void
install_roots(const char **out, int *n)
{
	static const char *defs[] = {
		"/data/movian/plugins",
		"/mnt/usb0/plugins",
		"/mnt/usb1/plugins",
		"/user/movian/plugins",
	};
	if (g_install_root[0]) {
		out[0] = g_install_root;
		*n = 1;
		return;
	}
	out[0] = defs[0];
	out[1] = defs[1];
	out[2] = defs[2];
	out[3] = defs[3];
	*n = 4;
}

static int
pick_install_dir(const char *id, char *dst, size_t n)
{
	const char *roots[4];
	int nr = 0;
	int i;
	install_roots(roots, &nr);
	for (i = 0; i < nr; i++) {
		char path[700];
		snprintf(path, sizeof path, "%s/%s", roots[i], id);
		mkdirs(path);
		if (access(path, 0) == 0) {
			copy_str(dst, n, path);
			return 0;
		}
	}
	return -1;
}

static size_t
json_escape(char *dst, size_t n, const char *src)
{
	size_t i = 0;
	if (!src)
		src = "";
	while (*src && i + 2 < n) {
		unsigned char c = (unsigned char)*src++;
		if (c == '"' || c == '\\') {
			if (i + 3 >= n)
				break;
			dst[i++] = '\\';
			dst[i++] = (char)c;
		} else if (c < 0x20) {
			continue;
		} else {
			dst[i++] = (char)c;
		}
	}
	dst[i] = 0;
	return i;
}

static void
write_manifest(const char *dir, const repo_item_t *it)
{
	char path[700];
	char body[1200];
	char id[OPT_ID * 2];
	char file[200];
	char title[OPT_TITLE * 2];
	char ver[64];
	FILE *f;
	size_t got;
	snprintf(path, sizeof path, "%s/plugin.json", dir);
	json_escape(ver, sizeof ver, it->version[0] ? it->version : "0");
	f = fopen(path, "rb");
	if (f) {
		got = fread(body, 1, sizeof body - 1, f);
		fclose(f);
		body[got] = 0;
		if (got + 1 < sizeof body) {
			char *key = strstr(body, "\"version\"");
			char *q1;
			char *q2;
			if (key && (q1 = strchr(key + 9, '"')) != NULL && (q2 = strchr(q1 + 1, '"')) != NULL) {
				char tmp[1200];
				size_t head = (size_t)(q1 + 1 - body);
				size_t vn = strlen(ver);
				size_t tail = strlen(q2);
				if (head + vn + tail + 1 < sizeof tmp) {
					memcpy(tmp, body, head);
					memcpy(tmp + head, ver, vn);
					memcpy(tmp + head + vn, q2, tail + 1);
					write_whole(path, tmp, head + vn + tail);
					return;
				}
			}
		}
		return;
	}
	json_escape(id, sizeof id, it->id);
	json_escape(file, sizeof file, it->file[0] ? it->file : "plugin.js");
	json_escape(title, sizeof title, it->title);
	snprintf(body, sizeof body,
		 "{\"id\":\"%s\",\"file\":\"%s\",\"title\":\"%s\",\"version\":\"%s\","
		 "\"type\":\"ecmascript\",\"apiversion\":2}\n",
		 id, file, title, ver);
	write_whole(path, body, strlen(body));
}

int
opt_install_blob(int index, const uint8_t *zip, size_t n, char *err, size_t errn)
{
	repo_item_t *it;
	char dir[700];
	int files = 0;
	if (index < 0 || index >= g_repo_n) {
		snprintf(err, errn, "no plugin");
		return -1;
	}
	it = &g_repo[index];
	if (!id_ok(it->id)) {
		snprintf(err, errn, "plugin id rejected");
		return -1;
	}
	if (pick_install_dir(it->id, dir, sizeof dir) != 0) {
		snprintf(err, errn, "plugins directory is not writable");
		return -1;
	}
	if (n >= 2 && zip[0] == 'P' && zip[1] == 'K') {
		if (unzip_to(zip, n, dir, &files) != 0) {
			snprintf(err, errn, "zip rejected");
			return -1;
		}
		write_manifest(dir, it);
	} else if (n >= 4 && zip[0] != '<') {
		char path[760];
		const char *file = it->file[0] ? it->file : "plugin.js";
		if (!path_safe(file)) {
			snprintf(err, errn, "plugin file rejected");
			return -1;
		}
		snprintf(path, sizeof path, "%s/%s", dir, file);
		if (write_whole(path, zip, n) != 0) {
			snprintf(err, errn, "could not write plugin");
			return -1;
		}
		write_manifest(dir, it);
		files = 1;
	} else {
		snprintf(err, errn, "download is not a plugin zip");
		return -1;
	}
	snprintf(err, errn, "installed %s (%d)", it->title, files);
	return 0;
}

static int
url_encode(const char *src, char *dst, size_t n)
{
	static const char *hex = "0123456789ABCDEF";
	size_t i = 0;
	const unsigned char *s = (const unsigned char *)src;
	for (; *s; s++) {
		unsigned char c = *s;
		int raw = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			  (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
		if (raw) {
			if (i + 1 >= n)
				return -1;
			dst[i++] = (char)c;
		} else {
			if (i + 3 >= n)
				return -1;
			dst[i++] = '%';
			dst[i++] = hex[c >> 4];
			dst[i++] = hex[c & 15];
		}
	}
	dst[i] = 0;
	return 0;
}

static void
repo_request(char *dst, size_t n)
{
	const char *base = active_repo();
	char extra[400];
	size_t used = 0;
	const char *pw = g_beta;
	extra[0] = 0;
	while (*pw && used + 20 < sizeof extra) {
		char tok[80];
		char enc[240];
		size_t k = 0;
		while (*pw == ' ')
			pw++;
		if (!*pw)
			break;
		while (*pw && *pw != ' ' && k + 1 < sizeof tok)
			tok[k++] = *pw++;
		tok[k] = 0;
		if (url_encode(tok, enc, sizeof enc) != 0)
			break;
		used += (size_t)snprintf(extra + used, sizeof extra - used, "%sbetapassword=%s",
					 used ? "&" : "", enc);
	}
	if (extra[0]) {
		size_t bl = strlen(base);
		size_t el = strlen(extra);
		if (bl + 1 + el + 1 > n) {
			copy_str(dst, n, base);
			return;
		}
		memcpy(dst, base, bl);
		dst[bl] = strchr(base, '?') ? '&' : '?';
		memcpy(dst + bl + 1, extra, el + 1);
	} else {
		copy_str(dst, n, base);
	}
}

static int
ver_cmp(const char *a, const char *b)
{
	const char *pa = a ? a : "";
	const char *pb = b ? b : "";
	while (*pa || *pb) {
		long na = 0, nb = 0;
		if (*pa == '.')
			pa++;
		if (*pb == '.')
			pb++;
		while (*pa >= '0' && *pa <= '9') {
			na = na * 10 + (*pa - '0');
			pa++;
		}
		while (*pb >= '0' && *pb <= '9') {
			nb = nb * 10 + (*pb - '0');
			pb++;
		}
		if (na != nb)
			return na < nb ? -1 : 1;
		while (*pa && *pa != '.')
			pa++;
		while (*pb && *pb != '.')
			pb++;
	}
	return 0;
}

static int
read_installed_version(const char *id, char *ver, size_t n)
{
	const char *roots[4];
	int nr = 0;
	int i;
	ver[0] = 0;
	install_roots(roots, &nr);
	for (i = 0; i < nr; i++) {
		char path[700];
		FILE *f;
		char buf[2048];
		size_t got;
		snprintf(path, sizeof path, "%s/%s/plugin.json", roots[i], id);
		f = fopen(path, "rb");
		if (!f)
			continue;
		got = fread(buf, 1, sizeof buf - 1, f);
		fclose(f);
		buf[got] = 0;
		json_string_range(buf, buf + got, "version", ver, n);
		return 1;
	}
	return 0;
}

static int
http_ok(const char *url, char **body, size_t *len, size_t cap, char *err, size_t errn)
{
	int rc = plug_http_get(url, body, len, cap, err, errn);
	if (rc != 0) {
		if (!err[0])
			snprintf(err, errn, "http failed");
		return -1;
	}
	if (!*body) {
		snprintf(err, errn, "empty download");
		return -1;
	}
	return 0;
}

static int
job_fetch(void)
{
	char url[OPT_URL];
	char err[240];
	char *body = NULL;
	size_t len = 0;
	repo_request(url, sizeof url);
	set_status("loading repository");
	err[0] = 0;
	if (http_ok(url, &body, &len, 1024 * 1024, err, sizeof err) != 0) {
		set_status(err);
		free(body);
		return -1;
	}
	body[len < 1024 * 1024 ? len : 1024 * 1024 - 1] = 0;
	if (opt_ingest_repo(body, err, sizeof err) != 0) {
		set_status(err);
		free(body);
		return -1;
	}
	free(body);
	g_view = VIEW_REPO;
	g_repo_i = 0;
	set_status(err);
	return 0;
}

static int
job_install_index(int index)
{
	char err[240];
	char *body = NULL;
	size_t len = 0;
	repo_item_t *it;
	if (index < 0 || index >= g_repo_n)
		return -1;
	it = &g_repo[index];
	snprintf(g_status, sizeof g_status, "installing %.80s", it->title);
	err[0] = 0;
	if (http_ok(it->url, &body, &len, 6 * 1024 * 1024, err, sizeof err) != 0) {
		set_status(err);
		free(body);
		return -1;
	}
	if (opt_install_blob(index, (const uint8_t *)body, len, err, sizeof err) != 0) {
		set_status(err);
		free(body);
		return -1;
	}
	free(body);
	set_status(err);
	return 0;
}

static int
job_upgrade(void)
{
	char have[32];
	while (g_upgrade_i < g_repo_n) {
		repo_item_t *it = &g_repo[g_upgrade_i++];
		if (!read_installed_version(it->id, have, sizeof have))
			continue;
		if (ver_cmp(have, it->version) >= 0)
			continue;
		if (job_install_index(g_upgrade_i - 1) != 0)
			return -1;
		if (g_upgrade_i < g_repo_n) {
			g_job = JOB_UPGRADE;
			g_armed = 1;
		}
		return 0;
	}
	set_status("plugins are current");
	return 0;
}

void
opt_work(void)
{
	int job = g_job;
	int then = g_then_upgrade;
	g_armed = 0;
	g_job = JOB_NONE;
	g_then_upgrade = 0;
	if (job == JOB_FETCH) {
		if (job_fetch() == 0 && (then || g_upgrade)) {
			g_job = JOB_UPGRADE;
			g_upgrade_i = 0;
			g_armed = 1;
		}
	} else if (job == JOB_INSTALL) {
		job_install_index(g_repo_i);
	} else if (job == JOB_UPGRADE) {
		job_upgrade();
	}
}

int
opt_install_for_uri(const char *uri, char *err, size_t errn)
{
	int i;
	int best = -1;
	int best_n = 0;
	char have[32];
	if (err && errn)
		err[0] = 0;
	if (!g_demand || !uri || !uri[0] || g_pref_n <= 0)
		return 0;
	for (i = 0; i < g_pref_n; i++) {
		int n = (int)strlen(g_pref[i].text);
		if (n > best_n && strncmp(uri, g_pref[i].text, (size_t)n) == 0) {
			best = g_pref[i].index;
			best_n = n;
		}
	}
	if (best < 0)
		return 0;
	if (read_installed_version(g_repo[best].id, have, sizeof have))
		return 0;
	if (job_install_index(best) != 0) {
		if (err && errn)
			snprintf(err, errn, "%s", g_status);
		return -1;
	}
	if (err && errn)
		snprintf(err, errn, "%s", g_status);
	return 1;
}

static int
menu_pad(unsigned bits)
{
	row_def_t row;
	if (row_skips(g_cursor))
		g_cursor = 1;
	row = ROWS[g_cursor];
	if (bits & OPT_UP)
		move_cursor(-1);
	if (bits & OPT_DOWN)
		move_cursor(1);
	if (bits & OPT_CIRCLE)
		return 0;
	if ((bits & (OPT_LEFT | OPT_RIGHT)) && row.kind == ROW_STR && row.id == ID_REPO)
		cycle_preset(bits & OPT_RIGHT ? 1 : -1);
	if ((bits & (OPT_LEFT | OPT_RIGHT)) && row.kind == ROW_CYCLE)
		cycle_id(row.id, bits & OPT_RIGHT ? 1 : -1);
	if (bits & OPT_CROSS) {
		if (row.kind == ROW_STR)
			keys_open(row.id);
		else if (row.kind == ROW_BOOL) {
			if (row.id == ID_UP)
				g_upgrade = !g_upgrade;
			else
				g_demand = !g_demand;
			save_settings();
		} else if (row.kind == ROW_CYCLE)
			cycle_id(row.id, 1);
		else if (row.id == ID_BROWSE) {
			g_job = JOB_FETCH;
			g_then_upgrade = 0;
			g_armed = 1;
			set_status("loading repository");
		} else if (row.id == ID_UPGRADE) {
			if (g_repo_n <= 0) {
				g_job = JOB_FETCH;
				g_then_upgrade = 1;
				g_armed = 1;
				set_status("loading repository");
			} else {
				g_job = JOB_UPGRADE;
				g_upgrade_i = 0;
				g_armed = 1;
			}
		} else if (row.id == ID_RESCAN) {
			g_rescan = 1;
			set_status("rescanning");
			return 0;
		}
	}
	return 1;
}

static int
repo_pad(unsigned bits)
{
	if (g_repo_n <= 0) {
		g_view = VIEW_MENU;
		return 1;
	}
	if (bits & OPT_UP) {
		g_repo_i--;
		if (g_repo_i < 0)
			g_repo_i = g_repo_n - 1;
	}
	if (bits & OPT_DOWN) {
		g_repo_i++;
		if (g_repo_i >= g_repo_n)
			g_repo_i = 0;
	}
	if (bits & OPT_CIRCLE) {
		g_view = VIEW_MENU;
		set_status("repository");
	}
	if (bits & OPT_CROSS) {
		g_job = JOB_INSTALL;
		g_armed = 1;
		snprintf(g_status, sizeof g_status, "installing %.80s", g_repo[g_repo_i].title);
	}
	return 1;
}

void
opt_open(void)
{
	g_open = 1;
	g_view = VIEW_MENU;
	g_cursor = 1;
	g_armed = 0;
	g_job = JOB_NONE;
	if (!g_status[0])
		set_status(g_alt[0] ? "alternate repository set" : "official repository");
}

int
opt_is_open(void)
{
	return g_open;
}

int
opt_pad(unsigned bits)
{
	int stay = 1;
	if (!bits)
		return g_open;
	if (g_armed)
		return 1;
	if (g_view == VIEW_KEYS)
		stay = keys_pad(bits);
	else if (g_view == VIEW_REPO)
		stay = repo_pad(bits);
	else
		stay = menu_pad(bits);
	g_open = stay;
	return stay;
}

int
opt_armed(void)
{
	return g_armed;
}

int
opt_row_count(void)
{
	if (g_view == VIEW_KEYS)
		return KEY_ROWS + 2;
	if (g_view == VIEW_REPO)
		return g_repo_n;
	return ROW_N;
}

void
opt_row(int index, char *text, size_t n, int *header, int *selected)
{
	if (header)
		*header = 0;
	if (selected)
		*selected = 0;
	if (!text || n == 0)
		return;
	text[0] = 0;
	if (g_view == VIEW_REPO) {
		if (index < 0 || index >= g_repo_n)
			return;
		if (selected)
			*selected = index == g_repo_i;
		snprintf(text, n, "%c %s  %s", index == g_repo_i ? '>' : ' ',
			 g_repo[index].title, g_repo[index].version);
		return;
	}
	if (g_view == VIEW_KEYS) {
		if (index == 0) {
			snprintf(text, n, "%s", g_edit[0] ? g_edit : "_");
			if (selected)
				*selected = 1;
			return;
		}
		if (index >= 1 && index <= KEY_ROWS) {
			char line[80];
			int y = index - 1;
			int x;
			size_t o = 0;
			int w = (int)strlen(KEY_ROW[y]);
			for (x = 0; x < w && o + 4 < sizeof line; x++) {
				char c = key_at(x, y);
				int on = (g_ky == y && g_kx == x);
				if (on)
					line[o++] = '[';
				line[o++] = c ? c : '?';
				if (on)
					line[o++] = ']';
				else
					line[o++] = ' ';
			}
			line[o] = 0;
			snprintf(text, n, "%s%s", g_shift ? "SHIFT " : "", line);
			return;
		}
		snprintf(text, n, "%s  %s  %s", g_ky == KEY_ROWS && g_kx <= 2 ? "[space]" : " space ",
			 g_ky == KEY_ROWS && g_kx > 2 && g_kx <= 5 ? "[bksp]" : " bksp ",
			 g_ky == KEY_ROWS && g_kx > 5 ? "[done]" : " done ");
		return;
	}
	if (index < 0 || index >= ROW_N)
		return;
	if (ROWS[index].kind == ROW_SEC) {
		if (header)
			*header = 1;
		snprintf(text, n, "%s", ROWS[index].title);
		return;
	}
	if (selected)
		*selected = index == g_cursor;
	{
		char val[OPT_URL];
		val[0] = 0;
		value_of(ROWS[index].id, val, sizeof val);
		if (val[0])
			snprintf(text, n, "%c %s  %s", index == g_cursor ? '>' : ' ', ROWS[index].title, val);
		else
			snprintf(text, n, "%c %s", index == g_cursor ? '>' : ' ', ROWS[index].title);
	}
}

const char *
opt_title(void)
{
	if (g_view == VIEW_KEYS)
		return g_edit_id == ID_BETA ? "BETA PASSWORDS" : "REPOSITORY URL";
	if (g_view == VIEW_REPO)
		return "AVAILABLE PLUGINS";
	return "SETTINGS";
}

const char *
opt_status(void)
{
	return g_status;
}

const char *
opt_hint(void)
{
	if (g_armed)
		return "LOADING";
	if (g_view == VIEW_KEYS)
		return "Cross type   Square erase   Triangle shift   Circle cancel   R1 save";
	if (g_view == VIEW_REPO)
		return "Up/Down   Cross install   Circle back";
	return "Up/Down   Cross change   Left/Right   Circle close   hold Options quits";
}

int
opt_take_rescan(void)
{
	int v = g_rescan;
	g_rescan = 0;
	return v;
}

int
opt_take_reopen(void)
{
	int v = g_reopen;
	g_reopen = 0;
	return v;
}

int
opt_layout(void)
{
	return g_layout;
}

void
opt_set_layout(int layout)
{
	if (layout < 0 || layout >= LAY_N || layout == g_layout)
		return;
	g_layout = layout;
	save_settings();
}

int
opt_audio(void)
{
	return g_audio;
}

void
opt_set_audio(int audio)
{
	if (audio < 0 || audio > 2 || audio == g_audio)
		return;
	g_audio = audio;
	save_settings();
}

double
opt_speed(void)
{
	if (g_speed < 0 || g_speed >= SPEED_N)
		return 1.0;
	return SPEEDS[g_speed];
}

void
opt_set_speed(double speed)
{
	int i;
	int best = 2;
	double err = 100.0;
	for (i = 0; i < SPEED_N; i++) {
		double d = SPEEDS[i] - speed;
		if (d < 0)
			d = -d;
		if (d < err) {
			err = d;
			best = i;
		}
	}
	if (best == g_speed)
		return;
	g_speed = best;
	save_settings();
}

double
opt_seek(void)
{
	if (g_seek < 0 || g_seek >= SEEK_N)
		return 5.0;
	return (double)SEEKS[g_seek];
}

int
opt_force_software(void)
{
	return g_dec;
}

const char *
opt_repo_url(void)
{
	return g_alt;
}

int
opt_repo_count(void)
{
	return g_repo_n;
}

const char *
opt_repo_id(int index)
{
	if (index < 0 || index >= g_repo_n)
		return "";
	return g_repo[index].id;
}

const char *
opt_repo_title(int index)
{
	if (index < 0 || index >= g_repo_n)
		return "";
	return g_repo[index].title;
}

const char *
opt_repo_version(int index)
{
	if (index < 0 || index >= g_repo_n)
		return "";
	return g_repo[index].version;
}
