/*
 * Movian for PS5 — software FFmpeg path (no libSceVideodec2).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "avdec.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef MOVIAN_HAVE_FFMPEG
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
}
#endif

namespace movian::avdec
{
namespace
{
#ifdef MOVIAN_HAVE_FFMPEG
AVFormatContext *g_fmt = nullptr;
AVCodecContext *g_vcodec = nullptr;
AVCodecContext *g_acodec = nullptr;
AVPacket *g_pkt = nullptr;
AVFrame *g_vframe = nullptr;
AVFrame *g_aframe = nullptr;
std::uint32_t *g_rgba = nullptr;
int g_w = 0;
int g_h = 0;
int g_vindex = -1;
int g_aindex = -1;
int g_rate = 0;
bool g_ended = false;
double g_clock = 0;
double g_pts = 0;
double g_duration = 0;
AVRational g_tb{1, 1};
PcmFn g_pcm = nullptr;
float *g_pcm_buf = nullptr;
int g_pcm_cap = 0;

void copy_text(char *dst, int cap, const char *src) noexcept
{
    if (!dst || cap <= 0)
        return;
    std::snprintf(dst, static_cast<std::size_t>(cap), "%s", src ? src : "");
}

int clip8(int v) noexcept
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

void yuv420_to_rgba(const AVFrame *f) noexcept
{
    if (!f || !g_rgba || g_w <= 0 || g_h <= 0)
        return;
    const int sw = f->width;
    const int sh = f->height;
    if (sw < 2 || sh < 2)
        return;
    int stepx = sw / g_w;
    int stepy = sh / g_h;
    if (stepx < 1)
        stepx = 1;
    if (stepy < 1)
        stepy = 1;
    const bool jpeg = f->format == AV_PIX_FMT_YUVJ420P;
    for (int y = 0; y < g_h; ++y)
    {
        const int sy = y * stepy;
        const std::uint8_t *yp = f->data[0] + sy * f->linesize[0];
        const std::uint8_t *up = f->data[1] + (sy / 2) * f->linesize[1];
        const std::uint8_t *vp = f->data[2] + (sy / 2) * f->linesize[2];
        std::uint32_t *dst = g_rgba + y * g_w;
        for (int x = 0; x < g_w; ++x)
        {
            const int sx = x * stepx;
            const int Y = yp[sx];
            const int U = up[sx / 2];
            const int V = vp[sx / 2];
            int r, g, b;
            if (jpeg)
            {
                const int d = U - 128;
                const int e = V - 128;
                r = (256 * Y + 359 * e) >> 8;
                g = (256 * Y - 88 * d - 183 * e) >> 8;
                b = (256 * Y + 454 * d) >> 8;
            }
            else
            {
                const int c = Y - 16;
                const int d = U - 128;
                const int e = V - 128;
                r = (298 * c + 409 * e + 128) >> 8;
                g = (298 * c - 100 * d - 208 * e + 128) >> 8;
                b = (298 * c + 516 * d + 128) >> 8;
            }
            dst[x] = 0xff000000u | (static_cast<std::uint32_t>(clip8(b)) << 16) |
                     (static_cast<std::uint32_t>(clip8(g)) << 8) |
                     static_cast<std::uint32_t>(clip8(r));
        }
    }
}

float sample_at(const AVFrame *f, int ch, int i) noexcept
{
    const int nch = f->ch_layout.nb_channels > 0 ? f->ch_layout.nb_channels : 1;
    if (ch < 0 || ch >= nch)
        return 0;
    if (f->format == AV_SAMPLE_FMT_FLTP)
        return reinterpret_cast<const float *>(f->extended_data[ch])[i];
    if (f->format == AV_SAMPLE_FMT_FLT)
        return reinterpret_cast<const float *>(f->extended_data[0])[i * nch + ch];
    if (f->format == AV_SAMPLE_FMT_S16P)
        return static_cast<float>(reinterpret_cast<const std::int16_t *>(f->extended_data[ch])[i]) /
               32768.f;
    if (f->format == AV_SAMPLE_FMT_S16)
        return static_cast<float>(
                   reinterpret_cast<const std::int16_t *>(f->extended_data[0])[i * nch + ch]) /
               32768.f;
    return 0;
}

void emit_audio() noexcept
{
    if (!g_acodec || !g_aframe || !g_pcm)
        return;
    for (;;)
    {
        const int r = avcodec_receive_frame(g_acodec, g_aframe);
        if (r)
            return;
        const int n = g_aframe->nb_samples;
        const int ch = g_aframe->ch_layout.nb_channels > 0 ? g_aframe->ch_layout.nb_channels : 1;
        if (n > g_pcm_cap)
        {
            auto *p = static_cast<float *>(std::realloc(g_pcm_buf, static_cast<std::size_t>(n) * 2 *
                                                                       sizeof(float)));
            if (!p)
            {
                av_frame_unref(g_aframe);
                return;
            }
            g_pcm_buf = p;
            g_pcm_cap = n;
        }
        for (int i = 0; i < n; ++i)
        {
            float L = sample_at(g_aframe, 0, i);
            float R = ch > 1 ? sample_at(g_aframe, 1, i) : L;
            if (ch > 2)
            {
                const float c = sample_at(g_aframe, 2, i) * 0.707f;
                L += c;
                R += c;
            }
            g_pcm_buf[i * 2] = L > 1.f ? 1.f : L < -1.f ? -1.f : L;
            g_pcm_buf[i * 2 + 1] = R > 1.f ? 1.f : R < -1.f ? -1.f : R;
        }
        g_rate = g_aframe->sample_rate > 0 ? g_aframe->sample_rate : 48000;
        g_pcm(g_pcm_buf, n, g_rate);
        av_frame_unref(g_aframe);
    }
}

int open_codec(AVCodecContext **ctx, int index) noexcept
{
    const AVStream *st = g_fmt->streams[index];
    const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec)
        return -1;
    *ctx = avcodec_alloc_context3(codec);
    if (!*ctx)
        return -1;
    if (avcodec_parameters_to_context(*ctx, st->codecpar) < 0)
        return -1;
    (*ctx)->thread_count = 1;
    if (avcodec_open2(*ctx, codec, nullptr) < 0)
        return -1;
    return 0;
}

int pull_one() noexcept
{
    if (!g_fmt || !g_pkt)
        return -1;
    if (g_ended)
        return -1;
    const int rr = av_read_frame(g_fmt, g_pkt);
    if (rr < 0)
    {
        g_ended = true;
        return -1;
    }
    if (g_pkt->stream_index == g_aindex && g_acodec)
    {
        if (avcodec_send_packet(g_acodec, g_pkt) == 0)
            emit_audio();
        av_packet_unref(g_pkt);
        return 0;
    }
    if (g_pkt->stream_index != g_vindex)
    {
        av_packet_unref(g_pkt);
        return 0;
    }
    if (avcodec_send_packet(g_vcodec, g_pkt) == 0)
    {
        while (avcodec_receive_frame(g_vcodec, g_vframe) == 0)
        {
            int64_t p = g_vframe->best_effort_timestamp;
            if (p == AV_NOPTS_VALUE)
                p = g_vframe->pts;
            if (p != AV_NOPTS_VALUE)
                g_pts = av_q2d(g_tb) * static_cast<double>(p);
            else
                g_pts += 1.0 / 30.0;
            if (g_vframe->format == AV_PIX_FMT_YUV420P || g_vframe->format == AV_PIX_FMT_YUVJ420P)
                yuv420_to_rgba(g_vframe);
            av_frame_unref(g_vframe);
            av_packet_unref(g_pkt);
            return 1;
        }
    }
    av_packet_unref(g_pkt);
    return 0;
}
#endif
} // namespace

bool available() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return true;
#else
    return false;
#endif
}

bool open(const char *path, char *why, int why_cap, PcmFn pcm) noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    close();
    if (!path || !path[0])
    {
        copy_text(why, why_cap, "empty path");
        return false;
    }
    if (avformat_open_input(&g_fmt, path, nullptr, nullptr) < 0)
    {
        copy_text(why, why_cap, "FFmpeg could not open the file");
        close();
        return false;
    }
    if (avformat_find_stream_info(g_fmt, nullptr) < 0)
    {
        copy_text(why, why_cap, "no streams");
        close();
        return false;
    }
    g_vindex = av_find_best_stream(g_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    g_aindex = av_find_best_stream(g_fmt, AVMEDIA_TYPE_AUDIO, -1, g_vindex, nullptr, 0);
    if (g_vindex < 0)
    {
        copy_text(why, why_cap, "no video stream");
        close();
        return false;
    }
    if (open_codec(&g_vcodec, g_vindex) != 0)
    {
        copy_text(why, why_cap, "no software decoder for this video");
        close();
        return false;
    }
    if (g_aindex >= 0 && open_codec(&g_acodec, g_aindex) != 0)
        g_aindex = -1;
    g_w = g_vcodec->width;
    g_h = g_vcodec->height;
    if (g_w <= 0 || g_h <= 0)
    {
        copy_text(why, why_cap, "bad video size");
        close();
        return false;
    }
    if (g_w > 1920)
        g_w = 1920;
    if (g_h > 1080)
        g_h = 1080;
    g_rgba = static_cast<std::uint32_t *>(
        std::calloc(static_cast<std::size_t>(g_w) * static_cast<std::size_t>(g_h), sizeof(std::uint32_t)));
    g_pkt = av_packet_alloc();
    g_vframe = av_frame_alloc();
    g_aframe = av_frame_alloc();
    if (!g_rgba || !g_pkt || !g_vframe)
    {
        copy_text(why, why_cap, "out of memory");
        close();
        return false;
    }
    g_tb = g_fmt->streams[g_vindex]->time_base;
    g_duration = g_fmt->duration > 0 ? static_cast<double>(g_fmt->duration) / AV_TIME_BASE : 0;
    g_pcm = pcm;
    g_clock = 0;
    g_pts = 0;
    g_ended = false;
    return true;
#else
    (void)path;
    (void)pcm;
    if (why && why_cap > 0)
        std::snprintf(why, static_cast<std::size_t>(why_cap), "FFmpeg not linked in this binary");
    return false;
#endif
}

void close() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    if (g_aframe)
        av_frame_free(&g_aframe);
    if (g_vframe)
        av_frame_free(&g_vframe);
    if (g_pkt)
        av_packet_free(&g_pkt);
    if (g_acodec)
        avcodec_free_context(&g_acodec);
    if (g_vcodec)
        avcodec_free_context(&g_vcodec);
    if (g_fmt)
        avformat_close_input(&g_fmt);
    std::free(g_rgba);
    std::free(g_pcm_buf);
    g_rgba = nullptr;
    g_pcm_buf = nullptr;
    g_pcm_cap = 0;
    g_w = g_h = 0;
    g_vindex = g_aindex = -1;
    g_ended = false;
    g_clock = g_pts = g_duration = 0;
    g_pcm = nullptr;
#endif
}

bool active() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return g_fmt != nullptr;
#else
    return false;
#endif
}

bool ended() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return g_fmt && g_ended;
#else
    return false;
#endif
}

int width() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return g_w;
#else
    return 0;
#endif
}

int height() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return g_h;
#else
    return 0;
#endif
}

double time() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return g_pts;
#else
    return 0;
#endif
}

double duration() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return g_duration;
#else
    return 0;
#endif
}

const std::uint32_t *rgba() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return g_rgba;
#else
    return nullptr;
#endif
}

bool tick(double dt) noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    if (!g_fmt)
        return false;
    if (dt < 0)
        dt = 0;
    if (dt > 0.25)
        dt = 0.25;
    g_clock += dt;
    int n = 0;
    while (g_pts + 0.0005 < g_clock && n < 8 && !g_ended)
    {
        const int r = pull_one();
        if (r < 0)
            break;
        if (r == 1)
            ++n;
    }
    return true;
#else
    (void)dt;
    return false;
#endif
}

void seek(double seconds) noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    if (!g_fmt)
        return;
    if (seconds < 0)
        seconds = 0;
    const int64_t ts = static_cast<int64_t>(seconds * static_cast<double>(AV_TIME_BASE));
    if (av_seek_frame(g_fmt, -1, ts, AVSEEK_FLAG_BACKWARD) < 0)
        return;
    if (g_vcodec)
        avcodec_flush_buffers(g_vcodec);
    if (g_acodec)
        avcodec_flush_buffers(g_acodec);
    g_ended = false;
    g_clock = seconds;
    g_pts = seconds;
    pull_one();
#else
    (void)seconds;
#endif
}
} // namespace movian::avdec
