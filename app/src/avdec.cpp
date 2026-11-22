/*
 * Movian for PS5 — FFmpeg demux + software H.264, or libSceVideodec2.
 *
 * HEVC follows VLC (ps5/modules/ps5vdec.c): the console chip, never a
 * software decoder. Software HEVC crashed this title on the TV.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "avdec.hpp"
#include "diagnostics.hpp"
#include "audio.hpp"
#include "torrent.hpp"

#include "hwdec.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#ifdef MOVIAN_HAVE_FFMPEG
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
int sceKernelMapFlexibleMemory(void **address, std::size_t length, int protection, int flags);
}
#endif



namespace movian::avdec
{
namespace
{
#ifdef MOVIAN_HAVE_FFMPEG
AVFormatContext *g_fmt = nullptr;
AVIOContext *g_torrent_io = nullptr;
const torrent::VerifiedFile *g_torrent_input = nullptr;
std::uint64_t g_torrent_offset = 0;
bool g_reading_packet = false;
std::atomic<bool> g_peer_buffering{false};
std::atomic<double> g_pending_seek{-1};
double g_seek_picture = -1;
AVCodecContext *g_vcodec = nullptr;
AVCodecContext *g_acodec = nullptr;
AVPacket *g_pkt = nullptr;
AVFrame *g_vframe = nullptr;
AVFrame *g_aframe = nullptr;
std::uint32_t *g_front = nullptr;
std::uint32_t *g_back = nullptr;
void *g_picture_mapping = nullptr;
hwdec_t *g_hw = nullptr;
AVBSFContext *g_video_bsf = nullptr;
int g_hw_demux_result = 0;
bool g_sw_demux_eof = false;
int g_w = 0;
int g_h = 0;
int g_src_w = 0;
int g_src_h = 0;
SwsContext *g_scaler = nullptr;
int g_vindex = -1;
int g_aindex = -1;
// Open/close and the pickers run on the UI thread. Snapshot the streams before
// decoding starts so a picker never reads a demuxer concurrently with AVIO.
constexpr int max_picker_tracks = 128;
struct TrackList
{
    Track tracks[max_picker_tracks]{};
    int count = 0;
};
TrackList g_audio_tracks;
TrackList g_subtitle_tracks;

void snapshot_tracks() noexcept
{
    for (unsigned i = 0; i < g_fmt->nb_streams; ++i)
    {
        const auto *stream = g_fmt->streams[i];
        const auto type = stream->codecpar->codec_type;
        if (type != AVMEDIA_TYPE_AUDIO && type != AVMEDIA_TYPE_SUBTITLE)
            continue;
        auto &list = type == AVMEDIA_TYPE_AUDIO ? g_audio_tracks : g_subtitle_tracks;
        if (list.count >= max_picker_tracks)
            continue;
        const auto *title = av_dict_get(stream->metadata, "title", nullptr, 0);
        const auto *language = av_dict_get(stream->metadata, "language", nullptr, 0);
        auto &track = list.tracks[list.count++];
        track.stream = static_cast<int>(i);
        track.supported = type == AVMEDIA_TYPE_AUDIO && avcodec_find_decoder(stream->codecpar->codec_id);
        if (title && title->value[0])
            std::snprintf(track.title, sizeof track.title, "%s", title->value);
        else
            std::snprintf(track.title, sizeof track.title, "%s %d",
                          type == AVMEDIA_TYPE_AUDIO ? "Audio" : "Subtitle", list.count);
        std::snprintf(track.language, sizeof track.language, "%s", language ? language->value : "");
        std::snprintf(track.format, sizeof track.format, "%s", avcodec_get_name(stream->codecpar->codec_id));
    }
}

SwrContext *g_resampler = nullptr;
AVChannelLayout g_pcm_input_layout{};
int g_pcm_input_rate = 0;
AVSampleFormat g_pcm_input_format = AV_SAMPLE_FMT_NONE;
bool g_pcm_reported = false;
unsigned long long g_pcm_frames = 0;
bool g_ended = false;
bool g_torrent_error = false;
unsigned g_hw_presented = 0;
unsigned g_sw_presented = 0;
unsigned g_sw_errors = 0;
unsigned g_hw_calls = 0;
std::uint64_t g_hw_first_us = 0;
std::uint64_t g_hw_total_us = 0;
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
bool seek_locked(double seconds) noexcept;
pthread_mutex_t g_picture_lock = PTHREAD_MUTEX_INITIALIZER;

std::uint64_t monotonic_us() noexcept
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::uint64_t>(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
}
void record_decode(const char *event, int value_a, int value_b, unsigned frames) noexcept
{
    diagnostics::record("decoder", "%s codec=%s a=%d b=%d frames=%u", event, g_codec,
                        value_a, value_b, frames);
    if (auto *log = std::fopen("/download0/hevc-test-decode.log", "a"))
    {
        std::fprintf(log, "v52 %s codec=%s a=%d b=%d frames=%u\n", event, g_codec, value_a, value_b, frames);
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
    const int fd = ::open(path, O_RDONLY);
    if (fd < 0)
        return nullptr;
    unsigned char header[4]{};
    const auto size = ::read(fd, header, sizeof header);
    ::close(fd);
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
    if (g_seek_picture >= 0)
    {
        record_decode("torrent picture after seek", static_cast<int>(g_seek_picture),
                      static_cast<int>(g_pts), g_hw ? g_hw_presented : g_sw_presented + 1);
        g_seek_picture = -1;
    }
    pthread_mutex_lock(&g_picture_lock);
    std::uint32_t *t = g_front;
    g_front = g_back;
    g_back = t;
    if (g_hw && g_back)
        hwdec_bind(g_hw, reinterpret_cast<std::uint8_t *>(g_back), g_w, g_h);
    pthread_mutex_unlock(&g_picture_lock);
}
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_t g_thread{};
bool g_thread_on = false;
std::atomic<bool> g_run{false};

int torrent_read(void *, std::uint8_t *buffer, int size)
{
    const auto total = g_torrent_input->size();
    if (!total)
        return AVERROR(EIO);
    const auto started = monotonic_us();
    const auto deadline = started + 30000000;
    auto end_wait = [&](const char *result) {
        g_peer_buffering = false;
        const auto elapsed = (monotonic_us() - started) / 1000;
        if (elapsed >= 250)
            diagnostics::record("buffer", "%s offset=%llu wait_ms=%llu", result,
                                static_cast<unsigned long long>(g_torrent_offset),
                                static_cast<unsigned long long>(elapsed));
    };
    using Result = torrent::VerifiedFile::ReadResult;
    for (;;)
    {
        if (g_reading_packet && g_pending_seek.load(std::memory_order_relaxed) >= 0)
        {
            end_wait("seek requested");
            return AVERROR_EXIT;
        }
        std::size_t count = 0;
        const auto state = g_torrent_input->read(g_torrent_offset, buffer, size, count);
        if (state == Result::eof || state == Result::failed)
        {
            end_wait(state == Result::eof ? "eof" : "peer input failed");
            return state == Result::eof ? AVERROR_EOF : AVERROR(EIO);
        }
        if (state == Result::data && count)
        {
            end_wait("verified bytes ready");
            g_torrent_offset += count;
            return static_cast<int>(count);
        }
        if (!g_reading_packet)
            return AVERROR(EAGAIN);
        if (!g_run.load(std::memory_order_relaxed))
        {
            end_wait("playback closed");
            return AVERROR_EXIT;
        }
        if (monotonic_us() >= deadline)
        {
            end_wait("peer input timed out");
            return AVERROR(ETIMEDOUT);
        }
        g_peer_buffering = true;
        usleep(20000);
    }
}

std::int64_t torrent_seek(void *, std::int64_t offset, int how)
{
    const auto total = static_cast<std::int64_t>(g_torrent_input->size());
    if (!total)
        return AVERROR(EIO);
    how &= ~AVSEEK_FORCE;
    if (how == AVSEEK_SIZE)
        return total;
    const auto base = how == SEEK_SET ? 0 :
                      how == SEEK_CUR ? static_cast<std::int64_t>(g_torrent_offset) :
                      how == SEEK_END ? total : -1;
    if (base < 0 || offset < -base || offset > total - base)
        return AVERROR(EINVAL);
    g_torrent_offset = base + offset;
    return static_cast<std::int64_t>(g_torrent_offset);
}

int read_packet()
{
    if (!g_torrent_input)
        return av_read_frame(g_fmt, g_pkt);
    g_reading_packet = true;
    // Only the decode worker enters libavformat on a progressive input
    // after open. The UI can still draw and close while a piece is pending.
    pthread_mutex_unlock(&g_lock);
    const int result = av_read_frame(g_fmt, g_pkt);
    pthread_mutex_lock(&g_lock);
    g_reading_packet = false;
    if (g_pending_seek.load(std::memory_order_relaxed) >= 0 && result < 0)
    {
        g_torrent_io->error = 0;
        g_torrent_io->eof_reached = 0;
        return AVERROR(EAGAIN);
    }
    return result;
}

void *decode_loop(void *) noexcept
{
    while (g_run.load(std::memory_order_relaxed))
    {
        int r = 0;
        pthread_mutex_lock(&g_lock);
        const double pending = g_pending_seek.exchange(-1);
        if (pending >= 0)
            seek_locked(pending);
        else if (g_fmt && !g_ended && g_pts + 0.0005 < g_clock)
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
        std::fprintf(log, "v52 %s PCM EOF codec=%s frames=%llu\n",
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
    (*ctx)->thread_count = st->codecpar->codec_id == AV_CODEC_ID_AV1 ? 4 : 1;
    if (avcodec_open2(*ctx, codec, nullptr) < 0)
        return -1;
    return 0;
}

int end_hardware(int result) noexcept
{
    g_ended = true;
    g_torrent_error = g_torrent_input && g_hw_demux_result < 0 &&
                      g_hw_demux_result != AVERROR_EOF;
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
            const auto started = monotonic_us();
            if (g_hw_calls++ == 0)
                g_hw_first_us = started;
            const int decoded = hwdec_feed(g_hw, g_pkt->data, g_pkt->size, pts_us);
            g_hw_total_us += monotonic_us() - started;
            av_packet_unref(g_pkt);
            if (decoded < 0)
                return end_hardware(decoded);
            if (decoded > 0)
                return show_hardware_frame(step);
            continue;
        }
        if (filtered != AVERROR(EAGAIN) || g_hw_demux_result == AVERROR_EOF)
            return end_hardware(filtered);

        g_hw_demux_result = read_packet();
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
            g_sw_errors = 0;
            int64_t p = g_vframe->best_effort_timestamp;
            if (p == AV_NOPTS_VALUE)
                p = g_vframe->pts;
            if (p != AV_NOPTS_VALUE)
                g_pts = av_q2d(g_tb) * static_cast<double>(p);
            else
                g_pts += 1.0 / 30.0;
            g_scaler = sws_getCachedContext(g_scaler, g_vframe->width, g_vframe->height,
                                            static_cast<AVPixelFormat>(g_vframe->format),
                                            g_w, g_h, AV_PIX_FMT_RGBA, SWS_FAST_BILINEAR,
                                            nullptr, nullptr, nullptr);
            std::uint8_t *planes[4] = {reinterpret_cast<std::uint8_t *>(g_back)};
            const int strides[4] = {g_w * 4};
            const bool copied = g_scaler &&
                sws_scale(g_scaler, g_vframe->data, g_vframe->linesize, 0,
                          g_vframe->height, planes, strides) == g_h;
            av_frame_unref(g_vframe);
            if (copied)
            {
                present();
                if (++g_sw_presented == 1)
                    record_decode("first software frame", g_w, g_h, g_sw_presented);
            }
            else
            {
                record_decode("software conversion failed", g_w, g_h, 0);
                g_ended = true;
                return -1;
            }
            return 1;
        }
        const bool recoverable = received == AVERROR_INVALIDDATA && !g_sw_demux_eof &&
                                 ++g_sw_errors <= 16;
        if (recoverable && g_sw_errors == 1)
            record_decode("software packet rejected", received, 0, g_sw_presented);
        if ((received != AVERROR(EAGAIN) && !recoverable) || g_sw_demux_eof)
        {
            g_ended = true;
            record_decode("software drain", received, 0, g_sw_presented);
            return -1;
        }

        const int rr = read_packet();
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
            g_torrent_error = g_torrent_input != nullptr;
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
    g_src_w = w;
    g_src_h = h;
    // The native Canvas is 1920x1080. Keep decoder surfaces at source
    // resolution and convert just the on-screen pixels.
    if (w > 1920 || h > 1080)
    {
        const double scale = std::min(1920.0 / w, 1080.0 / h);
        w = std::max(1, static_cast<int>(w * scale));
        h = std::max(1, static_cast<int>(h * scale));
    }
    constexpr std::size_t page_mask = 16 * 1024 - 1;
    const std::size_t image_size = static_cast<std::size_t>(w) * h * sizeof(std::uint32_t);
    const std::size_t image_span = (image_size + page_mask) & ~page_mask;
    // The title has one active player. Reuse its flexible memory across opens.
    if (!g_picture_mapping)
    {
        constexpr std::size_t capacity = 16 * 1024 * 1024;
        void *mapping = nullptr;
        const int result = sceKernelMapFlexibleMemory(&mapping, capacity, 0x03, 0);
        if (result != 0 || !mapping)
        {
            record_decode("picture mapping failed", result, static_cast<int>(capacity), 0);
            copy_text(why, why_cap, "video buffer mapping failed");
            return false;
        }
        g_picture_mapping = mapping;
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
    record_decode("hardware display buffer", g_w, g_h, 0);
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

bool open(const char *path, char *why, int why_cap, PcmFn pcm,
          const torrent::VerifiedFile *stream) noexcept
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
    const bool needs_probe = !stream || stream->complete();
    const AVInputFormat *input_hint = !stream ? mpeg_program_stream(path) :
        needs_probe ? nullptr : av_find_input_format(stream->uses_cache() ? "matroska" : "mov");
    if (stream)
    {
        constexpr int buffer_size = 32 * 1024;
        auto *buffer = static_cast<std::uint8_t *>(av_malloc(buffer_size));
        g_fmt = avformat_alloc_context();
        if (buffer && g_fmt && (needs_probe || input_hint))
            g_torrent_io = avio_alloc_context(buffer, buffer_size, 0, nullptr,
                                             torrent_read, nullptr, torrent_seek);
        if (!g_torrent_io)
        {
            av_free(buffer);
            avformat_free_context(g_fmt);
            g_fmt = nullptr;
            copy_text(why, why_cap, "Could not initialize progressive input");
            return false;
        }
        g_torrent_input = stream;
        g_fmt->pb = g_torrent_io;
        g_fmt->flags |= AVFMT_FLAG_CUSTOM_IO;
    }
    const int opened = avformat_open_input(&g_fmt, path, input_hint, nullptr);
    if (opened < 0)
    {
        record_decode("input open failed", opened, input_hint != nullptr, 0);
        copy_text(why, why_cap, "FFmpeg could not open the file");
        close();
        return false;
    }
    /* Demux only: no HEVC software decoder is linked. find_stream_info may
     * still fail on some HEVC files; the container often already named the
     * streams (VLC packetizes from extra data, not from a CPU decoder). */
    g_fmt->probesize = 1 << 20;
    g_fmt->max_analyze_duration = AV_TIME_BASE;
    if ((needs_probe && avformat_find_stream_info(g_fmt, nullptr) < 0 && g_fmt->nb_streams == 0) ||
        (!needs_probe && g_fmt->nb_streams == 0))
    {
        copy_text(why, why_cap, "no streams");
        close();
        return false;
    }
    g_vindex = av_find_best_stream(g_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    g_aindex = av_find_best_stream(g_fmt, AVMEDIA_TYPE_AUDIO, -1, g_vindex, nullptr, 0);
    if (g_vindex < 0)
    {
        record_decode("no video stream", g_vindex, input_hint != nullptr,
                      g_fmt->nb_streams);
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
    snapshot_tracks();
    g_tb = st->time_base;
    g_duration = g_fmt->duration > 0 ? static_cast<double>(g_fmt->duration) / AV_TIME_BASE : 0;
    g_pcm = pcm;
    g_clock = 0;
    g_pts = 0;
    g_ended = false;
    g_torrent_error = false;

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
    (void)stream;
    if (why && why_cap > 0)
        std::snprintf(why, static_cast<std::size_t>(why_cap), "FFmpeg not linked in this binary");
    return false;
#endif
}

bool progressive() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return g_torrent_input != nullptr;
#else
    return false;
#endif
}

bool seekable() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return !g_torrent_input || g_torrent_input->uses_cache() || g_torrent_input->complete();
#else
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
        if (g_hw_calls)
        {
            record_decode("hardware timing ms and mean feed us",
                          static_cast<int>((monotonic_us() - g_hw_first_us) / 1000),
                          static_cast<int>(g_hw_total_us / g_hw_calls), g_hw_presented);
            record_decode("mean hardware conversion us", hwdec_mean_conversion_us(g_hw),
                          0, g_hw_presented);
        }
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
    if (g_torrent_io)
    {
        av_freep(&g_torrent_io->buffer);
        avio_context_free(&g_torrent_io);
    }
    g_torrent_input = nullptr;
    g_torrent_offset = 0;
    g_peer_buffering = false;
    g_pending_seek = -1;
    g_seek_picture = -1;
    reset_resampler();
    sws_freeContext(g_scaler);
    g_scaler = nullptr;
    g_pcm_reported = false;
    g_pcm_frames = 0;
    std::free(g_pcm_buf);
    av_bsf_free(&g_video_bsf);
    g_front = g_back = nullptr;
    g_pcm_buf = nullptr;
    g_pcm_cap = 0;
    g_hw_demux_result = 0;
    g_sw_presented = 0;
    g_sw_errors = 0;
    g_hw_calls = 0;
    g_hw_total_us = 0;
    g_sw_demux_eof = false;
    g_w = g_h = 0;
    g_src_w = g_src_h = 0;
    g_vindex = g_aindex = -1;
    g_audio_tracks.count = g_subtitle_tracks.count = 0;
    g_ended = false;
    g_torrent_error = false;
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

EndState end_state() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    pthread_mutex_lock(&g_lock);
    auto result = EndState::pending;
    if (g_fmt && g_ended && !g_reading_packet && g_pending_seek.load() < 0)
        result = g_torrent_error ? EndState::torrent_error : EndState::eof;
    pthread_mutex_unlock(&g_lock);
    return result;
#else
    return EndState::pending;
#endif
}

int width() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return g_src_w;
#else
    return 0;
#endif
}

int height() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return g_src_h;
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
    pthread_mutex_lock(&g_picture_lock);
    if (g_front)
        draw(context, g_front, g_w, g_h);
    pthread_mutex_unlock(&g_picture_lock);
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
    if (!g_peer_buffering)
        g_clock += dt;
    pthread_mutex_unlock(&g_lock);
    return true;
#else
    (void)dt;
    return false;
#endif
}

#ifdef MOVIAN_HAVE_FFMPEG
namespace
{
bool seek_locked(double seconds) noexcept
{
    g_seek_picture = -1;
    const int64_t ts = static_cast<int64_t>(seconds * static_cast<double>(AV_TIME_BASE));
    if (g_torrent_input)
    {
        g_torrent_io->error = 0;
        g_torrent_io->eof_reached = 0;
        g_reading_packet = true;
        pthread_mutex_unlock(&g_lock);
    }
    const int result = av_seek_frame(g_fmt, -1, ts, AVSEEK_FLAG_BACKWARD);
    if (g_torrent_input)
    {
        pthread_mutex_lock(&g_lock);
        g_reading_packet = false;
        record_decode(result >= 0 ? "torrent seek complete" : "torrent seek failed",
                      static_cast<int>(seconds), result, 0);
    }
    if (result >= 0)
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
        g_sw_errors = 0;
        g_ended = false;
        g_torrent_error = false;
        if (g_torrent_input) g_seek_picture = seconds;
        g_clock = seconds;
        g_pts = seconds;
        pull_one();
    }
    return result >= 0;
}
}
#endif

void seek(double seconds) noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    if (!g_fmt || !seekable())
        return;
    if (seconds < 0)
        seconds = 0;
    if (g_torrent_input)
    {
        g_pending_seek.store(seconds, std::memory_order_relaxed);
        return;
    }
    pthread_mutex_lock(&g_lock);
    seek_locked(seconds);
    pthread_mutex_unlock(&g_lock);
#else
    (void)seconds;
#endif
}

int track_count(TrackKind kind) noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return (kind == TrackKind::audio ? g_audio_tracks : g_subtitle_tracks).count;
#else
    (void)kind;
    return 0;
#endif
}

bool track(TrackKind kind, int row, Track &value) noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    const auto &list = kind == TrackKind::audio ? g_audio_tracks : g_subtitle_tracks;
    if (row >= 0 && row < list.count)
    {
        value = list.tracks[row];
        return true;
    }
#else
    (void)kind;
    (void)row;
    (void)value;
#endif
    return false;
}

int selected_audio_track() noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    return g_aindex;
#else
    return -1;
#endif
}

bool select_audio_track(int stream) noexcept
{
#ifdef MOVIAN_HAVE_FFMPEG
    pthread_mutex_lock(&g_lock);
    bool selected = false;
    if (g_fmt && !g_torrent_input && !g_ended && stream >= 0 &&
        static_cast<unsigned>(stream) < g_fmt->nb_streams &&
        g_fmt->streams[stream]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        AVCodecContext *next = nullptr;
        if (stream == g_aindex)
            selected = true;
        else if (open_codec(&next, stream) == 0)
        {
            avcodec_free_context(&g_acodec);
            g_acodec = next;
            g_aindex = stream;
            reset_resampler();
            audio::clear();
            g_pcm_frames = 0;
            g_pcm_reported = false;
            record_decode("audio track selected", stream, next->codec_id, 0);
            selected = true;
        }
        else
            avcodec_free_context(&next);
    }
    pthread_mutex_unlock(&g_lock);
    return selected;
#else
    (void)stream;
    return false;
#endif
}
} // namespace movian::avdec
