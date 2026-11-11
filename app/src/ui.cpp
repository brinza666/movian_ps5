/*
 * Movian for PS5 — DualSense shell with a Phase 1 Videos file browser.
 *
 * Honest about what this build does: DualSense navigation, four Movian
 * places, a real directory listing of /download0, /data/movian, and USB,
 * and software MPEG-1 playback (pl_mpeg). H.264/FFmpeg, plugins, and
 * BitTorrent are not in this binary.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ui.hpp"

#include "audio.hpp"
#include "fs.hpp"
#include "pad.hpp"
#include "paths.hpp"
#include "player.hpp"
#include "settings.hpp"

#include <cstddef>
#include <cstdint>
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

constexpr const char *kVersion = "0.4.0-options";
constexpr const char *kTitleId = "PPSA99455";
constexpr const char *kBookmarksUri = "bookmarks:";

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

enum class Overlay
{
    none,
    settings,
    item,
};

Overlay g_overlay = Overlay::none;
int g_settings_row = 0;
int g_item_row = 0;
char g_item_path[fs::max_path + 1] = "";
char g_item_title[96] = "";
bool g_item_folder = false;
bool g_item_file = false;

constexpr int setting_count = 9;
constexpr int item_action_count = 4;

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

bool path_is_dir(const char *path) noexcept
{
    if (!path || !path[0] || std::strcmp(path, kBookmarksUri) == 0)
        return false;
    struct stat st{};
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

void format_clock(char *dst, std::size_t cap) noexcept
{
    const time_t now = time(nullptr);
    const struct tm *tm = localtime(&now);
    if (!tm)
    {
        copy_text(dst, cap, "--:--");
        return;
    }
    if (settings::get().clock_24h)
    {
        snprintf(dst, cap, "%02d:%02d", tm->tm_hour, tm->tm_min);
        return;
    }
    int h = tm->tm_hour % 12;
    if (h == 0)
        h = 12;
    snprintf(dst, cap, "%d:%02d %s", h, tm->tm_min, tm->tm_hour >= 12 ? "PM" : "AM");
}

void format_resume_detail(char *dst, std::size_t cap, const settings::Resume &r) noexcept
{
    if (!r.valid)
    {
        copy_text(dst, cap, "Nothing saved yet — play an MPEG-1 file.");
        return;
    }
    const int t = static_cast<int>(r.time);
    const int d = static_cast<int>(r.duration);
    if (d > 0)
        snprintf(dst, cap, "%s  %d:%02d / %d:%02d", r.title, t / 60, t % 60, d / 60, d % 60);
    else
        snprintf(dst, cap, "%s  %d:%02d", r.title, t / 60, t % 60);
}

bool play_path(const char *path, const char *title, bool from_start) noexcept
{
    if (!path || !path[0])
        return false;
    char why[128];
    const player::Kind pk = player::classify(path);
    if (pk == player::Kind::mpeg1 && player::open(path, why, sizeof why, from_start))
    {
        snprintf(g_status, sizeof g_status, "playing %s", title && title[0] ? title : path);
        append_log(g_status);
        screen::notify(from_start ? "Play from start" : "MPEG-1");
        g_overlay = Overlay::none;
        return true;
    }
    if (pk == player::Kind::mpeg1)
    {
        snprintf(g_selected_note, sizeof g_selected_note, "%s", why);
        screen::notify(why);
    }
    else if (pk == player::Kind::later)
    {
        snprintf(g_selected_note, sizeof g_selected_note,
                 "%s — H.264/MP4 needs FFmpeg (not linked yet).", title ? title : path);
        screen::notify("Needs FFmpeg");
    }
    else
    {
        snprintf(g_selected_note, sizeof g_selected_note, "Cannot play %s", title ? title : path);
        screen::notify("Not a video");
    }
    return false;
}

const char *setting_title(int row) noexcept
{
    switch (row)
    {
    case 0:
        return "Resume playback";
    case 1:
        return "Seek step";
    case 2:
        return "Volume";
    case 3:
        return "24-hour clock";
    case 4:
        return "Debug log";
    case 5:
        return "Hardware decode";
    case 6:
        return "Show all files";
    case 7:
        return "Plugin repository";
    default:
        return "About";
    }
}

void setting_value(int row, char *dst, std::size_t cap) noexcept
{
    const settings::Data &d = settings::get();
    switch (row)
    {
    case 0:
        copy_text(dst, cap, d.resume ? "on" : "off");
        break;
    case 1:
        snprintf(dst, cap, "%d seconds", d.seek_step);
        break;
    case 2:
        snprintf(dst, cap, "%d%%", d.volume);
        break;
    case 3:
        copy_text(dst, cap, d.clock_24h ? "24h" : "12h");
        break;
    case 4:
        copy_text(dst, cap, d.debug_log ? "on" : "off");
        break;
    case 5:
        copy_text(dst, cap, d.hw_decode ? "prefer (not in this binary)" : "software MPEG-1");
        break;
    case 6:
        copy_text(dst, cap, d.show_all ? "on" : "media only");
        break;
    case 7:
        copy_text(dst, cap, settings::repo_label());
        break;
    default:
        copy_text(dst, cap, kVersion);
        break;
    }
}

void setting_nudge(int row, int delta) noexcept
{
    settings::Data &d = settings::get();
    switch (row)
    {
    case 0:
        d.resume = !d.resume;
        settings::save();
        screen::notify(d.resume ? "Resume on" : "Resume off");
        break;
    case 1:
        settings::cycle_seek(delta == 0 ? 1 : delta);
        screen::notify("Seek step");
        break;
    case 2:
        settings::cycle_volume(delta == 0 ? 1 : delta);
        audio::set_volume(settings::get().volume);
        screen::notify("Volume");
        break;
    case 3:
        d.clock_24h = !d.clock_24h;
        settings::save();
        break;
    case 4:
        d.debug_log = !d.debug_log;
        settings::save();
        screen::notify(d.debug_log ? "Debug log on" : "Debug log off");
        break;
    case 5:
        d.hw_decode = !d.hw_decode;
        settings::save();
        screen::notify("HW decode is stored for Phase 2b");
        break;
    case 6:
        d.show_all = !d.show_all;
        settings::save();
        g_videos_dirty = true;
        screen::notify(d.show_all ? "Showing all files" : "Media files only");
        break;
    case 7:
        settings::cycle_repo(delta == 0 ? 1 : delta);
        screen::notify("Repo URL stored — not fetched yet");
        break;
    default:
        screen::notify("Movian PS5 native title");
        break;
    }
}

const char *item_action_title(int row) noexcept
{
    switch (row)
    {
    case 0:
        return g_item_folder ? "Open" : "Play";
    case 1:
        return "Play from beginning";
    case 2:
        return settings::is_bookmarked(g_item_path) ? "Remove bookmark" : "Bookmark";
    default:
        return "Show path";
    }
}

bool item_action_enabled(int row) noexcept
{
    if (row == 0)
        return g_item_folder || g_item_file;
    if (row == 1)
        return g_item_file && player::classify(g_item_path) == player::Kind::mpeg1;
    if (row == 2)
        return g_item_path[0] != 0;
    return g_item_path[0] != 0;
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
    const player::Kind pk = player::classify(e.path);
    if (pk == player::Kind::mpeg1)
        snprintf(r.detail, sizeof r.detail, "MPEG-1 — Cross plays (software)");
    else if (pk == player::Kind::later)
        snprintf(r.detail, sizeof r.detail, "%s — needs FFmpeg (not in this binary)",
                 kind_label(e.kind));
    else
        snprintf(r.detail, sizeof r.detail, "%s", kind_label(e.kind));
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
    if (fs::list_dir(path, entries, max_entries, &n, &err, settings::get().show_all) != 0)
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
        const int opened = fs::list_dir(s.path, entries, 4, &n, &err, settings::get().show_all);
        char detail[96];
        if (opened != 0)
            snprintf(detail, sizeof detail, "%s — %s (errno %d)", s.hint, errno_hint(err), err);
        else if (err > 0)
            snprintf(detail, sizeof detail, "%s — opened, %d+ entries", s.hint, n + err);
        else
            snprintf(detail, sizeof detail, "%s — %d listed", s.hint, n);
        add_folder_row(s.title, detail, s.path, true);
    }

    char resume_detail[96];
    format_resume_detail(resume_detail, sizeof resume_detail, settings::last_resume());
    if (settings::last_resume().valid)
        add_folder_row("Continue watching", resume_detail, settings::last_resume().path, false);
    else
        add_message_row("Continue watching", resume_detail);

    char bm_detail[96];
    snprintf(bm_detail, sizeof bm_detail, "%d saved  ·  Square on a file to bookmark",
             settings::bookmark_count());
    add_folder_row("Bookmarks", bm_detail, kBookmarksUri, true);

    add_message_row("MPEG-1 plays in this build",
                    "Cross a .mpg to play. MP4/MKV/H.264 wait for FFmpeg.");
    snprintf(g_status, sizeof g_status, "Videos roots: %s, %s, %s", MOVIAN_DOWNLOAD0, MOVIAN_DATA_DIR,
             kUsb0);
}

void load_bookmarks() noexcept
{
    g_row_count = 0;
    g_truncated = 0;
    const int n = settings::bookmark_count();
    if (n == 0)
    {
        add_message_row("No bookmarks yet", "Square on a file or folder, or use item options.");
        snprintf(g_status, sizeof g_status, "Bookmarks empty");
        return;
    }
    for (int i = 0; i < n; ++i)
    {
        const char *path = settings::bookmark_path(i);
        const char *title = settings::bookmark_title(i);
        if (path_is_dir(path))
            add_folder_row(title, path, path, true);
        else
        {
            fs::Entry e{};
            copy_text(e.name, sizeof e.name, title);
            copy_text(e.path, sizeof e.path, path);
            e.kind = fs::classify_name(title, false);
            add_file_row(e);
        }
    }
    snprintf(g_status, sizeof g_status, "Bookmarks  %d", n);
}

void reload_videos() noexcept
{
    const BrowseLevel &lv = g_stack[g_depth];
    if (lv.path[0] == 0)
        load_videos_root();
    else if (std::strcmp(lv.path, kBookmarksUri) == 0)
        load_bookmarks();
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

void open_item_menu(const Row &r) noexcept
{
    if (r.path[0] == 0 && r.kind != RowKind::file && r.kind != RowKind::folder)
        return;
    copy_text(g_item_path, sizeof g_item_path, r.path);
    copy_text(g_item_title, sizeof g_item_title, r.title);
    g_item_folder = r.kind == RowKind::folder && r.openable;
    g_item_file = r.kind == RowKind::file;
    g_item_row = 0;
    g_overlay = Overlay::item;
}

void run_item_action(int row) noexcept
{
    if (!item_action_enabled(row))
        return;
    if (row == 0)
    {
        if (g_item_folder)
        {
            push_folder(g_item_path, g_item_title);
            g_overlay = Overlay::none;
            return;
        }
        play_path(g_item_path, g_item_title, false);
        return;
    }
    if (row == 1)
    {
        play_path(g_item_path, g_item_title, true);
        return;
    }
    if (row == 2)
    {
        if (settings::toggle_bookmark(g_item_path, g_item_title))
            screen::notify("Bookmarked");
        else
            screen::notify("Bookmark removed");
        g_videos_dirty = true;
        g_overlay = Overlay::none;
        return;
    }
    snprintf(g_selected_note, sizeof g_selected_note, "%s", g_item_path);
    screen::notify(g_item_path);
    g_overlay = Overlay::none;
}

void select_row() noexcept
{
    if (g_row_count <= 0)
        return;
    const Row &r = g_rows[g_stack[g_depth].selected];
    if (r.kind == RowKind::folder && r.path[0] && std::strcmp(r.path, kBookmarksUri) != 0 &&
        !r.openable && player::classify(r.path) == player::Kind::mpeg1)
    {
        play_path(r.path, r.title, false);
        return;
    }
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
        play_path(r.path, r.title, false);
        snprintf(g_status, sizeof g_status, "selected %s", r.title);
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
    char clock[24];
    format_clock(clock, sizeof clock);
    c.text(1760.f, 54.f, clock, font::mono_medium, 20, Color::muted);
}

void draw_footer(screen::Canvas &c) noexcept
{
    c.rect(0, 1008, screen::width, 72, Color::surface);
    c.rect(0, 1008, screen::width, 2, Color::line);
    const char *hint = g_overlay == Overlay::settings
                           ? "Cross toggle    Left/Right change    Options / Circle close"
                           : g_overlay == Overlay::item
                                 ? "Cross action    Circle close"
                                 : g_place == Place::videos
                                       ? "Cross open/play    Square item    Options settings    Circle back"
                                       : "L1 / R1 places    Cross open    Options settings    Circle back";
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
    c.text(48.f, 208.f, "Continue watching, folders, settings. MPEG-1 (.mpg) plays in software.",
           font::regular, 20, Color::muted);

    char resume_body[96];
    format_resume_detail(resume_body, sizeof resume_body, settings::last_resume());
    char bm_body[96];
    snprintf(bm_body, sizeof bm_body, "%d saved — Square on a Videos row to add one",
             settings::bookmark_count());
    const char *titles[4] = {"Continue watching", "Videos", "Bookmarks", "Settings"};
    const char *bodies[4] = {
        resume_body,
        "/download0, /data/movian, USB — Cross opens the browser",
        bm_body,
        "Resume, seek, volume, clock, debug log, plugin repo URL",
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
    const char *crumb = lv.path[0] ? lv.path : "Sources  ·  MPEG-1 plays  ·  other video needs FFmpeg";
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

void draw_settings_overlay(screen::Canvas &c) noexcept
{
    c.rect(0, 0, screen::width, screen::height, Color::bg, 140);
    c.round_rect(360, 120, 1200, 820, 20, Color::surface);
    c.round_frame(360, 120, 1200, 820, 20, 2, Color::accent, 180);
    c.text(400.f, 180.f, "Settings", font::semibold, 34, Color::text);
    c.text(400.f, 220.f, "Short M7 subset. Stored in /download0/settings.txt", font::regular, 18,
           Color::muted);
    for (int i = 0; i < setting_count; ++i)
    {
        const int y = 250 + i * 68;
        const bool on = i == g_settings_row;
        if (on)
            c.round_rect(390, y, 1140, 58, 12, Color::hover);
        c.text(420.f, static_cast<float>(y + 38), setting_title(i), font::medium, 22,
               on ? Color::text : Color::muted);
        char value[64];
        setting_value(i, value, sizeof value);
        char shown[64];
        c.text(980.f, static_cast<float>(y + 38),
               fit(value, 520, shown, sizeof shown, font::mono_medium, 18), font::mono_medium, 18,
               Color::accent);
    }
}

void draw_item_overlay(screen::Canvas &c) noexcept
{
    c.rect(0, 0, screen::width, screen::height, Color::bg, 140);
    c.round_rect(520, 220, 880, 560, 20, Color::surface);
    c.round_frame(520, 220, 880, 560, 20, 2, Color::accent, 180);
    c.text(560.f, 280.f, g_item_title[0] ? g_item_title : "Item", font::semibold, 28, Color::text);
    char shown[128];
    c.text(560.f, 320.f, fit(g_item_path, 800, shown, sizeof shown, font::regular, 16),
           font::regular, 16, Color::muted);
    for (int i = 0; i < item_action_count; ++i)
    {
        const int y = 360 + i * 80;
        const bool on = i == g_item_row;
        const bool enabled = item_action_enabled(i);
        if (on)
            c.round_rect(550, y, 820, 68, 12, Color::hover);
        c.text(580.f, static_cast<float>(y + 42), item_action_title(i), font::medium, 24,
               enabled ? (on ? Color::text : Color::muted) : Color::faint);
    }
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
    settings::load();
    audio::set_volume(settings::get().volume);
    if (settings::get().debug_log)
        append_log("movian-ps5 options start");
    snprintf(g_status, sizeof g_status, "PPSA99455 options %s", kVersion);
    (void)audio::init();
    enter_videos();
    g_place = Place::home;
    screen::notify("Movian PS5");
}

bool handle_overlay(unsigned pressed) noexcept
{
    if (g_overlay == Overlay::none)
        return false;
    if ((pressed & pad::circle) || (g_overlay == Overlay::settings && (pressed & pad::options)))
    {
        g_overlay = Overlay::none;
        return true;
    }
    if (g_overlay == Overlay::settings)
    {
        if (pressed & pad::up)
            g_settings_row = (g_settings_row + setting_count - 1) % setting_count;
        if (pressed & pad::down)
            g_settings_row = (g_settings_row + 1) % setting_count;
        if (pressed & pad::left)
            setting_nudge(g_settings_row, -1);
        if ((pressed & pad::right) || (pressed & pad::cross))
            setting_nudge(g_settings_row, 1);
        return true;
    }
    if (pressed & pad::up)
        g_item_row = (g_item_row + item_action_count - 1) % item_action_count;
    if (pressed & pad::down)
        g_item_row = (g_item_row + 1) % item_action_count;
    if (pressed & pad::cross)
        run_item_action(g_item_row);
    return true;
}

bool update() noexcept
{
    const unsigned pressed = pad::poll();
    bool dirty = pressed != 0;

    if (player::active())
    {
        dirty = player::handle(pressed) || dirty;
        dirty = player::tick() || dirty;
        return dirty;
    }

    if (handle_overlay(pressed))
        return true;

    if (pressed & pad::options)
    {
        g_overlay = Overlay::settings;
        return true;
    }

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
            if (g_home_row == 0)
            {
                const settings::Resume r = settings::last_resume();
                if (r.valid)
                    play_path(r.path, r.title, false);
                else
                    screen::notify("Nothing to continue");
            }
            else if (g_home_row == 1)
            {
                g_place = Place::videos;
                enter_videos();
            }
            else if (g_home_row == 2)
            {
                g_place = Place::videos;
                enter_videos();
                push_folder(kBookmarksUri, "Bookmarks");
            }
            else
            {
                g_overlay = Overlay::settings;
            }
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
        if ((pressed & pad::square) && g_row_count > 0)
            open_item_menu(g_rows[g_stack[g_depth].selected]);
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
    if (player::active())
    {
        player::draw(canvas);
        return;
    }
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
    if (g_overlay == Overlay::settings)
        draw_settings_overlay(canvas);
    else if (g_overlay == Overlay::item)
        draw_item_overlay(canvas);
    draw_footer(canvas);
}
} // namespace movian::ui
