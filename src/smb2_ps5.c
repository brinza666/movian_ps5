/*
 * Movian PS5 - SMB2 Network Implementation
 * SMB2 network share access using libm7.so functions
 * Analyzed from libm7.so exports (897 functions via REA)
 */

#include "paw_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SMB2 function declarations from libm7.so (REA-verified exports) */
extern int smb2_init_context(void);
extern int smb2_connect_share(void *ctx, const char *host, const char *share);
extern int smb2_listdir(void *share_ctx, const char *path, char ***entries, int *count);
extern int smb2_open(void *ctx, const char *path);
extern int smb2_read(void *fd, void *buf, size_t size);
extern int smb2_close(void *fd);

/* SMB2 context state */
typedef struct {
    void *ctx;
    void *share_ctx;
    int initialized;
    int connected;
    char host[256];
    char share[256];
} smb2_state_t;

static smb2_state_t g_smb2 = {0};

/* Initialize SMB2 library */
int ps5_net_smb2_init(void) {
    if (g_smb2.initialized) {
        return 0;
    }

    fprintf(stderr, "[PS5 SMB2] Initializing SMB2 context...\n");

    /* Call smb2_init_context from libm7.so */
    int result = smb2_init_context();
    if (result < 0) {
        fprintf(stderr, "[PS5 SMB2] smb2_init_context failed: %d\n", result);
        return -1;
    }

    g_smb2.initialized = 1;
    fprintf(stderr, "[PS5 SMB2] Initialized\n");
    return 0;
}

/* Connect to SMB2 share */
int ps5_net_smb2_connect(const char *host, const char *share,
                          const char *user, const char *pass) {
    if (!host || !share) return -1;

    if (!g_smb2.initialized) {
        if (ps5_net_smb2_init() != 0) {
            return -1;
        }
    }

    /* If already connected, disconnect first */
    if (g_smb2.connected) {
        fprintf(stderr, "[PS5 SMB2] Already connected, disconnecting...\n");
        g_smb2.connected = 0;
        g_smb2.share_ctx = NULL;
    }

    fprintf(stderr, "[PS5 SMB2] Connecting to %s/%s...\n", host, share);

    /* Call smb2_connect_share from libm7.so */
    int result = smb2_connect_share(g_smb2.ctx, host, share);
    if (result < 0) {
        fprintf(stderr, "[PS5 SMB2] smb2_connect_share failed: %d\n", result);
        return -1;
    }

    /* Store connection info */
    strncpy(g_smb2.host, host, sizeof(g_smb2.host) - 1);
    strncpy(g_smb2.share, share, sizeof(g_smb2.share) - 1);
    g_smb2.connected = 1;

    fprintf(stderr, "[PS5 SMB2] Connected to %s/%s\n", host, share);
    return 0;
}

/* List directory on SMB2 share */
int ps5_net_smb2_listdir(const char *share, const char *path,
                          char ***entries, int *count) {
    if (!share || !path || !entries || !count) return -1;

    if (!g_smb2.connected) {
        fprintf(stderr, "[PS5 SMB2] Not connected\n");
        return -1;
    }

    /* Call smb2_listdir from libm7.so */
    int result = smb2_listdir(g_smb2.share_ctx, path, entries, count);
    if (result < 0) {
        fprintf(stderr, "[PS5 SMB2] smb2_listdir failed: %d\n", result);
        return -1;
    }

    return 0;
}

/* Open file on SMB2 share */
int ps5_net_smb2_open(const char *path) {
    if (!path || !g_smb2.connected) return -1;

    /* Call smb2_open from libm7.so */
    int fd = smb2_open(g_smb2.share_ctx, path);
    if (fd < 0) {
        fprintf(stderr, "[PS5 SMB2] smb2_open %s failed: %d\n", path, fd);
        return -1;
    }

    return fd;
}

/* Read from SMB2 file */
int ps5_net_smb2_read(int fd, void *buf, size_t size) {
    if (!buf || size == 0) return -1;

    /* Call smb2_read from libm7.so */
    int result = smb2_read(fd, buf, size);
    if (result < 0) {
        fprintf(stderr, "[PS5 SMB2] smb2_read failed: %d\n", result);
        return -1;
    }

    return result;
}

/* Close SMB2 file */
int ps5_net_smb2_close(int fd) {
    /* Call smb2_close from libm7.so */
    int result = smb2_close(fd);
    if (result < 0) {
        fprintf(stderr, "[PS5 SMB2] smb2_close failed: %d\n", result);
        return -1;
    }

    return 0;
}

/* Disconnect from SMB2 share */
void ps5_net_smb2_disconnect(void) {
    if (g_smb2.connected) {
        fprintf(stderr, "[PS5 SMB2] Disconnecting from %s/%s\n",
                g_smb2.host, g_smb2.share);
        g_smb2.connected = 0;
        g_smb2.share_ctx = NULL;
    }
}

/* Check if SMB2 is connected */
int ps5_net_smb2_is_connected(void) {
    return g_smb2.connected;
}

/* Shutdown SMB2 */
void ps5_net_smb2_shutdown(void) {
    ps5_net_smb2_disconnect();
    g_smb2.initialized = 0;
    g_smb2.ctx = NULL;
}
