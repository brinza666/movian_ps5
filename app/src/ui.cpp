/*
 * Movian for PS5 — DualSense shell with a Phase 1 Videos file browser.
 *
 * Honest about what this build does: DualSense navigation, four Movian
 * places, and a real directory listing of /download0, /data/movian, and
 * USB. Playback, plugins, and BitTorrent are not implemented in this
 * binary. They are the next porting phases.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ui.hpp"

#include "fs.hpp"
#include "pad.hpp"
#include "paths.hpp"

#include <cstddef>
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

constexpr const char *kVersion = "0.2.0-browse";
constexpr const char *kTitleId = "PPSA99455";

// ---- Videos browser --------------------------------------------------------
constexpr int max_entries = 80;
constexpr int max_depth = 8;
constexpr int row_h = 76;
constexpr int list_x = 72;
constexpr int list_y = 280;
constexpr int list_w = 1776;
constexpr int list_h = 680;
constexpr int rows_in_view = list_h / row_h;

constexpr const char *kUsb0 =
#ifdef MOVIAN_HOST_PREVIEW
    "usb0";
#else
    "/mnt/usb0";
#endif

struct BrowseLevel
{
    char path[fs::max_path + 1];
    char title[48];
    int selected;
    int first;
};

enum class RowKind : unsigned char
{
    folder,
    file,
    message,
};

struct Row
{
    char title[96];
    char detail[96];
    char path[fs::max_path + 1];
    RowKind kind;
    bool openable;
};

BrowseLevel g_stack[max_depth];
int g_depth = 0;
Row g_rows[max_entries];
int g_row_count = 0;
int g_truncated = 0;
float g_scroll = 0;
bool g_videos_dirty = true;
char g_selected_note[160] = "";

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

void copy_text(char *dst, std::size_t cap, const char *src) noexcept
{
    if (!dst || cap == 0)
        return;
    if (!src)
    {
        dst[0] = 0;
        return;
    }
    snprintf(dst, cap, "%s", src);
}

const char *kind_label(fs::Kind k) noexcept
{
    switch (k)
    {
    case fs::Kind::directory:
        return "folder";
    case fs::Kind::video:
        return "video";
    case fs::Kind::audio:
        return "audio";
    default:
        return "file";
    }
}

void keep_in_view() noexcept
{
    BrowseLevel &lv = g_stack[g_depth];
    if (g_row_count <= 0)
    {
        lv.selected = 0;
        lv.first = 0;
        return;
    }
    if (lv.selected < 0)
        lv.selected = 0;
    if (lv.selected >= g_row_count)
        lv.selected = g_row_count - 1;
    if (lv.selected < lv.first)
        lv.first = lv.selected;
    if (lv.selected >= lv.first + rows_in_view)
        lv.first = lv.selected - rows_in_view + 1;
    const int last_first = g_row_count > rows_in_view ? g_row_count - rows_in_view : 0;
    if (lv.first > last_first)
        lv.first = last_first;
    if (lv.first < 0)
        lv.first = 0;
}

void add_message_row(const char *title, const char *detail) noexcept
{
    if (g_row_count >= max_entries)
        return;
    Row &r = g_rows[g_row_count++];
    copy_text(r.title, sizeof r.title, title);
    copy_text(r.detail, sizeof r.detail, detail);
    r.path[0] = 0;
    r.kind = RowKind::message;
    r.openable = false;
}

void add_folder_row(const char *title, const char *detail, const char *path, bool openable) noexcept
{
    if (g_row_count >= max_entries)
        return;
    Row &r = g_rows[g_row_count++];
    copy_text(r.title, sizeof r.title, title);
    copy_text(r.detail, sizeof r.detail, detail);
    copy_text(r.path, sizeof r.path, path);
    r.kind = RowKind::folder;
    r.openable = openable;
}

void add_file_row(const fs::Entry &e) noexcept
{
    if (g_row_count >= max_entries)
        return;
    Row &r = g_rows[g_row_count++];
    copy_text(r.title, sizeof r.title, e.name);
    snprintf(r.detail, sizeof r.detail, "%s — playback is Phase 2", kind_label(e.kind));
    copy_text(r.path, sizeof r.path, e.path);
    r.kind = RowKind::file;
    r.openable = false;
}

const char *errno_hint(int err) noexcept
{
    switch (err)
    {
    case ENOENT:
        return "path not found";
    case EACCES:
    case EPERM:
        return "sandbox cannot open this path";
    case ENOTDIR:
        return "not a directory";
    case EIO:
        return "I/O error";
    default:
        return "cannot open path";
    }
}

void load_directory(const char *path) noexcept
{
    g_row_count = 0;
    g_truncated = 0;
    fs::Entry entries[max_entries];
    int n = 0;
    int err = 0;
    if (fs::list_dir(path, entries, max_entries, &n, &err) != 0)
    {
        char title[96];
        snprintf(title, sizeof title, "Cannot open %s", path);
        char detail[96];
        snprintf(detail, sizeof detail, "%s (errno %d)", errno_hint(err), err);
        add_message_row(title, detail);
        fprintf(stderr, "[movian] opendir %s failed errno=%d\n", path, err);
        return;
    }
    g_truncated = err;
    int folders = 0, files = 0;
    for (int i = 0; i < n; ++i)
    {
        if (entries[i].kind == fs::Kind::directory)
        {
            add_folder_row(entries[i].name, "folder", entries[i].path, true);
            ++folders;
        }
        else
        {
            add_file_row(entries[i]);
            ++files;
        }
    }
    if (g_row_count == 0)
        add_message_row("No video or audio files here",
                        "Copy a clip into this folder over FTP while Movian is closed.");
    if (g_truncated > 0)
    {
        char extra[96];
        snprintf(extra, sizeof extra, "+%d more not shown (cap %d)", g_truncated, max_entries);
        add_message_row(extra, "Listing truncated");
    }
    snprintf(g_status, sizeof g_status, "%s  %d folders, %d files", path, folders, files);
}

void load_videos_root() noexcept
{
    g_row_count = 0;
    g_truncated = 0;

    const struct Source
    {
        const char *title;
        const char *path;
        const char *hint;
    } sources[] = {
        {"Title data", MOVIAN_DOWNLOAD0, "settings, log, test clips"},
        {"Movian library", MOVIAN_DATA_DIR, "FTP extra media if the sandbox can see /data"},
        {"USB", kUsb0, "often hidden until a helper lifts the sandbox"},
    };

    for (const Source &s : sources)
    {
        fs::Entry entries[4];
        int n = 0;
        int err = 0;
        const int opened = fs::list_dir(s.path, entries, 4, &n, &err);
        char detail[96];
        if (opened != 0)
            snprintf(detail, sizeof detail, "%s — %s (errno %d)", s.hint, errno_hint(err), err);
        else if (err > 0)
            snprintf(detail, sizeof detail, "%s — opened, %d+ entries", s.hint, n + err);
        else
            snprintf(detail, sizeof detail, "%s — %d listed", s.hint, n);
        add_folder_row(s.title, detail, s.path, true);
    }

    add_message_row("Playback is not in this build",
                    "Cross opens a folder. Circle goes back. Phase 2 will play the file.");
    snprintf(g_status, sizeof g_status, "Videos roots: %s, %s, %s", MOVIAN_DOWNLOAD0, MOVIAN_DATA_DIR,
             kUsb0);
}

void reload_videos() noexcept
{
    const BrowseLevel &lv = g_stack[g_depth];
    if (lv.path[0] == 0)
        load_videos_root();
    else
        load_directory(lv.path);
    keep_in_view();
    g_scroll = static_cast<float>(lv.first * row_h);
    g_videos_dirty = false;
}

void enter_videos() noexcept
{
    g_depth = 0;
    g_stack[0].path[0] = 0;
    copy_text(g_stack[0].title, sizeof g_stack[0].title, "Videos");
    g_stack[0].selected = 0;
    g_stack[0].first = 0;
    g_selected_note[0] = 0;
    g_videos_dirty = true;
}

void push_folder(const char *path, const char *title) noexcept
{
    if (g_depth + 1 >= max_depth)
        return;
    ++g_depth;
    copy_text(g_stack[g_depth].path, sizeof g_stack[g_depth].path, path);
    copy_text(g_stack[g_depth].title, sizeof g_stack[g_depth].title, title);
    g_stack[g_depth].selected = 0;
    g_stack[g_depth].first = 0;
    g_selected_note[0] = 0;
    g_videos_dirty = true;
}

bool pop_folder() noexcept
{
    if (g_depth > 0)
    {
        --g_depth;
        g_selected_note[0] = 0;
        g_videos_dirty = true;
        return true;
    }
    return false;
}

void select_row() noexcept
{
    if (g_row_count <= 0)
        return;
    const Row &r = g_rows[g_stack[g_depth].selected];
    if (r.kind == RowKind::folder && r.openable && r.path[0])
    {
        push_folder(r.path, r.title);
        char line[160];
        snprintf(line, sizeof line, "open %s", r.path);
        append_log(line);
        return;
    }
    if (r.kind == RowKind::file)
    {
        snprintf(g_selected_note, sizeof g_selected_note,
                 "Selected %s — no decoder in this binary (Phase 2).", r.title);
        snprintf(g_status, sizeof g_status, "selected %s", r.title);
        screen::notify("No playback yet (Phase 2)");
        char line[160];
        snprintf(line, sizeof line, "select %s", r.path);
        append_log(line);
        return;
    }
    if (r.detail[0])
        screen::notify(r.detail);
}

void move_selection(int delta) noexcept
{
    if (g_row_count <= 0)
        return;
    BrowseLevel &lv = g_stack[g_depth];
    int next = lv.selected + delta;
    if (next < 0)
        next = 0;
    if (next >= g_row_count)
        next = g_row_count - 1;
    lv.selected = next;
    keep_in_view();
}

const char *fit(const char *text, int room, char *buf, std::size_t buf_bytes, font::Face face,
                int px) noexcept
{
    constexpr const char *dots = "\xE2\x80\xA6";
    if (screen::Canvas::text_width(text, face, px) <= static_cast<float>(room))
        return text;
    const int len = static_cast<int>(strlen(text));
    int lo = 0, hi = len;
    while (hi - lo > 1)
    {
        int mid = (lo + hi) / 2;
        while (mid > lo && (static_cast<unsigned char>(text[mid]) & 0xc0) == 0x80)
            --mid;
        if (mid == lo)
            break;
        if (static_cast<std::size_t>(mid) + 4 >= buf_bytes)
        {
            hi = mid;
            continue;
        }
        memcpy(buf, text, static_cast<std::size_t>(mid));
        strcpy(buf + mid, dots);
        if (screen::Canvas::text_width(buf, face, px) <= static_cast<float>(room))
            lo = mid;
        else
            hi = mid;
    }
    if (static_cast<std::size_t>(lo) + 4 >= buf_bytes)
        lo = static_cast<int>(buf_bytes) - 5;
    if (lo < 0)
        lo = 0;
    memcpy(buf, text, static_cast<std::size_t>(lo));
    strcpy(buf + lo, dots);
    return buf;
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
    const char *hint = g_place == Place::videos
                           ? "L1 / R1  places    Cross  open    Circle  back    L2 / R2  page"
                           : "L1 / R1  places    Cross  open    Circle  back    Options  status";
    c.text(48.f, 1052.f, hint, font::regular, 18, Color::muted);
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
    c.text(48.f, 208.f, "Phase 1 browse. Local files list for real; USB may be sandboxed.",
           font::regular, 20, Color::muted);

    const char *titles[4] = {"Local files", "USB storage", "Network", "Plugins"};
    const char *bodies[4] = {
        "/download0 and /data/movian — Videos lists them",
        "/mnt/usb0 — best-effort; Cross opens Videos",
        "SMB2 / HTTP / HLS — later phases",
        "Duktape API v2 — later phase",
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

void draw_videos(screen::Canvas &c) noexcept
{
    if (g_videos_dirty)
        reload_videos();

    const BrowseLevel &lv = g_stack[g_depth];
    c.text(48.f, 160.f, lv.title, font::semibold, 40, Color::text);
    const char *crumb = lv.path[0] ? lv.path : "Sources  ·  video/audio only  ·  no decoder yet";
    char crumb_buf[160];
    c.text(48.f, 208.f, fit(crumb, 1500, crumb_buf, sizeof crumb_buf, font::regular, 20),
           font::regular, 20, Color::muted);

    c.round_rect(48, 236, 1824, 748, 20, Color::surface2);
    c.clip(list_x, list_y, list_w, list_h);
    const int first = static_cast<int>(g_scroll) / row_h;
    for (int i = first; i < g_row_count && i <= first + rows_in_view + 1; ++i)
    {
        const float y = static_cast<float>(list_y + i * row_h) - g_scroll;
        const int top = static_cast<int>(y);
        const bool chosen = i == lv.selected;
        const Row &r = g_rows[i];
        if (chosen)
            c.round_rect(list_x, top + 4, list_w, row_h - 8, 12, Color::hover);
        const char *mark = r.kind == RowKind::folder ? ">" : r.kind == RowKind::file ? "·" : "!";
        Color mark_c = r.kind == RowKind::message ? Color::orange : Color::accent;
        c.text(static_cast<float>(list_x + 16), y + 47, mark, font::semibold, 22, mark_c);

        char shown[128];
        c.text(static_cast<float>(list_x + 56), y + 36,
               fit(r.title, list_w - 80, shown, sizeof shown, font::medium, 24), font::medium, 24,
               chosen ? Color::text : Color::muted);
        if (r.detail[0])
            c.text(static_cast<float>(list_x + 56), y + 64,
                   fit(r.detail, list_w - 80, shown, sizeof shown, font::regular, 16),
                   font::regular, 16, Color::faint);
    }
    c.unclip();

    if (g_row_count > rows_in_view)
    {
        const int track_x = 48 + 1824 - 30;
        const int track_y = list_y;
        const int track_h = list_h;
        c.round_rect(track_x, track_y, 6, track_h, 3, Color::line);
        const float shown = static_cast<float>(rows_in_view) / static_cast<float>(g_row_count);
        const int thumb_h = static_cast<int>(static_cast<float>(track_h) * shown);
        const float most = static_cast<float>((g_row_count - rows_in_view) * row_h);
        float at = most > 0 ? g_scroll / most : 0;
        at = at < 0 ? 0 : at > 1 ? 1 : at;
        const int thumb_y = track_y + static_cast<int>((track_h - thumb_h) * at);
        c.round_rect(track_x, thumb_y, 6, thumb_h, 3, Color::muted);
    }

    if (g_selected_note[0])
        c.text(72.f, 980.f, g_selected_note, font::regular, 18, Color::orange);
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
    mkdir(MOVIAN_DATA_DIR, 0777);
    append_log("movian-ps5 browse start");
    snprintf(g_status, sizeof g_status, "PPSA99455 browse %s", kVersion);
    enter_videos();
    g_place = Place::home;
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
            if (g_place == Place::videos)
                enter_videos();
        }
    }
    else if (g_place == Place::videos)
    {
        if (g_videos_dirty)
            reload_videos();
        if (pressed & pad::up)
            move_selection(-1);
        if (pressed & pad::down)
            move_selection(1);
        if (pressed & pad::l2)
            move_selection(-rows_in_view);
        if (pressed & pad::r2)
            move_selection(rows_in_view);
        if (pressed & pad::cross)
            select_row();
        if (pressed & pad::circle)
        {
            if (!pop_folder())
                g_place = Place::home;
        }
        if (pressed & pad::triangle)
        {
            g_videos_dirty = true;
            screen::notify("Reloaded folder");
        }
    }
    else if (pressed & pad::circle)
    {
        g_place = Place::home;
    }

    if (pressed & pad::options)
        screen::notify(g_status);

    if (g_place == Place::videos)
    {
        const float target = static_cast<float>(g_stack[g_depth].first * row_h);
        const float gap = target - g_scroll;
        if (gap != 0)
        {
            g_scroll = (gap > -0.5f && gap < 0.5f) ? target : g_scroll + gap * 0.25f;
            dirty = true;
        }
    }

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
    case Place::videos:
        draw_videos(canvas);
        break;
    default:
        draw_placeholder(canvas, g_place);
        break;
    }
    if (g_place != Place::videos)
        draw_status_strip(canvas);
    draw_footer(canvas);
}
} // namespace movian::ui
