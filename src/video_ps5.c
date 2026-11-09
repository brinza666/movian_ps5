/*
 * Movian PS5 - Video Output Implementation
 * sceVideoOut integration for PS5 native display
 * Direct linking with CORRECT symbols, graceful error handling
 */

#include "paw_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* sceVideoOut function declarations - CORRECT symbols from libSceVideoOut.sprx */
extern int sceVideoOutOpen(int, int, int);
extern int sceVideoOutClose(int);
extern int sceVideoOutWaitVblank(int);
extern int sceVideoOutSubmitFlip(int, int);
extern int sceVideoOutRegisterBuffers(int, void**, int, int);
extern int sceVideoOutGetResolutionStatus(int, void*);

/* SceVideoOutState structure */
typedef struct {
    int screen_width;
    int screen_height;
} SceVideoOutState;

#define SCE_VIDEO_OUT_BINDING_MAIN 0

static ps5_video_t g_video = {0};

/* Video output configuration */
#define PS5_VIDEO_WIDTH  1920
#define PS5_VIDEO_HEIGHT 1080
#define SCE_VIDEO_OUT_PIXEL_FORMAT_RGBA8 0
#define PS5_VIDEO_FORMAT SCE_VIDEO_OUT_PIXEL_FORMAT_RGBA8  /* CPU RGBA8 */

/*
 * Open video output device
 * sceVideoOutOpen -> register buffers -> flip -> vblank pattern
 * Returns 0 on success, -1 on failure (non-fatal, runs headless)
 */
int ps5_video_open(ps5_video_t *video) {
    if (!video) return -1;

    memset(video, 0, sizeof(*video));
    video->width = PS5_VIDEO_WIDTH;
    video->height = PS5_VIDEO_HEIGHT;
    video->format = PS5_VIDEO_FORMAT;

    /* Open video output device */
    int result = sceVideoOutOpen(0, SCE_VIDEO_OUT_BINDING_MAIN, 0);
    if (result < 0) {
        fprintf(stderr, "[PS5 Video] sceVideoOutOpen failed: 0x%x (running headless)\n", result);
        return -1;
    }
    video->handle = result;

    /* Query display resolution - try primary function */
    SceVideoOutState state;
    memset(&state, 0, sizeof(state));
    result = sceVideoOutGetResolutionStatus(video->handle, &state);
    if (result < 0) {
        fprintf(stderr, "[PS5 Video] sceVideoOutGetResolutionStatus failed: 0x%x, trying fallback\n", result);
        /* Fallback: use default resolution */
        state.screen_width = PS5_VIDEO_WIDTH;
        state.screen_height = PS5_VIDEO_HEIGHT;
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
        sceVideoOutClose(video->handle);
        return -1;
    }
    video->framebuffer = fb_ptr;

    /* Register both buffers */
    void* buffers[2] = {
        fb_ptr,
        (uint8_t*)fb_ptr + video->pitch * video->height
    };
    result = sceVideoOutRegisterBuffers(video->handle, buffers, 2, fb_size / 2);
    if (result < 0) {
        fprintf(stderr, "[PS5 Video] sceVideoOutRegisterBuffers failed: 0x%x (running headless)\n", result);
        free(fb_ptr);
        sceVideoOutClose(video->handle);
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
    int result = sceVideoOutRegisterBuffers(video->handle, buffers, 1, size);
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
    int result = sceVideoOutSubmitFlip(video->handle, 0);
    if (result < 0) {
        /* 0x8001002D = busy, not an error */
        if (result != 0x8001002D) {
            fprintf(stderr, "[PS5 Video] sceVideoOutSubmitFlip failed: 0x%x\n", result);
            return -1;
        }
    }

    /* Wait for vblank to pace frame rate */
    result = sceVideoOutWaitVblank(video->handle);
    if (result < 0 && result != 0x8001002D) {
        fprintf(stderr, "[PS5 Video] sceVideoOutWaitVblank failed: 0x%x\n", result);
        return -1;
    }

    return 0;
}

/* Wait for vertical blank (separate call if needed) */
int ps5_video_wait_vblank(ps5_video_t *video) {
    if (!video || video->handle <= 0) return -1;

    int result = sceVideoOutWaitVblank(video->handle);
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
        sceVideoOutClose(video->handle);
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

/* Initialize global video - non-fatal, returns 0 even on failure */
int ps5_video_global_init(void) {
    int result = ps5_video_open(&g_video);
    if (result < 0) {
        fprintf(stderr, "[PS5 Video] Initialization failed, running headless\n");
        /* Zero out the video struct for headless mode */
        memset(&g_video, 0, sizeof(g_video));
        g_video.width = PS5_VIDEO_WIDTH;
        g_video.height = PS5_VIDEO_HEIGHT;
        g_video.format = PS5_VIDEO_FORMAT;
        g_video.pitch = PS5_VIDEO_WIDTH * 4;
        return 0;  /* Non-fatal */
    }
    return 0;
}

/* Shutdown global video */
void ps5_video_global_shutdown(void) {
    ps5_video_close(&g_video);
}