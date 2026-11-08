/* BitTorrent reader for the payload.
 *
 * A magnet or a .torrent becomes one video file. Pieces land in a side
 * file and bt_read returns bytes as soon as the pieces under that range
 * are checked. The player starts once the container header is in, the
 * same way Movian plays while the swarm is still uploading the rest.
 *
 * Peer connections are the plaintext wire. Trackers are HTTP and UDP.
 * There is no DHT and no message-stream encryption in this file.
 */

#include "bt.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define BT_PEERS 20
#define BT_TRACK 12
#define BT_KNOWN 64
#define BT_META_MAX (8 * 1024 * 1024)
#define BT_PIECE_MAX (8 * 1024 * 1024)
#define BT_BLOCK 16384

enum { TR_NONE = 0, TR_HTTP, TR_UDP, TR_WAIT, TR_ANN };
enum { P_EMPTY = 0, P_CONN, P_HS, P_LIVE };

typedef struct {
	char host[192];
	int port;
	char path[256];
	int kind;
	int fd;
	int state;
	uint32_t tx;
	uint64_t conn;
	double next_ms;
	uint8_t rbuf[2048];
	int rn;
	char req[1400];
} track_t;

typedef struct {
	int fd;
	int state;
	int choked;
	uint8_t ip[4];
	int port;
	uint8_t rbuf[48 * 1024];
	int rn;
	uint8_t *wbuf;
	int wn, wo;
	uint8_t *bits;
	int bits_n;
	int ut_meta;
	int ext;
	double last_ms;
	int reqs;
} peer_t;

typedef struct {
	char name[256];
	int64_t off;
	int64_t len;
	int video;
} file_t;

static struct {
	int on;
	char src[768];
	uint8_t infohash[20];
	uint8_t peer_id[20];
	int have_hash;
	uint8_t *meta;
	int meta_n;
	int meta_got;
	uint8_t *meta_have;
	int meta_pieces;
	char name[256];
	int64_t total;
	int piece_len;
	int npieces;
	uint8_t *hashes;
	uint8_t *have;
	uint64_t *map;
	int have_n;
	int64_t have_bytes;
	int64_t blob_end;
	FILE *blob;
	char dir[512];
	file_t files[64];
	int nfiles;
	int play;
	int64_t pos;
	uint8_t *build;
	int build_i;
	int build_blocks;
	uint8_t *build_got;
	int prio;
	track_t tr[BT_TRACK];
	int ntr;
	peer_t peers[BT_PEERS];
	uint8_t known_ip[BT_KNOWN][4];
	int known_port[BT_KNOWN];
	int nknown;
	double rx_stamp;
	int64_t rx_bytes;
	int64_t rx_mark;
	double rate;
	char line[96];
	int announced;
} S;

static double
now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

static uint32_t
be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t
be64(const uint8_t *p)
{
	return ((uint64_t)be32(p) << 32) | be32(p + 4);
}

static void
put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static void
put64(uint8_t *p, uint64_t v)
{
	put32(p, (uint32_t)(v >> 32));
	put32(p + 4, (uint32_t)v);
}

static uint32_t
rnd32(void)
{
	static uint32_t x;
	if (!x) {
		x = (uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)&x;
		if (!x)
			x = 0x5a17u;
	}
	x = x * 1664525u + 1013904223u;
	return x;
}

/* --- sha1 --- */

void
bt_sha1(const uint8_t *msg, int n, uint8_t out[20])
{
	uint32_t h0 = 0x67452301u, h1 = 0xEFCDAB89u, h2 = 0x98BADCFEu, h3 = 0x10325476u, h4 = 0xC3D2E1F0u;
	uint64_t bits = (uint64_t)n * 8u;
	uint8_t block[64];
	int off = 0;
	while (off + 64 <= n) {
		const uint8_t *p = msg + off;
		uint32_t w[80];
		uint32_t a, b, c, d, e;
		int i;
		for (i = 0; i < 16; i++)
			w[i] = be32(p + i * 4);
		for (i = 16; i < 80; i++) {
			uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
			w[i] = (x << 1) | (x >> 31);
		}
		a = h0;
		b = h1;
		c = h2;
		d = h3;
		e = h4;
		for (i = 0; i < 80; i++) {
			uint32_t f, k, t;
			if (i < 20) {
				f = (b & c) | ((~b) & d);
				k = 0x5A827999u;
			} else if (i < 40) {
				f = b ^ c ^ d;
				k = 0x6ED9EBA1u;
			} else if (i < 60) {
				f = (b & c) | (b & d) | (c & d);
				k = 0x8F1BBCDCu;
			} else {
				f = b ^ c ^ d;
				k = 0xCA62C1D6u;
			}
			t = ((a << 5) | (a >> 27)) + f + e + k + w[i];
			e = d;
			d = c;
			c = (b << 30) | (b >> 2);
			b = a;
			a = t;
		}
		h0 += a;
		h1 += b;
		h2 += c;
		h3 += d;
		h4 += e;
		off += 64;
	}
	{
		int rem = n - off;
		int i;
		uint32_t w[80];
		uint32_t a, b, c, d, e;
		memset(block, 0, 64);
		if (rem)
			memcpy(block, msg + off, (size_t)rem);
		block[rem] = 0x80;
		if (rem >= 56) {
			for (i = 0; i < 16; i++)
				w[i] = be32(block + i * 4);
			for (i = 16; i < 80; i++) {
				uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
				w[i] = (x << 1) | (x >> 31);
			}
			a = h0;
			b = h1;
			c = h2;
			d = h3;
			e = h4;
			for (i = 0; i < 80; i++) {
				uint32_t f, k, t;
				if (i < 20) {
					f = (b & c) | ((~b) & d);
					k = 0x5A827999u;
				} else if (i < 40) {
					f = b ^ c ^ d;
					k = 0x6ED9EBA1u;
				} else if (i < 60) {
					f = (b & c) | (b & d) | (c & d);
					k = 0x8F1BBCDCu;
				} else {
					f = b ^ c ^ d;
					k = 0xCA62C1D6u;
				}
				t = ((a << 5) | (a >> 27)) + f + e + k + w[i];
				e = d;
				d = c;
				c = (b << 30) | (b >> 2);
				b = a;
				a = t;
			}
			h0 += a;
			h1 += b;
			h2 += c;
			h3 += d;
			h4 += e;
			memset(block, 0, 64);
		}
		put64(block + 56, bits);
		for (i = 0; i < 16; i++)
			w[i] = be32(block + i * 4);
		for (i = 16; i < 80; i++) {
			uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
			w[i] = (x << 1) | (x >> 31);
		}
		a = h0;
		b = h1;
		c = h2;
		d = h3;
		e = h4;
		for (i = 0; i < 80; i++) {
			uint32_t f, k, t;
			if (i < 20) {
				f = (b & c) | ((~b) & d);
				k = 0x5A827999u;
			} else if (i < 40) {
				f = b ^ c ^ d;
				k = 0x6ED9EBA1u;
			} else if (i < 60) {
				f = (b & c) | (b & d) | (c & d);
				k = 0x8F1BBCDCu;
			} else {
				f = b ^ c ^ d;
				k = 0xCA62C1D6u;
			}
			t = ((a << 5) | (a >> 27)) + f + e + k + w[i];
			e = d;
			d = c;
			c = (b << 30) | (b >> 2);
			b = a;
			a = t;
		}
		h0 += a;
		h1 += b;
		h2 += c;
		h3 += d;
		h4 += e;
	}
	put32(out, h0);
	put32(out + 4, h1);
	put32(out + 8, h2);
	put32(out + 12, h3);
	put32(out + 16, h4);
}

/* --- bencode cursor --- */

typedef struct {
	const uint8_t *p;
	int n;
	int i;
} ben_t;

static int ben_skip(ben_t *b);

static int
ben_str(ben_t *b, const uint8_t **s, int *sn)
{
	int n = 0;
	if (b->i >= b->n || b->p[b->i] < '0' || b->p[b->i] > '9')
		return -1;
	while (b->i < b->n && b->p[b->i] >= '0' && b->p[b->i] <= '9') {
		n = n * 10 + (b->p[b->i] - '0');
		if (n > BT_META_MAX)
			return -1;
		b->i++;
	}
	if (b->i >= b->n || b->p[b->i] != ':')
		return -1;
	b->i++;
	if (n < 0 || b->i + n > b->n)
		return -1;
	*s = b->p + b->i;
	*sn = n;
	b->i += n;
	return 0;
}

static int
ben_int(ben_t *b, int64_t *v)
{
	int sig = 1;
	int64_t x = 0;
	if (b->i >= b->n || b->p[b->i] != 'i')
		return -1;
	b->i++;
	if (b->i < b->n && b->p[b->i] == '-') {
		sig = -1;
		b->i++;
	}
	if (b->i >= b->n || b->p[b->i] < '0' || b->p[b->i] > '9')
		return -1;
	while (b->i < b->n && b->p[b->i] >= '0' && b->p[b->i] <= '9') {
		x = x * 10 + (b->p[b->i] - '0');
		b->i++;
	}
	if (b->i >= b->n || b->p[b->i] != 'e')
		return -1;
	b->i++;
	*v = x * sig;
	return 0;
}

static int
ben_skip(ben_t *b)
{
	if (b->i >= b->n)
		return -1;
	if (b->p[b->i] == 'i') {
		int64_t v;
		return ben_int(b, &v);
	}
	if (b->p[b->i] >= '0' && b->p[b->i] <= '9') {
		const uint8_t *s;
		int n;
		return ben_str(b, &s, &n);
	}
	if (b->p[b->i] == 'd' || b->p[b->i] == 'l') {
		b->i++;
		while (b->i < b->n && b->p[b->i] != 'e') {
			if (ben_skip(b))
				return -1;
		}
		if (b->i >= b->n || b->p[b->i] != 'e')
			return -1;
		b->i++;
		return 0;
	}
	return -1;
}

static int
key_is(const uint8_t *s, int n, const char *lit)
{
	int L = (int)strlen(lit);
	return n == L && memcmp(s, lit, (size_t)L) == 0;
}

static int
video_name(const char *name)
{
	static const char *ext[] = {".mkv", ".mp4", ".m4v", ".mov", ".ts", ".m2ts", ".mts",
				    ".mpg", ".mpeg", ".m2v", NULL};
	int i;
	int L = (int)strlen(name);
	for (i = 0; ext[i]; i++) {
		int e = (int)strlen(ext[i]);
		int k;
		if (L < e)
			continue;
		for (k = 0; k < e; k++) {
			char a = name[L - e + k];
			char b = ext[i][k];
			if (a >= 'A' && a <= 'Z')
				a = (char)(a - 'A' + 'a');
			if (a != b)
				break;
		}
		if (k == e)
			return 1;
	}
	return 0;
}

static int
parse_info(const uint8_t *p, int n)
{
	ben_t b;
	int64_t plen = 0, length = 0;
	const uint8_t *pieces = NULL;
	int pieces_n = 0;
	char name[256];
	file_t tmp[64];
	int nf = 0;
	int64_t acc = 0;
	b.p = p;
	b.n = n;
	b.i = 0;
	name[0] = 0;
	if (b.i >= b.n || p[b.i] != 'd')
		return -1;
	b.i++;
	while (b.i < b.n && p[b.i] != 'e') {
		const uint8_t *k;
		int kn;
		if (ben_str(&b, &k, &kn))
			return -1;
		if (key_is(k, kn, "piece length")) {
			if (ben_int(&b, &plen))
				return -1;
		} else if (key_is(k, kn, "length")) {
			if (ben_int(&b, &length))
				return -1;
		} else if (key_is(k, kn, "name")) {
			const uint8_t *s;
			int sn;
			if (ben_str(&b, &s, &sn))
				return -1;
			if (sn > 240)
				sn = 240;
			memcpy(name, s, (size_t)sn);
			name[sn] = 0;
		} else if (key_is(k, kn, "pieces")) {
			if (ben_str(&b, &pieces, &pieces_n))
				return -1;
		} else if (key_is(k, kn, "files")) {
			if (b.i >= b.n || p[b.i] != 'l')
				return -1;
			b.i++;
			while (b.i < b.n && p[b.i] != 'e') {
				int64_t flen = 0;
				char fn[256];
				fn[0] = 0;
				if (b.i >= b.n || p[b.i] != 'd')
					return -1;
				b.i++;
				while (b.i < b.n && p[b.i] != 'e') {
					const uint8_t *fk;
					int fkn;
					if (ben_str(&b, &fk, &fkn))
						return -1;
					if (key_is(fk, fkn, "length")) {
						if (ben_int(&b, &flen))
							return -1;
					} else if (key_is(fk, fkn, "path")) {
						if (b.i >= b.n || p[b.i] != 'l')
							return -1;
						b.i++;
						fn[0] = 0;
						while (b.i < b.n && p[b.i] != 'e') {
							const uint8_t *ps;
							int pn;
							int used;
							if (ben_str(&b, &ps, &pn))
								return -1;
							used = (int)strlen(fn);
							if (used && used < 250) {
								fn[used++] = '/';
								fn[used] = 0;
							}
							if (pn > 240 - used)
								pn = 240 - used;
							if (pn > 0) {
								memcpy(fn + used, ps, (size_t)pn);
								fn[used + pn] = 0;
							}
						}
						if (b.i >= b.n || p[b.i] != 'e')
							return -1;
						b.i++;
					} else if (ben_skip(&b)) {
						return -1;
					}
				}
				if (b.i >= b.n || p[b.i] != 'e')
					return -1;
				b.i++;
				if (nf < 64 && flen > 0) {
					snprintf(tmp[nf].name, sizeof tmp[nf].name, "%s", fn[0] ? fn : "file");
					tmp[nf].off = acc;
					tmp[nf].len = flen;
					tmp[nf].video = video_name(tmp[nf].name);
					nf++;
				}
				acc += flen;
			}
			if (b.i >= b.n || p[b.i] != 'e')
				return -1;
			b.i++;
		} else if (ben_skip(&b)) {
			return -1;
		}
	}
	if (plen < BT_BLOCK || plen > BT_PIECE_MAX || (plen % BT_BLOCK) != 0)
		return -1;
	if (!pieces || pieces_n < 20 || (pieces_n % 20) != 0)
		return -1;
	if (nf == 0) {
		if (length <= 0)
			return -1;
		snprintf(tmp[0].name, sizeof tmp[0].name, "%s", name[0] ? name : "video");
		tmp[0].off = 0;
		tmp[0].len = length;
		tmp[0].video = video_name(tmp[0].name);
		nf = 1;
		acc = length;
	}
	if (acc <= 0)
		return -1;
	{
		int np = pieces_n / 20;
		int64_t expect = (acc + plen - 1) / plen;
		if (expect != np || np < 1 || np > 250000)
			return -1;
		free(S.hashes);
		free(S.have);
		free(S.map);
		S.hashes = (uint8_t *)malloc((size_t)pieces_n);
		S.have = (uint8_t *)calloc((size_t)np, 1);
		S.map = (uint64_t *)malloc((size_t)np * sizeof(uint64_t));
		if (!S.hashes || !S.have || !S.map)
			return -1;
		memcpy(S.hashes, pieces, (size_t)pieces_n);
		memset(S.map, 0xff, (size_t)np * sizeof(uint64_t));
		S.npieces = np;
		S.piece_len = (int)plen;
		S.total = acc;
		S.nfiles = nf;
		memcpy(S.files, tmp, (size_t)nf * sizeof tmp[0]);
		S.have_n = 0;
		S.have_bytes = 0;
		S.blob_end = 0;
		S.prio = 0;
		S.build_i = -1;
	}
	{
		int i, best = -1;
		for (i = 0; i < S.nfiles; i++) {
			if (!S.files[i].video)
				continue;
			if (best < 0 || S.files[i].len > S.files[best].len)
				best = i;
		}
		if (best < 0) {
			best = 0;
			for (i = 1; i < S.nfiles; i++)
				if (S.files[i].len > S.files[best].len)
					best = i;
		}
		S.play = best;
		snprintf(S.name, sizeof S.name, "%s", S.files[best].name);
	}
	return 0;
}

static int
add_tracker(const char *url)
{
	const char *p;
	track_t *t;
	int https = 0;
	if (S.ntr >= BT_TRACK || !url)
		return -1;
	if (strncmp(url, "udp://", 6) == 0) {
		p = url + 6;
	} else if (strncmp(url, "http://", 7) == 0) {
		p = url + 7;
	} else if (strncmp(url, "https://", 8) == 0) {
		p = url + 8;
		https = 1;
	} else {
		return -1;
	}
	if (https)
		return -1;
	t = &S.tr[S.ntr];
	memset(t, 0, sizeof *t);
	t->fd = -1;
	t->kind = strncmp(url, "udp://", 6) == 0 ? TR_UDP : TR_HTTP;
	{
		int i = 0;
		while (p[i] && p[i] != ':' && p[i] != '/' && i < 190) {
			t->host[i] = p[i];
			i++;
		}
		t->host[i] = 0;
		p += i;
	}
	t->port = t->kind == TR_UDP ? 80 : 80;
	if (*p == ':') {
		p++;
		t->port = atoi(p);
		while (*p && *p != '/')
			p++;
	}
	if (t->port <= 0 || t->port > 65535 || !t->host[0])
		return -1;
	if (*p == 0)
		snprintf(t->path, sizeof t->path, "/announce");
	else
		snprintf(t->path, sizeof t->path, "%s", p);
	{
		char *q = strchr(t->path, '?');
		/* announce path keeps its query; we append ours with & or ? later */
		(void)q;
	}
	t->state = TR_WAIT;
	t->next_ms = now_ms();
	S.ntr++;
	return 0;
}

static int
parse_torrent(const uint8_t *p, int n)
{
	ben_t b;
	const uint8_t *info = NULL;
	int info_n = 0;
	b.p = p;
	b.n = n;
	b.i = 0;
	if (n < 10 || p[0] != 'd')
		return -1;
	b.i = 1;
	while (b.i < n && p[b.i] != 'e') {
		const uint8_t *k;
		int kn;
		int start;
		if (ben_str(&b, &k, &kn))
			return -1;
		start = b.i;
		if (key_is(k, kn, "info")) {
			if (ben_skip(&b))
				return -1;
			info = p + start;
			info_n = b.i - start;
		} else if (key_is(k, kn, "announce")) {
			const uint8_t *s;
			int sn;
			char url[512];
			if (ben_str(&b, &s, &sn))
				return -1;
			if (sn > 500)
				sn = 500;
			memcpy(url, s, (size_t)sn);
			url[sn] = 0;
			add_tracker(url);
		} else if (key_is(k, kn, "announce-list")) {
			if (b.i >= n || p[b.i] != 'l')
				return -1;
			b.i++;
			while (b.i < n && p[b.i] != 'e') {
				if (p[b.i] != 'l') {
					if (ben_skip(&b))
						return -1;
					continue;
				}
				b.i++;
				while (b.i < n && p[b.i] != 'e') {
					const uint8_t *s;
					int sn;
					char url[512];
					if (ben_str(&b, &s, &sn))
						return -1;
					if (sn > 0 && sn < 500) {
						memcpy(url, s, (size_t)sn);
						url[sn] = 0;
						add_tracker(url);
					}
				}
				if (b.i >= n || p[b.i] != 'e')
					return -1;
				b.i++;
			}
			if (b.i >= n || p[b.i] != 'e')
				return -1;
			b.i++;
		} else if (ben_skip(&b)) {
			return -1;
		}
	}
	if (!info)
		return -1;
	bt_sha1(info, info_n, S.infohash);
	S.have_hash = 1;
	return parse_info(info, info_n);
}

static int
hex_nibble(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int
b32_val(int c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A';
	if (c >= 'a' && c <= 'z')
		return c - 'a';
	if (c >= '2' && c <= '7')
		return c - '2' + 26;
	return -1;
}

static int
decode_b32(const char *s, int n, uint8_t out[20])
{
	int bits = 0, val = 0, o = 0;
	int i;
	if (n != 32)
		return -1;
	for (i = 0; i < n; i++) {
		int v = b32_val((unsigned char)s[i]);
		if (v < 0)
			return -1;
		val = (val << 5) | v;
		bits += 5;
		if (bits >= 8) {
			bits -= 8;
			if (o < 20)
				out[o++] = (uint8_t)((val >> bits) & 0xff);
		}
	}
	return o == 20 ? 0 : -1;
}

static void
pctdec(const char *s, int n, char *out, int outn)
{
	int o = 0;
	int i = 0;
	while (i < n && o + 1 < outn) {
		if (s[i] == '%' && i + 2 < n) {
			int a = hex_nibble((unsigned char)s[i + 1]);
			int b = hex_nibble((unsigned char)s[i + 2]);
			if (a >= 0 && b >= 0) {
				out[o++] = (char)((a << 4) | b);
				i += 3;
				continue;
			}
		}
		if (s[i] == '+')
			out[o++] = ' ';
		else
			out[o++] = s[i];
		i++;
	}
	out[o] = 0;
}

static int
parse_magnet(const char *src)
{
	const char *q;
	if (strncmp(src, "magnet:?", 8) != 0)
		return -1;
	q = src + 8;
	S.have_hash = 0;
	while (*q) {
		const char *amp = strchr(q, '&');
		int n = amp ? (int)(amp - q) : (int)strlen(q);
		const char *eq = memchr(q, '=', (size_t)n);
		char key[32];
		char val[700];
		int kn, vn;
		if (!eq) {
			q = amp ? amp + 1 : q + n;
			continue;
		}
		kn = (int)(eq - q);
		if (kn > 30)
			kn = 30;
		memcpy(key, q, (size_t)kn);
		key[kn] = 0;
		vn = n - kn - 1;
		if (vn > 680)
			vn = 680;
		pctdec(eq + 1, vn, val, (int)sizeof val);
		if (strcmp(key, "xt") == 0) {
			const char *h = strstr(val, "urn:btih:");
			if (h) {
				h += 9;
				if (strlen(h) == 40) {
					int i, ok = 1;
					for (i = 0; i < 20; i++) {
						int a = hex_nibble((unsigned char)h[i * 2]);
						int b = hex_nibble((unsigned char)h[i * 2 + 1]);
						if (a < 0 || b < 0) {
							ok = 0;
							break;
						}
						S.infohash[i] = (uint8_t)((a << 4) | b);
					}
					if (ok)
						S.have_hash = 1;
				} else if (decode_b32(h, (int)strlen(h), S.infohash) == 0) {
					S.have_hash = 1;
				}
			}
		} else if (strcmp(key, "dn") == 0) {
			if (val[0])
				snprintf(S.name, sizeof S.name, "%.240s", val);
		} else if (strcmp(key, "tr") == 0) {
			add_tracker(val);
		}
		if (!amp)
			break;
		q = amp + 1;
	}
	return S.have_hash ? 0 : -1;
}

static int
piece_size(int i)
{
	int64_t off, n;
	if (i < 0 || i >= S.npieces)
		return 0;
	off = (int64_t)i * S.piece_len;
	n = S.total - off;
	if (n > S.piece_len)
		n = S.piece_len;
	return (int)n;
}

static int
ensure_dir(void)
{
	static const char *roots[] = {"/data/splicer/bt", "/mnt/usb0/splicer-bt", "/user/splicer/bt",
				      "/tmp/splicer-bt", NULL};
	char hex[41];
	int i;
	for (i = 0; i < 20; i++)
		sprintf(hex + i * 2, "%02x", S.infohash[i]);
	hex[40] = 0;
	for (i = 0; roots[i]; i++) {
		char path[512];
		const char *r = roots[i];
		const char *slash = r;
		while ((slash = strchr(slash + 1, '/'))) {
			char bit[256];
			int n = (int)(slash - r);
			if (n <= 0 || n > 240)
				break;
			memcpy(bit, r, (size_t)n);
			bit[n] = 0;
			mkdir(bit, 0755);
		}
		mkdir(r, 0755);
		snprintf(path, sizeof path, "%s/%s", r, hex);
		if (mkdir(path, 0755) && errno != EEXIST)
			continue;
		snprintf(S.dir, sizeof S.dir, "%s", path);
		return 0;
	}
	S.dir[0] = 0;
	return -1;
}

static int
open_blob(void)
{
	char path[640];
	if (!S.dir[0] && ensure_dir())
		return -1;
	snprintf(path, sizeof path, "%s/blob", S.dir);
	S.blob = fopen(path, "r+b");
	if (!S.blob)
		S.blob = fopen(path, "w+b");
	return S.blob ? 0 : -1;
}

static void
note_have(int piece, uint64_t off, int nbytes)
{
	if (piece < 0 || piece >= S.npieces || S.have[piece])
		return;
	S.map[piece] = off;
	S.have[piece] = 1;
	S.have_n++;
	S.have_bytes += nbytes;
}

static int
store_piece(int piece, const uint8_t *data, int n)
{
	uint8_t dig[20];
	if (n != piece_size(piece) || !S.hashes)
		return -1;
	bt_sha1(data, n, dig);
	if (memcmp(dig, S.hashes + piece * 20, 20) != 0)
		return -1;
	if (!S.blob && open_blob())
		return -1;
	if (fseeko(S.blob, (off_t)S.blob_end, SEEK_SET) != 0)
		return -1;
	if ((int)fwrite(data, 1, (size_t)n, S.blob) != n)
		return -1;
	fflush(S.blob);
	note_have(piece, (uint64_t)S.blob_end, n);
	S.blob_end += n;
	if (S.build_i == piece) {
		free(S.build);
		free(S.build_got);
		S.build = NULL;
		S.build_got = NULL;
		S.build_i = -1;
	}
	return 0;
}

static int
load_piece(int piece, uint8_t *dst, int n)
{
	uint64_t off;
	if (!S.have || !S.have[piece] || !S.blob)
		return -1;
	off = S.map[piece];
	if (fseeko(S.blob, (off_t)off, SEEK_SET) != 0)
		return -1;
	return fread(dst, 1, (size_t)n, S.blob) == (size_t)n ? 0 : -1;
}

static int
range_have(int64_t abs, int n)
{
	int64_t end;
	if (!S.have || S.piece_len <= 0 || n < 0)
		return 0;
	if (abs < 0 || abs + n > S.total)
		return 0;
	end = abs + n;
	while (abs < end) {
		int p = (int)(abs / S.piece_len);
		int64_t pend = (int64_t)(p + 1) * S.piece_len;
		if (p < 0 || p >= S.npieces || !S.have[p])
			return 0;
		abs = pend;
	}
	return 1;
}

static void
set_prio_span(int64_t abs, int64_t len)
{
	int p;
	if (S.piece_len <= 0 || abs < 0)
		return;
	if (abs >= S.total)
		return;
	p = (int)(abs / S.piece_len);
	if (!S.have || !S.have[p])
		S.prio = p;
	(void)len;
}

static int
pick_piece(const peer_t *peer)
{
	int i;
	if (S.npieces <= 0)
		return -1;
	if (S.prio >= 0 && S.prio < S.npieces && !S.have[S.prio]) {
		int p = S.prio;
		if (!peer->bits || (peer->bits[p >> 3] & (1u << (7 - (p & 7)))))
			return p;
	}
	for (i = 0; i < S.npieces; i++) {
		if (S.have[i])
			continue;
		if (peer->bits && !(peer->bits[i >> 3] & (1u << (7 - (i & 7)))))
			continue;
		return i;
	}
	return -1;
}

static void
peer_close(peer_t *p)
{
	if (p->fd >= 0)
		close(p->fd);
	free(p->wbuf);
	free(p->bits);
	memset(p, 0, sizeof *p);
	p->fd = -1;
}

static void
peer_queue(peer_t *p, const void *data, int n)
{
	if (!p->wbuf)
		p->wbuf = (uint8_t *)malloc(64 * 1024);
	if (!p->wbuf || p->wn + n > 64 * 1024) {
		p->state = P_EMPTY;
		return;
	}
	if (p->wo > 0 && p->wo < p->wn) {
		memmove(p->wbuf, p->wbuf + p->wo, (size_t)(p->wn - p->wo));
		p->wn -= p->wo;
		p->wo = 0;
	} else if (p->wo >= p->wn) {
		p->wn = p->wo = 0;
	}
	memcpy(p->wbuf + p->wn, data, (size_t)n);
	p->wn += n;
}

static void
peer_flush(peer_t *p)
{
	if (p->fd < 0 || !p->wbuf)
		return;
	while (p->wo < p->wn) {
		int r = (int)send(p->fd, p->wbuf + p->wo, (size_t)(p->wn - p->wo), MSG_NOSIGNAL);
		if (r < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return;
			peer_close(p);
			return;
		}
		if (r == 0) {
			peer_close(p);
			return;
		}
		p->wo += r;
	}
	p->wo = p->wn = 0;
}

static void
send_msg(peer_t *p, const uint8_t *body, int n)
{
	uint8_t lenb[4];
	put32(lenb, (uint32_t)n);
	peer_queue(p, lenb, 4);
	if (n)
		peer_queue(p, body, n);
}

static void
send_handshake(peer_t *p)
{
	uint8_t hs[68];
	memset(hs, 0, sizeof hs);
	hs[0] = 19;
	memcpy(hs + 1, "BitTorrent protocol", 19);
	hs[25] = 0x10;
	memcpy(hs + 28, S.infohash, 20);
	memcpy(hs + 48, S.peer_id, 20);
	peer_queue(p, hs, 68);
}

static void
send_ext_hs(peer_t *p)
{
	char msg[160];
	int n;
	uint8_t body[180];
	if (S.meta && S.meta_got) {
		n = snprintf(msg, sizeof msg, "d1:md11:ut_metadatai1ee13:metadata_sizei%de", S.meta_n);
	} else {
		n = snprintf(msg, sizeof msg, "d1:md11:ut_metadatai1ee");
	}
	if (n < 0 || n > 150)
		return;
	body[0] = 20;
	body[1] = 0;
	memcpy(body + 2, msg, (size_t)n);
	send_msg(p, body, n + 2);
}

static void
send_interested(peer_t *p)
{
	uint8_t b[2];
	b[0] = 2;
	b[1] = 1;
	send_msg(p, b, 1);
	send_msg(p, b + 1, 1);
}

static void
request_blocks(peer_t *p)
{
	int piece, psz, blocks, i, sent = 0;
	if (p->state != P_LIVE || p->choked || S.npieces <= 0)
		return;
	if (p->reqs >= 6)
		return;
	piece = S.build_i >= 0 ? S.build_i : pick_piece(p);
	if (piece < 0)
		return;
	if (S.have && S.have[piece])
		return;
	psz = piece_size(piece);
	blocks = (psz + BT_BLOCK - 1) / BT_BLOCK;
	if (S.build_i != piece) {
		free(S.build);
		free(S.build_got);
		S.build = (uint8_t *)malloc((size_t)psz);
		S.build_got = (uint8_t *)calloc((size_t)blocks, 1);
		if (!S.build || !S.build_got) {
			free(S.build);
			free(S.build_got);
			S.build = NULL;
			S.build_got = NULL;
			S.build_i = -1;
			return;
		}
		S.build_i = piece;
		S.build_blocks = blocks;
	}
	for (i = 0; i < blocks && p->reqs < 6 && sent < 4; i++) {
		uint8_t body[13];
		int begin, len;
		if (S.build_got[i])
			continue;
		begin = i * BT_BLOCK;
		len = psz - begin;
		if (len > BT_BLOCK)
			len = BT_BLOCK;
		body[0] = 6;
		put32(body + 1, (uint32_t)piece);
		put32(body + 5, (uint32_t)begin);
		put32(body + 9, (uint32_t)len);
		send_msg(p, body, 13);
		p->reqs++;
		sent++;
	}
}

static void
request_meta(peer_t *p)
{
	int i;
	char dict[80];
	uint8_t body[96];
	int n;
	if (!p->ut_meta || S.meta_got || !S.meta)
		return;
	for (i = 0; i < S.meta_pieces; i++) {
		if (!S.meta_have[i])
			break;
	}
	if (i >= S.meta_pieces)
		return;
	n = snprintf(dict, sizeof dict, "d8:msg_typei0e5:piecei%dee", i);
	if (n < 0)
		return;
	body[0] = 20;
	body[1] = (uint8_t)p->ut_meta;
	memcpy(body + 2, dict, (size_t)n);
	send_msg(p, body, n + 2);
}

static int
on_meta_blob(const uint8_t *dict, int dn, const uint8_t *raw, int raw_n)
{
	ben_t b;
	int piece = -1;
	int type = -1;
	int total = 0;
	b.p = dict;
	b.n = dn;
	b.i = 0;
	if (b.i >= b.n || dict[0] != 'd')
		return -1;
	b.i = 1;
	while (b.i < dn && dict[b.i] != 'e') {
		const uint8_t *k;
		int kn;
		int64_t v;
		if (ben_str(&b, &k, &kn))
			return -1;
		if (key_is(k, kn, "msg_type")) {
			if (ben_int(&b, &v))
				return -1;
			type = (int)v;
		} else if (key_is(k, kn, "piece")) {
			if (ben_int(&b, &v))
				return -1;
			piece = (int)v;
		} else if (key_is(k, kn, "total_size")) {
			if (ben_int(&b, &v))
				return -1;
			total = (int)v;
		} else if (ben_skip(&b)) {
			return -1;
		}
	}
	if (type != 1 || piece < 0)
		return 0;
	if (!S.meta && total > 0 && total < BT_META_MAX) {
		S.meta_n = total;
		S.meta_pieces = (total + BT_BLOCK - 1) / BT_BLOCK;
		S.meta = (uint8_t *)calloc(1, (size_t)total);
		S.meta_have = (uint8_t *)calloc((size_t)S.meta_pieces, 1);
	}
	if (!S.meta || piece >= S.meta_pieces)
		return 0;
	{
		int off = piece * BT_BLOCK;
		int want = S.meta_n - off;
		if (want > BT_BLOCK)
			want = BT_BLOCK;
		if (raw_n < want || S.meta_have[piece])
			return 0;
		memcpy(S.meta + off, raw, (size_t)want);
		S.meta_have[piece] = 1;
	}
	{
		int i, all = 1;
		uint8_t dig[20];
		for (i = 0; i < S.meta_pieces; i++)
			if (!S.meta_have[i])
				all = 0;
		if (!all)
			return 0;
		bt_sha1(S.meta, S.meta_n, dig);
		if (memcmp(dig, S.infohash, 20) != 0) {
			free(S.meta);
			free(S.meta_have);
			S.meta = NULL;
			S.meta_have = NULL;
			S.meta_got = 0;
			return -1;
		}
		if (parse_info(S.meta, S.meta_n) != 0)
			return -1;
		S.meta_got = 1;
		open_blob();
	}
	return 0;
}

static void
on_extended(peer_t *p, const uint8_t *pay, int n)
{
	uint8_t id;
	if (n < 1)
		return;
	id = pay[0];
	if (id == 0) {
		ben_t b;
		b.p = pay + 1;
		b.n = n - 1;
		b.i = 0;
		if (b.n > 0 && b.p[0] == 'd') {
			b.i = 1;
			while (b.i < b.n && b.p[b.i] != 'e') {
				const uint8_t *k;
				int kn;
				if (ben_str(&b, &k, &kn))
					break;
				if (key_is(k, kn, "m")) {
					if (b.i < b.n && b.p[b.i] == 'd') {
						b.i++;
						while (b.i < b.n && b.p[b.i] != 'e') {
							const uint8_t *mk;
							int mkn;
							int64_t v;
							if (ben_str(&b, &mk, &mkn))
								break;
							if (key_is(mk, mkn, "ut_metadata")) {
								if (ben_int(&b, &v) == 0)
									p->ut_meta = (int)v;
							} else if (ben_skip(&b)) {
								break;
							}
						}
					} else if (ben_skip(&b)) {
						break;
					}
				} else if (key_is(k, kn, "metadata_size")) {
					int64_t v;
					if (ben_int(&b, &v) == 0 && v > 0 && v < BT_META_MAX && !S.meta) {
						S.meta_n = (int)v;
						S.meta_pieces = (int)((v + BT_BLOCK - 1) / BT_BLOCK);
						S.meta = (uint8_t *)calloc(1, (size_t)v);
						S.meta_have = (uint8_t *)calloc((size_t)S.meta_pieces, 1);
					}
				} else if (ben_skip(&b)) {
					break;
				}
			}
		}
		if (!S.meta_got)
			request_meta(p);
		return;
	}
	if (p->ut_meta && id == 1) {
		ben_t b;
		int start;
		b.p = pay + 1;
		b.n = n - 1;
		b.i = 0;
		start = 0;
		if (ben_skip(&b) == 0) {
			on_meta_blob(pay + 1, b.i, pay + 1 + b.i, n - 1 - b.i);
			(void)start;
		}
	}
}

static void
on_message(peer_t *p, const uint8_t *msg, int n)
{
	uint8_t id;
	p->last_ms = now_ms();
	if (n == 0)
		return;
	id = msg[0];
	if (id == 0) {
		p->choked = 1;
		p->reqs = 0;
	} else if (id == 1) {
		p->choked = 0;
		request_blocks(p);
	} else if (id == 4 && n >= 5) {
		uint32_t idx = be32(msg + 1);
		if (S.npieces > 0 && idx < (uint32_t)S.npieces) {
			if (!p->bits)
				p->bits = (uint8_t *)calloc((size_t)((S.npieces + 7) / 8), 1);
			if (p->bits)
				p->bits[idx >> 3] |= (uint8_t)(1u << (7 - (idx & 7)));
		}
	} else if (id == 5 && n >= 1) {
		int bn = n - 1;
		free(p->bits);
		p->bits = (uint8_t *)malloc((size_t)bn);
		if (p->bits) {
			memcpy(p->bits, msg + 1, (size_t)bn);
			p->bits_n = bn;
		}
	} else if (id == 6 && n >= 13 && S.meta_got) {
		uint32_t idx = be32(msg + 1);
		uint32_t begin = be32(msg + 5);
		uint32_t len = be32(msg + 9);
		int psz = piece_size((int)idx);
		if (idx < (uint32_t)S.npieces && S.have && S.have[idx] && len > 0 && len <= BT_BLOCK &&
		    psz > 0 && begin + len <= (uint32_t)psz) {
			uint8_t *buf = (uint8_t *)malloc(13u + (uint32_t)psz);
			if (buf && load_piece((int)idx, buf + 13, psz) == 0) {
				uint8_t head[13];
				head[0] = 7;
				put32(head + 1, idx);
				put32(head + 5, begin);
				memcpy(buf, head, 13);
				if (begin)
					memmove(buf + 13, buf + 13 + begin, len);
				send_msg(p, buf, (int)(13 + len));
			}
			free(buf);
		}
	} else if (id == 7 && n >= 9) {
		uint32_t idx = be32(msg + 1);
		uint32_t begin = be32(msg + 5);
		int dlen = n - 9;
		int psz, bi;
		if (S.build_i == (int)idx && S.build && dlen > 0 && (begin % BT_BLOCK) == 0) {
			psz = piece_size((int)idx);
			bi = (int)(begin / BT_BLOCK);
			if (bi >= 0 && bi < S.build_blocks && begin + (uint32_t)dlen <= (uint32_t)psz) {
				if (!S.build_got[bi]) {
					memcpy(S.build + begin, msg + 9, (size_t)dlen);
					S.build_got[bi] = 1;
					S.rx_bytes += dlen;
				}
				if (p->reqs > 0)
					p->reqs--;
				{
					int i, all = 1;
					for (i = 0; i < S.build_blocks; i++)
						if (!S.build_got[i])
							all = 0;
					if (all) {
						if (store_piece((int)idx, S.build, psz) != 0) {
							free(S.build);
							free(S.build_got);
							S.build = NULL;
							S.build_got = NULL;
							S.build_i = -1;
						}
					}
				}
			}
		} else if (p->reqs > 0) {
			p->reqs--;
		}
		request_blocks(p);
	} else if (id == 20) {
		on_extended(p, msg + 1, n - 1);
	}
}

static void
peer_parse(peer_t *p)
{
	int o = 0;
	if (p->state == P_HS) {
		if (p->rn < 68)
			return;
		if (p->rbuf[0] != 19 || memcmp(p->rbuf + 1, "BitTorrent protocol", 19) != 0 ||
		    memcmp(p->rbuf + 28, S.infohash, 20) != 0) {
			peer_close(p);
			return;
		}
		p->ext = (p->rbuf[25] & 0x10) != 0;
		p->state = P_LIVE;
		p->choked = 1;
		if (p->ext)
			send_ext_hs(p);
		send_interested(p);
		o = 68;
	}
	while (p->state == P_LIVE && o + 4 <= p->rn) {
		uint32_t len = be32(p->rbuf + o);
		if (len > 40 * 1024) {
			peer_close(p);
			return;
		}
		if (o + 4 + (int)len > p->rn)
			break;
		on_message(p, p->rbuf + o + 4, (int)len);
		o += 4 + (int)len;
		if (p->fd < 0)
			return;
	}
	if (o > 0 && o <= p->rn) {
		memmove(p->rbuf, p->rbuf + o, (size_t)(p->rn - o));
		p->rn -= o;
	}
}

static int
peer_known(const uint8_t ip[4], int port)
{
	int i;
	for (i = 0; i < S.nknown; i++)
		if (S.known_port[i] == port && memcmp(S.known_ip[i], ip, 4) == 0)
			return 1;
	for (i = 0; i < BT_PEERS; i++)
		if (S.peers[i].fd >= 0 && S.peers[i].port == port && memcmp(S.peers[i].ip, ip, 4) == 0)
			return 1;
	return 0;
}

static int
peer_connect(const uint8_t ip[4], int port)
{
	int i, fd, fl;
	struct sockaddr_in a;
	peer_t *p = NULL;
	if (port <= 0 || port > 65535)
		return -1;
	if (peer_known(ip, port))
		return 0;
	for (i = 0; i < BT_PEERS; i++) {
		if (S.peers[i].state == P_EMPTY) {
			p = &S.peers[i];
			break;
		}
	}
	if (!p)
		return -1;
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	fl = fcntl(fd, F_GETFL, 0);
	if (fl >= 0)
		fcntl(fd, F_SETFL, fl | O_NONBLOCK);
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons((uint16_t)port);
	memcpy(&a.sin_addr, ip, 4);
	if (connect(fd, (struct sockaddr *)&a, sizeof a) < 0 && errno != EINPROGRESS) {
		close(fd);
		return -1;
	}
	memset(p, 0, sizeof *p);
	p->fd = fd;
	p->state = P_CONN;
	p->port = port;
	memcpy(p->ip, ip, 4);
	p->last_ms = now_ms();
	if (S.nknown < BT_KNOWN) {
		memcpy(S.known_ip[S.nknown], ip, 4);
		S.known_port[S.nknown] = port;
		S.nknown++;
	}
	return 0;
}

int
bt_add_peer(const char *ip, int port)
{
	uint8_t b[4];
	if (!S.on || !ip)
		return -1;
	if (inet_pton(AF_INET, ip, b) != 1)
		return -1;
	return peer_connect(b, port);
}

static void
add_compact(const uint8_t *p, int n)
{
	int i;
	if (n < 6)
		return;
	for (i = 0; i + 6 <= n && S.nknown < BT_KNOWN; i += 6) {
		int port = ((int)p[i + 4] << 8) | p[i + 5];
		peer_connect(p + i, port);
	}
}

static void
pct_hash(const uint8_t *h, int n, char *out)
{
	static const char *hex = "0123456789ABCDEF";
	int i;
	for (i = 0; i < n; i++) {
		out[i * 3] = '%';
		out[i * 3 + 1] = hex[h[i] >> 4];
		out[i * 3 + 2] = hex[h[i] & 15];
	}
	out[n * 3] = 0;
}

static int64_t
left_bytes(void)
{
	if (!S.meta_got || S.total <= 0)
		return 1024 * 1024;
	if (S.have_bytes >= S.total)
		return 0;
	return S.total - S.have_bytes;
}

static int
tracker_http_start(track_t *t)
{
	char ih[80], pid[80];
	struct addrinfo hints, *res = NULL;
	char port[16];
	int fd, fl;
	pct_hash(S.infohash, 20, ih);
	pct_hash(S.peer_id, 20, pid);
	snprintf(t->req, sizeof t->req,
		 "GET %s%sinfo_hash=%s&peer_id=%s&port=6881&uploaded=0&downloaded=%lld&left=%lld&compact=1&event=%s&numwant=50 HTTP/1.0\r\n"
		 "Host: %s\r\nUser-Agent: Movian/7.0.273\r\nConnection: close\r\n\r\n",
		 t->path, strchr(t->path, '?') ? "&" : "?", ih, pid, (long long)S.have_bytes,
		 (long long)left_bytes(), S.announced ? "" : "started", t->host);
	/* event= with empty value is ugly when announced; rebuild if needed */
	if (S.announced) {
		snprintf(t->req, sizeof t->req,
			 "GET %s%sinfo_hash=%s&peer_id=%s&port=6881&uploaded=0&downloaded=%lld&left=%lld&compact=1&numwant=50 HTTP/1.0\r\n"
			 "Host: %s\r\nUser-Agent: Movian/7.0.273\r\nConnection: close\r\n\r\n",
			 t->path, strchr(t->path, '?') ? "&" : "?", ih, pid, (long long)S.have_bytes,
			 (long long)left_bytes(), t->host);
	}
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(port, sizeof port, "%d", t->port);
	if (getaddrinfo(t->host, port, &hints, &res) != 0 || !res)
		return -1;
	fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (fd < 0) {
		freeaddrinfo(res);
		return -1;
	}
	fl = fcntl(fd, F_GETFL, 0);
	if (fl >= 0)
		fcntl(fd, F_SETFL, fl | O_NONBLOCK);
	if (connect(fd, res->ai_addr, res->ai_addrlen) < 0 && errno != EINPROGRESS) {
		close(fd);
		freeaddrinfo(res);
		return -1;
	}
	freeaddrinfo(res);
	t->fd = fd;
	t->state = TR_ANN;
	t->rn = 0;
	t->tx = 1;
	return 0;
}

static int
tracker_udp_send(track_t *t, int action)
{
	uint8_t pkt[100];
	memset(pkt, 0, sizeof pkt);
	if (action == 0) {
		put64(pkt, 0x41727101980ULL);
		put32(pkt + 8, 0);
		t->tx = rnd32();
		put32(pkt + 12, t->tx);
		return (int)send(t->fd, pkt, 16, 0);
	}
	put64(pkt, t->conn);
	put32(pkt + 8, 1);
	t->tx = rnd32();
	put32(pkt + 12, t->tx);
	memcpy(pkt + 16, S.infohash, 20);
	memcpy(pkt + 36, S.peer_id, 20);
	put64(pkt + 56, (uint64_t)S.have_bytes);
	put64(pkt + 64, (uint64_t)left_bytes());
	put64(pkt + 72, 0);
	put32(pkt + 80, S.announced ? 0 : 2);
	put32(pkt + 84, 0);
	put32(pkt + 88, rnd32());
	put32(pkt + 92, (uint32_t)-1);
	pkt[96] = (6881 >> 8) & 0xff;
	pkt[97] = 6881 & 0xff;
	return (int)send(t->fd, pkt, 98, 0);
}

static int
tracker_udp_start(track_t *t)
{
	struct addrinfo hints, *res = NULL;
	char port[16];
	int fd, fl;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	snprintf(port, sizeof port, "%d", t->port);
	if (getaddrinfo(t->host, port, &hints, &res) != 0 || !res)
		return -1;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		freeaddrinfo(res);
		return -1;
	}
	fl = fcntl(fd, F_GETFL, 0);
	if (fl >= 0)
		fcntl(fd, F_SETFL, fl | O_NONBLOCK);
	if (connect(fd, res->ai_addr, res->ai_addrlen) < 0 && errno != EINPROGRESS) {
		close(fd);
		freeaddrinfo(res);
		return -1;
	}
	freeaddrinfo(res);
	t->fd = fd;
	t->state = TR_ANN;
	t->conn = 0;
	tracker_udp_send(t, 0);
	return 0;
}

static void
tracker_close(track_t *t)
{
	if (t->fd >= 0)
		close(t->fd);
	t->fd = -1;
	t->state = TR_WAIT;
	t->rn = 0;
}

static void
http_body_peers(track_t *t)
{
	const uint8_t *buf = t->rbuf;
	int n = t->rn;
	const uint8_t *body = NULL;
	int i;
	for (i = 0; i + 4 <= n; i++) {
		if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
			body = buf + i + 4;
			break;
		}
	}
	if (!body)
		return;
	{
		ben_t b;
		b.p = body;
		b.n = n - (int)(body - buf);
		b.i = 0;
		if (b.n <= 0 || body[0] != 'd')
			return;
		b.i = 1;
		while (b.i < b.n && body[b.i] != 'e') {
			const uint8_t *k;
			int kn;
			if (ben_str(&b, &k, &kn))
				break;
			if (key_is(k, kn, "peers")) {
				if (b.i < b.n && body[b.i] >= '0' && body[b.i] <= '9') {
					const uint8_t *s;
					int sn;
					if (ben_str(&b, &s, &sn) == 0)
						add_compact(s, sn);
				} else if (ben_skip(&b)) {
					break;
				}
			} else if (key_is(k, kn, "interval")) {
				int64_t v;
				if (ben_int(&b, &v) == 0 && v > 15 && v < 7200)
					t->next_ms = now_ms() + (double)v * 1000.0;
				else if (ben_skip(&b))
					break;
			} else if (ben_skip(&b)) {
				break;
			}
		}
	}
	S.announced = 1;
}

static void
trackers_pump(void)
{
	int i;
	double now = now_ms();
	for (i = 0; i < S.ntr; i++) {
		track_t *t = &S.tr[i];
		if (t->state == TR_WAIT && now >= t->next_ms) {
			int rc = t->kind == TR_UDP ? tracker_udp_start(t) : tracker_http_start(t);
			if (rc != 0) {
				t->next_ms = now + 20000;
				t->state = TR_WAIT;
			}
		} else if (t->state == TR_ANN && t->fd >= 0 && t->kind == TR_HTTP) {
			struct pollfd pf;
			pf.fd = t->fd;
			pf.events = POLLIN | POLLOUT;
			pf.revents = 0;
			if (poll(&pf, 1, 0) > 0) {
				if (pf.revents & POLLOUT) {
					int L = (int)strlen(t->req);
					if (L && send(t->fd, t->req, (size_t)L, MSG_NOSIGNAL) < 0 &&
					    errno != EAGAIN && errno != EWOULDBLOCK) {
						tracker_close(t);
						t->next_ms = now + 20000;
					} else {
						t->req[0] = 0;
					}
				}
				if (t->fd >= 0 && (pf.revents & POLLIN)) {
					int r = (int)recv(t->fd, t->rbuf + t->rn, sizeof t->rbuf - 1 - (size_t)t->rn, 0);
					if (r > 0) {
						t->rn += r;
						t->rbuf[t->rn] = 0;
					} else if (r == 0) {
						http_body_peers(t);
						tracker_close(t);
						if (t->next_ms < now + 15000)
							t->next_ms = now + 60000;
					}
				}
			}
		} else if (t->state == TR_ANN && t->fd >= 0 && t->kind == TR_UDP) {
			uint8_t buf[1500];
			int r = (int)recv(t->fd, buf, sizeof buf, 0);
			if (r >= 16) {
				uint32_t action = be32(buf);
				uint32_t tx = be32(buf + 4);
				if (tx == t->tx && action == 0 && r >= 16) {
					t->conn = be64(buf + 8);
					tracker_udp_send(t, 1);
				} else if (tx == t->tx && action == 1 && r >= 20) {
					add_compact(buf + 20, r - 20);
					S.announced = 1;
					tracker_close(t);
					t->next_ms = now + 60000;
				} else if (action == 3) {
					tracker_close(t);
					t->next_ms = now + 30000;
				}
			} else if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
				tracker_close(t);
				t->next_ms = now + 20000;
			}
		}
	}
}

static void
peers_pump(void)
{
	int i;
	double now = now_ms();
	for (i = 0; i < BT_PEERS; i++) {
		peer_t *p = &S.peers[i];
		struct pollfd pf;
		if (p->state == P_EMPTY)
			continue;
		if (now - p->last_ms > 25000 && p->state != P_CONN) {
			peer_close(p);
			continue;
		}
		pf.fd = p->fd;
		pf.events = POLLIN | (p->wn > p->wo || p->state == P_CONN ? POLLOUT : 0);
		pf.revents = 0;
		if (poll(&pf, 1, 0) <= 0)
			continue;
		if (p->state == P_CONN && (pf.revents & (POLLOUT | POLLERR))) {
			int err = 0;
			socklen_t el = sizeof err;
			getsockopt(p->fd, SOL_SOCKET, SO_ERROR, &err, &el);
			if (err) {
				peer_close(p);
				continue;
			}
			p->state = P_HS;
			p->last_ms = now;
			send_handshake(p);
		}
		peer_flush(p);
		if (p->fd >= 0 && (pf.revents & POLLIN)) {
			int cap = (int)sizeof p->rbuf - p->rn;
			int r;
			if (cap <= 0) {
				peer_close(p);
				continue;
			}
			r = (int)recv(p->fd, p->rbuf + p->rn, (size_t)cap, 0);
			if (r > 0) {
				p->rn += r;
				p->last_ms = now;
				peer_parse(p);
			} else if (r == 0 || (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
				peer_close(p);
			}
		}
		if (p->state == P_LIVE) {
			if (!S.meta_got)
				request_meta(p);
			else
				request_blocks(p);
			peer_flush(p);
		}
	}
}

static void
refresh_line(void)
{
	double now = now_ms();
	int peers = 0;
	int i;
	int pct = 0;
	for (i = 0; i < BT_PEERS; i++)
		if (S.peers[i].state != P_EMPTY)
			peers++;
	if (now - S.rx_stamp > 500) {
		double dt = (now - S.rx_stamp) / 1000.0;
		if (dt > 0)
			S.rate = (double)(S.rx_bytes - S.rx_mark) / dt;
		S.rx_mark = S.rx_bytes;
		S.rx_stamp = now;
	}
	if (S.total > 0)
		pct = (int)((S.have_bytes * 100) / S.total);
	if (pct > 100)
		pct = 100;
	if (!S.meta_got)
		snprintf(S.line, sizeof S.line, "BT metadata  %dp", peers);
	else if (S.rate > 800000)
		snprintf(S.line, sizeof S.line, "BT %d%%  %.1fMB/s  %dp", pct, S.rate / 1e6, peers);
	else
		snprintf(S.line, sizeof S.line, "BT %d%%  %.0fkB/s  %dp", pct, S.rate / 1000.0, peers);
}

static void
arm_ends(void)
{
	file_t *f;
	int64_t head, tail;
	if (!S.meta_got || S.play < 0)
		return;
	f = &S.files[S.play];
	head = f->len < 2 * 1024 * 1024 ? f->len : 2 * 1024 * 1024;
	if (head < S.piece_len && f->len > head)
		head = S.piece_len;
	if (head > f->len)
		head = f->len;
	set_prio_span(f->off, head);
	if (!video_name(f->name))
		return;
	/* mp4 keeps the index at the end. Pull that before the middle. */
	if (strstr(f->name, ".mp4") || strstr(f->name, ".m4v") || strstr(f->name, ".mov") ||
	    strstr(f->name, ".MP4")) {
		tail = f->len < 2 * 1024 * 1024 ? 0 : 2 * 1024 * 1024;
		if (tail > 0 && range_have(f->off, (int)(head > 0x7fffffff ? 0x7fffffff : head)))
			set_prio_span(f->off + f->len - tail, tail);
	}
}

void
bt_pump(int timeout_ms)
{
	double end;
	if (!S.on)
		return;
	end = now_ms() + (timeout_ms > 0 ? timeout_ms : 0);
	do {
		trackers_pump();
		peers_pump();
		arm_ends();
		refresh_line();
		if (timeout_ms <= 0)
			break;
		if (bt_playable())
			break;
		poll(NULL, 0, 5);
	} while (now_ms() < end);
}

int
bt_active(void)
{
	return S.on;
}

int
bt_same(const char *src)
{
	return S.on && src && strcmp(S.src, src) == 0;
}

const char *
bt_line(void)
{
	return S.line[0] ? S.line : "BT";
}

const char *
bt_name(void)
{
	return S.name[0] ? S.name : "BITTORRENT";
}

int
bt_playable(void)
{
	file_t *f;
	int64_t head, tail = 0;
	if (!S.on || !S.meta_got || S.play < 0)
		return 0;
	f = &S.files[S.play];
	head = f->len < 2 * 1024 * 1024 ? f->len : 2 * 1024 * 1024;
	if ((strstr(f->name, ".mp4") || strstr(f->name, ".m4v") || strstr(f->name, ".mov")) &&
	    f->len > 3 * 1024 * 1024)
		tail = 2 * 1024 * 1024;
	if (head > 0x7fffffff)
		head = 0x7fffffff;
	if (!range_have(f->off, (int)head))
		return 0;
	if (tail && !range_have(f->off + f->len - tail, (int)tail))
		return 0;
	return 1;
}

int
bt_read(uint8_t *buf, int n)
{
	file_t *f;
	int got = 0;
	if (!S.on || !S.meta_got || S.play < 0 || n <= 0)
		return -1;
	f = &S.files[S.play];
	if (S.pos >= f->len)
		return S.have_bytes >= S.total ? 0 : -1;
	if (S.pos + n > f->len)
		n = (int)(f->len - S.pos);
	while (got < n) {
		int64_t abs = f->off + S.pos;
		int p = (int)(abs / S.piece_len);
		int boff = (int)(abs % S.piece_len);
		int psz = piece_size(p);
		int chunk = psz - boff;
		uint8_t *tmp;
		if (chunk > n - got)
			chunk = n - got;
		if (p < 0 || p >= S.npieces || !S.have || !S.have[p]) {
			S.prio = p;
			return got > 0 ? got : -1;
		}
		tmp = (uint8_t *)malloc((size_t)psz);
		if (!tmp || load_piece(p, tmp, psz) != 0) {
			free(tmp);
			S.prio = p;
			return got > 0 ? got : -1;
		}
		memcpy(buf + got, tmp + boff, (size_t)chunk);
		free(tmp);
		got += chunk;
		S.pos += chunk;
	}
	return got;
}

int64_t
bt_seek(int64_t off, int whence)
{
	file_t *f;
	if (!S.on || !S.meta_got || S.play < 0)
		return -1;
	f = &S.files[S.play];
	if (whence == 0x10000)
		return f->len;
	if (whence == 1)
		off = S.pos + off;
	else if (whence == 2)
		off = f->len + off;
	if (off < 0)
		off = 0;
	if (off > f->len)
		off = f->len;
	S.pos = off;
	set_prio_span(f->off + off, S.piece_len);
	return S.pos;
}

static void
reset(void)
{
	int i;
	for (i = 0; i < BT_PEERS; i++)
		if (S.peers[i].fd >= 0 || S.peers[i].state != P_EMPTY)
			peer_close(&S.peers[i]);
	for (i = 0; i < S.ntr; i++)
		tracker_close(&S.tr[i]);
	if (S.blob)
		fclose(S.blob);
	free(S.meta);
	free(S.meta_have);
	free(S.hashes);
	free(S.have);
	free(S.map);
	free(S.build);
	free(S.build_got);
	memset(&S, 0, sizeof S);
	for (i = 0; i < BT_PEERS; i++)
		S.peers[i].fd = -1;
	for (i = 0; i < BT_TRACK; i++)
		S.tr[i].fd = -1;
	S.build_i = -1;
	S.play = -1;
}

void
bt_stop(void)
{
	if (S.on)
		reset();
}

static int
read_all(const char *path, uint8_t **out, int *n)
{
	FILE *f = fopen(path, "rb");
	uint8_t *b;
	long sz;
	if (!f)
		return -1;
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return -1;
	}
	sz = ftell(f);
	if (sz <= 0 || sz > BT_META_MAX) {
		fclose(f);
		return -1;
	}
	rewind(f);
	b = (uint8_t *)malloc((size_t)sz);
	if (!b || fread(b, 1, (size_t)sz, f) != (size_t)sz) {
		free(b);
		fclose(f);
		return -1;
	}
	fclose(f);
	*out = b;
	*n = (int)sz;
	return 0;
}

int
bt_start(const char *src)
{
	uint8_t *buf = NULL;
	int n = 0;
	int i;
	if (!src || !src[0])
		return -1;
	if (S.on && strcmp(S.src, src) == 0)
		return 0;
	bt_stop();
	memset(&S, 0, sizeof S);
	for (i = 0; i < BT_PEERS; i++)
		S.peers[i].fd = -1;
	for (i = 0; i < BT_TRACK; i++)
		S.tr[i].fd = -1;
	S.build_i = -1;
	S.play = -1;
	snprintf(S.src, sizeof S.src, "%s", src);
	memcpy(S.peer_id, "-SP1040-", 8);
	for (i = 8; i < 20; i++)
		S.peer_id[i] = (uint8_t)('a' + (rnd32() % 26));
	S.rx_stamp = now_ms();
	if (strncmp(src, "magnet:?", 8) == 0) {
		if (parse_magnet(src) != 0) {
			reset();
			return -1;
		}
		snprintf(S.name, sizeof S.name, "%s", S.name[0] ? S.name : "magnet");
	} else {
		char text[1200];
		FILE *f = fopen(src, "rb");
		if (!f) {
			reset();
			return -1;
		}
		if (fread(text, 1, 8, f) == 8 && memcmp(text, "magnet:?", 8) == 0) {
			int got;
			memcpy(text, "magnet:?", 8);
			got = (int)fread(text + 8, 1, sizeof text - 9, f);
			text[8 + (got > 0 ? got : 0)] = 0;
			fclose(f);
			{
				char *nl = strpbrk(text, "\r\n");
				if (nl)
					*nl = 0;
			}
			if (parse_magnet(text) != 0) {
				reset();
				return -1;
			}
		} else {
			fclose(f);
			if (read_all(src, &buf, &n) != 0 || parse_torrent(buf, n) != 0) {
				free(buf);
				reset();
				return -1;
			}
			free(buf);
			S.meta_got = 1;
			open_blob();
		}
	}
	S.on = 1;
	snprintf(S.line, sizeof S.line, "BT starting");
	if (S.ntr == 0 && S.nknown == 0)
		snprintf(S.line, sizeof S.line, "BT no tracker");
	return 0;
}
