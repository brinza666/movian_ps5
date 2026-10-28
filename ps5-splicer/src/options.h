#ifndef SPLICER_OPTIONS_H
#define SPLICER_OPTIONS_H

#include <stddef.h>
#include <stdint.h>

/* Settings opened with the Options button. The names match M7
 * (Settings, General, plugins): alternate repository, beta passwords,
 * automatic upgrade, install on demand. Playback rows are the knobs
 * this payload already has.
 */

#define OPT_UP 0x0001u
#define OPT_DOWN 0x0002u
#define OPT_LEFT 0x0004u
#define OPT_RIGHT 0x0008u
#define OPT_CROSS 0x0010u
#define OPT_CIRCLE 0x0020u
#define OPT_SQUARE 0x0040u
#define OPT_TRI 0x0080u
#define OPT_L1 0x0100u
#define OPT_R1 0x0200u

void opt_boot(void);
void opt_set_paths(const char *settings_file, const char *install_root);

void opt_open(void);
int opt_is_open(void);
/* 1 while the screen should stay up. */
int opt_pad(unsigned bits);
int opt_armed(void);
void opt_work(void);

int opt_row_count(void);
void opt_row(int index, char *text, size_t n, int *header, int *selected);
const char *opt_title(void);
const char *opt_status(void);
const char *opt_hint(void);

int opt_take_rescan(void);
int opt_take_reopen(void);

int opt_layout(void);
void opt_set_layout(int layout);
int opt_audio(void);
void opt_set_audio(int audio);
double opt_speed(void);
void opt_set_speed(double speed);
double opt_seek(void);
int opt_force_software(void);

const char *opt_repo_url(void);
int opt_repo_count(void);
const char *opt_repo_id(int index);
const char *opt_repo_title(int index);
const char *opt_repo_version(int index);

int opt_ingest_repo(const char *json, char *err, size_t errn);
int opt_install_blob(int index, const uint8_t *zip, size_t n, char *err, size_t errn);
/* Install the plugin whose uriprefix matches uri, if demand-install is on
 * and that plugin is not already on disk. 1 installed, 0 nothing to do, -1 error.
 * The catalog has to have been fetched once (Browse available plugins).
 */
int opt_install_for_uri(const char *uri, char *err, size_t errn);

/* GET url into a malloc'd buffer. Implemented next to the plugin HTTP client. */
int plug_http_get(const char *url, char **body, size_t *len, size_t cap, char *err, size_t errn);

#endif
