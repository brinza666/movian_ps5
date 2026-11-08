/*
 * Duktape PS5 Integration
 * Bridges Duktape JavaScript engine with PS5 native APIs
 * Part of Movian PS5 port
 */

#ifndef DUKTAPE_PS5_H
#define DUKTAPE_PS5_H

#include "duktape.h"

/* Initialize Duktape with PS5 APIs */
int duktape_ps5_init(duk_context **ctx_out);

/* Register PS5 native functions with Duktape */
int duktape_register_ps5_apis(duk_context *ctx);

/* Load Movian JS application */
int duktape_ps5_load_movian(duk_context *ctx);

/* Call JS render loop from native */
int duktape_ps5_call_render(duk_context *ctx);

/* Cleanup Duktape */
void duktape_ps5_cleanup(duk_context *ctx);

#endif /* DUKTAPE_PS5_H */
