#include "duktape_ps5.h"
#include "paw_platform.h"
#include "movian_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static duk_context *g_duk_ctx = NULL;

/* Forward declarations */
static duk_int_t duk_ps5_video_framebuffer(duk_context *ctx);
static duk_int_t duk_ps5_video_flip(duk_context *ctx);
static duk_int_t duk_ps5_audio_write(duk_context *ctx);
static duk_int_t duk_ps5_input_poll(duk_context *ctx);
static duk_int_t duk_ps5_fs_read(duk_context *ctx);
static duk_int_t duk_ps5_fs_write(duk_context *ctx);
static duk_int_t duk_ps5_net_smb2_connect(duk_context *ctx);
static duk_int_t duk_ps5_log(duk_context *ctx);

int duktape_ps5_init(duk_context **ctx_out) {
    g_duk_ctx = duk_create_heap_default();
    if (!g_duk_ctx) {
        fprintf(stderr, "[Movian PS5] Duktape heap creation failed\n");
        return -1;
    }
    if (duktape_register_ps5_apis(g_duk_ctx) != 0) {
        fprintf(stderr, "[Movian PS5] Failed to register PS5 APIs\n");
        duk_destroy_heap(g_duk_ctx);
        g_duk_ctx = NULL;
        return -1;
    }
    *ctx_out = g_duk_ctx;
    fprintf(stderr, "[Movian PS5] Duktape initialized\n");
    return 0;
}

int duktape_register_ps5_apis(duk_context *ctx) {
    duk_push_global_object(ctx);
    duk_push_object(ctx);

    duk_push_object(ctx);
    duk_push_c_function(ctx, duk_ps5_video_framebuffer, 1);
    duk_put_prop_string(ctx, -2, "framebuffer");
    duk_push_c_function(ctx, duk_ps5_video_flip, 1);
    duk_put_prop_string(ctx, -2, "flip");
    duk_put_prop_string(ctx, -2, "video");

    duk_push_object(ctx);
    duk_push_c_function(ctx, duk_ps5_audio_write, 2);
    duk_put_prop_string(ctx, -2, "write");
    duk_put_prop_string(ctx, -2, "audio");

    duk_push_object(ctx);
    duk_push_c_function(ctx, duk_ps5_input_poll, 1);
    duk_put_prop_string(ctx, -2, "poll");
    duk_put_prop_string(ctx, -2, "input");

    duk_push_object(ctx);
    duk_push_c_function(ctx, duk_ps5_fs_read, 2);
    duk_put_prop_string(ctx, -2, "read");
    duk_push_c_function(ctx, duk_ps5_fs_write, 3);
    duk_put_prop_string(ctx, -2, "write");
    duk_put_prop_string(ctx, -2, "fs");

    duk_push_object(ctx);
    duk_push_c_function(ctx, duk_ps5_net_smb2_connect, 3);
    duk_put_prop_string(ctx, -2, "smb2_connect");
    duk_put_prop_string(ctx, -2, "net");

    duk_push_c_function(ctx, duk_ps5_log, 2);
    duk_put_prop_string(ctx, -2, "log");

    duk_put_prop_string(ctx, -2, "ps5");
    duk_pop(ctx, 1);

    return 0;
}

int duktape_ps5_load_movian(duk_context *ctx) {
    const char *js_paths[] = {
        "app0:/assets/ui/main.js",
        "app0:/assets/ui/index.js",
        "host0:/movian/ui/main.js",
        NULL
    };
    for (int i = 0; js_paths[i] != NULL; i++) {
        FILE *f = fopen(js_paths[i], "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *js_code = (char *)malloc(size + 1);
            if (!js_code) { fclose(f); continue; }
            fread(js_code, 1, size, f);
            js_code[size] = '\0';
            fclose(f);
            if (duk_peval_string(ctx, js_code) != 0) {
                fprintf(stderr, "[Movian PS5] JS error: %s\n",
                        duk_safe_to_string(ctx, -1));
                duk_pop(ctx, 1);
            } else {
                fprintf(stderr, "[Movian PS5] JS loaded from %s\n", js_paths[i]);
                free(js_code);
                return 0;
            }
            free(js_code);
            break;
        }
    }
    duk_eval_string_noresult(ctx,
        "print('Movian PS5 running headless');"
        "ps5.log('Hello from PS5');");
    return -1;
}

int duktape_ps5_call_render(duk_context *ctx) {
    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, "onRender");
    int is_func = duk_is_function(ctx, -1);
    if (is_func) {
        if (duk_pcall(ctx, 0) != 0) {
            fprintf(stderr, "[Movian PS5] JS render error: %s\n",
                    duk_safe_to_string(ctx, -1));
            duk_pop(ctx, 1);
            return -1;
        }
    }
    duk_pop(ctx, 1);
    return is_func ? 0 : -1;
}

void duktape_ps5_cleanup(duk_context *ctx) {
    if (ctx) { duk_destroy_heap(ctx); g_duk_ctx = NULL; }
}

static duk_int_t duk_ps5_video_framebuffer(duk_context *ctx) {
    duk_push_pointer(ctx, NULL);
    return 1;
}
static duk_int_t duk_ps5_video_flip(duk_context *ctx) {
    return 0;
}
static duk_int_t duk_ps5_audio_write(duk_context *ctx) {
    return 0;
}
static duk_int_t duk_ps5_input_poll(duk_context *ctx) {
    duk_push_object(ctx);
    duk_push_boolean(ctx, 0); duk_put_prop_string(ctx, -2, "cross");
    duk_push_boolean(ctx, 0); duk_put_prop_string(ctx, -2, "circle");
    duk_push_boolean(ctx, 0); duk_put_prop_string(ctx, -2, "square");
    duk_push_boolean(ctx, 0); duk_put_prop_string(ctx, -2, "triangle");
    duk_push_number(ctx, 0); duk_put_prop_string(ctx, -2, "lx");
    duk_push_number(ctx, 0); duk_put_prop_string(ctx, -2, "ly");
    return 1;
}
static duk_int_t duk_ps5_fs_read(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    size_t size;
    void *buf;
    if (ps5_fs_read(path, &buf, &size) != 0) {
        duk_error(ctx, DUK_ERR_ERROR, "Cannot read file");
        return 0;
    }
    void *dst = duk_push_fixed_buffer_nozero(ctx, (duk_size_t)size);
    memcpy(dst, buf, size);
    free(buf);
    return 1;
}
static duk_int_t duk_ps5_fs_write(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    duk_size_t data_size;
    const uint8_t *data = duk_get_buffer(ctx, 1, &data_size);
    if (!data) {
        duk_error(ctx, DUK_ERR_TYPE_ERROR, "Expected buffer");
        return 0;
    }
    if (ps5_fs_write(path, data, data_size) != 0) {
        duk_error(ctx, DUK_ERR_ERROR, "Cannot write file");
        return 0;
    }
    return 0;
}
static duk_int_t duk_ps5_net_smb2_connect(duk_context *ctx) {
    const char *host = duk_require_string(ctx, 0);
    const char *share = duk_require_string(ctx, 1);
    if (ps5_net_smb2_connect(host, share, NULL, NULL) != 0) {
        duk_push_boolean(ctx, 0);
    } else {
        duk_push_boolean(ctx, 1);
    }
    return 1;
}
static duk_int_t duk_ps5_log(duk_context *ctx) {
    const char *msg = duk_require_string(ctx, 0);
    fprintf(stderr, "[Movian JS] %s\n", msg);
    return 0;
}
