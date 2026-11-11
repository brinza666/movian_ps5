/*
 * Movian for PS5 — software MPEG-1 player.
 *
 * Decode: PL_MPEG (MIT, Dominic Szablewski) via the archived splicer copy.
 * Present: CPU compositor (starter / Breeze). Audio: sceAudioOut 48 kHz.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "player.hpp"

#include "audio.hpp"
#include "font.hpp"
#include "image.hpp"
#include "pad.hpp"
#include "paths.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

namespace movian::player
{
namespace
{
using screen::Color;

constexpr int picture_w = 1280;
constexpr int picture_h = 720;
constexpr double seek_step = 10.0;

char g_path[384] = "";
char g_title[96] = "";
char g_error[128] = "";
plm_t *g_plm = nullptr;
movian::image::Image g_picture{};
std::uint64_t g_last_ms = 0;
bool g_paused = false;
bool g_ended = false;
int g_src_w = 0;
int g_src_h = 0;
int g_src_rate = 0;
double g_duration = 0;

bool ends_with_ci(const char *name, const char *ext) noexcept
{
    const std::size_t n = std::strlen(name);
    const std::size_t e = std::strlen(ext);
    if (n < e)
        return false;
    for (std::size_t i = 0; i < e; ++i)
    {
        char a = name[n - e + i];
        if (a >= 'A' && a <= 'Z')
            a = static_cast<char>(a - 'A' + 'a');
        if (a != ext[i])
            return false;
    }
    return true;
}

const char *basename_of(const char *path) noexcept
{
    const char *s = std::strrchr(path, '/');
    return s ? s + 1 : path;
}

void copy_text(char *dst, int cap, const char *src) noexcept
{
    if (!dst || cap <= 0)
        return;
    std::snprintf(dst, static_cast<std::size_t>(cap), "%s", src ? src : "");
}

void free_picture() noexcept
{
    std::free(g_picture.pixels);
    g_picture = image::Image{};
}

void on_video(plm_t *, plm_frame_t *frame, void *) noexcept
{
    if (!frame || !g_picture.pixels)
        return;
    // RGBA bytes = 0xAABBGGRR uint32 on little-endian, matching Canvas.
    plm_frame_to_rgba(frame, reinterpret_cast<uint8_t *>(g_picture.pixels), g_picture.w * 4);
}

void resample_push(const float *src, int src_frames, int src_rate) noexcept
{
    if (!src || src_frames <= 0)
        return;
    if (src_rate <= 0 || src_rate == audio::rate)
    {
        audio::push(src, src_frames);
        return;
    }
    // Linear resample into 48 kHz stereo. Bound the burst so a huge ratio
    // cannot stall the UI thread.
    const double step = static_cast<double>(src_rate) / static_cast<double>(audio::rate);
    const int out_frames = static_cast<int>(src_frames / step);
    constexpr int max_out = 8192;
    float buf[max_out * 2];
    const int n = out_frames > max_out ? max_out : out_frames;
    double pos = 0;
    for (int i = 0; i < n; ++i)
    {
        const int i0 = static_cast<int>(pos);
        const int i1 = i0 + 1 < src_frames ? i0 + 1 : src_frames - 1;
        const float t = static_cast<float>(pos - i0);
        buf[2 * i] = src[2 * i0] + (src[2 * i1] - src[2 * i0]) * t;
        buf[2 * i + 1] = src[2 * i0 + 1] + (src[2 * i1] - src[2 * i0 + 1]) * t;
        pos += step;
    }
    audio::push(buf, n);
}

void on_audio(plm_t *, plm_samples_t *samples, void *) noexcept
{
    if (!samples || g_paused)
        return;
    resample_push(samples->interleaved, static_cast<int>(samples->count), g_src_rate);
}

void format_time(char *dst, int cap, double seconds) noexcept
{
    if (seconds < 0)
        seconds = 0;
    const int t = static_cast<int>(seconds);
    std::snprintf(dst, static_cast<std::size_t>(cap), "%d:%02d", t / 60, t % 60);
}

void seek_by(double delta) noexcept
{
    if (!g_plm)
        return;
    double t = plm_get_time(g_plm) + delta;
    if (t < 0)
        t = 0;
    if (g_duration > 0 && t > g_duration)
        t = g_duration;
    audio::clear();
    plm_seek(g_plm, t, FALSE);
    g_ended = plm_has_ended(g_plm) != 0;
}
} // namespace

Kind classify(const char *path) noexcept
{
    if (!path || !path[0])
        return Kind::none;
    if (ends_with_ci(path, ".mpg") || ends_with_ci(path, ".mpeg") || ends_with_ci(path, ".mpv") ||
        ends_with_ci(path, ".m1v"))
        return Kind::mpeg1;
    if (ends_with_ci(path, ".mp4") || ends_with_ci(path, ".m4v") || ends_with_ci(path, ".mov") ||
        ends_with_ci(path, ".mkv") || ends_with_ci(path, ".ts") || ends_with_ci(path, ".m2ts") ||
        ends_with_ci(path, ".avi") || ends_with_ci(path, ".webm") || ends_with_ci(path, ".m2v"))
        return Kind::later;
    return Kind::none;
}

bool open(const char *path, char *why, int why_cap) noexcept
{
    close();
    if (!path || !path[0])
    {
        copy_text(why, why_cap, "empty path");
        return false;
    }
    if (classify(path) != Kind::mpeg1)
    {
        copy_text(why, why_cap, "this build plays MPEG-1 (.mpg) only; H.264 needs FFmpeg");
        return false;
    }

    g_plm = plm_create_with_filename(path);
    if (!g_plm)
    {
        copy_text(why, why_cap, "could not open file");
        return false;
    }
    plm_set_video_decode_callback(g_plm, on_video, nullptr);
    plm_set_audio_decode_callback(g_plm, on_audio, nullptr);
    plm_set_loop(g_plm, FALSE);
    plm_set_audio_lead_time(g_plm, 0.080);

    g_src_w = plm_get_width(g_plm);
    g_src_h = plm_get_height(g_plm);
    g_src_rate = plm_get_samplerate(g_plm);
    g_duration = plm_get_duration(g_plm);
    if (g_src_w <= 0 || g_src_h <= 0)
    {
        copy_text(why, why_cap, "no MPEG-1 video stream");
        close();
        return false;
    }

    auto *pixels = static_cast<std::uint32_t *>(std::malloc(
        static_cast<std::size_t>(g_src_w) * static_cast<std::size_t>(g_src_h) * sizeof(std::uint32_t)));
    if (!pixels)
    {
        copy_text(why, why_cap, "out of memory for the frame");
        close();
        return false;
    }
    std::memset(pixels, 0, static_cast<std::size_t>(g_src_w) * static_cast<std::size_t>(g_src_h) *
                               sizeof(std::uint32_t));
    g_picture.w = g_src_w;
    g_picture.h = g_src_h;
    g_picture.pixels = pixels;

    copy_text(g_path, static_cast<int>(sizeof g_path), path);
    copy_text(g_title, static_cast<int>(sizeof g_title), basename_of(path));
    g_paused = false;
    g_ended = false;
    g_last_ms = screen::now_ms();
    g_error[0] = 0;
    (void)audio::init();
    audio::clear();
    (void)picture_w;
    (void)picture_h;
    return true;
}

void close() noexcept
{
    if (g_plm)
    {
        plm_destroy(g_plm);
        g_plm = nullptr;
    }
    free_picture();
    audio::clear();
    g_path[0] = 0;
    g_title[0] = 0;
    g_paused = false;
    g_ended = false;
    g_src_w = g_src_h = g_src_rate = 0;
    g_duration = 0;
}

bool active() noexcept
{
    return g_plm != nullptr;
}

bool playing() noexcept
{
    return g_plm && !g_paused && !g_ended;
}

bool handle(unsigned pressed) noexcept
{
    if (!g_plm)
        return false;
    bool dirty = pressed != 0;
    if (pressed & pad::cross)
    {
        g_paused = !g_paused;
        if (g_paused)
            audio::clear();
        g_last_ms = screen::now_ms();
    }
    if (pressed & pad::left)
        seek_by(-seek_step);
    if (pressed & pad::right)
        seek_by(seek_step);
    if (pressed & pad::l2)
        seek_by(-60.0);
    if (pressed & pad::r2)
        seek_by(60.0);
    if (pressed & pad::circle)
    {
        close();
        return true;
    }
    return dirty;
}

bool tick() noexcept
{
    if (!g_plm)
        return false;
    const std::uint64_t now = screen::now_ms();
    double dt = 0;
    if (g_last_ms == 0)
        g_last_ms = now;
    else
        dt = static_cast<double>(now - g_last_ms) / 1000.0;
    g_last_ms = now;
    if (dt < 0)
        dt = 0;
    if (dt > 0.25)
        dt = 0.25;
    if (!g_paused && !g_ended)
    {
        plm_decode(g_plm, dt);
        if (plm_has_ended(g_plm))
        {
            g_ended = true;
            g_paused = true;
            audio::clear();
        }
    }
    return true;
}

void draw(screen::Canvas &c) noexcept
{
    c.clear(Color::bg);
    if (g_picture.loaded())
    {
        // Fit inside 1920x900, keep aspect.
        const float max_w = 1920.f;
        const float max_h = 900.f;
        float w = static_cast<float>(g_picture.w);
        float h = static_cast<float>(g_picture.h);
        const float s = w / max_w > h / max_h ? max_w / w : max_h / h;
        w *= s;
        h *= s;
        const int x = static_cast<int>((1920.f - w) * 0.5f);
        const int y = static_cast<int>((900.f - h) * 0.5f) + 40;
        c.image(x, y, static_cast<int>(w), static_cast<int>(h), g_picture);
    }

    c.rect(0, 0, screen::width, 64, Color::surface, 180);
    c.text(48.f, 44.f, g_title[0] ? g_title : "MPEG-1", font::semibold, 24, Color::text);
    c.text(1600.f, 44.f, "SW MPEG-1", font::mono_medium, 16, Color::accent);

    const double t = g_plm ? plm_get_time(g_plm) : 0;
    const double d = g_duration > 0 ? g_duration : 1;
    char left[16], right[16];
    format_time(left, sizeof left, t);
    format_time(right, sizeof right, g_duration);
    c.rect(0, 960, screen::width, 120, Color::surface, 200);
    c.round_rect(80, 992, 1760, 10, 4, Color::line);
    const int filled = static_cast<int>(1760.0 * (t / d));
    c.round_rect(80, 992, filled < 4 ? 4 : filled, 10, 4, Color::accent);
    c.text(80.f, 1040.f, left, font::mono_medium, 18, Color::muted);
    c.text(1760.f, 1040.f, right, font::mono_medium, 18, Color::muted);
    const char *state = g_ended ? "ended" : g_paused ? "paused" : "playing";
    c.text(900.f, 1040.f, state, font::medium, 18, Color::text);
    c.text(48.f, 1070.f, "Cross pause    Left/Right 10s    L2/R2 60s    Circle back",
           font::regular, 16, Color::faint);
    (void)g_error;
    (void)g_path;
}
} // namespace movian::player
