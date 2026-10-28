#ifndef SPLICER_PLUGIN_HOST_H
#define SPLICER_PLUGIN_HOST_H

#include <stddef.h>

/* Plugins run in this process. Duktape evaluates the package script.
 * HTTP is sceHttp2 on the console and curl in the host test.
 */

#define PLUG_MAX 64
#define PLUG_ITEMS 48

int plug_scan(void);
int plug_count(void);
const char *plug_title(int index);
const char *plug_id(int index);

/* Load one scanned package and open its first service. 0 on a page. */
int plug_activate(int index);
int plug_open(const char *url);
int plug_back(void);

const char *plug_page_title(void);
const char *plug_page_error(void);
const char *plug_status(void);
int plug_item_count(void);
const char *plug_item_title(int index);
const char *plug_item_url(int index);
const char *plug_item_type(int index);
int plug_can_back(void);

/* ASCII for the 8x8 font. Cyrillic is transliterated. */
void plug_ascii(const char *utf8, char *out, size_t n);

const char *plug_engine_line(void);

#endif
