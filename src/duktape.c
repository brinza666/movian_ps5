#include "duktape.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

typedef struct { int initialized; } duk_heap_t;
static duk_heap_t g_heap = {0};

duk_context *duk_create_heap_default(void) {
    if (!g_heap.initialized) {
        g_heap.initialized = 1;
        fprintf(stderr, "[Duktape stub] heap created\n");
    }
    return (duk_context *)&g_heap;
}

void duk_destroy_heap(duk_context *ctx) {
    (void)ctx;
    g_heap.initialized = 0;
    fprintf(stderr, "[Duktape stub] heap destroyed\n");
}

void duk_push_global_object(duk_context *ctx) { (void)ctx; }
void duk_push_object(duk_context *ctx) { (void)ctx; }
void duk_push_c_function(duk_context *ctx, duk_c_function func, duk_int_t nargs) {
    (void)ctx; (void)func; (void)nargs;
}
void duk_put_prop_string(duk_context *ctx, duk_idx_t oi, const char *k) {
    (void)ctx; (void)oi; (void)k;
}
void duk_pop(duk_context *ctx, duk_idx_t c) { (void)ctx; (void)c; }
int duk_peval_string(duk_context *ctx, const char *src) {
    (void)ctx;
    fprintf(stderr, "[Duktape stub] peval: %s\n", src);
    return 0;
}
void duk_eval_string_noresult(duk_context *ctx, const char *src) {
    (void)ctx;
    fprintf(stderr, "[Duktape stub] eval: %s\n", src);
}
const char *duk_safe_to_string(duk_context *ctx, duk_idx_t i) {
    (void)ctx; (void)i;
    return "";
}
void duk_get_prop_string(duk_context *ctx, duk_idx_t oi, const char *k) {
    (void)ctx; (void)oi; (void)k;
}
int duk_is_function(duk_context *ctx, duk_idx_t i) {
    (void)ctx; (void)i;
    return 0;
}
int duk_pcall(duk_context *ctx, duk_idx_t n) {
    (void)ctx; (void)n;
    return 0;
}
void duk_push_boolean(duk_context *ctx, duk_int_t v) { (void)ctx; (void)v; }
void duk_push_number(duk_context *ctx, duk_double_t v) { (void)ctx; (void)v; }
void duk_push_string(duk_context *ctx, const char *s) { (void)ctx; (void)s; }
const char *duk_require_string(duk_context *ctx, duk_idx_t i) {
    (void)ctx; (void)i;
    return "";
}
void *duk_get_buffer(duk_context *ctx, duk_idx_t i, duk_size_t *s) {
    (void)ctx; (void)i; (void)s;
    return NULL;
}
void *duk_push_fixed_buffer_nozero(duk_context *ctx, duk_size_t s) {
    (void)ctx; (void)s;
    return NULL;
}
void duk_push_pointer(duk_context *ctx, void *p) { (void)ctx; (void)p; }
void duk_error(duk_context *ctx, duk_idx_t ec, const char *fmt, ...) {
    (void)ctx; (void)ec;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

DUK_INTERNAL_DECL const duk_c_function duk_bi_native_functions[149] = {0};
