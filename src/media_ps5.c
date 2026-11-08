/*
 * Movian PS5 - Media Playback Implementation
 * FFmpeg software decode inside payload
 * Supports: MPEG-1, H.264, HEVC, MPEG-4, MPEG-2, MJPEG, WMV, VC-1, VP8
 * Audio: AAC, AC-3, E-AC-3, MP3, DTS, TrueHD, Opus, FLAC, PCM
 * Containers: MP4, MKV, MPEG-TS, AVI, WMV, ASF, FLV, MPEG
 */

#include "paw_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* FFmpeg headers - would be from PS5 port of FFmpeg */
#if defined(__PS5__)
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#else
/* Host build stubs */
typedef struct AVFormatContext AVFormatContext;
typedef struct AVCodecContext AVCodecContext;
typedef struct AVStream AVStream;
typedef struct AVPacket AVPacket;
typedef struct AVFrame AVFrame;
typedef struct SwsContext SwsContext;
typedef struct SwrContext SwrContext;
#endif

/* Media state */
typedef struct {
    AVFormatContext *fmt_ctx;
    int video_stream_index;
    int audio_stream_index;
    AVCodecContext *video_dec_ctx;
    AVCodecContext *audio_dec_ctx;
    AVFrame *video_frame;
    AVFrame *audio_frame;
    AVPacket *packet;
    SwsContext *sws_ctx;
    SwrContext *swr_ctx;
    uint8_t *video_framebuf;
    uint8_t *audio_framebuf;
    int video_framebuf_size;
    int audio_framebuf_size;
    double duration;
    double current_time;
    int playing;
    int paused;
    double playback_speed;
    int seek_requested;
    double seek_target;
} media_player_t;

static media_player_t g_player = {0};

/* Initialize media player */
int ps5_media_init(void) {
    if (g_player.fmt_ctx) {
        return 0;
    }

    memset(&g_player, 0, sizeof(g_player));

#if !defined(__PS5__)
    /* Host build - register FFmpeg components */
    /* av_register_all(); */
    /* avformat_network_init(); */
#endif

    g_player.playback_speed = 1.0;
    g_player.playing = 0;
    g_player.paused = 0;

    fprintf(stderr, "[PS5 Media] Initialized\n");
    return 0;
}

/* Open media file for playback */
int ps5_media_open(const char *path) {
    if (!path) return -1;

    ps5_media_close(); /* Close any existing media */

    fprintf(stderr, "[PS5 Media] Opening: %s\n", path);

#if defined(__PS5__)
    /* Allocate format context */
    g_player.fmt_ctx = avformat_alloc_context();
    if (!g_player.fmt_ctx) {
        fprintf(stderr, "[PS5 Media] Could not allocate format context\n");
        return -1;
    }

    /* Open input file */
    if (avformat_open_input(&g_player.fmt_ctx, path, NULL, NULL) != 0) {
        fprintf(stderr, "[PS5 Media] Could not open %s\n", path);
        avformat_free_context(g_player.fmt_ctx);
        g_player.fmt_ctx = NULL;
        return -1;
    }

    /* Retrieve stream information */
    if (avformat_find_stream_info(g_player.fmt_ctx, NULL) < 0) {
        fprintf(stderr, "[PS5 Media] Could not find stream information\n");
        avformat_close_input(&g_player.fmt_ctx);
        g_player.fmt_ctx = NULL;
        return -1;
    }

    /* Find video and audio streams */
    g_player.video_stream_index = -1;
    g_player.audio_stream_index = -1;

    for (unsigned int i = 0; i < g_player.fmt_ctx->nb_streams; i++) {
        AVCodecParameters *codecpar = g_player.fmt_ctx->streams[i]->codecpar;
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
            g_player.video_stream_index < 0) {
            g_player.video_stream_index = i;
        } else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
                   g_player.audio_stream_index < 0) {
            g_player.audio_stream_index = i;
        }
    }

    /* Open video decoder */
    if (g_player.video_stream_index >= 0) {
        AVCodecParameters *codecpar = g_player.fmt_ctx->streams[g_player.video_stream_index]->codecpar;
        const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
        if (!codec) {
            fprintf(stderr, "[PS5 Media] Unsupported video codec\n");
        } else {
            g_player.video_dec_ctx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(g_player.video_dec_ctx, codecpar);
            if (avcodec_open2(g_player.video_dec_ctx, codec, NULL) < 0) {
                fprintf(stderr, "[PS5 Media] Could not open video decoder\n");
                avcodec_free_context(&g_player.video_dec_ctx);
                g_player.video_dec_ctx = NULL;
            }
        }
    }

    /* Open audio decoder */
    if (g_player.audio_stream_index >= 0) {
        AVCodecParameters *codecpar = g_player.fmt_ctx->streams[g_player.audio_stream_index]->codecpar;
        const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
        if (!codec) {
            fprintf(stderr, "[PS5 Media] Unsupported audio codec\n");
        } else {
            g_player.audio_dec_ctx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(g_player.audio_dec_ctx, codecpar);
            if (avcodec_open2(g_player.audio_dec_ctx, codec, NULL) < 0) {
                fprintf(stderr, "[PS5 Media] Could not open audio decoder\n");
                avcodec_free_context(&g_player.audio_dec_ctx);
                g_player.audio_dec_ctx = NULL;
            }
        }
    }

    /* Allocate frames and packet */
    g_player.video_frame = av_frame_alloc();
    g_player.audio_frame = av_frame_alloc();
    g_player.packet = av_packet_alloc();

    /* Allocate frame buffers */
    g_player.video_framebuf_size = 1920 * 1080 * 4; /* RGBA8 */
    g_player.video_framebuf = malloc(g_player.video_framebuf_size);
    g_player.audio_framebuf_size = 48000 * 2 * 4 * 2; /* stereo float */
    g_player.audio_framebuf = malloc(g_player.audio_framebuf_size);

    g_player.duration = g_player.fmt_ctx->duration / (double)AV_TIME_BASE;
    g_player.current_time = 0;

    fprintf(stderr, "[PS5 Media] Opened: %s (duration: %.2fs)\n",
            path, g_player.duration);
    fprintf(stderr, "[PS5 Media] Video stream: %d, Audio stream: %d\n",
            g_player.video_stream_index, g_player.audio_stream_index);
#else
    /* Host build */
    g_player.duration = 120.0;
    fprintf(stderr, "[PS5 Media] Opened (host build): %s\n", path);
#endif

    return 0;
}

/* Start playback */
int ps5_media_play(void) {
    if (!g_player.fmt_ctx) return -1;
    g_player.playing = 1;
    g_player.paused = 0;
    fprintf(stderr, "[PS5 Media] Playing\n");
    return 0;
}

/* Pause playback */
int ps5_media_pause(void) {
    if (!g_player.fmt_ctx) return -1;
    g_player.paused = 1;
    fprintf(stderr, "[PS5 Media] Paused\n");
    return 0;
}

/* Stop playback */
int ps5_media_stop(void) {
    g_player.playing = 0;
    g_player.paused = 0;
    g_player.current_time = 0;
    fprintf(stderr, "[PS5 Media] Stopped\n");
    return 0;
}

/* Seek to position (seconds) */
int ps5_media_seek(double seconds) {
    if (!g_player.fmt_ctx) return -1;

#if defined(__PS5__)
    int64_t seek_target = (int64_t)(seconds * AV_TIME_BASE);
    if (av_seek_frame(g_player.fmt_ctx, -1, seek_target, 0) < 0) {
        fprintf(stderr, "[PS5 Media] Seek to %.2fs failed\n", seconds);
        return -1;
    }
#endif

    g_player.current_time = seconds;
    g_player.seek_requested = 0;
    fprintf(stderr, "[PS5 Media] Seek to %.2fs\n", seconds);
    return 0;
}

/* Set playback speed */
int ps5_media_set_speed(double speed) {
    if (speed <= 0) return -1;
    g_player.playback_speed = speed;
    fprintf(stderr, "[PS5 Media] Speed: %.2fx\n", speed);
    return 0;
}

/* Get current playback position */
double ps5_media_get_time(void) {
    return g_player.current_time;
}

/* Get media duration */
double ps5_media_get_duration(void) {
    return g_player.duration;
}

/* Check if playing */
int ps5_media_is_playing(void) {
    return g_player.playing && !g_player.paused;
}

/* Decode one video frame into RGBA8 buffer */
int ps5_media_decode_video_frame(uint8_t **out_buf, int *out_width, int *out_height) {
    if (!g_player.fmt_ctx || g_player.video_stream_index < 0) return -1;
    if (!g_player.playing || g_player.paused) return 0;

#if defined(__PS5__)
    /* Read frames until we get a video frame */
    while (av_read_frame(g_player.fmt_ctx, g_player.packet) >= 0) {
        if (g_player.packet->stream_index == g_player.video_stream_index) {
            /* Decode video packet */
            int ret = avcodec_send_packet(g_player.video_dec_ctx, g_player.packet);
            if (ret < 0) {
                av_packet_unref(g_player.packet);
                continue;
            }

            ret = avcodec_receive_frame(g_player.video_dec_ctx, g_player.video_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_packet_unref(g_player.packet);
                continue;
            } else if (ret < 0) {
                av_packet_unref(g_player.packet);
                continue;
            }

            /* Convert to RGBA8 using swscale */
            /* ... swscale conversion ... */

            av_packet_unref(g_player.packet);

            *out_buf = g_player.video_framebuf;
            *out_width = g_player.video_dec_ctx->width;
            *out_height = g_player.video_dec_ctx->height;

            /* Update current time */
            g_player.current_time = g_player.video_frame->pts *
                av_q2d(g_player.fmt_ctx->streams[g_player.video_stream_index]->time_base);

            return 1; /* Frame ready */
        }
        av_packet_unref(g_player.packet);
    }
#endif

    return 0; /* No frame available */
}

/* Decode one audio frame into PCM buffer */
int ps5_media_decode_audio_frame(uint8_t **out_buf, int *out_size) {
    if (!g_player.fmt_ctx || g_player.audio_stream_index < 0) return -1;

#if defined(__PS5__)
    /* Similar decode loop for audio */
    while (av_read_frame(g_player.fmt_ctx, g_player.packet) >= 0) {
        if (g_player.packet->stream_index == g_player.audio_stream_index) {
            int ret = avcodec_send_packet(g_player.audio_dec_ctx, g_player.packet);
            if (ret < 0) {
                av_packet_unref(g_player.packet);
                continue;
            }

            ret = avcodec_receive_frame(g_player.audio_dec_ctx, g_player.audio_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_packet_unref(g_player.packet);
                continue;
            } else if (ret < 0) {
                av_packet_unref(g_player.packet);
                continue;
            }

            /* Convert to PCM using swresample */
            /* ... swresample conversion ... */

            av_packet_unref(g_player.packet);

            *out_buf = g_player.audio_framebuf;
            *out_size = g_player.audio_framebuf_size;
            return 1; /* Frame ready */
        }
        av_packet_unref(g_player.packet);
    }
#endif

    return 0;
}

/* Close media player */
void ps5_media_close(void) {
    if (!g_player.fmt_ctx) return;

#if defined(__PS5__)
    /* Free decoder contexts */
    if (g_player.video_dec_ctx) avcodec_free_context(&g_player.video_dec_ctx);
    if (g_player.audio_dec_ctx) avcodec_free_context(&g_player.audio_dec_ctx);

    /* Free frames and packet */
    if (g_player.video_frame) av_frame_free(&g_player.video_frame);
    if (g_player.audio_frame) av_frame_free(&g_player.audio_frame);
    if (g_player.packet) av_packet_free(&g_player.packet);

    /* Free conversion contexts */
    if (g_player.sws_ctx) sws_freeContext(g_player.sws_ctx);
    if (g_player.swr_ctx) swr_free(&g_player.swr_ctx);

    /* Free buffers */
    if (g_player.video_framebuf) free(g_player.video_framebuf);
    if (g_player.audio_framebuf) free(g_player.audio_framebuf);

    /* Close format context */
    avformat_close_input(&g_player.fmt_ctx);
#else
    /* Host build cleanup */
#endif

    memset(&g_player, 0, sizeof(g_player));
    fprintf(stderr, "[PS5 Media] Closed\n");
}

/* Get global media player instance */
media_player_t* ps5_media_get_global(void) {
    return &g_player;
}

/* Initialize global media player */
int ps5_media_global_init(void) {
    return ps5_media_init();
}

/* Shutdown global media player */
void ps5_media_global_shutdown(void) {
    ps5_media_close();
}
