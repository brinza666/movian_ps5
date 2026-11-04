#include "ftpio.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define AVDEC_SEEK_SIZE 0x10000

static void
arm(int fd)
{
	struct timeval tv;
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
}

static int
connect_in(int fd, struct sockaddr_in *a, int seconds)
{
	int flags = fcntl(fd, F_GETFL, 0);
	int rc;
	if (flags >= 0)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	rc = connect(fd, (struct sockaddr *)a, sizeof *a);
	if (rc != 0 && errno == EINPROGRESS) {
		fd_set wset;
		struct timeval tv;
		int soerr = 0;
		socklen_t sl = sizeof soerr;
		FD_ZERO(&wset);
		FD_SET(fd, &wset);
		tv.tv_sec = seconds;
		tv.tv_usec = 0;
		if (select(fd + 1, NULL, &wset, NULL, &tv) <= 0) {
			if (flags >= 0)
				fcntl(fd, F_SETFL, flags);
			return -1;
		}
		getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl);
		if (soerr != 0) {
			if (flags >= 0)
				fcntl(fd, F_SETFL, flags);
			return -1;
		}
		rc = 0;
	}
	if (flags >= 0)
		fcntl(fd, F_SETFL, flags);
	return rc;
}

static int
dial(const char *host, int port)
{
	int fd;
	struct sockaddr_in a;
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons((uint16_t)port);
	if (inet_pton(AF_INET, host, &a.sin_addr) != 1) {
		close(fd);
		return -1;
	}
	if (connect_in(fd, &a, 1) != 0) {
		close(fd);
		return -1;
	}
	arm(fd);
	return fd;
}

static int
read_line(int fd, char *buf, int n)
{
	int i = 0;
	while (i < n - 1) {
		char c;
		int r = (int)recv(fd, &c, 1, 0);
		if (r <= 0)
			return -1;
		if (c == '\n')
			break;
		if (c != '\r')
			buf[i++] = c;
	}
	buf[i] = 0;
	return i;
}

static int
read_reply(int fd, char *buf, int n)
{
	char line[512];
	int code = -1;
	for (;;) {
		if (read_line(fd, line, (int)sizeof line) < 0)
			return -1;
		if (buf && n > 0)
			snprintf(buf, (size_t)n, "%s", line);
		if (line[0] >= '0' && line[0] <= '9' && line[3] == ' ')
			return atoi(line);
		if (line[0] >= '0' && line[0] <= '9' && line[3] == '-') {
			code = atoi(line);
			continue;
		}
		if (code > 0 && line[0] < '0')
			continue;
	}
}

static int
send_cmd(int fd, const char *text, char *reply, int replyn)
{
	char msg[640];
	int n = snprintf(msg, sizeof msg, "%s\r\n", text);
	if (n <= 0 || (int)send(fd, msg, (size_t)n, 0) != n)
		return -1;
	return read_reply(fd, reply, replyn);
}

static int
login(int fd)
{
	if (read_reply(fd, NULL, 0) / 100 != 2)
		return -1;
	if (send_cmd(fd, "USER anonymous", NULL, 0) / 100 == 5)
		return -1;
	if (send_cmd(fd, "PASS movian", NULL, 0) / 100 != 2)
		return -1;
	send_cmd(fd, "TYPE I", NULL, 0);
	return 0;
}

static int
pasv_connect(int ctrl)
{
	char reply[256];
	char *p;
	int nums[6];
	int i, code, data, port;
	struct sockaddr_in a;
	code = send_cmd(ctrl, "PASV", reply, (int)sizeof reply);
	if (code != 227)
		return -1;
	p = strchr(reply, '(');
	if (!p)
		return -1;
	for (i = 0; i < 6; i++) {
		p++;
		nums[i] = atoi(p);
		p = strchr(p, i == 5 ? ')' : ',');
		if (!p)
			return -1;
	}
	port = nums[4] * 256 + nums[5];
	data = socket(AF_INET, SOCK_STREAM, 0);
	if (data < 0)
		return -1;
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons((uint16_t)port);
	inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
	if (connect_in(data, &a, 2) != 0) {
		char ip[32];
		snprintf(ip, sizeof ip, "%d.%d.%d.%d", nums[0], nums[1], nums[2], nums[3]);
		inet_pton(AF_INET, ip, &a.sin_addr);
		if (connect_in(data, &a, 2) != 0) {
			close(data);
			return -1;
		}
	}
	arm(data);
	return data;
}

int
ftp_probe(int *port)
{
	static const int ports[] = {2121, 1337, 2122, 21};
	int i;
	for (i = 0; i < 4; i++) {
		int fd = dial("127.0.0.1", ports[i]);
		if (fd < 0)
			continue;
		if (login(fd) == 0) {
			send_cmd(fd, "QUIT", NULL, 0);
			close(fd);
			if (port)
				*port = ports[i];
			return 0;
		}
		close(fd);
	}
	if (port)
		*port = 0;
	return -1;
}

static int
entry_name(char *line, int *is_dir, char **name)
{
	char *p = line;
	int tok = 0;
	if (!line[0] || (line[0] == 't' && strncmp(line, "type=", 5) != 0))
		return -1;
	if (strncmp(line, "type=", 5) == 0) {
		char *sp = strstr(line, "; ");
		if (!sp)
			return -1;
		*is_dir = strncmp(line, "type=dir", 8) == 0;
		p = sp + 2;
		if (!*p || strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
			return -1;
		*name = p;
		return 0;
	}
	if (line[0] >= '0' && line[0] <= '9' && strchr(line, ':')) {
		char *dir = strstr(line, "<DIR>");
		if (dir) {
			*is_dir = 1;
			p = dir + 5;
		} else {
			*is_dir = 0;
			while (*p && tok < 3) {
				while (*p == ' ')
					p++;
				if (!*p)
					return -1;
				while (*p && *p != ' ')
					p++;
				tok++;
			}
		}
		while (*p == ' ')
			p++;
		if (!*p || strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
			return -1;
		*name = p;
		return 0;
	}
	if (line[0] != 'd' && line[0] != '-' && line[0] != 'l')
		return -1;
	*is_dir = line[0] == 'd';
	while (*p && tok < 8) {
		while (*p == ' ')
			p++;
		if (!*p)
			return -1;
		while (*p && *p != ' ')
			p++;
		tok++;
	}
	while (*p == ' ')
		p++;
	if (!*p || strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
		return -1;
	*name = p;
	return 0;
}

static int
list_dir(int ctrl, const char *path, char **text, size_t *len)
{
	int data;
	char cmd[768];
	char *buf;
	size_t cap = 64 * 1024;
	size_t n = 0;
	int code;
	data = pasv_connect(ctrl);
	if (data < 0)
		return -1;
	snprintf(cmd, sizeof cmd, "LIST %s", path);
	code = send_cmd(ctrl, cmd, NULL, 0);
	if (code / 100 != 1) {
		close(data);
		return -1;
	}
	buf = (char *)malloc(cap);
	if (!buf) {
		close(data);
		return -1;
	}
	for (;;) {
		int r;
		if (n + 1024 > cap) {
			char *grown = (char *)realloc(buf, cap * 2);
			if (!grown)
				break;
			buf = grown;
			cap *= 2;
		}
		r = (int)recv(data, buf + n, 1024, 0);
		if (r <= 0)
			break;
		n += (size_t)r;
		if (n > 1024 * 1024)
			break;
	}
	close(data);
	read_reply(ctrl, NULL, 0);
	buf[n < cap ? n : cap - 1] = 0;
	*text = buf;
	*len = n;
	return 0;
}

int
ftp_walk(int port, const char *path, int depth, ftp_visit_fn fn, void *ud, int *err)
{
	int ctrl;
	char *text = NULL;
	size_t len = 0;
	char *line;
	if (err)
		*err = 0;
	ctrl = dial("127.0.0.1", port);
	if (ctrl < 0 || login(ctrl) != 0) {
		if (ctrl >= 0)
			close(ctrl);
		if (err)
			*err = errno ? errno : ECONNREFUSED;
		return -1;
	}
	if (list_dir(ctrl, path, &text, &len) != 0) {
		if (err)
			*err = ENOENT;
		send_cmd(ctrl, "QUIT", NULL, 0);
		close(ctrl);
		return -1;
	}
	line = text;
	while (line && *line) {
		char *nl = strchr(line, '\n');
		char *name = NULL;
		int is_dir = 0;
		char full[768];
		char url[800];
		if (nl)
			*nl = 0;
		if (entry_name(line, &is_dir, &name) == 0) {
			snprintf(full, sizeof full, "%s/%s", path, name);
			snprintf(url, sizeof url, "ftp://127.0.0.1:%d%s", port, full);
			if (fn && fn(url, name, is_dir, ud))
				break;
			if (is_dir && depth > 0)
				ftp_walk(port, full, depth - 1, fn, ud, NULL);
		}
		if (!nl)
			break;
		line = nl + 1;
	}
	free(text);
	send_cmd(ctrl, "QUIT", NULL, 0);
	close(ctrl);
	return 0;
}

struct ftp_file {
	int ctrl;
	int data;
	int64_t pos;
	int64_t size;
	char path[700];
};

static int
retr_at(ftp_file *f)
{
	char cmd[760];
	int code;
	if (f->data >= 0) {
		close(f->data);
		f->data = -1;
		read_reply(f->ctrl, NULL, 0);
	}
	f->data = pasv_connect(f->ctrl);
	if (f->data < 0)
		return -1;
	if (f->pos > 0) {
		snprintf(cmd, sizeof cmd, "REST %lld", (long long)f->pos);
		code = send_cmd(f->ctrl, cmd, NULL, 0);
		if (code / 100 != 3) {
			close(f->data);
			f->data = -1;
			return -1;
		}
	}
	snprintf(cmd, sizeof cmd, "RETR %s", f->path);
	code = send_cmd(f->ctrl, cmd, NULL, 0);
	if (code / 100 != 1) {
		close(f->data);
		f->data = -1;
		return -1;
	}
	return 0;
}

ftp_file *
ftp_file_open(const char *url)
{
	ftp_file *f;
	const char *p;
	int port = 21;
	char cmd[760];
	char reply[128];
	int code;
	if (!url || strncmp(url, "ftp://", 6) != 0)
		return NULL;
	p = url + 6;
	p = strchr(p, '/');
	if (!p)
		return NULL;
	f = (ftp_file *)calloc(1, sizeof *f);
	if (!f)
		return NULL;
	f->ctrl = -1;
	f->data = -1;
	f->size = -1;
	if (sscanf(url, "ftp://127.0.0.1:%d/", &port) != 1)
		port = 2121;
	snprintf(f->path, sizeof f->path, "%s", p);
	f->ctrl = dial("127.0.0.1", port);
	if (f->ctrl < 0 || login(f->ctrl) != 0) {
		ftp_file_close(f);
		return NULL;
	}
	snprintf(cmd, sizeof cmd, "SIZE %s", f->path);
	code = send_cmd(f->ctrl, cmd, reply, (int)sizeof reply);
	if (code == 213)
		f->size = atoll(reply + 4);
	return f;
}

void
ftp_file_close(ftp_file *f)
{
	if (!f)
		return;
	if (f->data >= 0)
		close(f->data);
	if (f->ctrl >= 0) {
		send_cmd(f->ctrl, "QUIT", NULL, 0);
		close(f->ctrl);
	}
	free(f);
}

int
ftp_file_read(void *user, uint8_t *buf, int n)
{
	ftp_file *f = user;
	int r;
	if (!f || n <= 0)
		return 0;
	if (f->data < 0 && retr_at(f) != 0)
		return -1;
	r = (int)recv(f->data, buf, (size_t)n, 0);
	if (r < 0)
		return -1;
	if (r == 0) {
		close(f->data);
		f->data = -1;
		read_reply(f->ctrl, NULL, 0);
		return 0;
	}
	f->pos += r;
	return r;
}

int64_t
ftp_file_seek(void *user, int64_t off, int whence)
{
	ftp_file *f = user;
	int64_t next;
	if (!f)
		return -1;
	if (whence == AVDEC_SEEK_SIZE)
		return f->size;
	if (whence == 1)
		next = f->pos + off;
	else if (whence == 2)
		next = (f->size >= 0 ? f->size : 0) + off;
	else
		next = off;
	if (next < 0)
		return -1;
	if (next != f->pos) {
		if (f->data >= 0) {
			close(f->data);
			f->data = -1;
			/* The server still owes a 226. Drain it so the next
			 * command is not reading the previous reply. */
			read_reply(f->ctrl, NULL, 0);
		}
		f->pos = next;
	}
	return f->pos;
}
