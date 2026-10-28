/* Movian plugins inside the payload. Duktape 2.7 (MIT) evaluates the
 * package. The standard library is the Movian ECMAScript modules.
 * On the console, pages are fetched with sceHttp2. The host test uses curl.
 */
#include "plugin_host.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "duktape.h"
#include "plugin_embed.h"

#ifdef PLUGIN_HOST_TEST
#include <sys/wait.h>
#include <unistd.h>
#else
#include <unistd.h>

int sceNetInit(void);
int sceNetPoolCreate(const char *, int, int);
int sceSslInit(size_t);
int sceHttp2Init(int, int, size_t, int);
int sceHttp2CreateTemplate(int, const char *, int, int);
int sceHttp2CreateRequestWithURL(int, const char *, const char *, uint64_t);
int sceHttp2AddRequestHeader(int, const char *, const char *, int);
int sceHttp2SendRequest(int, const void *, size_t);
int sceHttp2GetStatusCode(int, int *);
int sceHttp2ReadData(int, void *, size_t);
int sceHttp2DeleteRequest(int);
#endif

#define PLUG_URL 720
#define PLUG_NAME 160
#define HTTP_CAP (2 * 1024 * 1024)

static const char PLUG_ENGINE[] = "movian-ps5-plugin-host Duktape-2.7.0";

typedef struct {
	char title[PLUG_NAME];
	char url[PLUG_URL];
	char type[32];
} plug_row_t;

typedef struct {
	char dir[512];
	char id[64];
	char title[PLUG_NAME];
	char file[128];
} plug_pkg_t;

static duk_context *g_ctx;
static plug_pkg_t g_pkgs[PLUG_MAX];
static int g_pkg_n;
static plug_row_t g_services[16];
static int g_service_n;
static plug_row_t g_items[PLUG_ITEMS];
static int g_item_n;
static char g_page_title[PLUG_NAME];
static char g_page_error[480];
static char g_status[480];
static char g_log[240];
static char g_stack[8][PLUG_URL];
static int g_sp;
static int g_http_ready;
static int g_pool = -1;
static int g_ssl = -1;
static int g_http = -1;
static int g_tmpl = -1;

const char *
plug_engine_line(void)
{
	return PLUG_ENGINE;
}

const char *
plug_status(void)
{
	return g_status;
}

const char *
plug_page_title(void)
{
	return g_page_title;
}

const char *
plug_page_error(void)
{
	return g_page_error;
}

int
plug_count(void)
{
	return g_pkg_n;
}

const char *
plug_title(int index)
{
	if (index < 0 || index >= g_pkg_n)
		return "";
	return g_pkgs[index].title;
}

const char *
plug_id(int index)
{
	if (index < 0 || index >= g_pkg_n)
		return "";
	return g_pkgs[index].id;
}

int
plug_item_count(void)
{
	return g_item_n;
}

const char *
plug_item_title(int index)
{
	if (index < 0 || index >= g_item_n)
		return "";
	return g_items[index].title;
}

const char *
plug_item_url(int index)
{
	if (index < 0 || index >= g_item_n)
		return "";
	return g_items[index].url;
}

const char *
plug_item_type(int index)
{
	if (index < 0 || index >= g_item_n)
		return "";
	return g_items[index].type;
}

int
plug_service_count(void)
{
	return g_service_n;
}

const char *
plug_service_url(int index)
{
	if (index < 0 || index >= g_service_n)
		return "";
	return g_services[index].url;
}

int
plug_can_back(void)
{
	return g_sp > 1;
}

static void
set_status(const char *s)
{
	snprintf(g_status, sizeof g_status, "%s", s ? s : "");
}

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

static const char *
blob(const char *name, int *len)
{
	int i;
	for (i = 0; i < PLUG_BLOB_COUNT; i++) {
		if (strcmp(PLUG_BLOBS[i].name, name) == 0) {
			if (len)
				*len = PLUG_BLOBS[i].len;
			return PLUG_BLOBS[i].data;
		}
	}
	return NULL;
}

static char *
read_file(const char *path, size_t *out, size_t cap)
{
	FILE *f;
	char *buf;
	size_t n;
	f = fopen(path, "rb");
	if (!f)
		return NULL;
	buf = (char *)malloc(cap + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	n = fread(buf, 1, cap, f);
	fclose(f);
	buf[n] = 0;
	if (out)
		*out = n;
	return buf;
}

static int
write_file(const char *path, const char *data, size_t n)
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
json_string(const char *json, const char *key, char *out, size_t n)
{
	char pat[80];
	const char *p;
	size_t i;
	if (!json || !out || n == 0)
		return -1;
	out[0] = 0;
	snprintf(pat, sizeof pat, "\"%s\"", key);
	p = strstr(json, pat);
	if (!p)
		return -1;
	p += strlen(pat);
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;
	if (*p != ':')
		return -1;
	p++;
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;
	if (*p != '"')
		return -1;
	p++;
	i = 0;
	while (*p && *p != '"') {
		char c = *p++;
		if (c == '\\' && *p) {
			char e = *p++;
			if (e == 'n')
				c = '\n';
			else if (e == 't')
				c = '\t';
			else if (e == 'u' && p[0] && p[1] && p[2] && p[3])
				p += 4;
			else
				c = e;
		}
		if (i + 1 < n)
			out[i++] = c;
	}
	out[i] = 0;
	return 0;
}

void
plug_ascii(const char *utf8, char *out, size_t n)
{
	static const char *up[32] = {
		"A", "B", "V", "G", "D", "E", "Zh", "Z", "I", "Y", "K", "L", "M", "N", "O", "P",
		"R", "S", "T", "U", "F", "Kh", "Ts", "Ch", "Sh", "Shch", "", "Y", "", "E", "Yu", "Ya"
	};
	size_t o = 0;
	const unsigned char *s = (const unsigned char *)(utf8 ? utf8 : "");
	if (!out || n == 0)
		return;
	while (*s && o + 1 < n) {
		unsigned cp;
		const char *rep = NULL;
		if (*s < 0x80) {
			out[o++] = (*s >= 32 && *s < 127) ? (char)*s : ' ';
			s++;
			continue;
		}
		if ((*s & 0xE0) == 0xC0 && s[1]) {
			cp = ((unsigned)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
			s += 2;
		} else if ((*s & 0xF0) == 0xE0 && s[1] && s[2]) {
			cp = ((unsigned)(s[0] & 0x0F) << 12) | ((unsigned)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
			s += 3;
		} else {
			s++;
			if (o + 1 < n)
				out[o++] = '?';
			continue;
		}
		if (cp == 0x401)
			rep = "Yo";
		else if (cp == 0x451)
			rep = "yo";
		else if (cp >= 0x410 && cp <= 0x42F)
			rep = up[cp - 0x410];
		else if (cp >= 0x430 && cp <= 0x44F)
			rep = up[cp - 0x430];
		if (!rep) {
			out[o++] = '?';
			continue;
		}
		while (*rep && o + 1 < n)
			out[o++] = *rep++;
	}
	out[o] = 0;
}

static int
utf8_ok(const unsigned char *s, size_t n)
{
	size_t i = 0;
	while (i < n) {
		unsigned char c = s[i];
		size_t need = 0;
		if (c < 0x80) {
			i++;
			continue;
		}
		if ((c & 0xE0) == 0xC0)
			need = 2;
		else if ((c & 0xF0) == 0xE0)
			need = 3;
		else if ((c & 0xF8) == 0xF0)
			need = 4;
		else
			return 0;
		if (i + need > n)
			return 0;
		if (need == 2 && c < 0xC2)
			return 0;
		{
			size_t k;
			for (k = 1; k < need; k++) {
				if ((s[i + k] & 0xC0) != 0x80)
					return 0;
			}
		}
		i += need;
	}
	return 1;
}

static void
utf8_put(char **p, char *end, unsigned cp)
{
	if (cp < 0x80) {
		if (*p < end)
			*(*p)++ = (char)cp;
	} else if (cp < 0x800) {
		if (*p + 1 < end) {
			*(*p)++ = (char)(0xC0 | (cp >> 6));
			*(*p)++ = (char)(0x80 | (cp & 0x3F));
		}
	} else {
		if (*p + 2 < end) {
			*(*p)++ = (char)(0xE0 | (cp >> 12));
			*(*p)++ = (char)(0x80 | ((cp >> 6) & 0x3F));
			*(*p)++ = (char)(0x80 | (cp & 0x3F));
		}
	}
}

static const unsigned cp1251[128] = {
	0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
	0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
	0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x0098, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
	0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
	0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
	0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
	0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457
};

static duk_ret_t
js_log(duk_context *ctx)
{
	const char *s = duk_safe_to_string(ctx, 0);
	copy_str(g_log, sizeof g_log, s);
#ifdef PLUGIN_HOST_TEST
	fprintf(stderr, "js: %s\n", g_log);
#endif
	return 0;
}

static duk_ret_t
js_source(duk_context *ctx)
{
	int len = 0;
	const char *name = duk_require_string(ctx, 0);
	const char *s = blob(name, &len);
	if (!s)
		duk_push_null(ctx);
	else
		duk_push_lstring(ctx, s, (size_t)len);
	return 1;
}

static duk_ret_t
js_read(duk_context *ctx)
{
	size_t n = 0;
	const char *path = duk_require_string(ctx, 0);
	char *buf = read_file(path, &n, HTTP_CAP);
	if (!buf) {
		duk_push_null(ctx);
		return 1;
	}
	duk_push_lstring(ctx, buf, n);
	free(buf);
	return 1;
}

static duk_ret_t
js_write(duk_context *ctx)
{
	size_t n = 0;
	const char *path = duk_require_string(ctx, 0);
	const char *data = duk_require_lstring(ctx, 1, &n);
	duk_push_boolean(ctx, write_file(path, data, n) == 0);
	return 1;
}

static duk_ret_t
js_mkdirs(duk_context *ctx)
{
	mkdirs(duk_require_string(ctx, 0));
	return 0;
}

static duk_ret_t
js_is_utf8(duk_context *ctx)
{
	size_t n = 0;
	const char *s = duk_to_lstring(ctx, 0, &n);
	duk_push_boolean(ctx, s && utf8_ok((const unsigned char *)s, n));
	return 1;
}

static duk_ret_t
js_cp1251(duk_context *ctx)
{
	size_t n = 0;
	size_t i;
	const unsigned char *s = (const unsigned char *)duk_to_lstring(ctx, 0, &n);
	char *out;
	char *p;
	char *end;
	if (!s) {
		duk_push_string(ctx, "");
		return 1;
	}
	out = (char *)malloc(n * 3 + 1);
	if (!out) {
		duk_push_string(ctx, "");
		return 1;
	}
	p = out;
	end = out + n * 3;
	for (i = 0; i < n; i++) {
		unsigned char c = s[i];
		unsigned cp = c < 0x80 ? c : (c < 0xC0 ? cp1251[c - 0x80] : (0x410 + (c - 0xC0)));
		utf8_put(&p, end, cp);
	}
	duk_push_lstring(ctx, out, (size_t)(p - out));
	free(out);
	return 1;
}

static uint32_t
rotl(uint32_t x, int n)
{
	return (x << n) | (x >> (32 - n));
}

static duk_ret_t
js_sha1(duk_context *ctx)
{
	size_t n = 0;
	const unsigned char *msg = (const unsigned char *)duk_require_lstring(ctx, 0, &n);
	uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
	uint64_t bits = (uint64_t)n * 8;
	size_t pad = (n % 64 < 56) ? (56 - n % 64) : (120 - n % 64);
	size_t total = n + pad + 8;
	unsigned char *buf = (unsigned char *)calloc(1, total ? total : 1);
	unsigned char *out;
	size_t off;
	if (!buf)
		return duk_error(ctx, DUK_ERR_ERROR, "sha1");
	memcpy(buf, msg, n);
	buf[n] = 0x80;
	buf[total - 8] = (unsigned char)(bits >> 56);
	buf[total - 7] = (unsigned char)(bits >> 48);
	buf[total - 6] = (unsigned char)(bits >> 40);
	buf[total - 5] = (unsigned char)(bits >> 32);
	buf[total - 4] = (unsigned char)(bits >> 24);
	buf[total - 3] = (unsigned char)(bits >> 16);
	buf[total - 2] = (unsigned char)(bits >> 8);
	buf[total - 1] = (unsigned char)bits;
	for (off = 0; off < total; off += 64) {
		uint32_t w[80];
		uint32_t a, b, c, d, e;
		int i;
		for (i = 0; i < 16; i++)
			w[i] = ((uint32_t)buf[off + i * 4] << 24) | ((uint32_t)buf[off + i * 4 + 1] << 16) |
			       ((uint32_t)buf[off + i * 4 + 2] << 8) | buf[off + i * 4 + 3];
		for (i = 16; i < 80; i++)
			w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
		a = h0;
		b = h1;
		c = h2;
		d = h3;
		e = h4;
		for (i = 0; i < 80; i++) {
			uint32_t f, k, t;
			if (i < 20) {
				f = (b & c) | ((~b) & d);
				k = 0x5A827999;
			} else if (i < 40) {
				f = b ^ c ^ d;
				k = 0x6ED9EBA1;
			} else if (i < 60) {
				f = (b & c) | (b & d) | (c & d);
				k = 0x8F1BBCDC;
			} else {
				f = b ^ c ^ d;
				k = 0xCA62C1D6;
			}
			t = rotl(a, 5) + f + e + k + w[i];
			e = d;
			d = c;
			c = rotl(b, 30);
			b = a;
			a = t;
		}
		h0 += a;
		h1 += b;
		h2 += c;
		h3 += d;
		h4 += e;
	}
	free(buf);
	out = (unsigned char *)duk_push_fixed_buffer(ctx, 20);
	out[0] = (unsigned char)(h0 >> 24);
	out[1] = (unsigned char)(h0 >> 16);
	out[2] = (unsigned char)(h0 >> 8);
	out[3] = (unsigned char)h0;
	out[4] = (unsigned char)(h1 >> 24);
	out[5] = (unsigned char)(h1 >> 16);
	out[6] = (unsigned char)(h1 >> 8);
	out[7] = (unsigned char)h1;
	out[8] = (unsigned char)(h2 >> 24);
	out[9] = (unsigned char)(h2 >> 16);
	out[10] = (unsigned char)(h2 >> 8);
	out[11] = (unsigned char)h2;
	out[12] = (unsigned char)(h3 >> 24);
	out[13] = (unsigned char)(h3 >> 16);
	out[14] = (unsigned char)(h3 >> 8);
	out[15] = (unsigned char)h3;
	out[16] = (unsigned char)(h4 >> 24);
	out[17] = (unsigned char)(h4 >> 16);
	out[18] = (unsigned char)(h4 >> 8);
	out[19] = (unsigned char)h4;
	return 1;
}

static duk_ret_t
js_md5(duk_context *ctx)
{
	size_t n = 0;
	const unsigned char *msg = (const unsigned char *)duk_require_lstring(ctx, 0, &n);
	static const uint32_t K[64] = {
		0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
		0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
		0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
		0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
		0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
		0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
		0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
		0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
	};
	static const int S[64] = {
		7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
		5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
		4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
		6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
	};
	uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
	size_t pad = (n % 64 < 56) ? (56 - n % 64) : (120 - n % 64);
	size_t total = n + pad + 8;
	unsigned char *buf = (unsigned char *)calloc(1, total ? total : 1);
	uint64_t bits = (uint64_t)n * 8;
	unsigned char *out;
	size_t off;
	if (!buf)
		return duk_error(ctx, DUK_ERR_ERROR, "md5");
	memcpy(buf, msg, n);
	buf[n] = 0x80;
	memcpy(buf + total - 8, &bits, 8);
	for (off = 0; off < total; off += 64) {
		uint32_t M[16];
		uint32_t A = a0, B = b0, C = c0, D = d0;
		int i;
		for (i = 0; i < 16; i++)
			memcpy(&M[i], buf + off + i * 4, 4);
		for (i = 0; i < 64; i++) {
			uint32_t F, g, t;
			if (i < 16) {
				F = (B & C) | ((~B) & D);
				g = (uint32_t)i;
			} else if (i < 32) {
				F = (D & B) | ((~D) & C);
				g = (5u * i + 1u) % 16u;
			} else if (i < 48) {
				F = B ^ C ^ D;
				g = (3u * i + 5u) % 16u;
			} else {
				F = C ^ (B | (~D));
				g = (7u * i) % 16u;
			}
			t = D;
			D = C;
			C = B;
			B = B + rotl(A + F + K[i] + M[g], S[i]);
			A = t;
		}
		a0 += A;
		b0 += B;
		c0 += C;
		d0 += D;
	}
	free(buf);
	out = (unsigned char *)duk_push_fixed_buffer(ctx, 16);
	memcpy(out, &a0, 4);
	memcpy(out + 4, &b0, 4);
	memcpy(out + 8, &c0, 4);
	memcpy(out + 12, &d0, 4);
	return 1;
}

static void
push_http(duk_context *ctx, int status, const char *body, size_t n, const char *hdrs, const char *err)
{
	int idx = 0;
	duk_push_object(ctx);
	duk_push_int(ctx, status);
	duk_put_prop_string(ctx, -2, "statuscode");
	duk_push_lstring(ctx, body ? body : "", n);
	duk_put_prop_string(ctx, -2, "body");
	if (err && err[0]) {
		duk_push_string(ctx, err);
		duk_put_prop_string(ctx, -2, "error");
	}
	duk_push_array(ctx);
	if (hdrs) {
		const char *p = hdrs;
		const char *last = hdrs;
		const char *q;
		for (q = hdrs; *q; q++) {
			if (q[0] == '\n' && (strncmp(q + 1, "HTTP/", 5) == 0))
				last = q + 1;
		}
		p = last;
		while (*p && idx < 64) {
			const char *eol = strchr(p, '\n');
			size_t len = eol ? (size_t)(eol - p) : strlen(p);
			const char *col;
			if (len && p[len - 1] == '\r')
				len--;
			col = memchr(p, ':', len);
			if (col && col > p) {
				duk_push_lstring(ctx, p, (size_t)(col - p));
				duk_put_prop_index(ctx, -2, (duk_uarridx_t)idx++);
				col++;
				while (col < p + len && *col == ' ')
					col++;
				duk_push_lstring(ctx, col, (size_t)((p + len) - col));
				duk_put_prop_index(ctx, -2, (duk_uarridx_t)idx++);
			}
			if (!eol)
				break;
			p = eol + 1;
		}
	}
	duk_put_prop_string(ctx, -2, "headers");
}

#ifdef PLUGIN_HOST_TEST
static int
http_curl(const char *url, const char *method, const char *headerblob, const char *post,
	  char **body, size_t *blen, char **hdrs, int *status, char *err, size_t errn)
{
	char bodypath[64], hdrpath[64];
	int pipes[2];
	pid_t pid;
	int code = 0;
	char *argv[64];
	int argc = 0;
	char statusbuf[16];
	size_t got = 0;
	snprintf(bodypath, sizeof bodypath, "/tmp/mvhttp-%d.body", (int)getpid());
	snprintf(hdrpath, sizeof hdrpath, "/tmp/mvhttp-%d.hdr", (int)getpid());
	argv[argc++] = "curl";
	argv[argc++] = "-sS";
	argv[argc++] = "-L";
	argv[argc++] = "--max-time";
	argv[argc++] = "12";
	argv[argc++] = "--max-filesize";
	argv[argc++] = "2000000";
	argv[argc++] = "-D";
	argv[argc++] = hdrpath;
	argv[argc++] = "-o";
	argv[argc++] = bodypath;
	argv[argc++] = "-w";
	argv[argc++] = "%{http_code}";
	argv[argc++] = "-A";
	argv[argc++] = "Movian/7.0.273";
	if (method && method[0] && strcmp(method, "GET") != 0) {
		argv[argc++] = "-X";
		argv[argc++] = (char *)method;
	}
	if (headerblob && headerblob[0]) {
		const char *p = headerblob;
		while (*p && argc < 58) {
			const char *eol = strchr(p, '\n');
			size_t len = eol ? (size_t)(eol - p) : strlen(p);
			char *line;
			if (len && p[len - 1] == '\r')
				len--;
			if (len) {
				line = (char *)malloc(len + 1);
				memcpy(line, p, len);
				line[len] = 0;
				argv[argc++] = "-H";
				argv[argc++] = line;
			}
			if (!eol)
				break;
			p = eol + 1;
		}
	}
	if (post && post[0] && argc < 60) {
		argv[argc++] = "--data-binary";
		argv[argc++] = (char *)post;
	}
	argv[argc++] = (char *)url;
	argv[argc] = NULL;
	if (pipe(pipes) != 0) {
		snprintf(err, errn, "pipe");
		return -1;
	}
	pid = fork();
	if (pid < 0) {
		snprintf(err, errn, "fork");
		return -1;
	}
	if (pid == 0) {
		dup2(pipes[1], 1);
		close(pipes[0]);
		close(pipes[1]);
		execvp("curl", argv);
		_exit(127);
	}
	close(pipes[1]);
	while (got + 1 < sizeof statusbuf) {
		ssize_t r = read(pipes[0], statusbuf + got, sizeof statusbuf - 1 - got);
		if (r <= 0)
			break;
		got += (size_t)r;
	}
	statusbuf[got] = 0;
	close(pipes[0]);
	waitpid(pid, &code, 0);
	*status = atoi(statusbuf);
	*body = read_file(bodypath, blen, HTTP_CAP);
	*hdrs = read_file(hdrpath, NULL, 65536);
	remove(bodypath);
	remove(hdrpath);
	if (!WIFEXITED(code) || WEXITSTATUS(code) != 0 || *status == 0) {
		snprintf(err, errn, "http %s", url);
		return -1;
	}
	if (!*body) {
		*body = (char *)calloc(1, 1);
		*blen = 0;
	}
	return 0;
}
#else
static int
http_ps5_init(char *err, size_t errn)
{
	if (g_http_ready)
		return 0;
	if (sceNetInit()) {
		snprintf(err, errn, "sceNetInit");
		return -1;
	}
	g_pool = sceNetPoolCreate("movianhttp", 256 * 1024, 0);
	if (g_pool < 0) {
		snprintf(err, errn, "sceNetPoolCreate");
		return -1;
	}
	g_ssl = sceSslInit(256 * 1024);
	if (g_ssl < 0) {
		snprintf(err, errn, "sceSslInit");
		return -1;
	}
	g_http = sceHttp2Init(g_pool, g_ssl, 256 * 1024, 1);
	if (g_http < 0) {
		snprintf(err, errn, "sceHttp2Init");
		return -1;
	}
	g_tmpl = sceHttp2CreateTemplate(g_http, "Movian/7.0.273", 3, 1);
	if (g_tmpl < 0) {
		snprintf(err, errn, "sceHttp2CreateTemplate");
		return -1;
	}
	g_http_ready = 1;
	return 0;
}

static int
http_ps5(const char *url, const char *method, const char *headerblob, const char *post,
	 char **body, size_t *blen, int *status, char *err, size_t errn)
{
	int req;
	int st = 0;
	char *buf;
	size_t used = 0;
	const char *m = (method && method[0]) ? method : "GET";
	if (http_ps5_init(err, errn) != 0)
		return -1;
	req = sceHttp2CreateRequestWithURL(g_tmpl, m, url, 0);
	if (req < 0) {
		snprintf(err, errn, "sceHttp2CreateRequestWithURL");
		return -1;
	}
	if (headerblob && headerblob[0]) {
		const char *p = headerblob;
		while (*p) {
			const char *eol = strchr(p, '\n');
			size_t len = eol ? (size_t)(eol - p) : strlen(p);
			char line[300];
			char *col;
			if (len && p[len - 1] == '\r')
				len--;
			if (len > 0 && len < sizeof line) {
				memcpy(line, p, len);
				line[len] = 0;
				col = strchr(line, ':');
				if (col) {
					*col = 0;
					col++;
					while (*col == ' ')
						col++;
					sceHttp2AddRequestHeader(req, line, col, 0);
				}
			}
			if (!eol)
				break;
			p = eol + 1;
		}
	}
	if (sceHttp2SendRequest(req, (post && post[0]) ? post : NULL, (post && post[0]) ? strlen(post) : 0)) {
		sceHttp2DeleteRequest(req);
		snprintf(err, errn, "sceHttp2SendRequest");
		return -1;
	}
	if (sceHttp2GetStatusCode(req, &st)) {
		sceHttp2DeleteRequest(req);
		snprintf(err, errn, "sceHttp2GetStatusCode");
		return -1;
	}
	buf = (char *)malloc(HTTP_CAP);
	if (!buf) {
		sceHttp2DeleteRequest(req);
		snprintf(err, errn, "http buffer");
		return -1;
	}
	for (;;) {
		char tmp[4096];
		int n = sceHttp2ReadData(req, tmp, sizeof tmp);
		if (n < 0) {
			free(buf);
			sceHttp2DeleteRequest(req);
			snprintf(err, errn, "sceHttp2ReadData");
			return -1;
		}
		if (n == 0)
			break;
		if (used + (size_t)n > HTTP_CAP)
			n = (int)(HTTP_CAP - used);
		if (n > 0) {
			memcpy(buf + used, tmp, (size_t)n);
			used += (size_t)n;
		}
		if (used >= HTTP_CAP)
			break;
	}
	sceHttp2DeleteRequest(req);
	*body = buf;
	*blen = used;
	*status = st;
	return 0;
}
#endif

static duk_ret_t
js_http(duk_context *ctx)
{
	const char *url = duk_require_string(ctx, 0);
	const char *method = duk_require_string(ctx, 1);
	const char *headers = duk_require_string(ctx, 2);
	const char *post = duk_require_string(ctx, 3);
	char err[240];
	char *body = NULL;
	char *hdrs = NULL;
	size_t blen = 0;
	int status = 0;
	int rc;
	err[0] = 0;
	if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
		push_http(ctx, 0, "", 0, NULL, "Only http and https");
		return 1;
	}
#ifdef PLUGIN_HOST_TEST
	rc = http_curl(url, method, headers, post, &body, &blen, &hdrs, &status, err, sizeof err);
	(void)0;
#else
	rc = http_ps5(url, method, headers, post, &body, &blen, &status, err, sizeof err);
#endif
	if (rc != 0 && !err[0])
		snprintf(err, sizeof err, "http failed");
	push_http(ctx, status, body, blen, hdrs, rc ? err : NULL);
	free(body);
	free(hdrs);
	return 1;
}

static void
bind(duk_context *ctx, const char *name, duk_c_function fn, int nargs)
{
	duk_push_c_function(ctx, fn, nargs);
	duk_put_global_string(ctx, name);
}

static int
boot_heap(void)
{
	int len = 0;
	const char *src;
	if (g_ctx)
		duk_destroy_heap(g_ctx);
	g_ctx = duk_create_heap_default();
	if (!g_ctx) {
		set_status("duktape heap failed");
		return -1;
	}
	bind(g_ctx, "mov_log", js_log, 1);
	bind(g_ctx, "mov_source", js_source, 1);
	bind(g_ctx, "mov_read", js_read, 1);
	bind(g_ctx, "mov_write", js_write, 2);
	bind(g_ctx, "mov_mkdirs", js_mkdirs, 1);
	bind(g_ctx, "mov_http", js_http, 5);
	bind(g_ctx, "mov_is_utf8", js_is_utf8, 1);
	bind(g_ctx, "mov_cp1251", js_cp1251, 1);
	bind(g_ctx, "mov_sha1", js_sha1, 1);
	bind(g_ctx, "mov_md5", js_md5, 1);
	src = blob("boot", &len);
	if (!src) {
		set_status("missing plugin bootstrap");
		return -1;
	}
	if (duk_peval_lstring(g_ctx, src, (size_t)len) != 0) {
		snprintf(g_status, sizeof g_status, "bootstrap: %.400s", duk_safe_to_string(g_ctx, -1));
		duk_pop(g_ctx);
		return -1;
	}
	duk_pop(g_ctx);
	return 0;
}

static void
take_string(duk_context *ctx, duk_idx_t obj, const char *key, char *dst, size_t n)
{
	dst[0] = 0;
	if (!duk_get_prop_string(ctx, obj, key)) {
		duk_pop(ctx);
		return;
	}
	copy_str(dst, n, duk_safe_to_string(ctx, -1));
	duk_pop(ctx);
}

static int
read_services(void)
{
	duk_idx_t arr;
	duk_size_t n;
	duk_size_t i;
	g_service_n = 0;
	duk_get_global_string(g_ctx, "__movianServices");
	if (duk_pcall(g_ctx, 0) != 0) {
		snprintf(g_status, sizeof g_status, "services: %.400s", duk_safe_to_string(g_ctx, -1));
		duk_pop(g_ctx);
		return -1;
	}
	if (!duk_is_array(g_ctx, -1)) {
		duk_pop(g_ctx);
		return 0;
	}
	arr = duk_normalize_index(g_ctx, -1);
	n = duk_get_length(g_ctx, arr);
	for (i = 0; i < n && g_service_n < 16; i++) {
		duk_get_prop_index(g_ctx, arr, (duk_uarridx_t)i);
		take_string(g_ctx, -1, "title", g_services[g_service_n].title, sizeof g_services[g_service_n].title);
		take_string(g_ctx, -1, "url", g_services[g_service_n].url, sizeof g_services[g_service_n].url);
		take_string(g_ctx, -1, "type", g_services[g_service_n].type, sizeof g_services[g_service_n].type);
		duk_pop(g_ctx);
		if (g_services[g_service_n].url[0])
			g_service_n++;
	}
	duk_pop(g_ctx);
	return 0;
}

static int
read_page(void)
{
	duk_idx_t arr;
	duk_size_t n;
	duk_size_t i;
	g_item_n = 0;
	g_page_title[0] = 0;
	g_page_error[0] = 0;
	take_string(g_ctx, -1, "title", g_page_title, sizeof g_page_title);
	take_string(g_ctx, -1, "error", g_page_error, sizeof g_page_error);
	if (!duk_get_prop_string(g_ctx, -1, "items")) {
		duk_pop(g_ctx);
		return 0;
	}
	if (!duk_is_array(g_ctx, -1)) {
		duk_pop(g_ctx);
		return 0;
	}
	arr = duk_normalize_index(g_ctx, -1);
	n = duk_get_length(g_ctx, arr);
	for (i = 0; i < n && g_item_n < PLUG_ITEMS; i++) {
		duk_get_prop_index(g_ctx, arr, (duk_uarridx_t)i);
		take_string(g_ctx, -1, "title", g_items[g_item_n].title, sizeof g_items[g_item_n].title);
		take_string(g_ctx, -1, "url", g_items[g_item_n].url, sizeof g_items[g_item_n].url);
		take_string(g_ctx, -1, "type", g_items[g_item_n].type, sizeof g_items[g_item_n].type);
		duk_pop(g_ctx);
		g_item_n++;
	}
	duk_pop(g_ctx);
	return 0;
}

int
plug_load_dir(const char *dir)
{
	char manifest_path[640];
	char entry_path[700];
	char *manifest = NULL;
	char *source = NULL;
	size_t slen = 0;
	char id[64], file[128], title[PLUG_NAME];
	if (!dir || !dir[0]) {
		set_status("no plugin directory");
		return -1;
	}
	snprintf(manifest_path, sizeof manifest_path, "%s/plugin.json", dir);
	manifest = read_file(manifest_path, NULL, 256 * 1024);
	if (!manifest) {
		snprintf(g_status, sizeof g_status, "no plugin.json in %.200s", dir);
		return -1;
	}
	id[0] = file[0] = title[0] = 0;
	json_string(manifest, "id", id, sizeof id);
	json_string(manifest, "file", file, sizeof file);
	json_string(manifest, "title", title, sizeof title);
	if (!file[0])
		copy_str(file, sizeof file, "plugin.js");
	snprintf(entry_path, sizeof entry_path, "%s/%s", dir, file);
	source = read_file(entry_path, &slen, HTTP_CAP);
	if (!source) {
		snprintf(g_status, sizeof g_status, "missing %s", file);
		free(manifest);
		return -1;
	}
	if (boot_heap() != 0) {
		free(manifest);
		free(source);
		return -1;
	}
	duk_get_global_string(g_ctx, "__movianLoad");
	duk_push_string(g_ctx, dir);
	duk_push_string(g_ctx, manifest);
	duk_push_lstring(g_ctx, source, slen);
	free(manifest);
	free(source);
	if (duk_pcall(g_ctx, 3) != 0) {
		snprintf(g_status, sizeof g_status, "load: %.400s", duk_safe_to_string(g_ctx, -1));
		duk_pop(g_ctx);
		return -1;
	}
	if (duk_is_string(g_ctx, -1) && duk_get_length(g_ctx, -1) > 0) {
		snprintf(g_status, sizeof g_status, "%.400s", duk_safe_to_string(g_ctx, -1));
		duk_pop(g_ctx);
		return -1;
	}
	duk_pop(g_ctx);
	if (read_services() != 0)
		return -1;
	snprintf(g_status, sizeof g_status, "%s  %d routes", id[0] ? id : "plugin", g_service_n);
	g_sp = 0;
	g_item_n = 0;
	g_page_title[0] = 0;
	g_page_error[0] = 0;
	(void)title;
	return 0;
}

static int
open_url(const char *url, int push)
{
	if (!g_ctx || !url || !url[0]) {
		set_status("no page");
		return -1;
	}
	duk_get_global_string(g_ctx, "__movianOpen");
	duk_push_string(g_ctx, url);
	if (duk_pcall(g_ctx, 1) != 0) {
		snprintf(g_status, sizeof g_status, "open: %.400s", duk_safe_to_string(g_ctx, -1));
		duk_pop(g_ctx);
		return -1;
	}
	read_page();
	duk_pop(g_ctx);
	if (push && g_sp < 8) {
		copy_str(g_stack[g_sp], sizeof g_stack[g_sp], url);
		g_sp++;
	}
	if (g_page_error[0])
		copy_str(g_status, sizeof g_status, g_page_error);
	else
		snprintf(g_status, sizeof g_status, "%d items", g_item_n);
	return 0;
}

int
plug_open(const char *url)
{
	return open_url(url, 1);
}

int
plug_activate(int index)
{
	int rc;
	if (index < 0 || index >= g_pkg_n)
		return -1;
	rc = plug_load_dir(g_pkgs[index].dir);
	if (rc != 0)
		return rc;
	if (g_service_n < 1) {
		set_status("plugin has no service");
		return -1;
	}
	return plug_open(g_services[0].url);
}

int
plug_back(void)
{
	if (g_sp <= 1)
		return 1;
	g_sp--;
	return open_url(g_stack[g_sp - 1], 0);
}

static int
has_plugin_json(const char *dir)
{
	char path[640];
	struct stat st;
	snprintf(path, sizeof path, "%s/plugin.json", dir);
	return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void
add_pkg(const char *dir)
{
	char path[640];
	char *json;
	char id[64], title[PLUG_NAME], file[128];
	int i;
	if (g_pkg_n >= PLUG_MAX)
		return;
	for (i = 0; i < g_pkg_n; i++) {
		if (strcmp(g_pkgs[i].dir, dir) == 0)
			return;
	}
	snprintf(path, sizeof path, "%s/plugin.json", dir);
	json = read_file(path, NULL, 256 * 1024);
	if (!json)
		return;
	id[0] = title[0] = file[0] = 0;
	json_string(json, "id", id, sizeof id);
	json_string(json, "title", title, sizeof title);
	json_string(json, "file", file, sizeof file);
	free(json);
	if (!id[0] && !title[0])
		return;
	copy_str(g_pkgs[g_pkg_n].dir, sizeof g_pkgs[g_pkg_n].dir, dir);
	copy_str(g_pkgs[g_pkg_n].id, sizeof g_pkgs[g_pkg_n].id, id[0] ? id : title);
	copy_str(g_pkgs[g_pkg_n].title, sizeof g_pkgs[g_pkg_n].title, title[0] ? title : id);
	copy_str(g_pkgs[g_pkg_n].file, sizeof g_pkgs[g_pkg_n].file, file);
	g_pkg_n++;
}

static void
scan_tree(const char *dir, int depth)
{
	DIR *d;
	struct dirent *de;
	if (depth > 2 || g_pkg_n >= PLUG_MAX)
		return;
	if (has_plugin_json(dir)) {
		add_pkg(dir);
		return;
	}
	d = opendir(dir);
	if (!d)
		return;
	while ((de = readdir(d)) != NULL) {
		char next[640];
		struct stat st;
		if (de->d_name[0] == '.')
			continue;
		snprintf(next, sizeof next, "%s/%s", dir, de->d_name);
		if (stat(next, &st) != 0 || !S_ISDIR(st.st_mode))
			continue;
		scan_tree(next, depth + 1);
	}
	closedir(d);
}

int
plug_scan(void)
{
	static const char *roots[] = {
		"/mnt/usb0/plugins",
		"/mnt/usb1/plugins",
		"/mnt/usb0/movian/plugins",
		"/data/movian/plugins",
		"/user/movian/plugins",
		NULL
	};
	int i;
	g_pkg_n = 0;
	for (i = 0; roots[i]; i++)
		scan_tree(roots[i], 0);
	snprintf(g_status, sizeof g_status, "%d plugin%s", g_pkg_n, g_pkg_n == 1 ? "" : "s");
	return g_pkg_n;
}

#ifndef PLUGIN_HOST_TEST
/* PS5 libc has gettimeofday, not these. UTC is enough. strptime fails closed. */
static void
civil_from_unix(time_t t, struct tm *out)
{
	long long z = (long long)t;
	long long days, secs;
	int era, doe, yoe, y, doy, mp, d, m;
	memset(out, 0, sizeof *out);
	if (z < 0)
		z = 0;
	days = z / 86400;
	secs = z % 86400;
	out->tm_hour = (int)(secs / 3600);
	out->tm_min = (int)((secs % 3600) / 60);
	out->tm_sec = (int)(secs % 60);
	z = days + 719468;
	era = (int)(z / 146097);
	doe = (int)(z - (long long)era * 146097);
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	y = yoe + era * 400;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	d = doy - (153 * mp + 2) / 5 + 1;
	m = mp + (mp < 10 ? 3 : -9);
	y += (m <= 2);
	out->tm_year = y - 1900;
	out->tm_mon = m - 1;
	out->tm_mday = d;
	out->tm_wday = (int)((days + 4) % 7);
	out->tm_yday = doy;
	out->tm_isdst = 0;
}

struct tm *
gmtime_r(const time_t *tp, struct tm *out)
{
	if (!tp || !out)
		return NULL;
	civil_from_unix(*tp, out);
	return out;
}

struct tm *
localtime_r(const time_t *tp, struct tm *out)
{
	return gmtime_r(tp, out);
}

char *
strptime(const char *s, const char *fmt, struct tm *tm)
{
	(void)s;
	(void)fmt;
	(void)tm;
	return NULL;
}
#endif

#ifdef PLUGIN_HOST_TEST
int
main(int argc, char **argv)
{
	int i;
	const char *dir = argc > 1 ? argv[1] : "/tmp/movian-plugins/installed/YooTV";
	const char *open_at = argc > 2 ? argv[2] : NULL;
	printf("%s\n", plug_engine_line());
	if (plug_load_dir(dir) != 0) {
		printf("LOAD FAIL %s\n", plug_status());
		return 1;
	}
	printf("STATUS %s\n", plug_status());
	printf("SERVICES %d\n", plug_service_count());
	for (i = 0; i < plug_service_count(); i++)
		printf("SVC %s\n", plug_service_url(i));
	if (!open_at && plug_service_count() > 0)
		open_at = plug_service_url(0);
	if (open_at && plug_open(open_at) != 0) {
		printf("OPEN FAIL %s\n", plug_status());
		return 1;
	}
	printf("PAGE %s\n", plug_page_title());
	if (plug_page_error()[0])
		printf("ERR %s\n", plug_page_error());
	printf("ITEMS %d\n", plug_item_count());
	for (i = 0; i < plug_item_count(); i++)
		printf("ITEM %s | %s | %s\n", plug_item_type(i), plug_item_title(i), plug_item_url(i));
	return plug_item_count() > 0 || plug_service_count() > 0 ? 0 : 2;
}
#endif
