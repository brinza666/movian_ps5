/*
 * Movian PS5 - Audio Output Implementation
 * sceAudioOut integration for PS5 native audio
 * PCM audio output via AudioOut API
 */

#include "paw_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__PS5__)
#include <psx/sceAudioOut.h>
#endif

static ps5_audio_t g_audio = {0};

/* Audio configuration */
#define PS5_AUDIO_SAMPLE_RATE 48000
#define PS5_AUDIO_CHANNELS 2
#define PS5_AUDIO_BUFFER_SIZE 4096

/*
 * Open audio output device
 * sceAudioOut for PCM output
 */
int ps5_audio_open(ps5_audio_t *audio) {
    if (!audio) return -1;

    memset(audio, 0, sizeof(*audio));
    audio->sample_rate = PS5_AUDIO_SAMPLE_RATE;
    audio->channels = PS5_AUDIO_CHANNELS;
    audio->format = 0; /* PCM */

#if defined(__PS5__)
    /*
     * Open audio output port
     * sceAudioOutOpen(port, attr, length, type)
     */
    int result = sceAudioOutOpen(
        SCE_AUDIO_OUT_PORT_TYPE_MAIN,
        0,  /* attribute */
        PS5_AUDIO_BUFFER_SIZE,
        SCE_AUDIO_OUT_MODE_STEREO
    );
    if (result < 0) {
        fprintf(stderr, "[PS5 Audio] sceAudioOutOpen failed: 0x%x\n", result);
        return -1;
    }
    audio->handle = (void*)(intptr_t)result;
    fprintf(stderr, "[PS5 Audio] Opened: %u Hz, %u channels\n",
            audio->sample_rate, audio->channels);
#else
    /* Host build - no-op */
    fprintf(stderr, "[PS5 Audio] Opened (host build)\n");
#endif

    return 0;
}

/* Write PCM audio data to output */
int ps5_audio_write(ps5_audio_t *audio, const void *data, uint32_t size) {
    if (!audio || !data || !size) return -1;

#if defined(__PS5__)
    /*
     * Write PCM audio to output port
     * sceAudioOutWrite(port, data, length)
     */
    int result = sceAudioOutWrite(audio->handle, data, size);
    if (result < 0 && result != 0x8001002D) {
        /* 0x8001002D = busy, retry later */
        fprintf(stderr, "[PS5 Audio] sceAudioOutWrite failed: 0x%x\n", result);
        return -1;
    }
#else
    /* Host build - no-op */
#endif

    return 0;
}

/* Close audio output device */
int ps5_audio_close(ps5_audio_t *audio) {
    if (!audio) return -1;

#if defined(__PS5__)
    if (audio->handle) {
        sceAudioOutClose(audio->handle);
        audio->handle = NULL;
    }
#endif

    return 0;
}

/* Get global audio instance */
ps5_audio_t* ps5_audio_get_global(void) {
    return &g_audio;
}

/* Initialize global audio */
int ps5_audio_global_init(void) {
    return ps5_audio_open(&g_audio);
}

/* Shutdown global audio */
void ps5_audio_global_shutdown(void) {
    ps5_audio_close(&g_audio);
}
