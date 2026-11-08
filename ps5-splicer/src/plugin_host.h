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

/* Square menu: addOptAction / addOptURL / addOptSeparator. */
int plug_item_option_count(int item);
const char *plug_item_option_title(int item, int opt);
const char *plug_item_option_kind(int item, int opt);
const char *plug_item_option_url(int item, int opt);

/* Options button: page.options and plugin.createSettings.
 * Every Movian type: bool, string, integer, multiopt, action, separator, info. */
int plug_setting_count(void);
const char *plug_setting_title(int index);
const char *plug_setting_kind(int index);
const char *plug_setting_value(int index);

/* how is toggle, inc, dec, go, or the new string.
 * 0 stays on the page, 1 caller opens url, 2 the page already moved (url is the redirect),
 * -1 failed. url may be NULL. */
int plug_setting_apply(int index, const char *how, char *url, size_t n);
/* opt < 0 runs the item itself when its type is action. Same return codes. */
int plug_use_option(int item, int opt, char *url, size_t n);

int plug_can_back(void);

/* ASCII for the 8x8 font. Cyrillic is transliterated. */
void plug_ascii(const char *utf8, char *out, size_t n);

const char *plug_engine_line(void);

/* GET. *body is malloc'd and NUL-terminated. cap is the byte limit. */
int plug_http_get(const char *url, char **body, size_t *len, size_t cap, char *err, size_t errn);

/* Called with each page URL before the script opens it. Optional. */
void plug_set_uri_hook(int (*fn)(const char *uri));

#endif
