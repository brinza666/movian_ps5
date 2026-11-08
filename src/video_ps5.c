/*
 * Movian PS5 - Video Output Implementation
 * sceVideoOut integration for PS5 native display
 * Supports CPU RGBA8 framebuffer rendering
 * Firmware 6.02 and 12.70 video modes
 */

#include "paw_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__PS5__)
#include <psx/sceVideoOut.h>
#include <psx/sceDisplay.h>
#endif

static ps5_video_t g_video = {0};

/* Video output configuration */
#define PS5_VIDEO_WIDTH  1920
#define PS5_VIDEO_HEIGHT 1080
#define SCE_VIDEO_OUT_PIXEL_FORMAT_RGBA8 0
#define PS5_VIDEO_FORMAT SCE_VIDEO_OUT_PIXEL_FORMAT_RGBA8  /* CPU RGBA8 */

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

#if defined(__PS5__)
    /* Open video output device */
    int result = sceVideoOutOpen(0, SCE_VIDEO_OUT_BINDING_MAIN, 0);
    if (result < 0) {
        fprintf(stderr, "[PS5 Video] sceVideoOutOpen failed: 0x%x\n", result);
        return -1;
    }
    video->handle = (void*)(intptr_t)result;

    /* Query display capabilities */
    SceVideoOutState state;
    memset(&state, 0, sizeof(state));
    result = sceVideoOutGetState(video->handle, &state);
    if (result < 0) {
        fprintf(stderr, "[PS5 Video] sceVideoOutGetState failed: 0x%x\n", result);
        sceVideoOutClose(video->handle);
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
    video->framebuffer = memalign(64, fb_size);
    if (!video->framebuffer) {
        fprintf(stderr, "[PS5 Video] Failed to allocate framebuffer\n");
        sceVideoOutClose(video->handle);
        return -1;
    }

    /* Register both buffers */
    for (int i = 0; i < 2; i++) {
        void *buf = (uint8_t*)video->framebuffer + (i * video->pitch * video->height);
        result = sceVideoOutRegisterBuffer(video->handle, buf, fb_size / 2);
        if (result < 0) {
            fprintf(stderr, "[PS5 Video] sceVideoOutRegisterBuffer[%d] failed: 0x%x\n", i, result);
        }
    }

    fprintf(stderr, "[PS5 Video] Opened: %ux%u format=RGBA8 pitch=%u\n",
            video->width, video->height, video->pitch);
#else
    /* Host build - allocate dummy framebuffer */
    video->framebuffer = malloc(video->pitch * video->height);
    if (!video->framebuffer) return -1;
    memset(video->framebuffer, 0, video->pitch * video->height);
#endif

    return 0;
}

/* Register a framebuffer with the video output */
int ps5_video_register_buffer(ps5_video_t *video, void *buf, uint32_t size) {
    if (!video || !buf) return -1;

#if defined(__PS5__)
    int result = sceVideoOutRegisterBuffer(video->handle, buf, size);
    if (result < 0) {
        fprintf(stderr, "[PS5 Video] sceVideoOutRegisterBuffer failed: 0x%x\n", result);
        return -1;
    }
#endif

    return 0;
}

/* Flip video buffers (present frame) */
int ps5_video_flip(ps5_video_t *video) {
    if (!video || !video->handle) return -1;

#if defined(__PS5__)
    /* Present current buffer */
    int result = sceVideoOutFlip(video->handle, 0);
    if (result < 0) {
        /* 0x8001002D = busy, not an error */
        if (result != 0x8001002D) {
            fprintf(stderr, "[PS5 Video] sceVideoOutFlip failed: 0x%x\n", result);
            return -1;
        }
    }

    /* Swap front/back buffer pointers */
    /* The next draw goes to the buffer not currently being displayed */
#else
    /* Host build - simulate flip */
#endif

    return 0;
}

/* Wait for vertical blank */
int ps5_video_wait_vblank(ps5_video_t *video) {
    if (!video) return -1;

#if defined(__PS5__)
    int result = sceVideoOutWaitVblank(video->handle);
    if (result < 0 && result != 0x8001002D) {
        fprintf(stderr, "[PS5 Video] sceVideoOutWaitVblank failed: 0x%x\n", result);
        return -1;
    }
#endif

    return 0;
}

/* Close video output device */
int ps5_video_close(ps5_video_t *video) {
    if (!video) return -1;

#if defined(__PS5__)
    if (video->handle) {
        sceVideoOutClose(video->handle);
        video->handle = NULL;
    }
#endif

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
}
