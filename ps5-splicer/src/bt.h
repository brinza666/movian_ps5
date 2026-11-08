#ifndef SPLICER_BT_H
#define SPLICER_BT_H

#include <stdint.h>

/* Movian's client, in this payload: a magnet or .torrent is a video file
 * that can be read before the download finishes. One transfer at a time.
 * Plaintext peer wire, HTTP and UDP trackers. Not TorrServer.
 */

int bt_start(const char *src);
void bt_stop(void);
int bt_active(void);
int bt_same(const char *src);

/* Inject a peer. Used by the host self-test and by a future peers file. */
int bt_add_peer(const char *ip, int port);

void bt_pump(int timeout_ms);

const char *bt_line(void);
const char *bt_name(void);

/* Largest video inside the torrent has its header (and mp4 tail) locally. */
int bt_playable(void);

/*  >0 bytes, 0 = clean EOF, -1 = that range is not here yet. */
int bt_read(uint8_t *buf, int n);
/* whence: 0 set, 1 cur, 2 end, 0x10000 = size. */
int64_t bt_seek(int64_t off, int whence);

#endif
