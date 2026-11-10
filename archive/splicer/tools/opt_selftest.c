/* Host checks for the settings screen. Not part of payload.elf.
 *   gcc -Wall -Wextra -O2 -I../src tools/opt_selftest.c src/options.c -lz -o /tmp/opt_selftest
 *   /tmp/opt_selftest /tmp/optfix /path/to/plugins/repo.json
 */
#include "options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int g_fail;
static char g_hit[800];
static char *g_repo_body;
static unsigned char *g_blob;
static size_t g_blob_n;
static int g_http_fail;

#define CHECK(cond)                                                                          \
	do {                                                                                 \
		if (!(cond)) {                                                               \
			fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);     \
			g_fail++;                                                            \
		}                                                                            \
	} while (0)

int
plug_http_get(const char *url, char **body, size_t *len, size_t cap, char *err, size_t errn)
{
	(void)cap;
	snprintf(g_hit, sizeof g_hit, "%s", url ? url : "");
	*body = NULL;
	*len = 0;
	if (g_http_fail) {
		snprintf(err, errn, "offline");
		return -1;
	}
	if (url && (strstr(url, "/download/") || strstr(url, "files/plugin.js"))) {
		if (!g_blob) {
			snprintf(err, errn, "no blob");
			return -1;
		}
		*body = (char *)malloc(g_blob_n + 1);
		if (!*body)
			return -1;
		memcpy(*body, g_blob, g_blob_n);
		(*body)[g_blob_n] = 0;
		*len = g_blob_n;
		return 0;
	}
	if (!g_repo_body) {
		snprintf(err, errn, "no repo");
		return -1;
	}
	*body = strdup(g_repo_body);
	if (!*body)
		return -1;
	*len = strlen(*body);
	return 0;
}

static void
put16(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char)(v & 255);
	p[1] = (unsigned char)((v >> 8) & 255);
}

static void
put32(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char)(v & 255);
	p[1] = (unsigned char)((v >> 8) & 255);
	p[2] = (unsigned char)((v >> 16) & 255);
	p[3] = (unsigned char)((v >> 24) & 255);
}

static size_t
zip_store(unsigned char *dst, size_t cap, const char *name, const void *data, size_t n, int encrypt)
{
	size_t namelen = strlen(name);
	size_t local = 30 + namelen + n;
	size_t central = 46 + namelen;
	size_t total = local + central + 22;
	unsigned char *cd;
	unsigned char *e;
	if (total > cap)
		return 0;
	memset(dst, 0, total);
	dst[0] = 'P';
	dst[1] = 'K';
	dst[2] = 3;
	dst[3] = 4;
	put16(dst + 4, 20);
	put16(dst + 6, encrypt ? 1 : 0);
	put32(dst + 18, (unsigned)n);
	put32(dst + 22, (unsigned)n);
	put16(dst + 26, (unsigned)namelen);
	memcpy(dst + 30, name, namelen);
	memcpy(dst + 30 + namelen, data, n);
	cd = dst + local;
	cd[0] = 'P';
	cd[1] = 'K';
	cd[2] = 1;
	cd[3] = 2;
	put16(cd + 4, 20);
	put16(cd + 6, 20);
	put16(cd + 8, encrypt ? 1 : 0);
	put32(cd + 20, (unsigned)n);
	put32(cd + 24, (unsigned)n);
	put16(cd + 28, (unsigned)namelen);
	memcpy(cd + 46, name, namelen);
	e = cd + central;
	e[0] = 'P';
	e[1] = 'K';
	e[2] = 5;
	e[3] = 6;
	put16(e + 8, 1);
	put16(e + 10, 1);
	put32(e + 12, (unsigned)central);
	put32(e + 16, (unsigned)local);
	return total;
}

static size_t g_read_n;

static char *
read_all(const char *path)
{
	FILE *f = fopen(path, "rb");
	char *b;
	long n;
	g_read_n = 0;
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n < 0 || n > 8 * 1024 * 1024) {
		fclose(f);
		return NULL;
	}
	b = (char *)malloc((size_t)n + 1);
	if (!b) {
		fclose(f);
		return NULL;
	}
	if (n && fread(b, 1, (size_t)n, f) != (size_t)n) {
		free(b);
		fclose(f);
		return NULL;
	}
	fclose(f);
	b[n] = 0;
	g_read_n = (size_t)n;
	return b;
}

static void
select_label(const char *label)
{
	int i;
	for (i = 0; i < 24; i++) {
		char text[240];
		int header = 0;
		int selected = 0;
		int r;
		for (r = 0; r < opt_row_count(); r++) {
			opt_row(r, text, sizeof text, &header, &selected);
			if (selected && strstr(text, label))
				return;
		}
		opt_pad(OPT_DOWN);
	}
}

static void
write_text(const char *path, const char *text)
{
	FILE *f = fopen(path, "wb");
	CHECK(f != NULL);
	if (f) {
		fputs(text, f);
		fclose(f);
	}
}

static void
mkdir1(const char *path)
{
	mkdir(path, 0755);
}

int
main(int argc, char **argv)
{
	const char *fix = argc > 1 ? argv[1] : "/tmp/optfix";
	const char *repo_path = argc > 2 ? argv[2] : "";
	char settings[512], root[512], err[240], path[700];
	unsigned char zip[2048];
	size_t zn;
	char *repo;
	char *js;
	int i;

	snprintf(settings, sizeof settings, "%s/settings.txt", fix);
	snprintf(root, sizeof root, "%s/plugins", fix);
	snprintf(path, sizeof path, "rm -rf '%s'", root);
	CHECK(system(path) == 0);
	mkdir1(fix);
	mkdir1(root);
	write_text(settings,
		   "alt_repo=https://example.com/plugins/repo.json\n"
		   "beta=one two\n"
		   "autoupgrade=0\n"
		   "autoinstall=1\n"
		   "speed=3\n"
		   "seek=2\n"
		   "audio=2\n"
		   "layout=4\n"
		   "decoder=1\n");
	opt_set_paths(settings, root);
	opt_boot();
	CHECK(strcmp(opt_repo_url(), "https://example.com/plugins/repo.json") == 0);
	CHECK(opt_speed() > 1.2 && opt_speed() < 1.3);
	CHECK(opt_seek() == 30);
	CHECK(opt_audio() == 2);
	CHECK(opt_layout() == 4);
	CHECK(opt_force_software() == 1);

	opt_set_layout(1);
	opt_set_audio(0);
	opt_boot();
	CHECK(opt_layout() == 1);
	CHECK(opt_audio() == 0);
	CHECK(opt_force_software() == 1);

	CHECK(opt_ingest_repo("{\"version\":2,\"plugins\":[]}", err, sizeof err) != 0);
	CHECK(opt_ingest_repo("{\"version\":1,\"message\":\"beta closed\",\"plugins\":[]}", err,
			      sizeof err) != 0);
	CHECK(strstr(err, "beta closed") != NULL);

	CHECK(opt_ingest_repo(
		      "{\"version\":\"1\",\"plugins\":["
		      "{\"id\":\"Skip\",\"type\":\"bitcode\",\"version\":\"1\",\"title\":\"Skip\","
		      "\"downloadURL\":\"https://example.com/skip\"},"
		      "{\"id\":\"Rel\",\"type\":\"ecmascript\",\"version\":\"1.2.0\","
		      "\"title\":\"Quoted \\\"name\\\" \\u0422est {not a key}\","
		      "\"file\":\"plugin.js\",\"downloadURL\":\"files/plugin.js\","
		      "\"control\":{\"uriprefixes\":[\"rel:\",\"https://rel.example/\"]}},"
		      "{\"id\":\"Low\",\"type\":\"ecmascript\",\"version\":\"0.9.1\",\"title\":\"Low\","
		      "\"file\":\"plugin.js\",\"downloadURL\":\"https://example.com/download/low.js\"},"
		      "{\"id\":\"High\",\"type\":\"ecmascript\",\"version\":\"1.9\",\"title\":\"High\","
		      "\"file\":\"plugin.js\",\"downloadURL\":\"https://example.com/download/high.js\"}"
		      "]}",
		      err, sizeof err) == 0);
	CHECK(opt_repo_count() == 3);
	CHECK(strcmp(opt_repo_id(0), "Rel") == 0);
	CHECK(strcmp(opt_repo_version(1), "0.9.1") == 0);
	CHECK(strstr(opt_repo_title(0), "name") != NULL);

	zn = zip_store(zip, sizeof zip, "plugin.js", "var plugin = 1;\n", 16, 0);
	CHECK(zn > 0);
	CHECK(opt_install_blob(1, zip, zn, err, sizeof err) == 0);
	snprintf(path, sizeof path, "%s/Low/plugin.js", root);
	js = read_all(path);
	CHECK(js && strcmp(js, "var plugin = 1;\n") == 0);
	free(js);
	snprintf(path, sizeof path, "%s/Low/plugin.json", root);
	js = read_all(path);
	CHECK(js && strstr(js, "\"version\":\"0.9.1\"") != NULL);
	free(js);

	CHECK(opt_install_blob(0, (const uint8_t *)"<html>nope</html>", 17, err, sizeof err) != 0);
	CHECK(opt_install_blob(0, (const uint8_t *)"function start(){return 1}\n", 28, err, sizeof err) ==
	      0);
	snprintf(path, sizeof path, "%s/Rel/plugin.json", root);
	js = read_all(path);
	CHECK(js && strstr(js, "\\\"name\\\"") != NULL);
	CHECK(js && strstr(js, "\xD0\xA2" "est") != NULL);
	free(js);

	zn = zip_store(zip, sizeof zip, "../evil.js", "nope", 4, 0);
	CHECK(opt_install_blob(2, zip, zn, err, sizeof err) != 0);
	zn = zip_store(zip, sizeof zip, "secret.js", "nope", 4, 1);
	CHECK(opt_install_blob(2, zip, zn, err, sizeof err) != 0);

	snprintf(path, sizeof path, "%s/defl.zip", fix);
	js = read_all(path);
	CHECK(js != NULL);
	if (js) {
		CHECK(opt_install_blob(2, (const uint8_t *)js, g_read_n, err, sizeof err) == 0);
		free(js);
		snprintf(path, sizeof path, "%s/High/plugin.js", root);
		js = read_all(path);
		CHECK(js && strstr(js, "deflated-ok") != NULL);
		free(js);
	}

	snprintf(path, sizeof path, "%s/Low/plugin.json", root);
	write_text(path, "{\"id\":\"Low\",\"version\":\"0.0.1\",\"file\":\"plugin.js\"}\n");
	snprintf(path, sizeof path, "%s/Low/plugin.js", root);
	write_text(path, "old\n");
	snprintf(path, sizeof path, "%s/High/plugin.json", root);
	write_text(path, "{\"id\":\"High\",\"version\":\"1.10\",\"file\":\"plugin.js\"}\n");
	snprintf(path, sizeof path, "%s/High/plugin.js", root);
	write_text(path, "stay\n");

	g_blob = (unsigned char *)"fresh\n";
	g_blob_n = 6;
	opt_open();
	select_label("Upgrade installed");
	CHECK(opt_pad(OPT_CROSS) == 1);
	CHECK(opt_armed() == 1);
	opt_work();
	snprintf(path, sizeof path, "%s/Low/plugin.js", root);
	js = read_all(path);
	CHECK(js && strcmp(js, "fresh\n") == 0);
	free(js);
	CHECK(strstr(g_hit, "https://example.com/download/low.js") != NULL);
	CHECK(opt_armed() == 1);
	opt_work();
	CHECK(strcmp(opt_status(), "plugins are current") == 0);
	snprintf(path, sizeof path, "%s/High/plugin.js", root);
	js = read_all(path);
	CHECK(js && strcmp(js, "stay\n") == 0);
	free(js);

	g_repo_body =
		"{\"version\":1,\"plugins\":["
		"{\"id\":\"Low\",\"type\":\"ecmascript\",\"version\":\"0.9.1\",\"title\":\"Low\","
		"\"file\":\"plugin.js\",\"downloadURL\":\"https://example.com/download/low.js\"},"
		"{\"id\":\"High\",\"type\":\"ecmascript\",\"version\":\"1.9\",\"title\":\"High\","
		"\"file\":\"plugin.js\",\"downloadURL\":\"https://example.com/download/high.js\"}"
		"]}";
	opt_open();
	select_label("Browse available");
	CHECK(opt_pad(OPT_CROSS) == 1);
	CHECK(opt_armed() == 1);
	opt_work();
	CHECK(opt_armed() == 0);
	CHECK(opt_repo_count() == 2);
	CHECK(strcmp(opt_title(), "AVAILABLE PLUGINS") == 0);
	CHECK(strstr(g_hit, "betapassword=one") != NULL);
	CHECK(strstr(g_hit, "betapassword=two") != NULL);
	CHECK(opt_pad(OPT_CIRCLE) == 1);

	g_http_fail = 1;
	select_label("Browse available");
	CHECK(opt_pad(OPT_CROSS) == 1);
	opt_work();
	CHECK(strcmp(opt_status(), "offline") == 0);
	g_http_fail = 0;

	g_repo_body =
		"{\"version\":1,\"plugins\":[{\"id\":\"Rel\",\"type\":\"ecmascript\","
		"\"version\":\"1.2.0\",\"title\":\"Rel\",\"file\":\"plugin.js\","
		"\"downloadURL\":\"files/plugin.js\",\"control\":{\"uriprefixes\":[\"rel:\"]}}]}";
	CHECK(opt_ingest_repo(g_repo_body, err, sizeof err) == 0);
	snprintf(path, sizeof path, "%s/Rel", root);
	mkdir1(path);
	snprintf(path, sizeof path, "%s/Rel/plugin.json", root);
	remove(path);
	g_blob = (unsigned char *)"auto\n";
	g_blob_n = 5;
	g_hit[0] = 0;
	CHECK(opt_install_for_uri("other:nope", err, sizeof err) == 0);
	CHECK(opt_install_for_uri("rel:item", err, sizeof err) == 1);
	CHECK(strcmp(g_hit, "https://example.com/plugins/files/plugin.js") == 0);
	CHECK(opt_install_for_uri("rel:again", err, sizeof err) == 0);

	write_text(settings, "alt_repo=https://example.com/keep\nautoupgrade=0\nautoinstall=0\n");
	opt_boot();
	CHECK(opt_install_for_uri("rel:item", err, sizeof err) == 0);

	opt_open();
	CHECK(opt_pad(OPT_CROSS) == 1);
	CHECK(strcmp(opt_title(), "REPOSITORY URL") == 0);
	for (i = 0; i < 8; i++)
		CHECK(opt_pad(OPT_DOWN) == 1);
	CHECK(opt_pad(OPT_LEFT) == 1);
	CHECK(opt_pad(OPT_RIGHT) == 1);
	CHECK(opt_pad(OPT_CROSS) == 1);
	CHECK(opt_pad(OPT_CIRCLE) == 1);
	CHECK(strcmp(opt_repo_url(), "https://example.com/keep") == 0);
	CHECK(opt_pad(OPT_CROSS) == 1);
	CHECK(opt_pad(OPT_CROSS) == 1);
	CHECK(opt_pad(OPT_RIGHT) == 1);
	CHECK(opt_pad(OPT_CROSS) == 1);
	CHECK(opt_pad(OPT_CIRCLE) == 1);
	CHECK(strcmp(opt_repo_url(), "https://example.com/keep") == 0);
	CHECK(opt_pad(OPT_CROSS) == 1);
	CHECK(opt_pad(OPT_TRI) == 1);
	CHECK(opt_pad(OPT_DOWN) == 1);
	CHECK(opt_pad(OPT_CROSS) == 1);
	CHECK(opt_pad(OPT_R1) == 1);
	CHECK(strcmp(opt_repo_url(), "https://example.com/keepQ") == 0);

	opt_open();
	CHECK(opt_pad(OPT_RIGHT) == 1);
	CHECK(opt_repo_url()[0] == 0);
	CHECK(opt_pad(OPT_LEFT) == 1);
	CHECK(strcmp(opt_repo_url(), "https://repo.movian.eu/plugins-v1.json") == 0);
	select_label("Hardware decoding");
	CHECK(opt_pad(OPT_CROSS) == 1);
	CHECK(opt_force_software() == 1);
	CHECK(opt_take_reopen() == 1);
	select_label("Rescan USB");
	CHECK(opt_pad(OPT_CROSS) == 0);
	CHECK(opt_take_rescan() == 1);
	CHECK(opt_is_open() == 0);

	if (repo_path[0]) {
		repo = read_all(repo_path);
		CHECK(repo != NULL);
		if (repo) {
			write_text(settings, "alt_repo=\nautoupgrade=0\n");
			opt_boot();
			CHECK(opt_ingest_repo(repo, err, sizeof err) == 0);
			CHECK(opt_repo_count() == 80);
			CHECK(strcmp(opt_repo_id(0), "YooTV") == 0);
			CHECK(strcmp(opt_repo_version(0), "0.0.4") == 0);
			free(repo);
		}
	}

	if (g_fail) {
		fprintf(stderr, "%d checks failed\n", g_fail);
		return 1;
	}
	printf("opt_selftest ok\n");
	return 0;
}
