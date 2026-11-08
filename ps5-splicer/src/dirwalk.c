#include "dirwalk.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* The title libc opendir() uses a buffer smaller than a directory block.
 * getdents then returns EINVAL and the directory looks empty. Same fix
 * PS5MC uses: a buffer of at least one 64 KiB block, grown if asked.
 */

#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#endif
#ifndef DT_DIR
#define DT_DIR 4
#endif
#ifndef DT_REG
#define DT_REG 8
#endif
#ifndef DT_LNK
#define DT_LNK 10
#endif

int
dir_walk(const char *path,
	 int (*fn)(const char *full, const char *name, int is_dir, void *ud),
	 void *ud, int *err)
{
	int fd;
	int cap;
	int n;
	char *buf;
	struct stat st;

	if (err)
		*err = 0;
	if (!path || !path[0] || !fn) {
		if (err)
			*err = EINVAL;
		return -1;
	}
	fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) {
		if (err)
			*err = errno;
		return -1;
	}
	cap = 64 * 1024;
	if (fstat(fd, &st) == 0 && st.st_blksize > cap &&
	    st.st_blksize <= 1024 * 1024)
		cap = (int)st.st_blksize;
	buf = malloc((size_t)cap);
	if (!buf) {
		if (err)
			*err = ENOMEM;
		close(fd);
		return -1;
	}
	for (;;) {
		int off;
		n = getdents(fd, buf, cap);
		while (n < 0 && errno == EINVAL && cap < 1024 * 1024) {
			char *bigger;
			int ncap = cap * 2;
			bigger = realloc(buf, (size_t)ncap);
			if (!bigger)
				break;
			buf = bigger;
			cap = ncap;
			n = getdents(fd, buf, cap);
		}
		if (n < 0 && (errno == ENOSYS || errno == EPERM)) {
			long base = 0;
			n = getdirentries(fd, buf, cap, &base);
		}
		if (n < 0) {
			if (err)
				*err = errno;
			free(buf);
			close(fd);
			return -1;
		}
		if (n == 0)
			break;
		off = 0;
		while (off + 8 <= n) {
			struct dirent *e = (struct dirent *)(buf + off);
			int rec = (int)e->d_reclen;
			char name[256];
			char full[1024];
			int namelen;
			int is_dir;
			size_t bl;
			if (rec < 8 || off + rec > n)
				break;
			off += rec;
			if (e->d_fileno == 0)
				continue;
			namelen = (int)e->d_namlen;
			if (namelen <= 0 || namelen > 255 || 8 + namelen > rec)
				namelen = (int)strnlen(e->d_name, (size_t)(rec - 8));
			if (namelen <= 0 || namelen > 255)
				continue;
			memcpy(name, e->d_name, (size_t)namelen);
			name[namelen] = 0;
			if (name[0] == '.' &&
			    (name[1] == 0 || (name[1] == '.' && name[2] == 0)))
				continue;
			bl = strlen(path);
			if (bl + 1 + (size_t)namelen >= sizeof full)
				continue;
			memcpy(full, path, bl);
			full[bl] = '/';
			memcpy(full + bl + 1, name, (size_t)namelen + 1);
			is_dir = e->d_type == DT_DIR;
			if (e->d_type == DT_UNKNOWN || e->d_type == DT_LNK) {
				struct stat st2;
				if (stat(full, &st2) == 0)
					is_dir = S_ISDIR(st2.st_mode);
			}
			if (fn(full, name, is_dir, ud)) {
				free(buf);
				close(fd);
				return 0;
			}
		}
	}
	free(buf);
	close(fd);
	return 0;
}
