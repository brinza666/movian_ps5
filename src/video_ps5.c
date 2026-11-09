/*
 * Movian PS5 - Video Output Implementation
 * sceVideoOut integration for PS5 native display
 * Uses runtime symbol loading (dlopen/dlsym) for robustness
 */

#include "paw_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

/* Function pointer types for sceVideoOut */
typedef int (*sceVideoOutOpen_t)(int, int, int);
typedef int (*sceVideoOutClose_t)(int);
typedef int (*sceVideoOutWaitVblank_t)(int);
typedef int (*sceVideoOutSubmitFlip_t)(int, int);
typedef int (*sceVideoOutRegisterBuffers_t)(int, void**, int, int);
typedef int (*sceVideoOutGetResolutionStatus_t)(int, void*);

/* SceVideoOutState structure */
typedef struct {
    int screen_width;
    int screen_height;
} SceVideoOutState;

#define SCE_VIDEO_OUT_BINDING_MAIN 0

static ps5_video_t g_video = {0};

/* Runtime-loaded function pointers */
static void* g_videoout_handle = NULL;
static sceVideoOutOpen_t g_sceVideoOutOpen = NULL;
static sceVideoOutClose_t g_sceVideoOutClose = NULL;
static sceVideoOutWaitVblank_t g_sceVideoOutWaitVblank = NULL;
static sceVideoOutSubmitFlip_t g_sceVideoOutSubmitFlip = NULL;
static sceVideoOutRegisterBuffers_t g_sceVideoOutRegisterBuffers = NULL;
static sceVideoOutGetResolutionStatus_t g_sceVideoOutGetResolutionStatus = NULL;

/* Video output configuration */
#define PS5_VIDEO_WIDTH  1920
#define PS5_VIDEO_HEIGHT 1080
#define SCE_VIDEO_OUT_PIXEL_FORMAT_RGBA8 0
#define PS5_VIDEO_FORMAT SCE_VIDEO_OUT_PIXEL_FORMAT_RGBA8  /* CPU RGBA8 */

static int load_videoout_library(void) {
    if (g_videoout_handle) return 0;

    g_videoout_handle = dlopen("libSceVideoOut.sprx", RTLD_LAZY);
    if (!g_videoout_handle) {
        fprintf(stderr, "[PS5 Video] Failed to load libSceVideoOut.sprx: %s\n", dlerror());
        return -1;
    }

    g_sceVideoOutOpen = (sceVideoOutOpen_t)dlsym(g_videoout_handle, "sceVideoOutOpen");
    g_sceVideoOutClose = (sceVideoOutClose_t)dlsym(g_videoout_handle, "sceVideoOutClose");
    g_sceVideoOutWaitVblank = (sceVideoOutWaitVblank_t)dlsym(g_videoout_handle, "sceVideoOutWaitVblank");
    g_sceVideoOutSubmitFlip = (sceVideoOutSubmitFlip_t)dlsym(g_videoout_handle, "sceVideoOutSubmitFlip");
    g_sceVideoOutRegisterBuffers = (sceVideoOutRegisterBuffers_t)dlsym(g_videoout_handle, "sceVideoOutRegisterBuffers");
    g_sceVideoOutGetResolutionStatus = (sceVideoOutGetResolutionStatus_t)dlsym(g_videoout_handle, "sceVideoOutGetResolutionStatus");

    if (!g_sceVideoOutOpen || !g_sceVideoOutClose || !g_sceVideoOutWaitVblank ||
        !g_sceVideoOutSubmitFlip || !g_sceVideoOutRegisterBuffers || !g_sceVideoOutGetResolutionStatus) {
        fprintf(stderr, "[PS5 Video] Failed to resolve videoout symbols: %s\n", dlerror());
        dlclose(g_videoout_handle);
        g_videoout_handle = NULL;
        return -1;
    }

    fprintf(stderr, "[PS5 Video] Library loaded and symbols resolved\n");
    return 0;
}

static void unload_videoout_library(void) {
    if (g_videoout_handle) {
        dlclose(g_videoout_handle);
        g_videoout_handle = NULL;
        g_sceVideoOutOpen = NULL;
        g_sceVideoOutClose = NULL;
        g_sceVideoOutWaitVblank = NULL;
        g_sceVideoOutSubmitFlip = NULL;
        g_sceVideoOutRegisterBuffers = NULL;
        g_sceVideoOutGetResolutionStatus = NULL;
    }
}

/*
 * Open video output device
 * sceVideoOutOpen -> register buffers -> flip -> vblank pattern
 */
int ps5_video_open(ps5_video_t *video) {
    if (!video) return -1;

    memset(video, 0, sizeof(*video));
    video->width = PS5_VIDEO_WIDTH;
    video->height = PS5_VIDEO_HEIGHT;
    video->format = PS5_VIDEO_FORMAT;

    if (load_videoout_library() < 0) {
        fprintf(stderr, "[PS5 Video] Failed to load videoout library, running headless\n");
        return -1;
    }

    /* Open video output device */
    int result = g_sceVideoOutOpen(0, SCE_VIDEO_OUT_BINDING_MAIN, 0);
    if (result < 0) {
        fprintf(stderr, "[PS5 Video] sceVideoOutOpen failed: 0x%x\n", result);
        return -1;
    }
    video->handle = result;

    /* Query display resolution */
    SceVideoOutState state;
    memset(&state, 0, sizeof(state));
    result = g_sceVideoOutGetResolutionStatus(video->handle, &state);
    if (result < 0) {
        fprintf(stderr, "[PS5 Video] sceVideoOutGetResolutionStatus failed: 0x%x\n", result);
        g_sceVideoOutClose(video->handle);
        return -1;
    }

    /* Use display resolution if available */
    if (state.screen_width > 0 && state.screen_height > 0) {
        video->width = state.screen_width;
        video->height = state.screen_height;
    }

    /* Calculate pitch for RGBA8 (4 bytes per pixel) */
    video->pitch = video->width * 4;

    /* Allocate framebuffer (double-buffered) */
    uint32_t fb_size = video->pitch * video->height * 2;
    void* fb_ptr = NULL;
    if (posix_memalign(&fb_ptr, 64, fb_size) != 0) {
        fprintf(stderr, "[PS5 Video] Failed to allocate framebuffer\n");
        g_sceVideoOutClose(video->handle);
        return -1;
    }
    video->framebuffer = fb_ptr;

    /* Register both buffers */
    void* buffers[2] = {
        fb_ptr,
        (uint8_t*)fb_ptr + video->pitch * video->height
    };
    result = g_sceVideoOutRegisterBuffers(video->handle, buffers, 2, fb_size / 2);
    if (result < 0) {
        fprintf(stderr, "[PS5 Video] sceVideoOutRegisterBuffers failed: 0x%x\n", result);
        free(fb_ptr);
        g_sceVideoOutClose(video->handle);
        return -1;
    }

    fprintf(stderr, "[PS5 Video] Opened: %ux%u format=RGBA8 pitch=%u\n",
            video->width, video->height, video->pitch);

    return 0;
}

/* Register a framebuffer with the video output */
int ps5_video_register_buffer(ps5_video_t *video, void *buf, uint32_t size) {
    if (!video || video->handle <= 0 || !buf) return -1;

    void* buffers[1] = { buf };
    int result = g_sceVideoOutRegisterBuffers(video->handle, buffers, 1, size);
    if (result < 0) {
        fprintf(stderr, "[PS5 Video] sceVideoOutRegisterBuffers failed: 0x%x\n", result);
        return -1;
    }

    return 0;
}

/* Flip video buffers (present frame) AND wait for vblank */
int ps5_video_flip(ps5_video_t *video) {
    if (!video || video->handle <= 0) return -1;

    /* Present current buffer */
    int result = g_sceVideoOutSubmitFlip(video->handle, 0);
    if (result < 0) {
        /* 0x8001002D = busy, not an error */
        if (result != 0x8001002D) {
            fprintf(stderr, "[PS5 Video] sceVideoOutSubmitFlip failed: 0x%x\n", result);
            return -1;
        }
    }

    /* Wait for vblank to pace frame rate */
    result = g_sceVideoOutWaitVblank(video->handle);
    if (result < 0 && result != 0x8001002D) {
        fprintf(stderr, "[PS5 Video] sceVideoOutWaitVblank failed: 0x%x\n", result);
        return -1;
    }

    return 0;
}

/* Wait for vertical blank (separate call if needed) */
int ps5_video_wait_vblank(ps5_video_t *video) {
    if (!video || video->handle <= 0) return -1;

    int result = g_sceVideoOutWaitVblank(video->handle);
    if (result < 0 && result != 0x8001002D) {
        fprintf(stderr, "[PS5 Video] sceVideoOutWaitVblank failed: 0x%x\n", result);
        return -1;
    }

    return 0;
}

/* Close video output device */
int ps5_video_close(ps5_video_t *video) {
    if (!video) return -1;

    if (video->handle > 0) {
        g_sceVideoOutClose(video->handle);
        video->handle = 0;
    }

    if (video->framebuffer) {
        free(video->framebuffer);
        video->framebuffer = NULL;
    }

    return 0;
}

/* Get framebuffer pointer for rendering */
void* ps5_video_get_framebuffer(ps5_video_t *video) {
    if (!video) return NULL;
    return video->framebuffer;
}

/* Clear framebuffer to a color */
void ps5_video_clear(ps5_video_t *video, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!video || !video->framebuffer) return;

    uint32_t pixel = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    uint32_t *fb = (uint32_t*)video->framebuffer;
    uint32_t count = (video->pitch * video->height) / 4;

    for (uint32_t i = 0; i < count; i++) {
        fb[i] = pixel;
    }
}

/* Draw a rectangle on the framebuffer */
void ps5_video_draw_rect(ps5_video_t *video, int x, int y, int w, int h,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!video || !video->framebuffer) return;

    uint32_t pixel = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;

    for (int row = y; row < y + h && row < (int)video->height; row++) {
        if (row < 0) continue;
        uint32_t *row_ptr = (uint32_t*)((uint8_t*)video->framebuffer + row * video->pitch);
        int start = x < 0 ? 0 : x;
        int end = x + w > (int)video->width ? (int)video->width : x + w;
        for (int col = start; col < end; col++) {
            row_ptr[col] = pixel;
        }
    }
}

/* Global video instance accessor */
ps5_video_t* ps5_video_get_global(void) {
    return &g_video;
}

/* Initialize global video */
int ps5_video_global_init(void) {
    return ps5_video_open(&g_video);
}

/* Shutdown global video */
void ps5_video_global_shutdown(void) {
    ps5_video_close(&g_video);
    unload_videoout_library();
}