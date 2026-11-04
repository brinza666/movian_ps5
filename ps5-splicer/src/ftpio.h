#ifndef SPLICER_FTPIO_H
#define SPLICER_FTPIO_H

#include <stdint.h>

/* etaHEN's FTP server can see USB when the title sandbox cannot.
 * Loopback only: 127.0.0.1, the ports etaHEN actually listens on.
 */

int ftp_probe(int *port);

typedef int (*ftp_visit_fn)(const char *url, const char *name, int is_dir, void *ud);

/* path is an absolute FTP path such as /mnt/usb0. depth is how many
 * folders to enter. Returns 0 if the directory listed, -1 otherwise. */
int ftp_walk(int port, const char *path, int depth, ftp_visit_fn fn, void *ud, int *err);

typedef struct ftp_file ftp_file;

ftp_file *ftp_file_open(const char *url);
void ftp_file_close(ftp_file *f);
int ftp_file_read(void *user, uint8_t *buf, int n);
int64_t ftp_file_seek(void *user, int64_t off, int whence);

#endif
