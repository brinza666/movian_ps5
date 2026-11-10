/*
 * Movian for PS5 — Phase 0 native shell.
 *
 * Honest about what this build does: DualSense navigation, four Movian
 * places, and a status page. Playback, plugins, and BitTorrent are not
 * implemented in this binary. They are the next porting phases.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ui.hpp"

#include "pad.hpp"
#include "paths.hpp"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

namespace movian::ui
{
namespace
{
using screen::Color;

enum class Place
{
    home,
    videos,
    bittorrent,
    plugins,
};
constexpr int place_count = 4;

const char *const place_name[place_count] = {"Home", "Videos", "BitTorrent", "Plugins"};
const char *const place_blurb[place_count] = {
    "Sources, continue watching, and settings.",
    "Local files, USB, and network folders.",
    "Magnets and .torrent files — play while downloading.",
    "JavaScript plugins (Movian API v2) from a repository.",
};

Place g_place = Place::home;
int g_home_row = 0;
std::uint64_t g_started_ms = 0;
char g_status[96] = "shell ready";
int g_log_ok = 0;

constexpr const char *kVersion = "0.1.0-shell";
constexpr const char *kTitleId = "PPSA99455";

void append_log(const char *line) noexcept
{
    FILE *f = fopen(MOVIAN_LOG_PATH, "a");
    if (!f)
        return;
    time_t now = time(nullptr);
    fprintf(f, "%lld %s\n", static_cast<long long>(now), line);
    fclose(f);
    g_log_ok = 1;
}

void draw_tab_bar(screen::Canvas &c) noexcept
{
    c.rect(0, 0, screen::width, 88, Color::surface);
    c.rect(0, 88, screen::width, 2, Color::line);
    c.text(48.f, 58.f, "MOVIAN", font::semibold, 36, Color::accent);
    c.text(220.f, 54.f, "PS5", font::medium, 20, Color::muted);

    const float tab_x0 = 360.f;
    const float tab_w = 280.f;
    for (int i = 0; i < place_count; ++i)
    {
        const float x = tab_x0 + static_cast<float>(i) * tab_w;
        const bool on = static_cast<int>(g_place) == i;
        if (on)
            c.round_rect(static_cast<int>(x), 22, 250, 48, 10, Color::hover);
        c.text(x + 28.f, 54.f, place_name[i], on ? font::semibold : font::medium, 22,
               on ? Color::text : Color::muted);
    }
}

void draw_footer(screen::Canvas &c) noexcept
{
    c.rect(0, 1008, screen::width, 72, Color::surface);
    c.rect(0, 1008, screen::width, 2, Color::line);
    c.text(48.f, 1052.f, "L1 / R1  places    Cross  open    Circle  back    Options  status",
           font::regular, 18, Color::muted);
    c.text(1480.f, 1052.f, kVersion, font::mono_medium, 16, Color::faint);
}

void draw_card(screen::Canvas &c, int x, int y, int w, int h, const char *title, const char *body,
               bool selected) noexcept
{
    c.round_rect(x, y, w, h, 16, selected ? Color::hover : Color::surface2);
    if (selected)
        c.round_frame(x, y, w, h, 16, 3, Color::accent);
    c.text(static_cast<float>(x + 28), static_cast<float>(y + 48), title, font::semibold, 26,
           Color::text);
    c.text(static_cast<float>(x + 28), static_cast<float>(y + 92), body, font::regular, 18,
           Color::muted);
}

void draw_home(screen::Canvas &c) noexcept
{
    c.text(48.f, 160.f, "Home", font::semibold, 40, Color::text);
    c.text(48.f, 208.f, "Phase 0 shell. These tiles are the product map, not live backends.",
           font::regular, 20, Color::muted);

    const char *titles[4] = {"Local files", "USB storage", "Network", "Plugins"};
    const char *bodies[4] = {
        "/download0 and /data/movian — next phase",
        "/mnt/usb0 .. usb7 — next phase",
        "SMB2 / HTTP / HLS — later phases",
        "goja / Duktape API v2 — later phase",
    };
    for (int i = 0; i < 4; ++i)
    {
        const int col = i % 2;
        const int row = i / 2;
        draw_card(c, 48 + col * 920, 250 + row * 280, 880, 250, titles[i], bodies[i],
                  g_home_row == i);
    }
}

void draw_placeholder(screen::Canvas &c, Place place) noexcept
{
    const int idx = static_cast<int>(place);
    c.text(48.f, 160.f, place_name[idx], font::semibold, 40, Color::text);
    c.text(48.f, 208.f, place_blurb[idx], font::regular, 20, Color::muted);

    c.round_rect(48, 260, 1824, 680, 20, Color::surface2);
    c.text(80.f, 340.f, "Not in this build", font::semibold, 28, Color::accent);
    c.text(80.f, 400.f, "The previous repository claimed this feature was done.", font::regular, 22,
           Color::text);
    c.text(80.f, 444.f, "Those sources were stubs or a Go overlay that cannot boot on PS5.",
           font::regular, 22, Color::muted);
    c.text(80.f, 520.f, "Porting follows VLC for PS5: a native ShadowMount title, then",
           font::regular, 22, Color::muted);
    c.text(80.f, 564.f, "real libraries (FFmpeg, hardware decode, plugin host) one phase at a time.",
           font::regular, 22, Color::muted);
    c.text(80.f, 640.f, g_status, font::mono_medium, 20, Color::green);
}

void draw_status_strip(screen::Canvas &c) noexcept
{
    const std::uint64_t ms = screen::now_ms() - g_started_ms;
    char line[160];
    snprintf(line, sizeof line, "title %s   uptime %llu s   log %s   DualSense ready", kTitleId,
             static_cast<unsigned long long>(ms / 1000), g_log_ok ? "download0" : "none");
    c.text(48.f, 980.f, line, font::mono_medium, 16, Color::faint);
}
} // namespace

void load() noexcept
{
    g_started_ms = screen::now_ms();
    mkdir(MOVIAN_DOWNLOAD0, 0777);
    append_log("movian-ps5 shell start");
    snprintf(g_status, sizeof g_status, "PPSA99455 shell %s", kVersion);
    screen::notify("Movian PS5");
}

bool update() noexcept
{
    const unsigned pressed = pad::poll();
    bool dirty = pressed != 0;

    if (pressed & pad::l1)
        g_place = static_cast<Place>((static_cast<int>(g_place) + place_count - 1) % place_count);
    if (pressed & pad::r1)
        g_place = static_cast<Place>((static_cast<int>(g_place) + 1) % place_count);

    if (g_place == Place::home)
    {
        if (pressed & pad::right)
            g_home_row = (g_home_row + 1) % 4;
        if (pressed & pad::left)
            g_home_row = (g_home_row + 3) % 4;
        if (pressed & pad::down)
            g_home_row = (g_home_row + 2) % 4;
        if (pressed & pad::up)
            g_home_row = (g_home_row + 2) % 4;
        if (pressed & pad::cross)
        {
            static const Place jump[4] = {Place::videos, Place::videos, Place::bittorrent,
                                          Place::plugins};
            g_place = jump[g_home_row];
        }
    }
    else if (pressed & pad::circle)
    {
        g_place = Place::home;
    }

    if (pressed & pad::options)
        screen::notify(g_status);

    // Redraw on a slow clock so the uptime line stays honest even with no input.
    static std::uint64_t last_tick = 0;
    const std::uint64_t now = screen::now_ms();
    if (now - last_tick > 500)
    {
        last_tick = now;
        dirty = true;
    }
    return dirty;
}

void draw(screen::Canvas &canvas) noexcept
{
    canvas.clear(Color::bg);
    draw_tab_bar(canvas);
    switch (g_place)
    {
    case Place::home:
        draw_home(canvas);
        break;
    default:
        draw_placeholder(canvas, g_place);
        break;
    }
    draw_status_strip(canvas);
    draw_footer(canvas);
}
} // namespace movian::ui
