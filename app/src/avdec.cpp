/*
 * Movian for PS5 — FFmpeg demux + software H.264, or libSceVideodec2.
 *
 * HEVC follows VLC (ps5/modules/ps5vdec.c): the console chip, never a
 * software decoder. Software HEVC crashed this title on the TV.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "avdec.hpp"
#include "audio.hpp"

#include "hwdec.h"

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>

#ifdef MOVIAN_HAVE_FFMPEG
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
int sceKernelMapFlexibleMemory(void **address, std::size_t length, int protection, int flags);
int sceKernelMunmap(void *address, std::size_t length);
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
std::uint32_t *g_front = nullptr;
std::uint32_t *g_back = nullptr;
void *g_picture_mapping = nullptr;
std::size_t g_picture_mapping_size = 0;
hwdec_t *g_hw = nullptr;
AVBSFContext *g_video_bsf = nullptr;
int g_hw_demux_result = 0;
bool g_sw_demux_eof = false;
int g_w = 0;
int g_h = 0;
int g_vindex = -1;
int g_aindex = -1;
SwrContext *g_resampler = nullptr;
AVChannelLayout g_pcm_input_layout{};
int g_pcm_input_rate = 0;
AVSampleFormat g_pcm_input_format = AV_SAMPLE_FMT_NONE;
bool g_pcm_reported = false;
unsigned long long g_pcm_frames = 0;
bool g_ended = false;
unsigned g_hw_presented = 0;
double g_clock = 0;
double g_pts = 0;
double g_duration = 0;
AVRational g_tb{1, 1};
PcmFn g_pcm = nullptr;
float *g_pcm_buf = nullptr;
int g_pcm_cap = 0;
char g_codec[24] = "";
char g_mode[8] = "SW";
int pull_one() noexcept;
void record_decode(const char *event, int value_a, int value_b, unsigned frames) noexcept
{
    if (auto *log = std::fopen("/download0/hevc-test-decode.log", "a"))
    {
        std::fprintf(log, "v15 %s codec=%s a=%d b=%d frames=%u\n", event, g_codec, value_a, value_b, frames);
        std::fclose(log);
    }
}

void log_ffmpeg(void *, int level, const char *format, std::va_list arguments) noexcept
{
    if (level > AV_LOG_WARNING)
        return;
    char message[512];
    std::vsnprintf(message, sizeof message, format, arguments);
    if (auto *log = std::fopen("/download0/hevc-test-ffmpeg.log", "a"))
    {
        std::fputs(message, log);
        std::fclose(log);
    }
}

const AVInputFormat *mpeg_program_stream(const char *path) noexcept
{
    const char *ext = std::strrchr(path, '.');
    if (!ext || (strcasecmp(ext, ".mpg") != 0 && strcasecmp(ext, ".mpeg") != 0))
        return nullptr;

    // Only override autodetection when the file starts with an MPEG pack.
    auto *file = std::fopen(path, "rb");
    if (!file)
        return nullptr;
    unsigned char header[4]{};
    const auto size = std::fread(header, 1, sizeof header, file);
    std::fclose(file);
    return size == sizeof header && header[0] == 0 && header[1] == 0 &&
                   header[2] == 1 && header[3] == 0xba
               ? av_find_input_format("mpeg")
               : nullptr;
}

void present() noexcept
{
    if (g_hw && ++g_hw_presented == 1)
        record_decode("first hardware frame", hwdec_shown_width(g_hw),
                      hwdec_shown_height(g_hw), g_hw_presented);
    std::uint32_t *t = g_front;
    g_front = g_back;
    g_back = t;
    if (g_hw && g_back)
        hwdec_bind(g_hw, reinterpret_cast<std::uint8_t *>(g_back), g_w, g_h);
}
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_t g_thread{};
bool g_thread_on = false;
std::atomic<bool> g_run{false};

void *decode_loop(void *) noexcept
{
    while (g_run.load(std::memory_order_relaxed))
    {
        int r = 0;
        pthread_mutex_lock(&g_lock);
        if (g_fmt && !g_ended && g_pts + 0.0005 < g_clock)
            r = pull_one();
        pthread_mutex_unlock(&g_lock);
        if (r != 1)
            usleep(2000);
    }
    return nullptr;
}

void stop_thread() noexcept
{
    g_run.store(false, std::memory_order_relaxed);
    if (g_thread_on)
    {
        pthread_join(g_thread, nullptr);
        g_thread_on = false;
    }
}

bool start_thread() noexcept
{
    stop_thread();
    g_run.store(true, std::memory_order_relaxed);
    if (pthread_create(&g_thread, nullptr, decode_loop, nullptr) != 0)
    {
        g_run.store(false, std::memory_order_relaxed);
        return false;
    }
    g_thread_on = true;
    return true;
}

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
    if (!f || !g_back || g_w <= 0 || g_h <= 0)
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
        std::uint32_t *dst = g_back + y * g_w;
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

void reset_resampler() noexcept
{
    swr_free(&g_resampler);
    av_channel_layout_uninit(&g_pcm_input_layout);
    g_pcm_input_rate = 0;
    g_pcm_input_format = AV_SAMPLE_FMT_NONE;
}

bool prepare_resampler(const AVFrame *frame) noexcept
{
    AVChannelLayout input{};
    if (frame->ch_layout.nb_channels <= 0 || frame->sample_rate <= 0)
        return false;
    if (frame->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        av_channel_layout_default(&input, frame->ch_layout.nb_channels);
    else if (av_channel_layout_copy(&input, &frame->ch_layout) < 0)
        return false;

    const auto format = static_cast<AVSampleFormat>(frame->format);
    const int rate = frame->sample_rate;
    if (g_resampler && rate == g_pcm_input_rate && format == g_pcm_input_format &&
        av_channel_layout_compare(&input, &g_pcm_input_layout) == 0)
    {
        av_channel_layout_uninit(&input);
        return true;
    }

    reset_resampler();
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    const int result = swr_alloc_set_opts2(&g_resampler, &stereo, AV_SAMPLE_FMT_FLT,
                                           audio::rate, &input, format, rate, 0, nullptr);
    if (result < 0 || swr_init(g_resampler) < 0)
    {
        av_channel_layout_uninit(&input);
        reset_resampler();
        return false;
    }
    g_pcm_input_layout = input;
    g_pcm_input_rate = rate;
    g_pcm_input_format = format;
    return true;
}

void convert_pcm(const AVFrame *frame) noexcept
{
    const int input_samples = frame ? frame->nb_samples : 0;
    const int capacity = swr_get_out_samples(g_resampler, input_samples);
    if (capacity <= 0)
        return;
    if (capacity > g_pcm_cap)
    {
        auto *buffer = static_cast<float *>(
            std::realloc(g_pcm_buf, static_cast<std::size_t>(capacity) * 2 * sizeof(float)));
        if (!buffer)
            return;
        g_pcm_buf = buffer;
        g_pcm_cap = capacity;
    }
    auto *out = reinterpret_cast<std::uint8_t *>(g_pcm_buf);
    auto **in = frame ? const_cast<const std::uint8_t **>(frame->extended_data) : nullptr;
    const int written = swr_convert(g_resampler, &out, g_pcm_cap, in, input_samples);
    if (written <= 0)
        return;
    if (!g_pcm_reported)
    {
        record_decode(g_acodec->codec_id == AV_CODEC_ID_EAC3 ? "E-AC-3 stereo PCM" : "stereo PCM",
                      g_pcm_input_layout.nb_channels, audio::rate, static_cast<unsigned>(written));
        g_pcm_reported = true;
    }
    g_pcm(g_pcm_buf, written, audio::rate);
    g_pcm_frames += written;
}

void emit_audio() noexcept
{
    if (!g_acodec || !g_aframe || !g_pcm)
        return;
    while (avcodec_receive_frame(g_acodec, g_aframe) == 0)
    {
        if (prepare_resampler(g_aframe))
            convert_pcm(g_aframe);
        av_frame_unref(g_aframe);
    }
}

void finish_audio() noexcept
{
    if (!g_acodec || !g_pcm)
        return;
    if (avcodec_send_packet(g_acodec, nullptr) == 0)
        emit_audio();
    if (g_resampler)
        convert_pcm(nullptr);
    if (auto *log = std::fopen("/download0/hevc-test-decode.log", "a"))
    {
        std::fprintf(log, "v15 %s PCM EOF codec=%s frames=%llu\n",
                     g_acodec->codec_id == AV_CODEC_ID_EAC3 ? "E-AC-3" : "audio",
                     g_codec, g_pcm_frames);
        std::fclose(log);
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

int end_hardware(int result) noexcept
{
    g_ended = true;
    record_decode("hardware drain", g_hw_demux_result, result, g_hw_presented);
    return -1;
}

int show_hardware_frame(double step) noexcept
{
    const int64_t shown = hwdec_shown_pts(g_hw);
    g_pts = shown >= 0 ? static_cast<double>(shown) / 1e6 : g_pts + step;
    present();
    return 1;
}

int hw_pull() noexcept
{
    for (int packets = 0; packets < 64; ++packets)
    {
        const int filtered = av_bsf_receive_packet(g_video_bsf, g_pkt);
        if (filtered == AVERROR_EOF)
        {
            const int drained = hwdec_flush(g_hw);
            return drained > 0 ? show_hardware_frame(1.0 / 30.0) : end_hardware(drained);
        }
        if (filtered == 0)
        {
            const int64_t ts = g_pkt->pts != AV_NOPTS_VALUE ? g_pkt->pts : g_pkt->dts;
            const int64_t pts_us = ts == AV_NOPTS_VALUE ? -1 : av_rescale_q(ts, g_tb, AV_TIME_BASE_Q);
            const double step = g_pkt->duration > 0 ? av_q2d(g_tb) * g_pkt->duration : 1.0 / 30.0;
            const int decoded = hwdec_feed(g_hw, g_pkt->data, g_pkt->size, pts_us);
            av_packet_unref(g_pkt);
            if (decoded < 0)
                return end_hardware(decoded);
            if (decoded > 0)
                return show_hardware_frame(step);
            continue;
        }
        if (filtered != AVERROR(EAGAIN) || g_hw_demux_result == AVERROR_EOF)
            return end_hardware(filtered);

        g_hw_demux_result = av_read_frame(g_fmt, g_pkt);
        if (g_hw_demux_result == AVERROR_EOF)
        {
            finish_audio();
            const int flushed = av_bsf_send_packet(g_video_bsf, nullptr);
            if (flushed < 0)
                return end_hardware(flushed);
            continue;
        }
        if (g_hw_demux_result == AVERROR(EAGAIN))
            return 0;
        if (g_hw_demux_result < 0)
            return end_hardware(g_hw_demux_result);

        if (g_pkt->stream_index == g_vindex)
        {
            const int sent = av_bsf_send_packet(g_video_bsf, g_pkt);
            av_packet_unref(g_pkt);
            if (sent < 0)
                return end_hardware(sent);
        }
        else
        {
            if (g_pkt->stream_index == g_aindex && g_acodec &&
                avcodec_send_packet(g_acodec, g_pkt) == 0)
                emit_audio();
            av_packet_unref(g_pkt);
        }
    }
    return 0;
}

int sw_pull() noexcept
{
    for (int packets = 0; packets < 64; ++packets)
    {
        const int received = avcodec_receive_frame(g_vcodec, g_vframe);
        if (received == 0)
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
            present();
            return 1;
        }
        if (received != AVERROR(EAGAIN) || g_sw_demux_eof)
        {
            g_ended = true;
            record_decode("software drain", received, 0, 0);
            return -1;
        }

        const int rr = av_read_frame(g_fmt, g_pkt);
        if (rr == AVERROR(EAGAIN))
            return 0;
        if (rr == AVERROR_EOF)
        {
            finish_audio();
            g_sw_demux_eof = true;
            avcodec_send_packet(g_vcodec, nullptr);
            continue;
        }
        if (rr < 0)
        {
            g_ended = true;
            record_decode("software read failed", rr, 0, 0);
            return -1;
        }

        if (g_pkt->stream_index == g_vindex)
            avcodec_send_packet(g_vcodec, g_pkt);
        else if (g_pkt->stream_index == g_aindex && g_acodec &&
                 avcodec_send_packet(g_acodec, g_pkt) == 0)
            emit_audio();
        av_packet_unref(g_pkt);
    }
    return 0;
}

int pull_one() noexcept
{
    return g_hw ? hw_pull() : sw_pull();
}

bool alloc_picture(int w, int h, char *why, int why_cap) noexcept
{
    if (w <= 0 || h <= 0)
    {
        copy_text(why, why_cap, "bad video size");
        return false;
    }
    if (w > 3840)
        w = 3840;
    if (h > 2160)
        h = 2160;
    constexpr std::size_t page_mask = 16 * 1024 - 1;
    const std::size_t image_size = static_cast<std::size_t>(w) * h * sizeof(std::uint32_t);
    const std::size_t image_span = (image_size + page_mask) & ~page_mask;
    const std::size_t mapping_size = image_span * 2;
    // The title has one active player. Reuse its flexible memory across opens.
    if (mapping_size > g_picture_mapping_size)
    {
        constexpr std::size_t hd_capacity = 16 * 1024 * 1024;
        constexpr std::size_t uhd_capacity = 64 * 1024 * 1024;
        const std::size_t capacity = mapping_size <= hd_capacity ? hd_capacity : uhd_capacity;
        void *mapping = nullptr;
        const int result = sceKernelMapFlexibleMemory(&mapping, capacity, 0x03, 0);
        if (result != 0 || !mapping)
        {
            record_decode("picture mapping failed", result, static_cast<int>(capacity), 0);
            copy_text(why, why_cap, "video buffer mapping failed");
            return false;
        }
        if (g_picture_mapping)
        {
            const int unmapped = sceKernelMunmap(g_picture_mapping, g_picture_mapping_size);
            record_decode("old picture mapping unmap", unmapped,
                          static_cast<int>(g_picture_mapping_size), 0);
        }
        g_picture_mapping = mapping;
        g_picture_mapping_size = capacity;
        record_decode("picture mapping allocated", 0, static_cast<int>(capacity), 0);
    }
    g_w = w;
    g_h = h;
    g_front = static_cast<std::uint32_t *>(g_picture_mapping);
    g_back = reinterpret_cast<std::uint32_t *>(
        static_cast<std::uint8_t *>(g_picture_mapping) + image_span);
    std::memset(g_front, 0, image_size);
    std::memset(g_back, 0, image_size);
    return true;
}

bool supports_hardware_h264(const AVCodecParameters *par) noexcept
{
    if (par->codec_id != AV_CODEC_ID_H264 ||
        (par->format != AV_PIX_FMT_NONE && par->format != AV_PIX_FMT_YUV420P &&
         par->format != AV_PIX_FMT_YUVJ420P))
        return false;
    switch (par->profile)
    {
        case FF_PROFILE_UNKNOWN:
        case FF_PROFILE_H264_BASELINE:
        case FF_PROFILE_H264_CONSTRAINED_BASELINE:
        case FF_PROFILE_H264_MAIN:
        case FF_PROFILE_H264_EXTENDED:
        case FF_PROFILE_H264_HIGH:
            return true;
        default:
            return false;
    }
}

bool try_hardware(const AVStream *st) noexcept
{
    if (!hwdec_ready())
        return false;
    const AVCodecParameters *par = st->codecpar;
    const bool hevc = par->codec_id == AV_CODEC_ID_HEVC;
    const AVBitStreamFilter *filter =
        av_bsf_get_by_name(hevc ? "hevc_mp4toannexb" : "h264_mp4toannexb");
    if (!filter || av_bsf_alloc(filter, &g_video_bsf) < 0 ||
        avcodec_parameters_copy(g_video_bsf->par_in, par) < 0)
        return false;
    g_video_bsf->time_base_in = st->time_base;
    if (av_bsf_init(g_video_bsf) < 0)
        return false;

    const bool ten = par->format == AV_PIX_FMT_YUV420P10LE || par->format == AV_PIX_FMT_P010LE;
    const AVCodecParameters *annexb = g_video_bsf->par_out;
    int w = par->width;
    int h = par->height;
    int visible_w = 0;
    int visible_h = 0;
    if (hwdec_visible_from_headers(hevc, annexb->extradata, annexb->extradata_size,
                                   &visible_w, &visible_h) == 0)
    {
        w = visible_w;
        h = visible_h;
    }
    const int result = hwdec_open(&g_hw, hevc, ten, w, h);
    if (result != 0)
    {
        record_decode("hardware decoder open failed", result, 0, 0);
        return false;
    }
    if (!alloc_picture(w, h, nullptr, 0))
    {
        record_decode("picture allocation failed", w, h, 0);
        return false;
    }

    hwdec_bind(g_hw, reinterpret_cast<std::uint8_t *>(g_back), g_w, g_h);
    g_tb = g_video_bsf->time_base_out;
    copy_text(g_mode, static_cast<int>(sizeof g_mode), "HW");
    g_hw_presented = 0;
    record_decode("hardware decoder ready", w, h, 0);
    return true;
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
    // Avoid FFmpeg's terminal probing in a title without a terminal.
    av_log_set_callback(log_ffmpeg);
    if (!path || !path[0])
    {
        copy_text(why, why_cap, "empty path");
        return false;
    }
    if (avformat_open_input(&g_fmt, path, mpeg_program_stream(path), nullptr) < 0)
    {
        copy_text(why, why_cap, "FFmpeg could not open the file");
        close();
        return false;
    }
    /* Demux only: no HEVC software decoder is linked. find_stream_info may
     * still fail on some HEVC files; the container often already named the
     * streams (VLC packetizes from extra data, not from a CPU decoder). */
    g_fmt->probesize = 1 << 20;
    g_fmt->max_analyze_duration = AV_TIME_BASE;
    if (avformat_find_stream_info(g_fmt, nullptr) < 0 && g_fmt->nb_streams == 0)
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
    const AVStream *st = g_fmt->streams[g_vindex];
    const AVCodecID id = st->codecpar->codec_id;
    copy_text(g_codec, static_cast<int>(sizeof g_codec), avcodec_get_name(id));
    copy_text(g_mode, static_cast<int>(sizeof g_mode), "SW");

    g_pkt = av_packet_alloc();
    g_vframe = av_frame_alloc();
    g_aframe = av_frame_alloc();
    if (!g_pkt || !g_vframe)
    {
        copy_text(why, why_cap, "out of memory");
        close();
        return false;
    }
    if (g_aindex >= 0)
    {
        const int result = open_codec(&g_acodec, g_aindex);
        if (result < 0)
        {
            record_decode("audio decoder unavailable", g_fmt->streams[g_aindex]->codecpar->codec_id,
                          result, 0);
            avcodec_free_context(&g_acodec);
            g_aindex = -1;
        }
    }
    g_tb = st->time_base;
    g_duration = g_fmt->duration > 0 ? static_cast<double>(g_fmt->duration) / AV_TIME_BASE : 0;
    g_pcm = pcm;
    g_clock = 0;
    g_pts = 0;
    g_ended = false;

    if (id == AV_CODEC_ID_HEVC || supports_hardware_h264(st->codecpar))
    {
        if (try_hardware(st))
        {
            if (start_thread())
                return true;
            copy_text(why, why_cap, "decoder thread unavailable");
            close();
            return false;
        }
        if (id == AV_CODEC_ID_HEVC)
        {
            copy_text(why, why_cap, "Hardware HEVC unavailable");
            close();
            return false;
        }
        if (g_hw)
        {
            hwdec_close(g_hw);
            g_hw = nullptr;
        }
        av_bsf_free(&g_video_bsf);
        record_decode("software fallback", 0, 0, 0);
    }
    if (open_codec(&g_vcodec, g_vindex) != 0)
    {
        char buf[96];
        std::snprintf(buf, sizeof buf, "no software decoder for %s", g_codec);
        copy_text(why, why_cap, buf);
        close();
        return false;
    }
    int w = g_vcodec->width;
    int h = g_vcodec->height;
    if (w > 1920)
        w = 1920;
    if (h > 1080)
        h = 1080;
    if (!alloc_picture(w, h, why, why_cap))
    {
        close();
        return false;
    }
    copy_text(g_mode, static_cast<int>(sizeof g_mode), "SW");
    if (!start_thread())
    {
        copy_text(why, why_cap, "decoder thread unavailable");
        close();
        return false;
    }
    record_decode("software decoder ready", w, h, 0);
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
    const bool had_media = g_fmt != nullptr;
    if (had_media)
        record_decode("close begin", 0, 0, 0);
    stop_thread();
    pthread_mutex_lock(&g_lock);
    if (g_hw)
    {
        hwdec_close(g_hw);
        g_hw = nullptr;
    }
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
    reset_resampler();
    g_pcm_reported = false;
    g_pcm_frames = 0;
    std::free(g_pcm_buf);
    av_bsf_free(&g_video_bsf);
    g_front = g_back = nullptr;
    g_pcm_buf = nullptr;
    g_pcm_cap = 0;
    g_hw_demux_result = 0;
    g_sw_demux_eof = false;
    g_w = g_h = 0;
    g_vindex = g_aindex = -1;
    g_ended = false;
    g_clock = g_pts = g_duration = 0;
    g_pcm = nullptr;
    if (had_media)
        record_decode("close complete", 0, 0, 0);
    g_codec[0] = 0;
    copy_text(g_mode, static_cast<int>(sizeof g_mode), "SW");
    pthread_mutex_unlock(&g_lock);
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
    pthread_mutex_lock(&g_lock);
    const bool result = g_fmt && g_ended;
    pthread_mutex_unlock(&g_lock);
    return result;
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
    pthread_mutex_lock(&g_lock);
    const double result = g_pts;
    pthread_mutex_unlock(&g_lock);
    return result;
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

void with_picture(PictureFn draw, void *context) noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    pthread_mutex_lock(&g_lock);
    if (g_front)
        draw(context, g_front, g_w, g_h);
    pthread_mutex_unlock(&g_lock);
#else
    (void)draw;
    (void)context;
#endif
}

const char *codec_name() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return g_codec;
#else
    return "";
#endif
}

const char *mode() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return g_mode;
#else
    return "SW";
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
    pthread_mutex_lock(&g_lock);
    g_clock += dt;
    pthread_mutex_unlock(&g_lock);
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
    pthread_mutex_lock(&g_lock);
    const int64_t ts = static_cast<int64_t>(seconds * static_cast<double>(AV_TIME_BASE));
    if (av_seek_frame(g_fmt, -1, ts, AVSEEK_FLAG_BACKWARD) >= 0)
    {
        audio::clear();
        g_pcm_frames = 0;
        if (g_vcodec)
            avcodec_flush_buffers(g_vcodec);
        if (g_acodec)
            avcodec_flush_buffers(g_acodec);
        reset_resampler();
        if (g_video_bsf)
            av_bsf_flush(g_video_bsf);
        if (g_hw)
            hwdec_reset(g_hw);
        g_hw_demux_result = 0;
        g_sw_demux_eof = false;
        g_ended = false;
        g_clock = seconds;
        g_pts = seconds;
        pull_one();
    }
    pthread_mutex_unlock(&g_lock);
#else
    (void)seconds;
#endif
}
} // namespace movian::avdec
