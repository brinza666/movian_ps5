/* Local swarm: one seeder, the payload client, a two-piece clip.mkv. */
#include "../src/bt.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void bt_sha1(const uint8_t *msg, int n, uint8_t out[20]);

static uint32_t
be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void
put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static int g_lfd = -1;
static int g_cfd = -1;
static int g_hs;
static const uint8_t *g_file;
static int g_flen;
static uint8_t g_info[20];
static uint8_t g_rbuf[64 * 1024];
static int g_rn;

static void
seed_send(const void *p, int n)
{
	const uint8_t *b = p;
	int o = 0;
	while (o < n) {
		int r = (int)send(g_cfd, b + o, (size_t)(n - o), MSG_NOSIGNAL);
		if (r < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			return;
		}
		o += r;
	}
}

static void
seed_pump(void)
{
	struct sockaddr_in a;
	socklen_t al = sizeof a;
	int r;
	if (g_cfd < 0 && g_lfd >= 0) {
		int fd = accept(g_lfd, (struct sockaddr *)&a, &al);
		if (fd >= 0) {
			fcntl(fd, F_SETFL, O_NONBLOCK);
			g_cfd = fd;
		}
	}
	if (g_cfd < 0)
		return;
	r = (int)recv(g_cfd, g_rbuf + g_rn, sizeof g_rbuf - (size_t)g_rn, 0);
	if (r > 0)
		g_rn += r;
	if (!g_hs && g_rn >= 68) {
		uint8_t hs[68];
		uint8_t msg[6];
		if (memcmp(g_rbuf + 28, g_info, 20) != 0) {
			fprintf(stderr, "handshake infohash mismatch\n");
			exit(1);
		}
		memset(hs, 0, sizeof hs);
		hs[0] = 19;
		memcpy(hs + 1, "BitTorrent protocol", 19);
		memcpy(hs + 28, g_info, 20);
		memcpy(hs + 48, "-SEEDTEST-ABCDEFGH", 20);
		seed_send(hs, 68);
		put32(msg, 2);
		msg[4] = 5;
		msg[5] = 0xc0;
		seed_send(msg, 6);
		put32(msg, 1);
		msg[4] = 1;
		seed_send(msg, 5);
		g_hs = 1;
		memmove(g_rbuf, g_rbuf + 68, (size_t)(g_rn - 68));
		g_rn -= 68;
	}
	while (g_hs && g_rn >= 4) {
		uint32_t len = be32(g_rbuf);
		if (len > 40000 || g_rn < (int)(4 + len))
			return;
		if (len >= 13 && g_rbuf[4] == 6) {
			uint32_t idx = be32(g_rbuf + 5);
			uint32_t begin = be32(g_rbuf + 9);
			uint32_t bl = be32(g_rbuf + 13);
			uint32_t off = idx * 16384u + begin;
			uint8_t head[4];
			uint8_t body[9];
			if (bl <= 16384 && off + bl <= (uint32_t)g_flen) {
				put32(head, 9 + bl);
				body[0] = 7;
				put32(body + 1, idx);
				put32(body + 5, begin);
				seed_send(head, 4);
				seed_send(body, 9);
				seed_send(g_file + off, (int)bl);
			}
		}
		memmove(g_rbuf, g_rbuf + 4 + len, (size_t)(g_rn - (int)(4 + len)));
		g_rn -= 4 + (int)len;
	}
}

int
main(void)
{
	uint8_t file[20000];
	uint8_t dig0[20], dig1[20];
	uint8_t abc_dig[20];
	static const uint8_t abc_ok[20] = {0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a, 0xba, 0x3e,
					   0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c, 0x9c, 0xd0, 0xd8, 0x9d};
	char torrent[512];
	int i, body;
	struct sockaddr_in a;
	socklen_t al;
	int port;
	const char *info;
	int info_n;

	bt_sha1((const uint8_t *)"abc", 3, abc_dig);
	if (memcmp(abc_dig, abc_ok, 20) != 0) {
		fprintf(stderr, "sha1(abc) mismatch\n");
		return 1;
	}
	for (i = 0; i < (int)sizeof file; i++)
		file[i] = (uint8_t)(i * 17 + 3);
	bt_sha1(file, 16384, dig0);
	bt_sha1(file + 16384, (int)sizeof file - 16384, dig1);
	body = snprintf(torrent, sizeof torrent,
			"d8:announce27:http://127.0.0.1:9/announce4:infod6:lengthi%de4:name8:clip.mkv12:piece lengthi16384e6:pieces40:",
			(int)sizeof file);
	memcpy(torrent + body, dig0, 20);
	memcpy(torrent + body + 20, dig1, 20);
	memcpy(torrent + body + 40, "ee", 2);
	info = strstr(torrent, "4:infod");
	if (!info)
		return 1;
	info += 6;
	info_n = (body + 42) - (int)(info - torrent) - 1;
	bt_sha1((const uint8_t *)info, info_n, g_info);
	{
		FILE *f = fopen("/tmp/splicer-selftest.torrent", "wb");
		if (!f)
			return 1;
		fwrite(torrent, 1, (size_t)(body + 42), f);
		fclose(f);
	}
	g_file = file;
	g_flen = (int)sizeof file;
	g_lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (g_lfd < 0)
		return 1;
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(0x7f000001);
	a.sin_port = 0;
	if (bind(g_lfd, (struct sockaddr *)&a, sizeof a) || listen(g_lfd, 1))
		return 1;
	fcntl(g_lfd, F_SETFL, O_NONBLOCK);
	al = sizeof a;
	getsockname(g_lfd, (struct sockaddr *)&a, &al);
	port = ntohs(a.sin_port);
	if (bt_start("/tmp/splicer-selftest.torrent") != 0) {
		fprintf(stderr, "bt_start failed\n");
		return 1;
	}
	bt_add_peer("127.0.0.1", port);
	for (i = 0; i < 500; i++) {
		seed_pump();
		bt_pump(5);
		if (!bt_playable())
			continue;
		if (bt_seek(0, 0) != 0)
			continue;
		{
			uint8_t got[sizeof file];
			int o = 0;
			while (o < (int)sizeof got) {
				int r = bt_read(got + o, (int)sizeof got - o);
				if (r <= 0)
					break;
				o += r;
			}
			if (o == (int)sizeof file && memcmp(got, file, sizeof file) == 0) {
				printf("bt selftest ok  %s\n", bt_line());
				bt_stop();
				return 0;
			}
			fprintf(stderr, "read mismatch o=%d\n", o);
			bt_stop();
			return 1;
		}
	}
	fprintf(stderr, "bt selftest timeout  %s playable=%d\n", bt_line(), bt_playable());
	bt_stop();
	return 1;
}
