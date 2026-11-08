#ifndef DUK_H_INCLUDED
#define DUK_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/* Basic types - defined FIRST */
typedef int duk_int_t;
typedef unsigned int duk_uint_t;
typedef long duk_idx_t;
typedef unsigned long duk_uarridx_t;
typedef long long duk_double_t;
typedef unsigned long long duk_size_t;

/* Return type for Duktape C functions */
typedef duk_int_t duk_ret_t;

/* Opaque context type */
typedef struct duk_context duk_context;

/* Function pointer types */
typedef duk_ret_t (*duk_c_function)(duk_context *ctx);
typedef void *(*duk_alloc_function)(void *udata, duk_size_t size);
typedef void *(*duk_realloc_function)(void *udata, void *ptr, duk_size_t size);
typedef void (*duk_free_function)(void *udata, void *ptr);
typedef void (*duk_fatal_function)(void *udata, const char *msg);

/* Error codes */
#define DUK_ERR_TYPE_ERROR 1
#define DUK_ERR_ERROR 2
#define DUK_ERR_API_ERROR 3
#define DUK_ERR_INTERNAL_ERROR 4
#define DUK_VARARGS

#define DUK_INTERNAL_DECL extern

/* Core API declarations */
duk_context *duk_create_heap_default(void);
void duk_destroy_heap(duk_context *ctx);
void duk_push_global_object(duk_context *ctx);
void duk_push_object(duk_context *ctx);
void duk_push_c_function(duk_context *ctx, duk_c_function func, duk_int_t nargs);
void duk_put_prop_string(duk_context *ctx, duk_idx_t obj_index, const char *key);
void duk_pop(duk_context *ctx, duk_idx_t count);
int duk_peval_string(duk_context *ctx, const char *src);
void duk_eval_string_noresult(duk_context *ctx, const char *src);
const char *duk_safe_to_string(duk_context *ctx, duk_idx_t index);
void duk_get_prop_string(duk_context *ctx, duk_idx_t obj_index, const char *key);
int duk_is_function(duk_context *ctx, duk_idx_t index);
int duk_pcall(duk_context *ctx, duk_idx_t nargs);
void duk_push_boolean(duk_context *ctx, duk_int_t value);
void duk_push_number(duk_context *ctx, duk_double_t value);
void duk_push_string(duk_context *ctx, const char *str);
const char *duk_require_string(duk_context *ctx, duk_idx_t index);
void *duk_get_buffer(duk_context *ctx, duk_idx_t index, duk_size_t *out_size);
void *duk_push_fixed_buffer_nozero(duk_context *ctx, duk_size_t size);
void duk_push_pointer(duk_context *ctx, void *ptr);
void duk_error(duk_context *ctx, duk_idx_t err_code, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* DUK_H_INCLUDED */
