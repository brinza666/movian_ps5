/*
 * Movian Core Implementation
 * Wraps libm7.so functions for PS5 native port
 * Replaces Android JNI bridge
 */

#include "movian_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_core_initialized = 0;

int movian_core_init(void) {
    if (g_core_initialized) {
        return 0;
    }
    
    fprintf(stderr, "[Movian Core] Initializing...\n");
    
    /*
     * Initialize Movian core components:
     * - Duktape JS engine
     * - FreeType font engine
     * - SMB2 network
     * - Crypto
     *
     * On Android this was done via JNI_OnLoad.
     * On PS5 we do it here in native main().
     */
    
    /* Initialize SMB2 context */
    /* smb2_init_context(); */
    
    g_core_initialized = 1;
    fprintf(stderr, "[Movian Core] Initialized\n");
    return 0;
}

void movian_core_shutdown(void) {
    if (!g_core_initialized) {
        return;
    }
    
    fprintf(stderr, "[Movian Core] Shutting down...\n");
    g_core_initialized = 0;
}
