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
#include "avdec.hpp"
#include "font.hpp"
#include "image.hpp"
#include "pad.hpp"
#include "paths.hpp"
#include "settings.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

namespace movian::player
{
namespace
{
using screen::Color;

constexpr int picture_w = 1280;
constexpr int picture_h = 720;

char g_path[384] = "";
char g_title[96] = "";
char g_error[128] = "";
plm_t *g_plm = nullptr;
bool g_use_ffmpeg = false;
movian::image::Image g_picture{};
std::uint64_t g_last_ms = 0;
std::atomic<bool> g_paused{false};
bool g_ended = false;
bool g_torrent_error = false;
enum class Panel { hidden, transport, audio, subtitles, settings, video, sound, caption };
enum class Control { audio, previous, rewind, pause, forward, next, subtitles, settings };
constexpr const char *control_labels[]{
    "Audio tracks", "Previous", "Rewind", "Play / Pause", "Seek forward", "Next", "Subtitles", "Settings"
};
constexpr int default_control = static_cast<int>(Control::pause);
constexpr int visible_tracks = 7;

Panel g_panel = Panel::hidden;
int g_control = default_control;
int g_panel_row = 0;
int g_first_track = 0;
double g_seek_preview = 0;
bool g_scrubbing = false;
int g_src_w = 0;
int g_src_h = 0;
int g_src_rate = 0;
double g_duration = 0;
std::uint64_t g_saved_ms = 0;
std::uint64_t g_opened_ms = 0;

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
    if (!frame || !g_picture.pixels || g_picture.w <= 0 || g_picture.h <= 0)
        return;
    // RGBA bytes = 0xAABBGGRR uint32 on little-endian, matching Canvas.
    plm_frame_to_rgba(frame, reinterpret_cast<uint8_t *>(g_picture.pixels),
                      static_cast<int>(g_picture.w) * 4);
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
    // Keep scratch space small enough for the FFmpeg decode thread.
    const double step = static_cast<double>(src_rate) / audio::rate;
    constexpr int max_out = 8192;
    constexpr int batch_frames = 256;
    const int count = static_cast<int>(std::min(src_frames / step, static_cast<double>(max_out)));
    float batch[batch_frames * 2];
    double pos = 0;
    for (int written = 0; written < count;)
    {
        const int size = std::min(count - written, batch_frames);
        for (int i = 0; i < size; ++i)
        {
            const int i0 = std::min(static_cast<int>(pos), src_frames - 1);
            const int i1 = std::min(i0 + 1, src_frames - 1);
            const float t = static_cast<float>(pos - i0);
            batch[2 * i] = src[2 * i0] + (src[2 * i1] - src[2 * i0]) * t;
            batch[2 * i + 1] = src[2 * i0 + 1] + (src[2 * i1 + 1] - src[2 * i0 + 1]) * t;
            pos += step;
        }
        audio::push(batch, size);
        written += size;
    }
}

void on_audio(plm_t *, plm_samples_t *samples, void *) noexcept
{
    if (!samples || g_paused)
        return;
    resample_push(samples->interleaved, static_cast<int>(samples->count), g_src_rate);
}

void format_time(char *dst, int cap, double seconds) noexcept
{
    const auto t = std::isfinite(seconds) ? static_cast<unsigned>(std::clamp(seconds, 0.0, 35999999.0)) : 0U;
    if (t >= 3600)
        std::snprintf(dst, static_cast<std::size_t>(cap), "%u:%02u:%02u", t / 3600, t / 60 % 60, t % 60);
    else
        std::snprintf(dst, static_cast<std::size_t>(cap), "%u:%02u", t / 60, t % 60);
}

double seek_step() noexcept
{
    const int s = settings::get().seek_step;
    return s > 0 ? static_cast<double>(s) : 10.0;
}

void persist_resume() noexcept
{
    if (!g_path[0])
        return;
    if (!g_plm && !g_use_ffmpeg)
        return;
    const double t = g_use_ffmpeg ? avdec::time() : (g_plm ? plm_get_time(g_plm) : 0);
    settings::remember_resume(g_path, g_title, t, g_duration);
}

void on_ffmpeg_pcm(const float *interleaved, int frames, int rate) noexcept
{
    if (g_paused)
        return;
    resample_push(interleaved, frames, rate);
}

bool can_seek() noexcept
{
    return active() && (!g_use_ffmpeg || avdec::seekable());
}

void seek_to(double t) noexcept
{
    if (!g_plm && !g_use_ffmpeg)
        return;
    if (!can_seek())
    {
        screen::notify("Seeking is unavailable during progressive playback");
        return;
    }
    if (t < 0)
        t = 0;
    if (g_duration > 0 && t > g_duration)
        t = g_duration;
    if (g_use_ffmpeg)
    {
        avdec::seek(t);
        g_ended = false;
        g_torrent_error = false;
    }
    else
    {
        audio::clear();
        plm_seek(g_plm, t, FALSE);
        g_ended = plm_has_ended(g_plm) != 0;
    }
    persist_resume();
}

void seek_by(double delta) noexcept
{
    seek_to(time() + delta);
}

void reset_overlay() noexcept
{
    g_panel = Panel::hidden;
    g_control = default_control;
    g_panel_row = 0;
    g_first_track = 0;
    g_scrubbing = false;
}

int picker_count() noexcept
{
    if (g_panel == Panel::subtitles)
        return 1 + avdec::track_count(avdec::TrackKind::subtitle);
    return std::max(1, avdec::track_count(avdec::TrackKind::audio));
}

void move_picker(int row) noexcept
{
    g_panel_row = std::clamp(row, 0, picker_count() - 1);
    if (g_panel_row < g_first_track)
        g_first_track = g_panel_row;
    if (g_panel_row >= g_first_track + visible_tracks)
        g_first_track = g_panel_row - visible_tracks + 1;
}

void open_control(Control control, bool (*skip)(int, bool)) noexcept
{
    g_panel_row = 0;
    g_first_track = 0;
    switch (control)
    {
    case Control::audio:
    {
        g_panel = Panel::audio;
        const int selected = avdec::selected_audio_track();
        avdec::Track track{};
        for (int row = 0; avdec::track(avdec::TrackKind::audio, row, track); ++row)
        {
            if (track.stream == selected)
            {
                move_picker(row);
                break;
            }
        }
        break;
    }
    case Control::previous:
        if (!skip || !skip(-1, true))
            seek_to(0);
        break;
    case Control::rewind: seek_by(-seek_step()); break;
    case Control::pause: toggle_pause(); break;
    case Control::forward: seek_by(seek_step()); break;
    case Control::next:
        if (!skip || !skip(1, true))
            screen::notify("There is no next video in the queue");
        break;
    case Control::subtitles: g_panel = Panel::subtitles; break;
    case Control::settings: g_panel = Panel::settings; break;
    }
}

constexpr const char *settings_rows[]{"Video settings", "Audio settings", "Subtitle settings"};
constexpr const char *video_rows[]{"Seek step", "Repeat", "Shuffle", "Bookmark this file", "Video adjustments"};
constexpr const char *sound_rows[]{"Mute", "Volume", "Audio delay"};
constexpr const char *caption_rows[]{"Subtitle delay", "Subtitle size", "Subtitle position"};

std::pair<const char *const *, int> rows_for(Panel panel) noexcept
{
    switch (panel)
    {
    case Panel::video: return {video_rows, std::size(video_rows)};
    case Panel::sound: return {sound_rows, std::size(sound_rows)};
    case Panel::caption: return {caption_rows, std::size(caption_rows)};
    default: return {settings_rows, std::size(settings_rows)};
    }
}

void change_setting(int direction, bool activate) noexcept
{
    if (g_panel == Panel::settings)
    {
        if (activate)
        {
            constexpr Panel pages[]{Panel::video, Panel::sound, Panel::caption};
            g_panel = pages[g_panel_row];
            g_panel_row = 0;
        }
        return;
    }
    if (g_panel == Panel::sound && g_panel_row == 0)
        audio::set_mute(!audio::muted());
    else if (g_panel == Panel::sound && g_panel_row == 1)
    {
        settings::cycle_volume(direction);
        audio::set_volume(settings::get().volume);
    }
    else if (g_panel == Panel::video && g_panel_row == 0)
        settings::cycle_seek(direction);
    else if (g_panel == Panel::video && g_panel_row == 1)
        settings::cycle_repeat(direction);
    else if (g_panel == Panel::video && g_panel_row == 2)
    {
        settings::get().shuffle = !settings::get().shuffle;
        settings::save();
    }
    else if (g_panel == Panel::video && g_panel_row == 3)
        settings::toggle_bookmark(g_path, g_title);
    else if (activate)
        screen::notify("This playback setting is pending on PS5");
}

void activate_track() noexcept
{
    avdec::Track track{};
    if (g_panel == Panel::subtitles)
    {
        if (g_panel_row == 0)
            g_panel = Panel::transport;
        else
            screen::notify("Subtitle rendering is pending on PS5");
        return;
    }
    if (!avdec::track(avdec::TrackKind::audio, g_panel_row, track))
        screen::notify(g_src_rate > 0 ? "Using the default audio track" : "No audio tracks available");
    else if (track.stream == avdec::selected_audio_track())
        screen::notify("Audio track already selected");
    else if (!track.supported)
        screen::notify("No decoder for this audio track");
    else if (g_use_ffmpeg && avdec::progressive())
        screen::notify("Audio track switching during torrent playback is pending");
    else if (!avdec::select_audio_track(track.stream))
        screen::notify("Could not switch to this audio track");
}

} // namespace

Kind classify(const char *path) noexcept
{
    if (!path || !path[0])
        return Kind::none;
    if (ends_with_ci(path, ".mpg") || ends_with_ci(path, ".mpeg") || ends_with_ci(path, ".mpv") ||
        ends_with_ci(path, ".m1v"))
        return avdec::available() ? Kind::ffmpeg : Kind::mpeg1;
    if (ends_with_ci(path, ".mp4") || ends_with_ci(path, ".m4v") || ends_with_ci(path, ".mov") ||
        ends_with_ci(path, ".mkv") || ends_with_ci(path, ".ts") || ends_with_ci(path, ".m2ts") ||
        ends_with_ci(path, ".avi") || ends_with_ci(path, ".webm") || ends_with_ci(path, ".m2v"))
        return avdec::available() ? Kind::ffmpeg : Kind::later;
    return Kind::none;
}

bool open(const char *path, char *why, int why_cap, bool from_start,
          const torrent::VerifiedFile *stream) noexcept
{
    close();
    if (!path || !path[0])
    {
        copy_text(why, why_cap, "empty path");
        return false;
    }
    const Kind kind = classify(path);
    if (kind == Kind::later || kind == Kind::none)
    {
        copy_text(why, why_cap, avdec::available()
                                    ? "this file is not a supported video"
                                    : "this build plays MPEG-1 (.mpg) only; H.264 needs FFmpeg");
        return false;
    }

    if (kind == Kind::ffmpeg)
    {
        if (!audio::init())
            fprintf(stderr, "[movian] player: audio init failed\n");
        audio::set_volume(settings::get().volume);
        if (!avdec::open(path, why, why_cap, on_ffmpeg_pcm, stream))
            return false;
        g_use_ffmpeg = true;
        g_src_w = avdec::width();
        g_src_h = avdec::height();
        g_duration = avdec::duration();
        copy_text(g_path, static_cast<int>(sizeof g_path), path);
        copy_text(g_title, static_cast<int>(sizeof g_title), basename_of(path));
        g_paused = false;
        g_ended = false;
        g_torrent_error = false;
        reset_overlay();
        g_last_ms = screen::now_ms();
        g_saved_ms = g_opened_ms = g_last_ms;
        g_error[0] = 0;
        double resume_at = 0;
        if (!stream && !from_start && settings::get().resume && settings::resume_for(path, &resume_at, nullptr) &&
            resume_at > 1.0)
            avdec::seek(resume_at);
        return true;
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
    g_torrent_error = false;
    reset_overlay();
    g_last_ms = screen::now_ms();
    g_saved_ms = g_opened_ms = g_last_ms;
    g_error[0] = 0;
    if (!audio::init())
    {
        fprintf(stderr, "[movian] player: audio init failed\n");
    }
    audio::set_volume(settings::get().volume);
    audio::clear();

    double resume_at = 0;
    if (!from_start && settings::get().resume && settings::resume_for(path, &resume_at, nullptr) &&
        resume_at > 1.0)
    {
        plm_seek(g_plm, resume_at, TRUE);
        g_ended = plm_has_ended(g_plm) != 0;
    }

    (void)picture_w;
    (void)picture_h;
    return true;
}

void close() noexcept
{
    if ((g_plm || g_use_ffmpeg) && !g_ended)
        persist_resume();
    if (g_plm)
    {
        plm_destroy(g_plm);
        g_plm = nullptr;
    }
    if (g_use_ffmpeg)
    {
        g_picture.pixels = nullptr;
        avdec::close();
        g_use_ffmpeg = false;
    }
    free_picture();
    audio::clear();
    g_path[0] = 0;
    g_title[0] = 0;
    g_paused = false;
    g_ended = false;
    g_torrent_error = false;
    reset_overlay();
    g_src_w = g_src_h = g_src_rate = 0;
    g_duration = 0;
}

bool active() noexcept
{
    return g_plm != nullptr || g_use_ffmpeg;
}

bool playing() noexcept
{
    return (g_plm || g_use_ffmpeg) && !g_paused && !g_ended;
}

const char *path() noexcept
{
    return g_path;
}

const char *title() noexcept
{
    return g_title;
}

double time() noexcept
{
    if (g_use_ffmpeg)
        return avdec::time();
    return g_plm ? plm_get_time(g_plm) : 0;
}

double duration() noexcept
{
    return g_duration;
}

bool ended() noexcept
{
    return (g_plm || g_use_ffmpeg) && g_ended && !g_torrent_error;
}

int width() noexcept
{
    return g_src_w;
}

int height() noexcept
{
    return g_src_h;
}

void toggle_pause() noexcept
{
    if (!active()) return;
    if (g_ended)
    {
        if (!can_seek()) return;
        seek_to(0);
        g_paused = false;
    }
    else
        g_paused = !g_paused;
    if (g_paused) audio::clear();
    g_last_ms = screen::now_ms();
    persist_resume();
}

void seek_step_by(int direction) noexcept
{
    if (active()) seek_by(direction * seek_step());
}

bool handle(unsigned pressed, bool (*skip)(int, bool)) noexcept
{
    if (!active() || !pressed)
        return false;
    if (pressed & pad::triangle)
    {
        if (g_panel == Panel::hidden)
        {
            g_panel = Panel::transport;
            g_control = default_control;
        }
        else
            reset_overlay();
        return true;
    }
    if (g_panel != Panel::hidden && (pressed & pad::circle))
    {
        if (g_panel == Panel::transport && g_scrubbing)
            g_scrubbing = false;
        else if (g_panel == Panel::transport)
            reset_overlay();
        else if (g_panel == Panel::video || g_panel == Panel::sound || g_panel == Panel::caption)
        {
            g_panel_row = g_panel == Panel::video ? 0 : g_panel == Panel::sound ? 1 : 2;
            g_panel = Panel::settings;
        }
        else
            g_panel = Panel::transport;
        return true;
    }
    if (g_panel == Panel::transport)
    {
        if (pressed & pad::up)
        {
            if (can_seek() && g_duration > 0 && !g_scrubbing)
            {
                g_scrubbing = true;
                g_seek_preview = std::clamp(time(), 0.0, g_duration);
            }
            else if (!g_scrubbing)
                screen::notify("Seeking is unavailable for this video");
        }
        if (pressed & pad::down)
            g_scrubbing = false;

        const int direction = !!(pressed & pad::right) - !!(pressed & pad::left);
        if (g_scrubbing)
        {
            const double step = std::min(seek_step(), g_duration / 20.0);
            g_seek_preview = std::clamp(g_seek_preview + direction * step, 0.0, g_duration);
            if (pressed & pad::cross)
            {
                seek_to(g_seek_preview);
                g_scrubbing = false;
            }
        }
        else
        {
            g_control = std::clamp(g_control + direction, 0, static_cast<int>(std::size(control_labels)) - 1);
            if (pressed & pad::cross)
                open_control(static_cast<Control>(g_control), skip);
        }
        return true;
    }
    if (g_panel == Panel::audio || g_panel == Panel::subtitles)
    {
        const int delta = !!(pressed & pad::down) - !!(pressed & pad::up) +
                          visible_tracks * (!!(pressed & pad::r2) - !!(pressed & pad::l2));
        move_picker(g_panel_row + delta);
        if (pressed & pad::cross)
            activate_track();
        return true;
    }
    if (g_panel != Panel::hidden)
    {
        const int count = rows_for(g_panel).second;
        const int delta = !!(pressed & pad::down) - !!(pressed & pad::up);
        g_panel_row = (g_panel_row + delta + count) % count;
        if (pressed & (pad::cross | pad::left | pad::right))
            change_setting((pressed & pad::left) ? -1 : 1, (pressed & pad::cross) != 0);
        return true;
    }
    if (pressed & pad::cross)
        toggle_pause();
    if (pressed & pad::left)
        seek_by(-seek_step());
    if (pressed & pad::right)
        seek_by(seek_step());
    if (pressed & pad::up)
        seek_by(seek_step());
    if (pressed & pad::down)
        seek_by(-seek_step());
    if (pressed & pad::l2)
        seek_by(-60.0);
    if (pressed & pad::r2)
        seek_by(60.0);
    if (pressed & pad::square)
    {
        if (g_path[0] && settings::toggle_bookmark(g_path, g_title))
            screen::notify("Bookmarked");
        else
            screen::notify("Bookmark removed");
    }
    if (pressed & pad::circle)
    {
        close();
        return true;
    }
    return true;
}

bool tick() noexcept
{
    if (!g_plm && !g_use_ffmpeg)
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
        if (g_use_ffmpeg)
        {
            avdec::tick(dt);
            const auto end = avdec::end_state();
            if (end != avdec::EndState::pending && audio::drained())
            {
                g_ended = true;
                g_paused = true;
                audio::log_drain();
                g_torrent_error = end == avdec::EndState::torrent_error;
                if (g_torrent_error)
                {
                    persist_resume();
                    screen::notify("Torrent playback interrupted. Reopen the torrent to retry.");
                }
                else
                    settings::clear_resume();
            }
        }
        else
        {
            plm_decode(g_plm, dt);
            if (plm_has_ended(g_plm) && audio::drained())
            {
                g_ended = true;
                g_paused = true;
                settings::clear_resume();
            }
        }
        if (!g_ended && now - g_saved_ms > 4000)
        {
            persist_resume();
            g_saved_ms = now;
        }
    }
    return true;
}

static void draw_picture(screen::Canvas &c, const image::Image &picture) noexcept
{
    if (picture.loaded())
    {
        // Fit inside 1920x900, keep aspect.
        const float max_w = 1920.f;
        const float max_h = 900.f;
        float w = static_cast<float>(picture.w);
        float h = static_cast<float>(picture.h);
        const float s = w / max_w > h / max_h ? max_w / w : max_h / h;
        w *= s;
        h *= s;
        const int x = static_cast<int>((1920.f - w) * 0.5f);
        const int y = static_cast<int>((900.f - h) * 0.5f) + 40;
        c.image(x, y, static_cast<int>(w), static_cast<int>(h), picture);
    }
}

void draw(screen::Canvas &c) noexcept
{
    c.clear(Color::bg);
    if (g_use_ffmpeg)
        avdec::with_picture([](void *context, const std::uint32_t *pixels, int width, int height) {
            const image::Image picture{width, height, const_cast<std::uint32_t *>(pixels)};
            draw_picture(*static_cast<screen::Canvas *>(context), picture);
        }, &c);
    else
        draw_picture(c, g_picture);

    if (g_panel != Panel::hidden && g_panel != Panel::transport)
    {
        constexpr int x = 1020, y = 60, w = 840, line = 58;
        const bool picker = g_panel == Panel::audio || g_panel == Panel::subtitles;
        const auto [rows, settings_count] = rows_for(g_panel);
        const char *heading = g_panel == Panel::audio ? "Audio / Video" :
                             g_panel == Panel::subtitles ? "Subtitles" :
                             g_panel == Panel::settings ? "Playback settings" :
                             settings_rows[g_panel == Panel::video ? 0 : g_panel == Panel::sound ? 1 : 2];
        const int count = picker ? picker_count() : settings_count;
        const int first = picker ? g_first_track : 0;
        const int end = std::min(count, first + visible_tracks);
        const int height = 138 + (end - first) * line;
        c.round_rect(x, y, w, height, 16, Color::surface, 242);
        c.round_frame(x, y, w, height, 16, 2, Color::line);
        c.text(x + 34.f, y + 54.f, heading, font::semibold, 30, Color::text);
        if (count > visible_tracks)
        {
            char page[24];
            std::snprintf(page, sizeof page, "%d / %d", g_panel_row + 1, count);
            c.text(x + 710.f, y + 52.f, page, font::mono_medium, 18, Color::muted);
        }
        for (int row = first; row < end; ++row)
        {
            const int top = y + 78 + (row - first) * line;
            const bool focused = row == g_panel_row;
            if (focused)
                c.round_rect(x + 16, top, w - 32, line - 4, 10, Color::hover);
            char value[40]{};
            char description[112]{};
            const char *label = nullptr;
            bool selected = false;
            avdec::Track track{};
            if (g_panel == Panel::audio)
            {
                if (avdec::track(avdec::TrackKind::audio, row, track))
                {
                    label = track.title;
                    selected = track.stream == avdec::selected_audio_track();
                    std::snprintf(description, sizeof description, "%s  %s%s",
                                  track.language[0] ? track.language : "Undetermined", track.format,
                                  track.supported ? "" : "  (pending)");
                }
                else
                {
                    selected = g_src_rate > 0;
                    label = selected ? "Default audio" : "No audio tracks available";
                }
            }
            else if (g_panel == Panel::subtitles)
            {
                if (row == 0)
                {
                    selected = true;
                    label = "Off";
                    if (count == 1)
                        copy_text(description, sizeof description, "No embedded subtitles found");
                }
                else if (avdec::track(avdec::TrackKind::subtitle, row - 1, track))
                {
                    label = track.title;
                    std::snprintf(description, sizeof description, "%s  %s  (pending)",
                                  track.language[0] ? track.language : "Undetermined", track.format);
                }
            }
            else
            {
                label = rows[row];
                if (g_panel == Panel::video)
                {
                    if (row == 0) std::snprintf(value, sizeof value, "%ds", settings::get().seek_step);
                    if (row == 1) copy_text(value, sizeof value, settings::repeat_label());
                    if (row == 2) copy_text(value, sizeof value, settings::get().shuffle ? "On" : "Off");
                    if (row == 3) copy_text(value, sizeof value, settings::is_bookmarked(g_path) ? "Saved" : "Add");
                    if (row == 4) copy_text(value, sizeof value, "Pending");
                }
                else if (g_panel == Panel::sound)
                {
                    if (row == 0) copy_text(value, sizeof value, audio::muted() ? "On" : "Off");
                    if (row == 1) std::snprintf(value, sizeof value, "%d%%", audio::volume());
                    if (row == 2) copy_text(value, sizeof value, "Pending");
                }
                else if (g_panel == Panel::caption)
                    copy_text(value, sizeof value, "Pending");
            }
            if (selected)
                c.rect(x + 30, top + 20, 10, 10, Color::accent);
            c.clip(x + 60, top, description[0] ? 382 : w - 110, line);
            c.text(x + 60.f, top + 34.f, label ? label : "", font::medium, 22, Color::text);
            c.unclip();
            if (description[0])
            {
                c.clip(x + 465, top, w - 485, line);
                c.text(x + 465.f, top + 34.f, description, font::regular, 17, Color::muted);
                c.unclip();
            }
            if (value[0])
                c.text(x + 650.f, top + 34.f, value, font::mono_medium, 18, Color::accent);
        }
        c.text(x + 34.f, y + height - 23.f, "Cross select    Circle back", font::regular, 18, Color::muted);
        return;
    }

    const bool transport = g_panel == Panel::transport;
    if (!transport && !g_paused && !g_ended && !g_torrent_error &&
        screen::now_ms() - g_opened_ms > 2600)
        return;

    constexpr int bar_x = 60, bar_w = 1800;
    const int bar_y = transport ? 928 : 1008;
    const double shown = g_scrubbing ? g_seek_preview : time();
    const double fraction = g_duration > 0 && std::isfinite(shown) ?
                            std::clamp(shown / g_duration, 0.0, 1.0) : 0.0;
    const int filled = static_cast<int>(bar_w * fraction);
    c.rect(0, bar_y - 68, screen::width, screen::height - bar_y + 68, Color::surface, 235);
    char badge[64];
    std::snprintf(badge, sizeof badge, "%s %s  %dx%d",
                  g_use_ffmpeg ? avdec::mode() : "SW", g_use_ffmpeg ? avdec::codec_name() : "MPEG-1",
                  g_src_w, g_src_h);
    const float badge_x = 1860.f - screen::Canvas::text_width(badge, font::mono_medium, 18);
    c.clip(60, bar_y - 68, static_cast<int>(badge_x) - 96, 65);
    c.text(60.f, bar_y - 25.f, g_title, font::semibold, 24, Color::text);
    c.unclip();
    c.text(badge_x, bar_y - 25.f, badge, font::mono_medium, 18, Color::accent);

    c.round_rect(bar_x, bar_y, bar_w, 6, 3, Color::line);
    if (filled > 0)
        c.round_rect(bar_x, bar_y, filled, 6, 3, Color::accent);
    c.round_rect(bar_x + filled - 7, bar_y - 4, 14, 14, 7, g_scrubbing ? Color::accent : Color::text);
    char elapsed[16], total[16];
    format_time(elapsed, sizeof elapsed, shown);
    format_time(total, sizeof total, g_duration);
    if (g_duration <= 0)
        copy_text(total, sizeof total, "--:--");
    c.text(60.f, bar_y + 43.f, elapsed, font::mono_medium, 19, Color::text);
    c.text(1860.f - screen::Canvas::text_width(total, font::mono_medium, 19),
           bar_y + 43.f, total, font::mono_medium, 19, Color::text);

    if (transport)
    {
        constexpr int control_x[]{145, 423, 610, 797, 984, 1171, 1410, 1600};
        for (int i = 0; i < static_cast<int>(std::size(control_labels)); ++i)
        {
            const bool focused = !g_scrubbing && i == g_control;
            if (focused)
                c.round_rect(control_x[i], 946, 176, 48, 10, Color::hover);
            const char *label = i == default_control ? (playing() ? "Pause" : "Play") : control_labels[i];
            c.text(control_x[i] + 88.f - screen::Canvas::text_width(label, font::medium, 20) / 2.f,
                   977.f, label, font::medium, 20, focused ? Color::text : Color::muted);
        }
    }
    const char *hint = g_torrent_error ? "Torrent interrupted" : g_ended ? "Playback ended" :
        g_scrubbing ? "Left / Right adjust    Cross seek    Circle cancel" :
        transport ? "Up seek bar    Cross select    Circle hide" :
        g_paused ? "Paused    Cross resume    Triangle controls" :
        "Cross pause    Triangle controls    Options sidebar    Circle back";
    const float hint_x = 960.f - screen::Canvas::text_width(hint, font::regular, 18) / 2.f;
    c.text(hint_x, transport ? 1044.f : 1051.f, hint, font::regular, 18, Color::muted);
}
} // namespace movian::player
