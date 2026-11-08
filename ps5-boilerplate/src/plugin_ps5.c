/*
 * Movian PS5 - Plugin System (Duktape API v2)
 * Plugin repository, install on demand, alternate repository URL
 * Based on Movian plugin system architecture
 */

#include "paw_platform.h"
#include "duktape.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Plugin state */
typedef struct {
    char *id;
    char *name;
    char *version;
    char *description;
    char *author;
    char *url;
    char *local_path;
    int installed;
    int enabled;
    duk_context *ctx;          /* Duktape context for plugin */
    int api_version;           /* API v2 */
} plugin_t;

/* Plugin repository */
static plugin_t g_plugins[64];
static int g_num_plugins = 0;

/* Repository URL */
static char g_repo_url[512] = "https://repo.movian.eu/plugins-v1.json";
static char g_beta_password[64] = {0};
static int g_auto_upgrade = 1;
static int g_install_on_demand = 1;

/* Repository source (default or alternate) */
#define DEFAULT_REPO_URL "https://repo.movian.eu/plugins-v1.json"
#define ALTERNATE_REPO_URL "https://bit.ly/mprepo"

/* Initialize plugin system */
int ps5_plugin_init(void) {
    fprintf(stderr, "[PS5 Plugin] Initializing plugin system...\n");
    fprintf(stderr, "[PS5 Plugin] Repository: %s\n", g_repo_url);

    /* Load installed plugins from disk */
    ps5_plugin_load_installed();

    return 0;
}

/* Load installed plugins from local storage */
void ps5_plugin_load_installed(void) {
    /* Check L2 - packages already on disk */
    char **entries;
    int count;
    if (ps5_fs_listdir("app0:/plugins/", &entries, &count) == 0) {
        for (int i = 0; i < count; i++) {
            char path[512];
            snprintf(path, sizeof(path), "app0:/plugins/%s", entries[i]);
            if (ps5_fs_is_directory(path)) {
                ps5_plugin_install_from_path(path, entries[i]);
            }
        }
        ps5_fs_listdir_free(entries, count);
    }
}

/* Install plugin from local path */
int ps5_plugin_install_from_path(const char *path, const char *id) {
    if (!path || !id) return -1;

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < 64; i++) {
        if (!g_plugins[i].id) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;

    memset(&g_plugins[slot], 0, sizeof(plugin_t));
    g_plugins[slot].id = strdup(id);
    g_plugins[slot].name = strdup(id);
    g_plugins[slot].version = strdup("1.0");
    g_plugins[slot].installed = 1;
    g_plugins[slot].enabled = 1;
    g_plugins[slot].local_path = strdup(path);
    g_plugins[slot].api_version = 2;

    g_num_plugins++;

    fprintf(stderr, "[PS5 Plugin] Installed: %s from %s\n", id, path);
    return slot;
}

/* Install plugin from repository */
int ps5_plugin_install_from_repo(const char *plugin_id) {
    if (!plugin_id) return -1;

    fprintf(stderr, "[PS5 Plugin] Installing from repository: %s\n", plugin_id);
    fprintf(stderr, "[PS5 Plugin] Repository: %s\n", g_repo_url);

    /* In real implementation, this would:
     * 1. Fetch repository JSON from g_repo_url
     * 2. Find plugin by ID
     * 3. Download plugin package
     * 4. Verify signature (popup acceptance)
     * 5. Install to app0:/plugins/
     * 6. Load plugin
     */

    /* For now, mark as installed */
    return ps5_plugin_install_from_path("app0:/plugins/", plugin_id);
}

/* Enable/disable plugin */
int ps5_plugin_enable(const char *id, int enabled) {
    for (int i = 0; i < 64; i++) {
        if (g_plugins[i].id && strcmp(g_plugins[i].id, id) == 0) {
            g_plugins[i].enabled = enabled;
            fprintf(stderr, "[PS5 Plugin] %s %s\n",
                    id, enabled ? "enabled" : "disabled");
            return 0;
        }
    }
    return -1;
}

/* Get plugin count */
int ps5_plugin_count(void) {
    return g_num_plugins;
}

/* Get plugin info */
int ps5_plugin_get_info(int index, plugin_t *out) {
    if (index < 0 || index >= 64 || !out) return -1;
    if (!g_plugins[index].id) return -1;
    memcpy(out, &g_plugins[index], sizeof(plugin_t));
    return 0;
}

/* Find plugin by ID */
int ps5_plugin_find(const char *id) {
    if (!id) return -1;
    for (int i = 0; i < 64; i++) {
        if (g_plugins[i].id && strcmp(g_plugins[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

/* Execute plugin function via Duktape */
int ps5_plugin_execute(duk_context *ctx, const char *plugin_id,
                        const char *function_name, int arg_count) {
    int slot = ps5_plugin_find(plugin_id);
    if (slot < 0) {
        fprintf(stderr, "[PS5 Plugin] Plugin not found: %s\n", plugin_id);
        return -1;
    }

    if (!g_plugins[slot].enabled) {
        fprintf(stderr, "[PS5 Plugin] Plugin disabled: %s\n", plugin_id);
        return -1;
    }

    /* Create plugin Duktape context if needed */
    if (!g_plugins[slot].ctx) {
        g_plugins[slot].ctx = duk_create_heap_default();
        if (!g_plugins[slot].ctx) {
            fprintf(stderr, "[PS5 Plugin] Failed to create Duktape context for %s\n", plugin_id);
            return -1;
        }

        /* Load plugin code */
        char plugin_code_path[512];
        snprintf(plugin_code_path, sizeof(plugin_code_path),
                "%s/main.js", g_plugins[slot].local_path);

        void *buf;
        size_t size;
        if (ps5_fs_read(plugin_code_path, &buf, &size) == 0) {
            char *code = malloc(size + 1);
            memcpy(code, buf, size);
            code[size] = '\0';
            free(buf);

            if (duk_peval_string(g_plugins[slot].ctx, code) != 0) {
                fprintf(stderr, "[PS5 Plugin] Load error for %s: %s\n",
                        plugin_id, duk_safe_to_string(g_plugins[slot].ctx, -1));
                duk_pop(g_plugins[slot].ctx);
            }
            free(code);
        }
    }

    /* Call plugin function */
    duk_ctx *pctx = g_plugins[slot].ctx;
    duk_push_global_object(pctx);
    duk_get_prop_string(pctx, -1, function_name);
    if (duk_is_function(pctx, -1)) {
        if (duk_pcall(pctx, arg_count) != 0) {
            fprintf(stderr, "[PS5 Plugin] Function error: %s\n",
                    duk_safe_to_string(pctx, -1));
            duk_pop(pctx);
            duk_pop(pctx);
            return -1;
        }
        duk_pop(pctx);
    }
    duk_pop(pctx);

    return 0;
}

/* Set repository URL */
void ps5_plugin_set_repo_url(const char *url) {
    if (url) {
        strncpy(g_repo_url, url, sizeof(g_repo_url) - 1);
        fprintf(stderr, "[PS5 Plugin] Repository URL set to: %s\n", g_repo_url);
    }
}

/* Get repository URL */
const char* ps5_plugin_get_repo_url(void) {
    return g_repo_url;
}

/* Set beta password */
void ps5_plugin_set_beta_password(const char *password) {
    if (password) {
        strncpy(g_beta_password, password, sizeof(g_beta_password) - 1);
    }
}

/* Enable/disable auto upgrade */
void ps5_plugin_set_auto_upgrade(int enabled) {
    g_auto_upgrade = enabled;
}

/* Enable/disable install on demand */
void ps5_plugin_set_install_on_demand(int enabled) {
    g_install_on_demand = enabled;
}

/* Check for updates */
int ps5_plugin_check_updates(void) {
    fprintf(stderr, "[PS5 Plugin] Checking for updates from %s\n", g_repo_url);
    /* Would fetch repository and compare versions */
    return 0;
}

/* Shutdown plugin system */
void ps5_plugin_shutdown(void) {
    for (int i = 0; i < 64; i++) {
        if (g_plugins[i].ctx) {
            duk_destroy_heap(g_plugins[i].ctx);
            g_plugins[i].ctx = NULL;
        }
        free(g_plugins[i].id);
        free(g_plugins[i].name);
        free(g_plugins[i].version);
        free(g_plugins[i].description);
        free(g_plugins[i].author);
        free(g_plugins[i].url);
        free(g_plugins[i].local_path);
        memset(&g_plugins[i], 0, sizeof(plugin_t));
    }
    g_num_plugins = 0;
    fprintf(stderr, "[PS5 Plugin] Shutdown\n");
}
