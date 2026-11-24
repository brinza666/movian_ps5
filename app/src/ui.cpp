/*
 * Movian for PS5 — DualSense shell with a Phase 1 Videos file browser.
 *
 * Honest about what this build does: DualSense navigation, four Movian
 * places, a real directory listing of /download0, /data/movian, and USB,
 * MPEG-1, FFmpeg playback and Videodec2. The Plugins tab hosts bundled
 * JavaScript services.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ui.hpp"

#include "audio.hpp"
#include "diagnostics.hpp"
#include "avdec.hpp"
#include "fs.hpp"
#include "network.hpp"
#include <cerrno>
#include "hwdec.h"
#include "keyboard.hpp"
#include "pad.hpp"
#include "paths.hpp"
#include "player.hpp"
#include "plugins.hpp"
#include "settings.hpp"
#include "settings_menu.hpp"
#include "torrent.hpp"
#include "stpp.hpp"
#include "title_access.hpp"
#include <utility>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <cstdint>
#include <iterator>
#include <span>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>

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
    "Open .torrent files and download from a direct peer.",
    "Installed JavaScript services.",
};

Place g_place = Place::home;
enum class HomeAction { plugins, network, settings, library, title_data, usb, resume, bookmarks };

struct HomeTile
{
    HomeAction action;
    const char *title;
    const char *detail;
};

constexpr HomeTile home_tiles[] = {
    {HomeAction::plugins, "Plugins", "Installed services and repository"},
    {HomeAction::network, "Local network", "Shares and media servers"},
    {HomeAction::settings, "Settings", "Playback and preferences"},
    {HomeAction::library, "Movian library", "Browse files in /data/movian"},
    {HomeAction::title_data, "Title data", "Files in the Movian title"},
    {HomeAction::usb, "USB", "Browse the first USB device"},
    {HomeAction::resume, "Continue watching", ""},
    {HomeAction::bookmarks, "Bookmarks", "Saved videos and folders"},
};
constexpr int home_columns = 5;
constexpr int last_home_tile = static_cast<int>(std::size(home_tiles)) - 1;
int g_home_tile = 0;
bool g_home_search_focused = false;
bool g_home_keyboard = false;
char g_home_query[keyboard::max_length * 4 + 1]{};
std::uint64_t g_started_ms = 0;
char g_status[96] = "shell ready";
int g_log_ok = 0;

constexpr const char *kVersion = "v63";
constexpr const char *kTitleId = "PPSA99456";
constexpr const char *kBookmarksUri = "bookmarks:";
constexpr const char *kSearchUri = "movian:local-search";

// ---- Videos browser --------------------------------------------------------
constexpr int max_entries = 80;
constexpr int max_depth = 8;
constexpr int row_h = 76;
constexpr int list_x = 72;
constexpr int list_y = 280;
constexpr int list_w = 1776;
constexpr int list_h = 680;
constexpr int rows_in_view = list_h / row_h;
constexpr int library_columns = 5;
constexpr int library_grid_rows = 4;
constexpr int library_grid_page = library_columns * library_grid_rows;

#ifdef MOVIAN_HOST_PREVIEW
constexpr const char *kUsb0 = "usb0";
#else
constexpr const char *kUsb0 = "/mnt/usb0";
#endif

struct BrowseLevel
{
    char path[fs::max_path + 1];
    char title[48];
    int selected;
    int first;
    char focused_path[fs::max_path + 1];
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
    long long size;
    long long mtime;
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
    setting_detail,
    resume_prompt,
    sidebar,
    plugin_details,
    browser_choice,
    menu_info,
    home_network,
    network_login,
    torrent_settings,
    item,
};

Overlay g_overlay = Overlay::none;
int g_settings_category = 0;
int g_settings_row = 0;
int g_setting_choice = 0;
const settings_menu::Row *g_settings_subpage = nullptr;
int g_settings_parent_row = 0;
bool g_settings_keyboard = false;
int g_network_keyboard = -1, g_network_slot = 0, g_network_row = 0;
int g_settings_text_action = -1;
network::Connection g_login_draft{};
int g_login_row=0;
char g_login_path[fs::max_path+1]{};
void open_network_login(const char *path) noexcept {
    if(!network::is_path(path) || path[6]<'0' || path[6]>='0'+network::max_connections) return;
    g_network_slot=path[6]-'0';
    g_login_draft=network::config().connections[g_network_slot];
    std::snprintf(g_login_path,sizeof g_login_path,"%s",path);
    g_login_row=0; g_overlay=Overlay::network_login;
}
char g_resume_path[fs::max_path + 1]{};
char g_resume_title[96]{};
int g_resume_choice = 0;
Overlay g_resume_parent = Overlay::none;
bool g_settings_in_category = false;
int g_torrent_settings_row = 0;
int g_item_row = 0;
int g_sidebar_row = 0;
bool g_settings_from_sidebar = false;
Overlay g_menu_parent = Overlay::none;
const char *g_menu_info_title = "";
constexpr int menu_info_lines = 8;
char g_menu_lines[menu_info_lines][192]{};

enum class MenuAction
{
    stop, previous, rewind, pause, forward, next, queue, repeat, shuffle,
    page_layout, include_library, sort_on, folders_first, supported_only, metadata_mode,
    seen_all, unseen_all, erase_playback, bookmark, paste, reload,
    volume, media, system, log, sleep, home, settings, screensaver, suspend, exit,
    start, resume, play_list, clipboard, copy, remove, unseen, seen, cast, open, file_info, path,
    plugin_open, plugin_settings, plugin_install, plugin_update, plugin_remove, plugin_changelog
};

struct MenuEntry
{
    MenuAction id;
    const char *label;
};

// The installed WSL sidebar defines transport first, then page and global actions.
constexpr MenuEntry sidebar_entries[] = {
    {MenuAction::stop, "Stop"}, {MenuAction::previous, "Previous"}, {MenuAction::rewind, "Rewind"},
    {MenuAction::pause, "Play / Pause"}, {MenuAction::forward, "Fast forward"}, {MenuAction::next, "Next"},
    {MenuAction::queue, "Play queue"}, {MenuAction::repeat, "Repeat"}, {MenuAction::shuffle, "Shuffle"},
    {MenuAction::page_layout, "Page layout"}, {MenuAction::include_library, "Include in library"},
    {MenuAction::sort_on, "Sort on"}, {MenuAction::folders_first, "Sort folders first"},
    {MenuAction::supported_only, "Show only supported files"}, {MenuAction::metadata_mode, "Metadata mode"},
    {MenuAction::seen_all, "Mark all as seen"}, {MenuAction::unseen_all, "Mark all as unseen"},
    {MenuAction::erase_playback, "Erase all playback info"},
    {MenuAction::bookmark, "Bookmark page"}, {MenuAction::paste, "Paste"}, {MenuAction::reload, "Reload page"},
    {MenuAction::volume, "Master volume"}, {MenuAction::media, "Media info"},
    {MenuAction::system, "System info"}, {MenuAction::log, "View log"},
    {MenuAction::sleep, "Activate sleep timer"},
    {MenuAction::home, "Home"}, {MenuAction::settings, "Settings"},
    {MenuAction::screensaver, "Screensaver"}, {MenuAction::suspend, "Suspend"}, {MenuAction::exit, "Exit"},
};
MenuEntry g_sidebar_visible[std::size(sidebar_entries)]{};
int g_sidebar_count = 0;
int g_browser_choice = 0;
constexpr MenuEntry video_item_entries[] = {
    {MenuAction::start, "Play from beginning"}, {MenuAction::resume, "Continue playback"},
    {MenuAction::play_list, "Play until end of list"}, {MenuAction::copy, "Copy to location"},
    {MenuAction::remove, "Delete"}, {MenuAction::unseen, "Mark as unseen"}, {MenuAction::seen, "Mark as seen"},
    {MenuAction::cast, "Video cast"}, {MenuAction::bookmark, "Bookmark"},
    {MenuAction::file_info, "File info"}, {MenuAction::path, "Show path"},
};
constexpr MenuEntry item_entries[] = {
    {MenuAction::queue, "Queue"}, {MenuAction::bookmark, "Bookmark"}, {MenuAction::clipboard, "Copy to clipboard"},
    {MenuAction::copy, "Copy to location"}, {MenuAction::remove, "Delete"}, {MenuAction::open, "Open"},
    {MenuAction::file_info, "File info"}, {MenuAction::path, "Show path"},
};
char g_item_path[fs::max_path + 1] = "";
char g_item_title[96] = "";
bool g_item_folder = false;
bool g_item_file = false;

constexpr int torrent_setting_count = 8;

char g_queue[max_entries][fs::max_path + 1];
char g_queue_title[max_entries][96];
int g_queue_n = 0;
int g_queue_i = 0;
bool g_auto_advanced = false;

plugins::Plugin g_installed_plugins[plugins::max_installed]{};
int g_installed_count = 0;
int g_plugin_catalog_selection = 0;
int g_active_plugin = 0;
bool g_plugin_import = false;
fs::Entry g_plugin_zips[max_entries];
int g_plugin_zip_count = 0;
int g_plugin_zip_selection = 0;
struct ZipLevel { char path[fs::max_path + 1]; int selection; };
ZipLevel g_zip_stack[max_depth]{};
int g_zip_depth = 0;
bool g_repo_categories = false;
int g_repo_category = 0;
constexpr int repo_columns = 5;
constexpr int plugin_page_limit = 8;
enum class PluginDetail { installed, repository, local };
PluginDetail g_plugin_detail{};
plugins::Plugin g_detail_plugin{};
plugins::Item g_detail_item{};
int g_plugin_detail_row = 0;
constexpr MenuEntry installed_actions[] = {
    {MenuAction::plugin_open, "Open"}, {MenuAction::plugin_settings, "Settings"},
    {MenuAction::plugin_update, "Check for updates"}, {MenuAction::plugin_remove, "Uninstall"},
    {MenuAction::plugin_changelog, "Changelog"},
};
constexpr MenuEntry package_actions[] = {
    {MenuAction::plugin_install, "Install"}, {MenuAction::plugin_changelog, "Changelog"},
};
std::span<const MenuEntry> plugin_detail_actions() noexcept
{
    return g_plugin_detail == PluginDetail::installed ? std::span<const MenuEntry>(installed_actions)
                                                      : std::span<const MenuEntry>(package_actions);
}
bool open_zip_details(const char *path) noexcept;
bool begin_file_inspection(const char *path, bool zip) noexcept;

plugins::Page g_plugin_pages[plugin_page_limit];
char g_plugin_page_urls[plugin_page_limit][1024]{};
int g_plugin_selection[plugin_page_limit]{};
bool g_plugin_page_repo[plugin_page_limit]{};
unsigned g_plugin_repo_offset[plugin_page_limit]{};
int g_plugin_depth = 0;
struct PluginJob
{
    enum class Action { page, repository, repository_refresh, install, install_local, catalog, login };
    pthread_t thread{};
    std::atomic<bool> ready{false};
    bool busy = false;
    bool success = false;
    bool dismissed = false;
    Action action = Action::page;
    int target = 0;
    plugins::Plugin plugin{};
    char url[1024]{};
    unsigned offset = 0;
    plugins::Category category = plugins::Category::all;
    char username[256]{};
    char password[256]{};
    char error[160]{};
    plugins::Page page{};
    plugins::Plugin installed[plugins::max_installed]{};
    int installed_count = 0;
};
PluginJob g_plugin_job;
struct ArtworkJob
{
    pthread_t thread{};
    std::atomic<bool> ready{false};
    bool busy = false;
    char url[512]{};
    plugins::Plugin plugin{};
    image::Image result{};
};
ArtworkJob g_artwork_job;
image::Image g_plugin_artwork{};
char g_artwork_url[512]{};
char g_artwork_plugin[64]{};
std::uint64_t g_artwork_idle = 0;
char g_plugin_text_key[64]{};
enum class LoginInput { none, username, password };
LoginInput g_login_input = LoginInput::none;
char g_login_origin[264]{};

void clear_login(PluginJob &job) noexcept
{
    volatile char *username = job.username;
    volatile char *password = job.password;
    for (std::size_t i = 0; i < sizeof job.username; ++i) username[i] = 0;
    for (std::size_t i = 0; i < sizeof job.password; ++i) password[i] = 0;
}
bool g_plugin_catalog_loaded = false;
fs::Entry g_torrent_sources[24];
int g_torrent_source_count = 0;
int g_torrent_selected = 0;
bool g_torrent_scanned = false;
char g_torrent_descriptor[384]{};
char g_torrent_hash[41]{};
torrent::Metadata g_torrent_metadata{};
struct FileInspection {
    pthread_t thread{};
    std::atomic<bool> ready{false};
    bool busy = false, dismissed = false, zip = false, ok = false;
    char path[384]{};
    plugins::Plugin plugin{};
    torrent::Metadata metadata{};
} g_file_inspection;
struct TorrentJob
{
    pthread_t thread{};
    std::atomic<bool> ready{false}, cancel{false};
    std::atomic<unsigned> progress{0};
    torrent::VerifiedFile stream;
    torrent::CacheStorage storage = torrent::CacheStorage::ram;
    torrent::TransferOptions options{};
    bool busy = false;
    bool ok = false;
    bool playback_attempted = false;
    bool playback_started = false;
    char descriptor[384]{}, peer[96]{}, path[384]{}, error[128]{};
} g_torrent_job;
bool g_torrent_peer_input = false;

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

using settings::format_clock;

void format_bytes(char *dst, std::size_t cap, long long bytes) noexcept
{
    if (bytes < 0)
        bytes = 0;
    if (bytes < 1024)
        snprintf(dst, cap, "%lld B", bytes);
    else if (bytes < 1024 * 1024)
        snprintf(dst, cap, "%.1f KB", static_cast<double>(bytes) / 1024.0);
    else
        snprintf(dst, cap, "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
}

void format_mtime(char *dst, std::size_t cap, long long t) noexcept
{
    if (t <= 0)
    {
        copy_text(dst, cap, "");
        return;
    }
    const time_t raw = static_cast<time_t>(t);
    const struct tm *tm = localtime(&raw);
    if (!tm)
    {
        copy_text(dst, cap, "");
        return;
    }
    snprintf(dst, cap, "%04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
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

void queue_add(const char *path, const char *title) noexcept
{
    if (!path || !path[0] || g_queue_n >= max_entries)
        return;
    const player::Kind k = player::classify(path);
    if (k != player::Kind::mpeg1 && k != player::Kind::ffmpeg)
        return;
    copy_text(g_queue[g_queue_n], sizeof g_queue[g_queue_n], path);
    copy_text(g_queue_title[g_queue_n], sizeof g_queue_title[g_queue_n],
              title && title[0] ? title : path);
    ++g_queue_n;
}

void rebuild_queue_from_rows(const char *playing) noexcept
{
    g_queue_n = 0;
    g_queue_i = 0;
    for (int i = 0; i < g_row_count; ++i)
    {
        if (g_rows[i].kind != RowKind::file)
            continue;
        queue_add(g_rows[i].path, g_rows[i].title);
        if (playing && std::strcmp(g_rows[i].path, playing) == 0)
            g_queue_i = g_queue_n - 1;
    }
}

void rebuild_queue_from_parent(const char *file_path) noexcept
{
    if (!file_path || !file_path[0])
        return;
    const char *slash = std::strrchr(file_path, '/');
    if (!slash || slash == file_path)
        return;
    char dir[fs::max_path + 1];
    const int n = static_cast<int>(slash - file_path);
    if (n <= 0 || n >= static_cast<int>(sizeof dir))
        return;
    memcpy(dir, file_path, static_cast<std::size_t>(n));
    dir[n] = 0;
    fs::Entry entries[max_entries];
    int count = 0;
    int err = 0;
    if (fs::list_dir(dir, entries, max_entries, &count, &err, false, settings::get().newest_first) != 0)
        return;
    g_queue_n = 0;
    g_queue_i = 0;
    for (int i = 0; i < count; ++i)
    {
        queue_add(entries[i].path, entries[i].name);
        if (std::strcmp(entries[i].path, file_path) == 0)
            g_queue_i = g_queue_n - 1;
    }
}

void ensure_queue(const char *playing) noexcept
{
    if (playing && playing[0])
    {
        for (int i = 0; i < g_queue_n; ++i)
        {
            if (std::strcmp(g_queue[i], playing) == 0)
            {
                g_queue_i = i;
                return;
            }
        }
    }
    rebuild_queue_from_rows(playing);
    if (g_queue_n == 0)
        rebuild_queue_from_parent(playing);
}

bool open_torrent_descriptor(const char *path) noexcept
{
    if (network::is_path(path)) return begin_file_inspection(path, false);
    if (!torrent::inspect(path, g_torrent_metadata))
    {
        screen::notify("Unsupported or damaged torrent file");
        return false;
    }
    copy_text(g_torrent_descriptor, sizeof g_torrent_descriptor, path);
    g_torrent_selected = 0;
    g_torrent_hash[0] = 0;
    g_place = Place::bittorrent;
    append_log("v59 opened torrent file list");
    return true;
}

bool play_path(const char *path, const char *title, bool from_start,
               const torrent::VerifiedFile *stream = nullptr) noexcept;

bool play_delta(int delta, bool wrap) noexcept
{
    const char *cur = player::path();
    ensure_queue(cur && cur[0] ? cur : g_item_path);
    if (g_queue_n <= 0)
        return false;
    const settings::Data &d = settings::get();
    if (d.repeat == 1 && delta != 0 && player::active() && !wrap)
        return play_path(g_queue[g_queue_i], g_queue_title[g_queue_i], true);
    int next = g_queue_i;
    if (d.shuffle && g_queue_n > 1)
    {
        int hops = (delta < 0 ? -delta : delta);
        if (hops < 1)
            hops = 1;
        for (int h = 0; h < hops; ++h)
        {
            int cand = static_cast<int>(screen::now_ms() + static_cast<std::uint64_t>(h) * 17u) %
                       g_queue_n;
            if (cand == next)
                cand = (cand + 1) % g_queue_n;
            next = cand;
        }
    }
    else
    {
        next = g_queue_i + delta;
        const bool folder_wrap = wrap || d.repeat == 2;
        if (folder_wrap)
        {
            next %= g_queue_n;
            if (next < 0)
                next += g_queue_n;
        }
        else if (next < 0 || next >= g_queue_n)
            return false;
    }
    if (next == g_queue_i && player::active() && d.repeat != 1)
        return false;
    g_queue_i = next;
    return play_path(g_queue[g_queue_i], g_queue_title[g_queue_i], true);
}

bool play_path(const char *path, const char *title, bool from_start,
               const torrent::VerifiedFile *stream) noexcept
{
    if (!path || !path[0])
        return false;
    const char *extension = std::strrchr(path, '.');
    if (!stream && extension && !strcasecmp(extension, ".torrent"))
        return open_torrent_descriptor(path);
    char why[128];
    const player::Kind pk = player::classify(path);
    if (pk == player::Kind::mpeg1 || pk == player::Kind::ffmpeg)
    {
        char event[fs::max_path + 64];
        snprintf(event, sizeof event, "v59 open video %s", path);
        append_log(event);
    }
    if ((pk == player::Kind::mpeg1 || pk == player::Kind::ffmpeg) &&
        player::open(path, why, sizeof why, from_start, stream))
    {
        snprintf(g_status, sizeof g_status, "playing %s", title && title[0] ? title : path);
        append_log(g_status);
        screen::notify(from_start ? "Play from start"
                                  : (pk == player::Kind::ffmpeg ? avdec::mode() : "MPEG-1"));
        g_overlay = Overlay::none;
        g_auto_advanced = false;
        if (stream)
            g_queue_n = 0;
        else
            ensure_queue(path);
        return true;
    }
    if (pk == player::Kind::mpeg1)
    {
        snprintf(g_selected_note, sizeof g_selected_note, "%s", why);
        screen::notify(why);
    }
    else if (pk == player::Kind::ffmpeg)
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

void setting_value(int row, char *dst, std::size_t cap) noexcept
{
    const settings::Data &d = settings::get();
    switch (row)
    {
    case 0:
        copy_text(dst, cap, !d.resume ? "Never" : d.resume_ask ? "Ask" : "Always");
        break;
    case 1:
        copy_text(dst, cap, d.auto_next ? "on" : "off");
        break;
    case 2:
        copy_text(dst, cap, settings::repeat_label());
        break;
    case 3:
        copy_text(dst, cap, d.shuffle ? "on" : "off");
        break;
    case 4:
        snprintf(dst, cap, "%d seconds", d.seek_step);
        break;
    case 5:
        snprintf(dst, cap, "%d%%", d.volume);
        break;
    case 6:
        copy_text(dst, cap, d.clock_24h ? "24h" : "12h");
        break;
    case 7:
        copy_text(dst, cap, d.debug_log ? "on" : "off");
        break;
    case 8:
        copy_text(dst, cap, hwdec_ready() ? (d.hw_decode ? "libSceVideodec2" : "software H.264")
                                         : "software H.264 (chip not loaded)");
        break;
    case 9:
        copy_text(dst, cap, d.show_all ? "on" : "media only");
        break;
    case 10:
        copy_text(dst, cap, d.newest_first ? "newest" : "A–Z");
        break;
    case 11:
        copy_text(dst, cap, "Installed services");
        break;
    case 12:
        copy_text(dst, cap, settings::repo_label());
        break;
    case 13:
        copy_text(dst, cap, d.torrent_trackers ? "on" : "off");
        break;
    case 14:
        copy_text(dst, cap, d.torrent_progressive ? "on" : "off");
        break;
    case 15:
        copy_text(dst, cap, d.torrent_cache_path == 2 ? "USB" :
                            d.torrent_cache_path == 1 ? "PS5 storage (HDD)" : "RAM");
        break;
    case 16:
        copy_text(dst, cap, d.remote_diagnostics ?
            (diagnostics::lan_enabled() ? "LAN port 42000" : "FTP log") : "off");
        break;
    case settings_menu::extensions:
        copy_text(dst, cap, d.show_extensions ? "On" : "Off");
        break;
    case settings_menu::playback_clock:
        copy_text(dst, cap, d.playback_clock ? "On" : "Off");
        break;
    case settings_menu::system_name: copy_text(dst,cap,network::config().system_name); break;
    case settings_menu::smb_large: copy_text(dst,cap,network::config().smb_large_buffer?"On":"Off"); break;
    case settings_menu::smb_attributes: copy_text(dst,cap,"Pending attribute storage"); break;
    case settings_menu::ftp_enabled: copy_text(dst,cap,network::server_running()?"On":"Off"); break;
    case settings_menu::ftp_port: snprintf(dst,cap,"%u",network::config().ftp_port); break;
    case settings_menu::ftp_user: copy_text(dst,cap,network::config().ftp_user); break;
    case settings_menu::ftp_password: copy_text(dst,cap,network::config().ftp_password[0]?"********":"Not set"); break;
    case settings_menu::network_servers: copy_text(dst,cap,"Open"); break;
    case settings_menu::custom_repository:
        copy_text(dst, cap, d.custom_repo_url[0] ? "Custom HTTPS URL" : "Use repository preset");
        break;
    case settings_menu::peer_connections:
        snprintf(dst, cap, "%u", d.torrent_connections);
        break;
    case settings_menu::peer_requests:
        snprintf(dst, cap, "%u", d.torrent_requests);
        break;
    case settings_menu::retrackers:
        copy_text(dst, cap, settings_menu::retracker_labels[d.torrent_retrackers]);
        break;
    case 17:
        copy_text(dst, cap, "Cache and peers");
        break;
    case 19:
        snprintf(dst, cap, "%d saved", settings::bookmark_count());
        break;
    default:
        copy_text(dst, cap, kVersion);
        break;
    }
}

void enter_videos() noexcept;
void push_folder(const char *path, const char *title) noexcept;

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
        d.auto_next = !d.auto_next;
        settings::save();
        screen::notify(d.auto_next ? "Auto-next on" : "Auto-next off");
        break;
    case 2:
        settings::cycle_repeat(delta == 0 ? 1 : delta);
        screen::notify("Repeat");
        break;
    case 3:
        d.shuffle = !d.shuffle;
        settings::save();
        screen::notify(d.shuffle ? "Shuffle on" : "Shuffle off");
        break;
    case 4:
        settings::cycle_seek(delta == 0 ? 1 : delta);
        screen::notify("Seek step");
        break;
    case 5:
        settings::cycle_volume(delta == 0 ? 1 : delta);
        audio::set_volume(settings::get().volume);
        screen::notify("Volume");
        break;
    case 6:
        d.clock_24h = !d.clock_24h;
        settings::save();
        break;
    case 7:
        d.debug_log = !d.debug_log;
        settings::save();
        screen::notify(d.debug_log ? "Debug log on" : "Debug log off");
        break;
    case 8:
        d.hw_decode = !d.hw_decode;
        hwdec_force_software(d.hw_decode ? 0 : 1);
        settings::save();
        screen::notify(d.hw_decode ? "Prefer Videodec2" : "Software H.264");
        break;
    case 9:
        d.show_all = !d.show_all;
        settings::save();
        g_videos_dirty = true;
        screen::notify(d.show_all ? "Showing all files" : "Media files only");
        break;
    case 10:
        d.newest_first = !d.newest_first;
        settings::save();
        g_videos_dirty = true;
        screen::notify(d.newest_first ? "Newest first" : "Name A–Z");
        break;
    case 11:
        g_overlay = Overlay::none;
        g_place = Place::plugins;
        g_plugin_import = false;
        g_plugin_depth = 0;
        g_repo_categories = false;
        g_plugin_catalog_selection = g_installed_count ? 1 : 0;
        screen::notify("Select a plugin, then press Square for settings");
        break;
    case 12:
        settings::cycle_repo(delta == 0 ? 1 : delta);
        break;
    case 13:
        d.torrent_trackers = !d.torrent_trackers;
        settings::save();
        break;
    case 14:
        d.torrent_progressive = !d.torrent_progressive;
        settings::save();
        break;
    case 15:
        d.torrent_cache_path = (d.torrent_cache_path + (delta < 0 ? 2 : 1)) % 3;
        settings::save();
        break;
    case 16:
        if (diagnostics::enable(!d.remote_diagnostics))
        {
            d.remote_diagnostics = !d.remote_diagnostics;
            settings::save();
            screen::notify(!d.remote_diagnostics ? "Remote diagnostics off" :
                diagnostics::lan_enabled() ? "Remote diagnostics listening on port 42001" :
                                             "Remote diagnostics on through FTP");
        }
        else
            screen::notify("Could not start LAN or FTP diagnostics");
        break;
    case settings_menu::extensions:
        d.show_extensions = !d.show_extensions;
        g_videos_dirty = true;
        settings::save();
        break;
    case settings_menu::playback_clock:
        d.playback_clock = !d.playback_clock;
        settings::save();
        break;
    case 17:
        g_torrent_settings_row = 0;
        g_overlay = Overlay::torrent_settings;
        break;
    case 19:
        g_overlay = Overlay::none;
        g_place = Place::videos;
        enter_videos();
        push_folder(kBookmarksUri, "Bookmarks");
        break;
    default:
        screen::notify("Movian PS5 native title");
        break;
    }
}

void nudge_torrent_setting(int row, int delta) noexcept
{
    auto &d = settings::get();
    switch (row)
    {
    case 0:
        setting_nudge(15, delta);
        return;
    case 1:
    {
        constexpr unsigned sizes[] = {0, 8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512};
        const auto limit = torrent::cache_limit_mib(static_cast<torrent::CacheStorage>(d.torrent_cache_path));
        const auto current = std::min(d.torrent_cache_mib, limit);
        const auto *end = std::upper_bound(std::begin(sizes), std::end(sizes), limit);
        const auto *position = std::lower_bound(std::begin(sizes), end, current);
        if (delta > 0)
        {
            position = std::upper_bound(std::begin(sizes), end, current);
            if (position == end) --position;
        }
        else if (position != std::begin(sizes))
            --position;
        d.torrent_cache_mib = *position;
        break;
    }
    case 2:
        d.torrent_read_ahead = std::clamp(static_cast<int>(d.torrent_read_ahead) + 5 * delta, 5, 100);
        break;
    case 3:
        if (d.torrent_preload < 0)
            d.torrent_preload = delta > 0 ? 0 : -1;
        else
            d.torrent_preload = std::clamp(d.torrent_preload + 5 * delta, -1, 100);
        break;
    case 4:
        d.torrent_connections = std::clamp(static_cast<int>(d.torrent_connections) + delta, 1, 8);
        break;
    case 5:
        d.torrent_requests = std::clamp(static_cast<int>(d.torrent_requests) + 4 * delta, 4, 32);
        break;
    case 6:
        d.torrent_block_timeout = std::clamp(static_cast<int>(d.torrent_block_timeout) + delta, 3, 30);
        break;
    case 7:
        d.torrent_retrackers = (d.torrent_retrackers + (delta < 0 ? 3 : 1)) % 4;
        break;
    }
    settings::save();
}

std::span<const MenuEntry> context_entries() noexcept
{
    return g_item_file && fs::classify_name(g_item_path, false) == fs::Kind::video
        ? std::span<const MenuEntry>(video_item_entries) : std::span<const MenuEntry>(item_entries);
}

void menu_info(const char *title, const char *message = "") noexcept
{
    g_menu_parent = g_overlay;
    g_menu_info_title = title;
    std::memset(g_menu_lines, 0, sizeof g_menu_lines);
    copy_text(g_menu_lines[0], sizeof g_menu_lines[0], message);
    g_overlay = Overlay::menu_info;
}

void *inspect_network_file(void *data) noexcept
{
    auto &job = *static_cast<FileInspection *>(data);
    job.ok = job.zip ? plugins::inspect_package(job.path, job.plugin)
                     : torrent::inspect(job.path, job.metadata);
    job.ready.store(true, std::memory_order_release);
    return nullptr;
}

bool begin_file_inspection(const char *path, bool zip) noexcept
{
    auto &job = g_file_inspection;
    if (job.busy) { screen::notify("A network file is still being read"); return false; }
    if (std::strlen(path) >= sizeof job.path) return false;
    copy_text(job.path, sizeof job.path, path);
    job.zip = zip; job.dismissed = false; job.ok = false;
    job.ready.store(false, std::memory_order_relaxed);
    pthread_attr_t attr;
    if (pthread_attr_init(&attr)) return false;
    pthread_attr_setstacksize(&attr, 512 * 1024);
    job.busy = pthread_create(&job.thread, &attr, inspect_network_file, &job) == 0;
    pthread_attr_destroy(&attr);
    if (!job.busy) { screen::notify("Could not start network file reader"); return false; }
    menu_info(zip ? "Plugin package" : "Torrent file", "Reading network file...  Circle  Cancel");
    return true;
}

bool finish_file_inspection() noexcept
{
    auto &job = g_file_inspection;
    if (!job.busy || !job.ready.load(std::memory_order_acquire)) return false;
    pthread_join(job.thread, nullptr);
    job.busy = false;
    if (job.dismissed) return true;
    g_overlay = Overlay::none;
    if (!job.ok) {
        menu_info(job.zip ? "Plugin package" : "Torrent file",
                  "Could not read a supported file. Check the connection, permissions and file size.");
        append_log("v59 network package/descriptor read failed");
    } else if (job.zip) {
        g_detail_plugin = job.plugin;
        g_plugin_detail = PluginDetail::local;
        g_plugin_detail_row = 0;
        g_overlay = Overlay::plugin_details;
        append_log("v59 network plugin ZIP ready");
    } else {
        g_torrent_metadata = job.metadata;
        copy_text(g_torrent_descriptor, sizeof g_torrent_descriptor, job.path);
        g_torrent_selected = 0; g_torrent_hash[0] = 0;
        g_place = Place::bittorrent;
        append_log("v59 network torrent file list ready");
    }
    return true;
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
    case fs::Kind::package:
        return "plugin ZIP";
    default:
        return "file";
    }
}

bool browser_grid() noexcept
{
    return settings::browser_prefs(g_stack[g_depth].path).grid;
}

void keep_in_view() noexcept
{
    auto &lv = g_stack[g_depth];
    if (g_row_count <= 0)
    {
        lv.selected = lv.first = 0;
        lv.focused_path[0] = 0;
        return;
    }
    lv.selected = std::clamp(lv.selected, 0, g_row_count - 1);
    copy_text(lv.focused_path, sizeof lv.focused_path, g_rows[lv.selected].path);
    if (browser_grid())
    {
        const int row = lv.selected / library_columns;
        const int last = (g_row_count - 1) / library_columns;
        int first = std::clamp(lv.first / library_columns, 0, std::max(0, last - library_grid_rows + 1));
        if (row < first) first = row;
        if (row >= first + library_grid_rows) first = row - library_grid_rows + 1;
        lv.first = first * library_columns;
    }
    else
    {
        if (lv.selected < lv.first) lv.first = lv.selected;
        if (lv.selected >= lv.first + rows_in_view) lv.first = lv.selected - rows_in_view + 1;
        lv.first = std::clamp(lv.first, 0, std::max(0, g_row_count - rows_in_view));
    }
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
    r.size = 0;
    r.mtime = 0;
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
    r.size = 0;
    r.mtime = 0;
}

void add_file_row(const fs::Entry &e) noexcept
{
    if (g_row_count >= max_entries)
        return;
    Row &r = g_rows[g_row_count++];
    const char *dot = settings::get().show_extensions ? nullptr : std::strrchr(e.name, '.');
    if (dot && dot != e.name)
        snprintf(r.title, sizeof r.title, "%.*s", static_cast<int>(dot - e.name), e.name);
    else
        copy_text(r.title, sizeof r.title, e.name);
    const player::Kind pk = player::classify(e.path);
    char bytes[24];
    char when[16];
    format_bytes(bytes, sizeof bytes, e.size);
    format_mtime(when, sizeof when, e.mtime);
    if (pk == player::Kind::mpeg1)
        snprintf(r.detail, sizeof r.detail, "MPEG-1  %s  %s", bytes, when);
    else if (pk == player::Kind::ffmpeg)
        snprintf(r.detail, sizeof r.detail, "FFmpeg/HW  %s  %s", bytes, when);
    else if (pk == player::Kind::later)
        snprintf(r.detail, sizeof r.detail, "%s — needs FFmpeg  %s  %s", kind_label(e.kind), bytes,
                 when);
    else
        snprintf(r.detail, sizeof r.detail, "%s  %s  %s", kind_label(e.kind), bytes, when);
    copy_text(r.path, sizeof r.path, e.path);
    r.kind = RowKind::file;
    r.openable = false;
    r.size = e.size;
    r.mtime = e.mtime;
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
    const auto prefs = settings::browser_prefs(path);
    if (fs::list_directory(path, entries, max_entries, &n, &err, !prefs.supported_only,
                           prefs.sort, prefs.folders_first) != 0)
    {
        if(network::is_path(path) && err==EAGAIN) {
            add_message_row("Loading network folder", "Circle returns while the connection is in progress.");
            return;
        }
        if(network::is_path(path) && err==EACCES && g_overlay==Overlay::none) open_network_login(path);
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
        bool hide_if_closed;
    } sources[] = {
        {"Title data", MOVIAN_DOWNLOAD0, "settings, log, test clips", false},
        {"Movian library", MOVIAN_DATA_DIR, "FTP extra media if the sandbox can see /data", false},
#ifndef MOVIAN_HOST_PREVIEW
        {"USB 0", kUsb0, "first USB device", false},
        {"USB 1", "/mnt/usb1", "second stick", true},
        {"USB 2", "/mnt/usb2", "third stick", true},
        {"USB 3", "/mnt/usb3", "fourth stick", true},
        {"External 0", "/mnt/ext0", "external HDD if mounted", true},
        {"External 1", "/mnt/ext1", "second external volume", true},
#else
        {"USB", kUsb0, "host preview folder", false},
#endif
    };

    int usb_open = 0;
    for (const Source &s : sources)
    {
        fs::Entry entries[4];
        int n = 0;
        int err = 0;
        const int opened = fs::list_dir(s.path, entries, 4, &n, &err, settings::get().show_all,
                                        settings::get().newest_first);
        if (opened != 0 && s.hide_if_closed)
            continue;
        char detail[96];
        if (opened != 0)
            snprintf(detail, sizeof detail, "%s — %s (errno %d)", s.hint, errno_hint(err), err);
        else if (err > 0)
            snprintf(detail, sizeof detail, "%s — opened, %d+ entries", s.hint, n + err);
        else
            snprintf(detail, sizeof detail, "%s — %d listed", s.hint, n);
        add_folder_row(s.title, detail, s.path, true);
        if (opened == 0 && std::strncmp(s.path, "/mnt/usb", 8) == 0)
            ++usb_open;
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

    add_message_row("MPEG-1, H.264, HEVC (chip) in this build",
                    "Cross a file. HEVC needs Videodec2; software HEVC is refused.");
    snprintf(g_status, sizeof g_status, "Videos  %s  USB open %d  %s", "sandboxed", usb_open,
             hwdec_status());
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
            struct stat st{};
            if (stat(path, &st) == 0)
            {
                e.size = static_cast<long long>(st.st_size);
                e.mtime = static_cast<long long>(st.st_mtime);
            }
            add_file_row(e);
        }
    }
    snprintf(g_status, sizeof g_status, "Bookmarks  %d", n);
}

bool matches_search(const char *name, const char *query) noexcept
{
    const auto *end = name + std::strlen(name);
    auto lower_ascii = [](unsigned char ch) {
        return ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch;
    };
    return std::search(name, end, query, query + std::strlen(query),
                       [&](char a, char b) { return lower_ascii(a) == lower_ascii(b); }) != end;
}

void load_search_results() noexcept
{
    g_row_count = 0;
    g_truncated = 0;
    constexpr const char *roots[] = {MOVIAN_DATA_DIR, MOVIAN_DOWNLOAD0, kUsb0
#ifndef MOVIAN_HOST_PREVIEW
        , "/mnt/usb1", "/mnt/usb2", "/mnt/usb3", "/mnt/ext0", "/mnt/ext1"
#endif
    };
    fs::Entry entries[max_entries];
    for (const char *root : roots)
    {
        int count = 0;
        int err = 0;
        if (fs::list_dir(root, entries, max_entries, &count, &err, settings::get().show_all,
                         settings::get().newest_first) != 0)
            continue;
        g_truncated += std::max(0, err);
        for (int i = 0; i < count; ++i)
        {
            const auto &entry = entries[i];
            if (!matches_search(entry.name, g_home_query))
                continue;
            if (g_row_count == max_entries)
            {
                ++g_truncated;
                continue;
            }
            if (entry.kind == fs::Kind::directory)
                add_folder_row(entry.name, root, entry.path, true);
            else
                add_file_row(entry);
        }
    }
    if (!g_row_count)
        add_message_row("No local matches", "Search checks the top level of available local sources.");
    if (g_truncated)
        snprintf(g_status, sizeof g_status, "Search limited by folder or result size");
    else
        snprintf(g_status, sizeof g_status, "Search results for %.70s", g_home_query);
}

void reload_videos() noexcept
{
    BrowseLevel &lv = g_stack[g_depth];
    if (lv.path[0] == 0)
        load_videos_root();
    else if (std::strcmp(lv.path, kBookmarksUri) == 0)
        load_bookmarks();
    else if (std::strcmp(lv.path, kSearchUri) == 0)
        load_search_results();
    else
        load_directory(lv.path);
    if (lv.focused_path[0])
        for (int i = 0; i < g_row_count; ++i)
            if (std::strcmp(g_rows[i].path, lv.focused_path) == 0)
            {
                lv.selected = i;
                break;
            }
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
    g_stack[0].focused_path[0] = 0;
    g_selected_note[0] = 0;
    g_videos_dirty = true;
}

void open_home_folder(const char *path, const char *title) noexcept
{
    enter_videos();
    copy_text(g_stack[0].path, sizeof g_stack[0].path, path);
    copy_text(g_stack[0].title, sizeof g_stack[0].title, title);
    g_place = Place::videos;
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
    g_stack[g_depth].focused_path[0] = 0;
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

void play_with_resume_prompt(const char *path, const char *title) noexcept
{
    if (settings::get().resume && settings::get().resume_ask && settings::resume_for(path, nullptr, nullptr))
    {
        copy_text(g_resume_path, sizeof g_resume_path, path);
        copy_text(g_resume_title, sizeof g_resume_title, title);
        g_resume_choice = 0;
        g_resume_parent = g_overlay;
        g_overlay = Overlay::resume_prompt;
    }
    else
        play_path(path, title, false);
}

void run_item_action(const MenuEntry &entry) noexcept
{
    switch (entry.id)
    {
    case MenuAction::start:
    case MenuAction::resume:
        play_path(g_item_path, g_item_title, entry.id == MenuAction::start);
        return;
    case MenuAction::open:
        if (g_item_folder)
        {
            push_folder(g_item_path, g_item_title);
            g_overlay = Overlay::none;
        }
        else if (g_item_file)
        {
            play_with_resume_prompt(g_item_path, g_item_title);
        }
        else
            menu_info(entry.label, "This source has no item to open.");
        return;
    case MenuAction::bookmark:
        if (g_item_path[0])
        {
            const bool removing = settings::is_bookmarked(g_item_path);
            settings::toggle_bookmark(g_item_path, g_item_title);
            screen::notify(removing ? "Bookmark removed" : "Bookmark updated");
            g_videos_dirty = true;
            g_overlay = Overlay::none;
        }
        return;
    case MenuAction::file_info:
    {
        struct stat st{};
        char bytes[24] = "Size unavailable";
        char when[16]{};
        if (stat(g_item_path, &st) == 0)
        {
            format_bytes(bytes, sizeof bytes, static_cast<long long>(st.st_size));
            format_mtime(when, sizeof when, static_cast<long long>(st.st_mtime));
        }
        menu_info(entry.label, g_item_title);
        snprintf(g_menu_lines[1], sizeof g_menu_lines[1], "%s  %s", bytes, when);
        copy_text(g_menu_lines[2], sizeof g_menu_lines[2], g_item_path);
        return;
    }
    case MenuAction::path:
        menu_info(entry.label, g_item_path);
        return;
    default:
        menu_info(entry.label, "This item action is planned for a later PS5 build.");
        return;
    }
}

void select_row() noexcept
{
    if (g_row_count <= 0)
        return;
    const Row &r = g_rows[g_stack[g_depth].selected];
    if (r.kind == RowKind::folder && r.path[0] && std::strcmp(r.path, kBookmarksUri) != 0 &&
        !r.openable && (player::classify(r.path) == player::Kind::mpeg1 ||
                       player::classify(r.path) == player::Kind::ffmpeg))
    {
        play_with_resume_prompt(r.path, r.title);
        return;
    }
    if (r.kind == RowKind::folder && r.openable && r.path[0])
    {
        push_folder(r.path, r.title);
        char line[160];
        snprintf(line, sizeof line, "open %.154s", r.path);
        append_log(line);
        return;
    }
    if (r.kind == RowKind::file)
    {
        const char *extension = std::strrchr(r.path, '.');
        if (extension && !strcasecmp(extension, ".zip"))
        {
            open_zip_details(r.path);
            return;
        }
        play_with_resume_prompt(r.path, r.title);
        snprintf(g_status, sizeof g_status, "selected %.86s", r.title);
        char line[160];
        snprintf(line, sizeof line, "select %.152s", r.path);
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
    const char *hint = g_overlay == Overlay::sidebar
                           ? "Cross select    Circle close    Left / Right adjust volume"
                           : g_overlay == Overlay::browser_choice
                                 ? "Cross select    Circle cancel"
                           : g_overlay == Overlay::menu_info
                                 ? "Cross / Circle back"
                           : g_overlay == Overlay::settings
                           ? (g_settings_in_category ? "Cross / Right open    Left / Circle back"
                                                     : "Cross open category    Circle back")
                           : g_overlay == Overlay::setting_detail
                                 ? "Cross choose    Circle back"
                           : g_overlay == Overlay::resume_prompt
                                 ? "Cross play    Circle cancel"
                           : g_overlay == Overlay::home_network
                                 ? "D-pad select / change server    Cross edit / open    Circle back"
                           : g_overlay == Overlay::network_login
                                 ? "D-pad select    Cross edit / confirm    Circle cancel"
                           : g_overlay == Overlay::item || g_overlay == Overlay::plugin_details
                                 ? "Cross action    Circle close"
                                 : g_place == Place::videos
                                       ? "Cross play    Square item    Options sidebar    Circle back"
                                       : g_place == Place::home
                                           ? "L1 / R1 places    Cross open    Triangle HEVC test    Options sidebar"
                                           : "L1 / R1 places    Cross open    Options sidebar    Circle back";
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
    char body_fit[160];
    c.text(static_cast<float>(x + 28), static_cast<float>(y + 92),
           fit(body, w - 56, body_fit, sizeof body_fit, font::regular, 18), font::regular, 18,
           Color::muted);
}

void draw_home(screen::Canvas &c) noexcept
{
    c.text(64.f, 168.f, "Home", font::semibold, 44, Color::text);

    c.round_rect(64, 209, 1792, 76, 12, g_home_search_focused ? Color::hover : Color::surface);
    if (g_home_search_focused)
        c.round_frame(64, 209, 1792, 76, 12, 3, Color::accent);
    c.ring(110.f, 245.f, 12.f, 3.f, Color::accent);
    const float handle[] = {119.f, 254.f, 129.f, 264.f};
    c.polyline(handle, 2, 3.f, Color::accent);
    char query[sizeof g_home_query];
    const char *prompt = g_home_query[0] ? g_home_query : "Search local media";
    c.text(155.f, 258.f, fit(prompt, 1560, query, sizeof query, font::medium, 27),
           font::medium, 27, g_home_query[0] ? Color::text : Color::muted);

    const auto resume = settings::last_resume();
    char resume_body[96];
    format_resume_detail(resume_body, sizeof resume_body, resume);
    for (int i = 0; i <= last_home_tile; ++i)
    {
        const auto &tile = home_tiles[i];
        const int col = i % home_columns;
        const int row = i / home_columns;
        draw_card(c, 64 + col * 363, 333 + row * 201, 340, 169, tile.title,
                  tile.action == HomeAction::resume ? (resume.valid ? resume_body : "No saved position") : tile.detail,
                  !g_home_search_focused && g_home_tile == i);
    }
}

const char *network_rows[] = {"Address", "Username", "Password", "Domain", "Open", "Remove saved server"};
void draw_home_network(screen::Canvas &c) noexcept
{
    c.rect(0,90,screen::width,918,Color::bg,225);
    c.round_rect(360,210,1200,650,18,Color::surface2);
    c.text(410.f,270.f,"Local network",font::semibold,36,Color::text);
    char label[128];
    snprintf(label,sizeof label,"Server %d of %d   Left / Right changes server",g_network_slot+1,network::max_connections);
    c.text(410.f,315.f,label,font::regular,23,Color::muted);
    const auto &n=network::config().connections[g_network_slot];
    for(int i=0;i<6;++i) {
        const int y=350+i*62;
        if(i==g_network_row) c.round_rect(395,y,1130,56,8,Color::hover);
        c.text(420.f,float(y+37),network_rows[i],font::medium,25,Color::text);
        const char *value=i==0?n.url:i==1?n.user:i==2?(n.password[0]?"********":""):i==3?n.domain:"";
        char shown[150];
        c.text(770.f,float(y+37),fit(value,720,shown,sizeof shown,font::regular,23),font::regular,23,Color::accent);
    }
    c.text(410.f,797.f,"smb2://server/share   or   ftp://server:port/folder",font::regular,22,Color::muted);
    c.text(410.f,831.f,"Cross edit / open    Circle back    Login details are saved on this title",font::regular,21,Color::muted);
}

void draw_network_login(screen::Canvas &c) noexcept {
    c.rect(0,0,screen::width,screen::height,Color::bg,190);
    c.round_rect(420,275,1080,510,8,Color::surface2);
    c.text(560.f,340.f,!strncmp(g_login_draft.url,"ftp:",4)?"FTP Client":"SMB Client",font::semibold,30,Color::text);
    c.text(560.f,380.f,"Login",font::regular,25,Color::text);
    const bool smb=!strncmp(g_login_draft.url,"smb2:",5);
    const int fields=smb?3:2;
    for(int row=0;row<fields;++row) {
        const int y=405+row*58;
        const char *label=row==0?"Username":row==1?"Password":"Domain";
        const char *value=row==0?g_login_draft.user:row==1?(g_login_draft.password[0]?"********":""):g_login_draft.domain;
        c.text(460.f,float(y+34),label,font::regular,24,Color::text);
        c.rect(660,y,770,46,row==g_login_row?Color::hover:Color::bg);
        c.text(680.f,float(y+33),value,font::regular,23,Color::text);
    }
    const int remember_y=405+fields*58;
    if(g_login_row==3) c.round_rect(450,remember_y,980,45,5,Color::hover);
    c.text(460.f,float(remember_y+32),g_login_draft.remember?"[x] Remember me":"[ ] Remember me",font::regular,24,Color::text);
    for(int i=0;i<2;++i) {
        c.round_rect(740+i*220,705,190,52,5,g_login_row==i+4?Color::hover:Color::surface);
        c.text(float(785+i*220),740.f,i==0?"Ok":"Cancel",font::medium,25,Color::text);
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

void *load_plugin_page(void *userdata) noexcept
{
    auto &job = *static_cast<PluginJob *>(userdata);
    switch (job.action)
    {
    case PluginJob::Action::page:
        job.success = plugins::open(job.plugin, job.url, job.page, job.error, sizeof job.error);
        break;
    case PluginJob::Action::repository:
    case PluginJob::Action::repository_refresh:
        job.success = plugins::repository(job.url, job.offset, job.page, job.error, sizeof job.error,
                                           job.action == PluginJob::Action::repository_refresh, job.category);
        break;
    case PluginJob::Action::install:
        job.success = plugins::install_remote(job.url, job.error, sizeof job.error);
        break;
    case PluginJob::Action::install_local:
        job.success = plugins::install(job.url, job.error, sizeof job.error);
        break;
    case PluginJob::Action::catalog:
        job.installed_count = plugins::catalog(job.installed);
        job.success = true;
        break;
    case PluginJob::Action::login:
        job.success = plugins::kinozal_login(job.url, job.username, job.password, job.error, sizeof job.error);
        clear_login(job);
        break;
    }
    if (job.success && (job.action == PluginJob::Action::install ||
                        job.action == PluginJob::Action::install_local))
        job.installed_count = plugins::catalog(job.installed);
    job.ready.store(true, std::memory_order_release);
    return nullptr;
}

void open_plugin_job(PluginJob::Action action, const char *url, bool replace = false,
                     unsigned offset = 0) noexcept
{
    auto &job = g_plugin_job;
    const int target = g_plugin_depth - (replace ? 1 : 0);
    if (job.busy || target < 0 || target >= plugin_page_limit)
        return;
    job.plugin = g_installed_plugins[g_active_plugin];
    job.action = action;
    job.dismissed = false;
    job.offset = offset;
    job.category = static_cast<plugins::Category>(g_repo_category);
    copy_text(job.url, sizeof job.url, url ? url : "");
    job.error[0] = 0;
    job.target = target;
    job.ready.store(false, std::memory_order_relaxed);
    pthread_attr_t attr;
    if (pthread_attr_init(&attr))
    {
        screen::notify("Could not start plugin");
        return;
    }
    pthread_attr_setstacksize(&attr, 512 * 1024);
    job.busy = pthread_create(&job.thread, &attr, load_plugin_page, &job) == 0;
    pthread_attr_destroy(&attr);
    if (!job.busy)
    {
        if (action == PluginJob::Action::catalog)
            g_plugin_catalog_loaded = true;
        screen::notify("Could not start plugin");
    }
    else if (action == PluginJob::Action::catalog)
        append_log("v59 plugin catalog started");
}

void open_plugin_page(const char *url, bool replace = false) noexcept
{
    open_plugin_job(PluginJob::Action::page, url, replace);
}

void apply_installed_catalog(const PluginJob &job) noexcept
{
    const bool zip_selected = g_plugin_catalog_selection == g_installed_count + 1;
    char selected[64]{}, active[64]{};
    if (g_plugin_catalog_selection > 0 && g_plugin_catalog_selection <= g_installed_count)
        copy_text(selected, sizeof selected, g_installed_plugins[g_plugin_catalog_selection - 1].id);
    if (g_active_plugin < g_installed_count)
        copy_text(active, sizeof active, g_installed_plugins[g_active_plugin].id);
    g_installed_count = job.installed_count;
    g_plugin_catalog_selection = zip_selected ? g_installed_count + 1 : 0;
    g_active_plugin = 0;
    for (int i = 0; i < g_installed_count; ++i)
    {
        g_installed_plugins[i] = job.installed[i];
        if (*selected && !std::strcmp(selected, job.installed[i].id)) g_plugin_catalog_selection = i + 1;
        if (*active && !std::strcmp(active, job.installed[i].id)) g_active_plugin = i;
    }
    g_plugin_catalog_loaded = true;
}

bool finish_plugin_page() noexcept
{
    auto &job = g_plugin_job;
    if (!job.busy || !job.ready.load(std::memory_order_acquire))
        return false;
    pthread_join(job.thread, nullptr);
    job.busy = false;
    if (job.action == PluginJob::Action::catalog)
    {
        apply_installed_catalog(job);
        char event[64];
        snprintf(event, sizeof event, "v59 plugin catalog ready count=%d", g_installed_count);
        append_log(event);
    }
    else if (job.dismissed && job.action != PluginJob::Action::install &&
             job.action != PluginJob::Action::install_local)
    {
        append_log("v59 dismissed plugin page result");
    }
    else if (!job.success)
    {
        screen::notify(job.error);
        append_log(job.error);
    }
    else if (job.action == PluginJob::Action::install ||
             job.action == PluginJob::Action::install_local)
    {
        apply_installed_catalog(job);
        screen::notify("Plugin installed");
        append_log(job.action == PluginJob::Action::install ? "v59 installed repository plugin"
                                                           : "v59 installed local plugin ZIP");
    }
    else if (job.action == PluginJob::Action::login)
    {
        screen::notify("Signed in to KinoZal.TV for this session");
        append_log("v59 KinoZal.TV signed in");
    }
    else
    {
        if (job.target >= g_plugin_depth)
            g_plugin_selection[job.target] = 0;
        g_plugin_pages[job.target] = job.page;
        g_plugin_page_repo[job.target] = job.action == PluginJob::Action::repository ||
                                         job.action == PluginJob::Action::repository_refresh;
        g_plugin_repo_offset[job.target] = job.offset;
        copy_text(g_plugin_page_urls[job.target], sizeof g_plugin_page_urls[job.target], job.url);
        g_plugin_depth = job.target + 1;
        if (g_plugin_selection[job.target] >= job.page.count)
            g_plugin_selection[job.target] = 0;
        append_log(job.page.title);
        if (job.action == PluginJob::Action::repository ||
            job.action == PluginJob::Action::repository_refresh)
            append_log("v59 repository page ready");
    }
    return true;
}

void *load_plugin_artwork(void *data) noexcept
{
    auto &job = *static_cast<ArtworkJob *>(data);
    plugins::artwork(job.plugin, job.url, job.result);
    job.ready.store(true, std::memory_order_release);
    return nullptr;
}

const plugins::Plugin *selected_installed() noexcept
{
    if (g_place != Place::plugins || g_plugin_import || g_repo_categories || g_plugin_depth != 0 ||
        g_plugin_catalog_selection < 1 || g_plugin_catalog_selection > g_installed_count)
        return nullptr;
    return &g_installed_plugins[g_plugin_catalog_selection - 1];
}

const plugins::Plugin &artwork_owner() noexcept
{
    static const plugins::Plugin empty{};
    if (g_overlay == Overlay::plugin_details)
        return g_plugin_detail == PluginDetail::repository ? empty : g_detail_plugin;
    if (const auto *plugin = selected_installed()) return *plugin;
    if (g_place == Place::plugins && g_plugin_depth > 0 && !g_plugin_page_repo[g_plugin_depth - 1])
        return g_installed_plugins[g_active_plugin];
    return empty;
}

const char *selected_plugin_icon() noexcept
{
    if (g_overlay == Overlay::plugin_details)
        return g_plugin_detail == PluginDetail::repository ? g_detail_item.icon : g_detail_plugin.icon;
    if (const auto *plugin = selected_installed()) return plugin->icon;
    if (g_place != Place::plugins || g_plugin_import || g_plugin_depth <= 0)
        return "";
    const auto &page = g_plugin_pages[g_plugin_depth - 1];
    return page.count ? page.items[g_plugin_selection[g_plugin_depth - 1]].icon : "";
}

bool update_plugin_artwork(unsigned pressed) noexcept
{
    auto &job = g_artwork_job;
    const char *wanted = selected_plugin_icon();
    const auto &plugin = artwork_owner();
    bool dirty = false;
    if (job.busy && job.ready.load(std::memory_order_acquire))
    {
        pthread_join(job.thread, nullptr);
        job.busy = false;
        image::unload(g_plugin_artwork);
        if (!std::strcmp(wanted, job.url) && !std::strcmp(plugin.id, job.plugin.id))
        {
            append_log(job.result.loaded() ? "v59 plugin artwork decoded" : "v59 plugin artwork unavailable");
            g_plugin_artwork = job.result;
            job.result = {};
            copy_text(g_artwork_url, sizeof g_artwork_url, wanted);
            copy_text(g_artwork_plugin, sizeof g_artwork_plugin, plugin.id);
        }
        image::unload(job.result);
        dirty = true;
    }
    if (pressed)
        g_artwork_idle = screen::now_ms();
    if (!job.busy && !g_plugin_job.busy && *wanted && screen::now_ms() - g_artwork_idle >= 250 &&
        (std::strcmp(wanted, g_artwork_url) || std::strcmp(plugin.id, g_artwork_plugin)))
    {
        job.plugin = plugin;
        copy_text(job.url, sizeof job.url, wanted);
        job.ready.store(false, std::memory_order_relaxed);
        pthread_attr_t attr;
        if (pthread_attr_init(&attr) == 0)
        {
            pthread_attr_setstacksize(&attr, 512 * 1024);
            job.busy = pthread_create(&job.thread, &attr, load_plugin_artwork, &job) == 0;
            pthread_attr_destroy(&attr);
        }
        if (!job.busy)
        {
            copy_text(g_artwork_url, sizeof g_artwork_url, wanted);
            copy_text(g_artwork_plugin, sizeof g_artwork_plugin, plugin.id);
        }
    }
    return dirty;
}

int scan_local_packages(const char *extension, fs::Entry *out, int capacity) noexcept
{
    constexpr const char *roots[] = {MOVIAN_DATA_DIR, MOVIAN_DOWNLOAD0};
    int total = 0;
    fs::Entry files[max_entries];
    for (const char *root : roots)
    {
        int count = 0;
        fs::list_dir(root, files, max_entries, &count, nullptr, true);
        for (int i = 0; i < count && total < capacity; ++i)
        {
            const char *ext = std::strrchr(files[i].name, '.');
            if (ext && !strcasecmp(ext, extension) && files[i].kind != fs::Kind::directory)
                out[total++] = files[i];
        }
    }
    return total;
}

void scan_plugin_zips() noexcept
{
    g_plugin_zip_count = 0;
    if (!g_zip_depth)
    {
        g_plugin_zips[0] = {"Movian library", MOVIAN_DATA_DIR, fs::Kind::directory};
        g_plugin_zips[1] = {"Title data", MOVIAN_DOWNLOAD0, fs::Kind::directory};
        g_plugin_zips[2] = {"USB", "", fs::Kind::directory};
        copy_text(g_plugin_zips[2].path, sizeof g_plugin_zips[2].path, kUsb0);
        g_plugin_zip_count = 3;
        for (int i = 0; i < network::max_connections; ++i) {
            if (!network::config().connections[i].url[0]) continue;
            auto &entry = g_plugin_zips[g_plugin_zip_count++];
            entry = {};
            snprintf(entry.name, sizeof entry.name, "Server %d", i + 1);
            network::root_path(i, entry.path, sizeof entry.path);
            entry.kind = fs::Kind::directory;
        }
    }
    else
    {
        fs::list_directory(g_zip_stack[g_zip_depth].path, g_plugin_zips, max_entries, &g_plugin_zip_count,
                           nullptr, true, fs::SortOrder::filename, true, ".zip");
    }
    g_plugin_zip_selection = std::clamp(g_zip_stack[g_zip_depth].selection, 0, std::max(0, g_plugin_zip_count - 1));
}

void begin_zip_picker() noexcept
{
    g_zip_depth = 0;
    g_zip_stack[0].selection = 0;
    g_plugin_import = true;
    scan_plugin_zips();
}

bool open_zip_details(const char *path) noexcept
{
    if (network::is_path(path)) return begin_file_inspection(path, true);
    if (!plugins::inspect_package(path, g_detail_plugin))
    {
        menu_info("Plugin package", "This ZIP has no supported Movian plugin manifest.");
        return false;
    }
    g_plugin_detail = PluginDetail::local;
    g_plugin_detail_row = 0;
    g_overlay = Overlay::plugin_details;
    return true;
}

void run_plugin_detail(const MenuEntry &entry) noexcept
{
    if (entry.id == MenuAction::plugin_open || entry.id == MenuAction::plugin_settings)
    {
        g_overlay = Overlay::none;
        open_plugin_page(entry.id == MenuAction::plugin_settings ? "#settings" : nullptr);
    }
    else if (entry.id == MenuAction::plugin_install)
    {
        if (g_plugin_job.busy) return;
        g_overlay = Overlay::none;
        if (g_plugin_detail == PluginDetail::local)
            open_plugin_job(PluginJob::Action::install_local, g_detail_plugin.archive);
        else
            open_plugin_job(PluginJob::Action::install, g_detail_item.url, true);
    }
    else
        menu_info(entry.label, "This plugin action is planned for a later PS5 build.");
}

void move_plugin_selection(unsigned pressed, int &selected, int count) noexcept
{
    if (count <= 0)
        return;
    if (pressed & pad::up)
        selected = (selected + count - 1) % count;
    if (pressed & pad::down)
        selected = (selected + 1) % count;
}

void update_plugins(unsigned pressed) noexcept
{
    if (g_plugin_job.busy && (pressed & pad::circle) && !g_plugin_job.dismissed)
    {
        g_plugin_job.dismissed = true;
        if (g_plugin_job.action == PluginJob::Action::catalog)
            g_place = Place::home;
        return;
    }
    if (pressed & pad::circle)
    {
        if (g_plugin_import && g_zip_depth > 0)
        {
            --g_zip_depth;
            scan_plugin_zips();
        }
        else if (g_plugin_import)
            g_plugin_import = false;
        else if (g_plugin_depth > 0)
            --g_plugin_depth;
        else if (g_repo_categories)
            g_repo_categories = false;
        else
            g_place = Place::home;
        return;
    }
    if (g_plugin_job.busy)
        return;
    if (!g_plugin_catalog_loaded && g_plugin_depth == 0 && !g_plugin_import && !g_repo_categories)
    {
        open_plugin_job(PluginJob::Action::catalog, nullptr);
        return;
    }
    if (g_plugin_import)
    {
        if (pressed & pad::triangle) scan_plugin_zips();
        move_plugin_selection(pressed, g_plugin_zip_selection, g_plugin_zip_count);
        g_zip_stack[g_zip_depth].selection = g_plugin_zip_selection;
        if (g_plugin_zip_count && (pressed & pad::cross))
        {
            const auto &entry = g_plugin_zips[g_plugin_zip_selection];
            if (entry.kind == fs::Kind::directory)
            {
                if (g_zip_depth + 1 == max_depth) screen::notify("Folder nesting limit reached");
                else
                {
                    ++g_zip_depth;
                    copy_text(g_zip_stack[g_zip_depth].path, sizeof g_zip_stack[g_zip_depth].path, entry.path);
                    g_zip_stack[g_zip_depth].selection = 0;
                    scan_plugin_zips();
                }
            }
            else open_zip_details(entry.path);
        }
        return;
    }
    if (g_plugin_depth == 0)
    {
        if (g_repo_categories)
        {
            const int last = static_cast<int>(std::size(plugins::category_names)) - 1;
            if ((pressed & pad::left) && g_repo_category % repo_columns) --g_repo_category;
            if ((pressed & pad::right) && g_repo_category % repo_columns < repo_columns - 1)
                g_repo_category = std::min(last, g_repo_category + 1);
            if ((pressed & pad::up) && g_repo_category >= repo_columns) g_repo_category -= repo_columns;
            if ((pressed & pad::down) && g_repo_category < repo_columns)
                g_repo_category = std::min(last, g_repo_category + repo_columns);
            if (pressed & pad::cross)
            {
                if (settings::repo_url()[0])
                    open_plugin_job(PluginJob::Action::repository, settings::repo_url());
                else
                    menu_info("Plugin repository", "Choose a repository in Settings → General.");
            }
            return;
        }
        move_plugin_selection(pressed, g_plugin_catalog_selection, g_installed_count + 2);
        if (pressed & pad::triangle)
            begin_zip_picker();
        else if (pressed & (pad::cross | pad::square))
        {
            if (g_plugin_catalog_selection == 0 && (pressed & pad::cross))
                g_repo_categories = true;
            else if (g_plugin_catalog_selection == g_installed_count + 1 && (pressed & pad::cross))
                begin_zip_picker();
            else if (const auto *plugin = selected_installed())
            {
                g_active_plugin = g_plugin_catalog_selection - 1;
                if (pressed & pad::square) open_plugin_page("#settings");
                else
                {
                    g_detail_plugin = *plugin;
                    g_plugin_detail = PluginDetail::installed;
                    g_plugin_detail_row = 0;
                    g_overlay = Overlay::plugin_details;
                }
            }
        }
        return;
    }
    const auto &page = g_plugin_pages[g_plugin_depth - 1];
    if (page.count == 0)
        return;
    int &selected = g_plugin_selection[g_plugin_depth - 1];
    move_plugin_selection(pressed, selected, page.count);
    if ((pressed & pad::triangle) && !strcmp(page.items[selected].type, "video"))
    {
        char request[sizeof page.items[selected].url];
        snprintf(request, sizeof request, "#hooks:video:%s", page.items[selected].title);
        open_plugin_page(request);
        return;
    }
    const bool repository = g_plugin_page_repo[g_plugin_depth - 1];
    if ((pressed & pad::square) && !repository)
    {
        open_plugin_page("#settings");
        return;
    }
    if (pressed & (repository ? pad::cross | pad::square : pad::cross))
    {
        const auto &item = page.items[selected];
        if (!std::strcmp(item.type, "package"))
        {
            g_detail_item = item;
            g_plugin_detail = PluginDetail::repository;
            g_plugin_detail_row = 0;
            g_overlay = Overlay::plugin_details;
        }
        else if (!std::strcmp(item.type, "repo"))
            open_plugin_job(PluginJob::Action::repository, settings::repo_url(),
                            g_plugin_depth >= plugin_page_limit,
                            static_cast<unsigned>(strtoul(item.url, nullptr, 10)));
        else if (!std::strcmp(item.type, "repo-refresh"))
            open_plugin_job(PluginJob::Action::repository_refresh, settings::repo_url(), true,
                            g_plugin_repo_offset[g_plugin_depth - 1]);
        else if (!std::strcmp(item.type, "next"))
            open_plugin_page(item.url, true);
        else if (!std::strncmp(item.url, "magnet:", 7))
        {
            if (torrent::magnet_hash(item.url, g_torrent_hash))
            {
                g_torrent_descriptor[0] = 0;
                g_torrent_selected = 0;
                g_place = Place::bittorrent;
                append_log("v59 opened BitTorrent magnet");
            }
            else
                screen::notify("Unsupported magnet format");
        }
        else if (!std::strcmp(item.type, "torrent-file"))
        {
            char path[384];
            if (plugins::torrent_path(g_installed_plugins[g_active_plugin], path, sizeof path))
                open_torrent_descriptor(path);
        }
        else if (const auto prefix = std::strlen(g_installed_plugins[g_active_plugin].id);
                 prefix && !std::strncmp(item.url, g_installed_plugins[g_active_plugin].id, prefix) &&
                 !std::strncmp(item.url + prefix, ":torrent:", 9))
            open_plugin_page(item.url);
        else if (!std::strncmp(item.url, "torrent:", 8))
            screen::notify("This torrent link needs a local descriptor or an embedded plugin redirect");
        else if (!std::strcmp(item.type, "directory") || !std::strcmp(item.type, "movie") ||
                 !std::strcmp(item.type, "hook") ||
                 (!std::strcmp(item.type, "video") &&
                  !std::strncmp(item.url, g_installed_plugins[g_active_plugin].id,
                                std::strlen(g_installed_plugins[g_active_plugin].id))))
            open_plugin_page(item.url);
        else if (!std::strcmp(item.type, "option") || !std::strcmp(item.type, "consent") ||
                 !std::strcmp(item.type, "page-option"))
        {
            if (plugins::set_option(g_installed_plugins[g_active_plugin], item.url))
                open_plugin_page(!std::strcmp(item.type, "option") ? "#settings" :
                                 !std::strcmp(item.type, "page-option") ? g_plugin_page_urls[g_plugin_depth - 1] :
                                 nullptr, true);
            else
                screen::notify("Could not save plugin setting");
        }
        else if (!std::strcmp(item.type, "text"))
        {
            snprintf(g_plugin_text_key, sizeof g_plugin_text_key, "%.62s", item.url);
            if (!keyboard::open(item.title, "Enter a value", item.detail, 60))
            {
                g_plugin_text_key[0] = 0;
                screen::notify("Keyboard unavailable");
            }
        }
        else if (!std::strcmp(item.type, "account") &&
                 !std::strcmp(g_installed_plugins[g_active_plugin].id, "KinoZal.TV"))
        {
            clear_login(g_plugin_job);
            char title[288];
            if (!plugins::kinozal_login_origin(g_login_origin))
            {
                append_log("KinoZal.TV sign-in target rejected");
                screen::notify("Select the KinoZal.TV domain and HTTPS in plugin settings");
                return;
            }
            snprintf(title, sizeof title, "Username for %s", g_login_origin);
            if (keyboard::open(title, "Username", "", 60))
                g_login_input = LoginInput::username;
            else
            {
                append_log("KinoZal.TV username keyboard did not open");
                screen::notify("Keyboard unavailable");
            }
        }
        else if (std::strcmp(item.type, "video") == 0 &&
                 std::strcmp(item.url, "file:///app0/assets/hevc-test.mp4") == 0)
            play_path(MOVIAN_APP0 "/assets/hevc-test.mp4", item.title, true);
        else
            menu_info(item.title, "This plugin action is planned for a later PS5 build.");
    }
}

void *download_torrent(void *opaque) noexcept
{
    auto &job = *static_cast<TorrentJob *>(opaque);
    job.ok = torrent::download(job.descriptor, job.peer, job.path, job.cancel,
                               job.progress, job.error, sizeof job.error, job.stream, job.storage, job.options);
    job.ready.store(true, std::memory_order_release);
    return nullptr;
}

void start_torrent_download(const char *peer) noexcept
{
    auto &job = g_torrent_job;
    if (g_torrent_metadata.stream_only && !settings::get().torrent_progressive)
    {
        screen::notify("Enable playback during download for this torrent");
        return;
    }
    copy_text(job.descriptor, sizeof job.descriptor, g_torrent_descriptor);
    copy_text(job.peer, sizeof job.peer, peer);
    job.ready = false;
    job.cancel = false;
    job.progress = 0;
    const auto &d = settings::get();
    job.storage = static_cast<torrent::CacheStorage>(d.torrent_cache_path);
    job.options = {{d.torrent_cache_mib, d.torrent_read_ahead, d.torrent_preload},
                   d.torrent_connections, d.torrent_requests, d.torrent_block_timeout,
                   static_cast<torrent::RetrackersMode>(d.torrent_retrackers),
                   static_cast<torrent::TrackerProtocols>(d.torrent_tracker_protocols)};
    diagnostics::record("torrent", "start cache=%d source=%s protocols=%u connections=%u requests=%u",
                        static_cast<int>(job.storage), peer[0] ? "direct peer" : "tracker",
                        d.torrent_tracker_protocols, d.torrent_connections, d.torrent_requests);
    diagnostics::record("torrent", "cache budget_mib=%u ahead_percent=%u preload_percent=%d timeout_seconds=%u",
                        d.torrent_cache_mib, d.torrent_read_ahead, d.torrent_preload, d.torrent_block_timeout);
    job.playback_attempted = false;
    job.playback_started = false;
    job.stream.reset();
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) == 0)
    {
        pthread_attr_setstacksize(&attr, 512 * 1024);
        job.busy = pthread_create(&job.thread, &attr, download_torrent, &job) == 0;
        pthread_attr_destroy(&attr);
    }
    if (job.busy)
        append_log(peer[0] ? "v59 direct peer transfer started" : "v59 tracker discovery started");
    else
        screen::notify("Could not start peer download");
}

bool finish_torrent_download() noexcept
{
    auto &job = g_torrent_job;
    if (!job.busy)
        return false;
    const bool in_torrent = g_place == Place::bittorrent && !player::active() &&
                            !std::strcmp(job.descriptor, g_torrent_descriptor);
    if (job.playback_started && job.stream.uses_cache() && !player::active())
        job.cancel = true;
    if (!job.ready.load(std::memory_order_acquire))
    {
        if (!in_torrent || job.playback_attempted || job.cancel || !settings::get().torrent_progressive ||
            !(job.stream.uses_cache() ? job.stream.can_start_mkv() : job.stream.can_start_mp4()))
            return false;
        job.playback_attempted = true;
        job.playback_started = play_path(job.path, g_torrent_metadata.title, true, &job.stream);
        if (job.playback_started)
            append_log("v59 peer playback started before download completed");
        else if (job.stream.uses_cache())
            job.cancel = true;
        return true;
    }
    pthread_join(job.thread, nullptr);
    job.busy = false;
    const bool used_cache = job.stream.uses_cache();
    if ((!job.ok || used_cache) && !avdec::progressive())
        job.stream.reset();
    if (!job.ok)
    {
        append_log(job.error);
        screen::notify(job.error);
        return true;
    }
    if (used_cache)
        return true;
    append_log("v59 direct peer verified download");
    if (in_torrent && !job.playback_started)
        play_path(job.path, g_torrent_metadata.title, true, &job.stream);
    else
        screen::notify("Verified torrent ready");
    return true;
}

void update_torrents(unsigned pressed) noexcept
{
    if (g_torrent_job.busy && (pressed & pad::circle))
    {
        g_torrent_job.cancel = true;
        return;
    }
    if (pressed & pad::circle)
    {
        if (g_torrent_descriptor[0] || g_torrent_hash[0])
        {
            g_torrent_descriptor[0] = 0;
            g_torrent_hash[0] = 0;
            g_torrent_selected = 0;
        }
        else
            g_place = Place::home;
        return;
    }
    if (g_torrent_hash[0])
        return;
    if (g_torrent_descriptor[0])
    {
        move_plugin_selection(pressed, g_torrent_selected, g_torrent_metadata.count);
        if (!g_torrent_job.busy && (pressed & (pad::square | pad::triangle)))
        {
            if (!g_torrent_metadata.direct_peer)
            {
                screen::notify("Torrent container, size or piece layout is unsupported");
                return;
            }
            char peer[sizeof g_torrent_job.peer]{};
            char sidecar[sizeof g_torrent_descriptor + 6];
            snprintf(sidecar, sizeof sidecar, "%s.peer", g_torrent_descriptor);
            if (auto *file = std::fopen(sidecar, "r"))
            {
                if (std::fgets(peer, sizeof peer, file))
                    peer[std::strcspn(peer, "\r\n")] = 0;
                std::fclose(file);
            }
            if (pressed & pad::triangle)
            {
                if (peer[0])
                    start_torrent_download(peer);
                else
                    screen::notify("No saved peer for this torrent. Press Cross to use its trackers.");
            }
            else
            {
                g_torrent_peer_input = keyboard::open("Direct BitTorrent peer", "IPv4:port", peer, 24);
                if (!g_torrent_peer_input)
                    screen::notify("Could not open the peer address keyboard");
            }
            return;
        }
        if (!g_torrent_job.busy && (pressed & pad::cross))
        {
            const auto &file = g_torrent_metadata.files[g_torrent_selected];
            char path[384];
            if (g_torrent_job.stream.complete() &&
                !std::strcmp(g_torrent_job.descriptor, g_torrent_descriptor))
                play_path(g_torrent_job.path, file.path, true, &g_torrent_job.stream);
            else if (!network::is_path(g_torrent_descriptor) &&
                (torrent::local_path(g_torrent_descriptor, file, path) ||
                 (g_torrent_metadata.direct_peer && torrent::cached_path(g_torrent_descriptor, path))))
                play_path(path, file.path, false);
            else if (g_torrent_metadata.direct_peer && settings::get().torrent_trackers)
                start_torrent_download("");
            else if (g_torrent_metadata.direct_peer)
                screen::notify("Local media unavailable. Press Square to enter a peer.");
            else
                screen::notify("Local media unavailable. This torrent needs swarm streaming.");
        }
        return;
    }
    if (!g_torrent_scanned || (pressed & pad::triangle))
    {
        g_torrent_source_count = scan_local_packages(".torrent", g_torrent_sources, 24);
        g_torrent_scanned = true;
        g_torrent_selected = 0;
    }
    move_plugin_selection(pressed, g_torrent_selected, g_torrent_source_count);
    if (g_torrent_source_count && (pressed & pad::cross))
    {
        const auto &entry = g_torrent_sources[g_torrent_selected];
        open_torrent_descriptor(entry.path);
    }
}

void draw_plugin_row(screen::Canvas &c, int row, bool selected, const char *title,
                     const char *detail, const char *tag, int start = 245,
                     int width = 1820) noexcept;

void draw_torrents(screen::Canvas &c) noexcept
{
    c.text(48.f, 160.f, "BitTorrent", font::semibold, 40, Color::text);
    if (g_torrent_hash[0])
    {
        c.text(48.f, 214.f, "Magnet info hash", font::regular, 21, Color::muted);
        c.text(76.f, 300.f, g_torrent_hash, font::mono_medium, 24, Color::text);
        c.text(76.f, 348.f, "Direct peer downloads require a .torrent file", font::regular, 22, Color::muted);
        return;
    }
    const bool viewing = g_torrent_descriptor[0];
    c.text(48.f, 214.f, viewing ? g_torrent_metadata.title : "Local torrent files",
           font::regular, 21, Color::muted);
    const int count = viewing ? g_torrent_metadata.count : g_torrent_source_count;
    const int first = g_torrent_selected < 7 ? 0 : g_torrent_selected - 6;
    for (int i = first; i < count && i < first + 7; ++i)
    {
        if (viewing)
        {
            const auto &file = g_torrent_metadata.files[i];
            char size[48];
            snprintf(size, sizeof size, "%llu bytes", static_cast<unsigned long long>(file.size));
            draw_plugin_row(c, i - first, i == g_torrent_selected, file.path, size, "Local");
        }
        else
            draw_plugin_row(c, i - first, i == g_torrent_selected, g_torrent_sources[i].name,
                            g_torrent_sources[i].path, "Torrent");
    }
    if (!count)
        c.text(76.f, 302.f, "No .torrent files in /data/movian or /download0",
               font::regular, 22, Color::muted);
    if (g_torrent_job.busy)
    {
        char progress[100];
        if (g_torrent_metadata.stream_only)
            copy_text(progress, sizeof progress, "Waiting for verified pieces     Circle  Cancel");
        else
            snprintf(progress, sizeof progress, "Download %u%% verified     Circle  Cancel",
                     g_torrent_job.progress.load(std::memory_order_relaxed));
        c.text(48.f, 930.f, progress, font::regular, 20, Color::muted);
        return;
    }
    c.text(48.f, 930.f, viewing ? "Cross  Play     Square  Enter peer     Triangle  Use saved peer     Circle  Back"
                               : "Cross  View files     Triangle  Refresh     Circle  Back",
           font::regular, 20, Color::muted);
}

void draw_plugin_row(screen::Canvas &c, int row, bool selected,
                     const char *title, const char *detail, const char *tag,
                     int start, int width) noexcept
{
    const int y = start + 88 * row;
    c.rect(50, y, width, 80, selected ? Color::hover : Color::surface2);
    if (selected)
        c.rect(50, y, 5, 80, Color::accent);
    char title_fit[112];
    c.text(76.f, y + 32.f, fit(title, width - 275, title_fit, sizeof title_fit, font::semibold, 26),
           font::semibold, 26, Color::text);
    char detail_fit[168];
    c.text(76.f, y + 60.f, fit(detail, width - 275, detail_fit, sizeof detail_fit, font::regular, 18),
           font::regular, 18, Color::muted);
    c.text(50.f + width - 230, y + 43.f, tag, font::regular, 20, Color::muted);
}

const plugins::Plugin *installed_by_id(const char *id) noexcept
{
    if (!*id) return nullptr;
    for (int i = 0; i < g_installed_count; ++i)
        if (!std::strcmp(g_installed_plugins[i].id, id)) return &g_installed_plugins[i];
    return nullptr;
}

bool draw_selected_artwork(screen::Canvas &c, int x, int y, int w, int h) noexcept
{
    const char *wanted = selected_plugin_icon();
    if (!*wanted || std::strcmp(wanted, g_artwork_url) || !g_plugin_artwork.loaded() ||
        std::strcmp(g_artwork_plugin, artwork_owner().id))
        return false;
    const auto &art = g_plugin_artwork;
    const float scale = std::min(static_cast<float>(w) / art.w, static_cast<float>(h) / art.h);
    const int width = std::max(1, static_cast<int>(art.w * scale));
    const int height = std::max(1, static_cast<int>(art.h * scale));
    c.image(x + (w - width) / 2, y + (h - height) / 2, width, height, art);
    return true;
}

void draw_repository_categories(screen::Canvas &c) noexcept
{
    c.text(50.f, 160.f, "Plugin categories", font::semibold, 40, Color::text);
    c.text(50.f, 214.f, settings::repo_label(), font::regular, 22, Color::muted);
    for (int i = 0; i < static_cast<int>(std::size(plugins::category_names)); ++i)
    {
        const int x = 50 + i % repo_columns * 366;
        const int y = 266 + i / repo_columns * 164;
        draw_card(c, x, y, 344, 124, plugins::category_names[i], "", i == g_repo_category);
    }
    c.text(50.f, 930.f, "Cross  Browse     Circle  Back", font::regular, 20, Color::muted);
}

void draw_plugins(screen::Canvas &c) noexcept
{
    const auto *page = g_plugin_depth > 0 ? &g_plugin_pages[g_plugin_depth - 1] : nullptr;
    const bool repository = page && g_plugin_page_repo[g_plugin_depth - 1];
    if (g_repo_categories && !page && !g_plugin_job.busy)
    {
        draw_repository_categories(c);
        return;
    }
    const char *heading = g_plugin_import ? "Install from ZIP" : page ? page->title :
                          g_repo_categories ? "Plugin categories" : "Plugins";
    const char *sub = g_plugin_import ? (g_zip_depth ? g_zip_stack[g_zip_depth].path : "Choose a source")
                      : repository || g_repo_categories ? settings::repo_label()
                      : page ? g_installed_plugins[g_active_plugin].title : "";
    c.text(48.f, 160.f, heading, font::semibold, 40, Color::text);
    if (*sub)
    {
        char shown[168];
        c.text(48.f, 214.f, fit(sub, 1650, shown, sizeof shown, font::regular, 21),
               font::regular, 21, Color::muted);
    }
    if (g_plugin_job.busy && !g_plugin_job.dismissed)
    {
        const auto action = g_plugin_job.action;
        const char *status = action == PluginJob::Action::repository || action == PluginJob::Action::repository_refresh
            ? "Loading repository…" : action == PluginJob::Action::install || action == PluginJob::Action::install_local
            ? "Installing plugin…" : "Loading plugin…";
        c.text(76.f, 300.f, status, font::regular, 24, Color::muted);
        c.text(48.f, 930.f, "Circle  Back", font::regular, 20, Color::muted);
        return;
    }
    if (g_plugin_import)
    {
        const int first = std::max(0, g_plugin_zip_selection - 6);
        for (int i = first; i < g_plugin_zip_count && i < first + 7; ++i)
        {
            const auto &entry = g_plugin_zips[i];
            draw_plugin_row(c, i - first, i == g_plugin_zip_selection, entry.name, "",
                            entry.kind == fs::Kind::directory ? "Folder" : "ZIP");
        }
        if (!g_plugin_zip_count)
            c.text(76.f, 304.f, "No folders or plugin ZIPs found", font::regular, 22, Color::muted);
        c.text(48.f, 930.f, "Cross  Open     Triangle  Refresh     Circle  Back",
               font::regular, 20, Color::muted);
        return;
    }
    if (!page)
    {
        draw_plugin_row(c, 0, g_plugin_catalog_selection == 0, "Browse available plugins",
                        settings::repo_label(), "Browse", 204);
        c.text(72.f, 338.f, "Installed plugins", font::semibold, 29, Color::text);
        if (!g_installed_count)
            c.text(73.f, 384.f, "No plugins installed", font::regular, 21, Color::muted);
        const int list_start = g_installed_count ? 356 : 432;
        const int first = std::max(1, g_plugin_catalog_selection - 5);
        const auto *plugin = selected_installed();
        const bool show_art = plugin && plugin->icon[0];
        if (show_art) draw_selected_artwork(c, 1330, 362, 495, 445);
        for (int i = first; i <= g_installed_count + 1 && i < first + 6; ++i)
            if (i == g_installed_count + 1)
                draw_plugin_row(c, i - first, i == g_plugin_catalog_selection, "Install from ZIP",
                                "Browse local files", "Local", list_start, show_art ? 1230 : 1820);
            else
            {
                const auto &entry = g_installed_plugins[i - 1];
                draw_plugin_row(c, i - first, i == g_plugin_catalog_selection, entry.title,
                                entry.synopsis, entry.version, list_start, show_art ? 1230 : 1820);
            }
        c.text(48.f, 930.f, "Cross  Open     Square  Plugin settings     Triangle  ZIP picker     Circle  Back",
               font::regular, 20, Color::muted);
        return;
    }
    int start = 245;
    int visible = 7;
    if (repository && page->notice[0])
    {
        c.text(76.f, 249.f, page->notice, font::regular, 20, Color::muted);
        start = 274;
    }
    else if (page->notice[0])
    {
        int position = 0;
        const int length = static_cast<int>(std::strlen(page->notice));
        for (int line = 0; position < length && line < 13; ++line)
        {
            const int begin = position;
            int last_space = -1;
            while (position < length && position - begin < 184 && page->notice[position] != '\n')
            {
                if (page->notice[position] == ' ')
                    last_space = position;
                (void)font::next(page->notice, length, position);
            }
            if (position < length && page->notice[position] != '\n' && last_space > begin)
                position = last_space;
            char text[192];
            snprintf(text, sizeof text, "%.*s", position - begin, page->notice + begin);
            c.text(76.f, 276.f + 31 * line, text, font::regular, 20, Color::text);
            while (page->notice[position] == ' ' || page->notice[position] == '\n')
                ++position;
        }
        start = 690;
        visible = 2;
    }
    const int selected = g_plugin_selection[g_plugin_depth - 1];
    const int first = selected < visible ? 0 : selected - visible + 1;
    const char *wanted = selected_plugin_icon();

    const bool show_art = *wanted && (repository || !page->notice[0]);
    if (show_art) draw_selected_artwork(c, 1310, start, 530, 590);
    if (repository && !page->count)
        c.text(76.f, start + 40.f, "No packages in this category", font::regular, 22, Color::muted);
    for (int index = first; index < page->count && index < first + visible; ++index)
    {
        const auto &item = page->items[index];
        const bool package = repository && !std::strcmp(item.type, "package");
        const auto *installed = package ? installed_by_id(item.id) : nullptr;
        const char *tag = package ? (installed ? "Installed" : item.version) :
                           repository || !std::strcmp(item.type, "option") ? "" : item.type;
        draw_plugin_row(c, index - first, index == selected, item.title, item.detail, tag,
                        start, show_art ? 1220 : 1820);
    }
    c.text(48.f, 930.f, repository ? "Cross  Details or open     Circle  Back"
                                   : "Cross  Select     Square  Plugin settings     Triangle  Video actions     Circle  Back",
           font::regular, 20, Color::muted);
}

void draw_library_grid(screen::Canvas &c, const BrowseLevel &level) noexcept
{
    constexpr int tile_width = 338, tile_height = 153;
    constexpr int x_step = 356, y_step = 168;
    for (int i = level.first; i < std::min(g_row_count, level.first + library_grid_page); ++i)
    {
        const int relative = i - level.first;
        const int x = 76 + (relative % library_columns) * x_step;
        const int y = 265 + (relative / library_columns) * y_step;
        const auto &item = g_rows[i];
        const bool focused = i == level.selected;
        c.round_rect(x, y, tile_width, tile_height, 12, focused ? Color::hover : Color::surface);
        if (focused) c.round_frame(x, y, tile_width, tile_height, 12, 3, Color::accent);
        if (item.kind == RowKind::folder)
        {
            c.round_rect(x + 23, y + 30, 35, 28, 4, Color::accent);
            c.rect(x + 26, y + 25, 14, 7, Color::accent);
        }
        else if (item.kind == RowKind::file)
        {
            c.round_frame(x + 23, y + 25, 31, 34, 4, 2, Color::muted);
            c.rect(x + 30, y + 36, 16, 2, Color::muted);
            c.rect(x + 30, y + 44, 16, 2, Color::muted);
        }
        else
            c.text(x + 24.f, y + 52.f, "!", font::semibold, 28, Color::orange);
        char shown[160];
        c.text(x + 23.f, y + 98.f, fit(item.title, tile_width - 46, shown, sizeof shown, font::medium, 23),
               font::medium, 23, focused ? Color::text : Color::muted);
        c.text(x + 23.f, y + 125.f, fit(item.detail, tile_width - 46, shown, sizeof shown, font::regular, 16),
               font::regular, 16, Color::muted);
    }
    const int total_rows = (g_row_count + library_columns - 1) / library_columns;
    if (total_rows > library_grid_rows)
    {
        constexpr int height = library_grid_rows * y_step - (y_step - tile_height);
        const int thumb = height * library_grid_rows / total_rows;
        const int offset = (height - thumb) * (level.first / library_columns) / (total_rows - library_grid_rows);
        c.rect(1847, 265, 3, height, Color::line);
        c.rect(1847, 265 + offset, 3, thumb, Color::accent);
    }
}

void draw_videos(screen::Canvas &c) noexcept
{
    if (g_videos_dirty)
        reload_videos();

    const BrowseLevel &lv = g_stack[g_depth];
    c.text(48.f, 160.f, lv.title, font::semibold, 40, Color::text);
    const bool search = !std::strcmp(lv.path, kSearchUri);
    const char *crumb = search ? g_home_query : lv.path[0] ? lv.path : "Local sources";
    char crumb_buf[160];
    c.text(48.f, 208.f, fit(crumb, 1440, crumb_buf, sizeof crumb_buf, font::regular, 20),
           font::regular, 20, Color::muted);
    if (search && g_truncated)
        c.text(1550.f, 208.f, "Some entries omitted", font::regular, 20, Color::muted);

    c.round_rect(48, 236, 1824, 748, 20, Color::surface2);
    if (browser_grid())
        draw_library_grid(c, lv);
    else
    {
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

    }

    if (g_selected_note[0])
        c.text(72.f, 980.f, g_selected_note, font::regular, 18, Color::orange);
}

std::span<const settings_menu::Row> settings_rows() noexcept
{
    if (g_settings_subpage)
        return {g_settings_subpage->children, g_settings_subpage->child_count};
    return settings_menu::categories[g_settings_category].rows;
}

int move_setting_row(int row, int direction) noexcept
{
    const auto rows = settings_rows();
    const int count = static_cast<int>(rows.size());
    for (int i = row + direction; i >= 0 && i < count; i += direction)
        if (rows[i].kind != settings_menu::Kind::section) return i;
    return row;
}

const settings_menu::Row &selected_setting() noexcept
{
    return settings_rows()[g_settings_row];
}

int setting_choices(int action) noexcept
{
    using namespace settings_menu;
    switch (action)
    {
    case smb_large: case ftp_enabled: case remote_control:
    case auto_next: case shuffle: case debug: case hardware: case show_all: case newest:
    case trackers: case progressive: case diagnostics: case extensions: case playback_clock:
    case time_format:
        return 2;
    case resume: case repeat: case cache: case tracker_protocols: return 3;
    case smb_attributes: return 4;
    case seek: return std::size(settings::seek_steps);
    case volume: return std::size(settings::volume_presets);
    case repository: return settings::repo_preset_count;
    case peer_connections: return 8;
    case peer_requests: return 29;
    case retrackers: return std::size(retracker_labels);
    default: return 0;
    }
}

int current_choice(int action) noexcept
{
    using namespace settings_menu;
    const auto &d = settings::get();
    switch (action)
    {
    case remote_control: return d.remote_control;
    case smb_large: return network::config().smb_large_buffer;
    case smb_attributes: return network::config().smb_xattr;
    case ftp_enabled: return network::server_running();
    case resume: return !d.resume ? 1 : d.resume_ask ? 2 : 0;
    case auto_next: return d.auto_next;
    case shuffle: return d.shuffle;
    case time_format: return !d.clock_24h;
    case debug: return d.debug_log;
    case hardware: return d.hw_decode;
    case show_all: return d.show_all;
    case newest: return d.newest_first;
    case trackers: return d.torrent_trackers;
    case progressive: return d.torrent_progressive;
    case diagnostics: return d.remote_diagnostics;
    case extensions: return d.show_extensions;
    case playback_clock: return d.playback_clock;
    case repeat: return d.repeat;
    case seek: return std::find(std::begin(settings::seek_steps), std::end(settings::seek_steps),
                                d.seek_step) - std::begin(settings::seek_steps);
    case volume: return std::lower_bound(std::begin(settings::volume_presets), std::end(settings::volume_presets),
                                         d.volume) - std::begin(settings::volume_presets);
    case repository: return d.custom_repo_url[0] ? -1 : d.repo_preset;
    case cache: return d.torrent_cache_path;
    case peer_connections: return static_cast<int>(d.torrent_connections) - 1;
    case peer_requests: return static_cast<int>(d.torrent_requests) - 4;
    case retrackers: return d.torrent_retrackers;
    case tracker_protocols: return d.torrent_tracker_protocols;
    default: return 0;
    }
}

void apply_setting_choice(int action, int choice) noexcept
{
    using namespace settings_menu;
    auto &d = settings::get();
    switch (action)
    {
    case remote_control: d.remote_control=choice; settings::save(); return;
    case smb_large: network::config().smb_large_buffer=choice; network::save(); return;
    case smb_attributes: network::config().smb_xattr=choice; network::save(); screen::notify("Attribute storage is pending on PS5"); return;
    case ftp_enabled:
        network::config().ftp_server=choice;
        if(!network::restart_server()) {
            network::config().ftp_server=false;
            screen::notify("FTP server unavailable. Set username and password and check the port.");
        }
        network::save(); return;
    case resume: d.resume = choice != 1; d.resume_ask = choice == 2; break;
    case repeat: d.repeat = choice; break;
    case seek: d.seek_step = settings::seek_steps[choice]; break;
    case volume:
        d.volume = settings::volume_presets[choice];
        audio::set_volume(d.volume);
        break;
    case repository: d.repo_preset = choice; d.custom_repo_url[0] = 0; break;
    case cache: d.torrent_cache_path = choice; break;
    case peer_connections: d.torrent_connections = choice + 1; break;
    case peer_requests: d.torrent_requests = choice + 4; break;
    case retrackers: d.torrent_retrackers = choice; break;
    case tracker_protocols: d.torrent_tracker_protocols = choice; break;
    default:
        if (current_choice(action) != choice) setting_nudge(action, 1);
        return;
    }
    settings::save();
}

void choice_label(int action, int choice, char *value, std::size_t cap) noexcept
{
    using namespace settings_menu;
    constexpr const char *repeat_labels[] = {"Off", "One video", "Folder"};
    constexpr const char *storage[] = {"RAM", "PS5 storage (HDD)", "USB"};
    constexpr const char *resume_labels[] = {"Always", "Never", "Ask"};
    constexpr const char *protocol_labels[] = {"TCP & UDP", "TCP", "UDP"};
    switch (action)
    {
    case smb_attributes: { constexpr const char *labels[]={"Off","SMBv1","SMBv2","On"};
        snprintf(value,cap,"%s (attribute storage pending)",labels[choice]); break; }
    case resume: copy_text(value, cap, resume_labels[choice]); break;
    case repeat: copy_text(value, cap, repeat_labels[choice]); break;
    case seek: snprintf(value, cap, "%d seconds", settings::seek_steps[choice]); break;
    case volume: snprintf(value, cap, "%d%%", settings::volume_presets[choice]); break;
    case repository: copy_text(value, cap, settings::repo_label(choice)); break;
    case cache: copy_text(value, cap, storage[choice]); break;
    case time_format: copy_text(value, cap, choice ? "12 Hour" : "24 Hour"); break;
    case peer_connections: snprintf(value, cap, "%d", choice + 1); break;
    case peer_requests: snprintf(value, cap, "%d", choice + 4); break;
    case retrackers: copy_text(value, cap, retracker_labels[choice]); break;
    case tracker_protocols: copy_text(value, cap, protocol_labels[choice]); break;
    default: copy_text(value, cap, choice ? "On" : "Off"); break;
    }
}

void open_settings(bool from_sidebar = false) noexcept
{
    g_settings_from_sidebar = from_sidebar;
    g_settings_in_category = false;
    g_settings_subpage = nullptr;
    g_settings_row = move_setting_row(-1, 1);
    g_overlay = Overlay::settings;
}

void update_home(unsigned pressed) noexcept
{
    if (g_home_search_focused)
    {
        if (pressed & pad::down)
            g_home_search_focused = false;
        else if (pressed & pad::cross)
        {
            g_home_keyboard = keyboard::open("Search local media", "File or folder name", g_home_query,
                                             keyboard::max_length);
            if (!g_home_keyboard) screen::notify("Could not open the search keyboard");
        }
        return;
    }

    if ((pressed & pad::left) && g_home_tile % home_columns > 0)
        --g_home_tile;
    else if ((pressed & pad::right) && g_home_tile % home_columns < home_columns - 1 &&
             g_home_tile < last_home_tile)
        ++g_home_tile;
    else if (pressed & pad::up)
    {
        if (g_home_tile < home_columns)
            g_home_search_focused = true;
        else
            g_home_tile -= home_columns;
    }
    else if ((pressed & pad::down) && g_home_tile < home_columns)
        g_home_tile = std::min(g_home_tile + home_columns, last_home_tile);
    else if (pressed & pad::cross)
    {
        switch (home_tiles[g_home_tile].action)
        {
        case HomeAction::plugins: g_place = Place::plugins; break;
        case HomeAction::network: g_overlay = Overlay::home_network; break;
        case HomeAction::settings: open_settings(); break;
        case HomeAction::library: open_home_folder(MOVIAN_DATA_DIR, "Movian library"); break;
        case HomeAction::title_data: open_home_folder(MOVIAN_DOWNLOAD0, "Title data"); break;
        case HomeAction::usb: open_home_folder(kUsb0, "USB"); break;
        case HomeAction::bookmarks: open_home_folder(kBookmarksUri, "Bookmarks"); break;
        case HomeAction::resume:
        {
            const auto resume = settings::last_resume();
            if (resume.valid) play_path(resume.path, resume.title, false);
            else screen::notify("Nothing to continue");
            break;
        }
        }
    }
}

bool local_folder_page() noexcept
{
    const char *path = g_stack[g_depth].path;
    return g_place == Place::videos && path[0] && std::strcmp(path, kBookmarksUri) &&
           std::strcmp(path, kSearchUri);
}

std::span<const char *const> browser_choices(MenuAction action) noexcept
{
    static constexpr const char *layouts[] = {"List", "Grid"};
    static constexpr const char *sorts[] = {"Filename", "Newest first", "Oldest first"};
    return action == MenuAction::page_layout ? std::span<const char *const>(layouts) :
                                              std::span<const char *const>(sorts);
}

int current_browser_choice(MenuAction action) noexcept
{
    const auto prefs = settings::browser_prefs(g_stack[g_depth].path);
    return action == MenuAction::page_layout ? prefs.grid : static_cast<int>(prefs.sort);
}

void apply_browser_choice(MenuAction action, int value) noexcept
{
    const char *path = g_stack[g_depth].path;
    auto prefs = settings::browser_prefs(path);
    switch (action)
    {
    case MenuAction::page_layout: prefs.grid = value == 1; g_stack[g_depth].first = 0; break;
    case MenuAction::sort_on: prefs.sort = static_cast<fs::SortOrder>(value); break;
    case MenuAction::folders_first: prefs.folders_first = !prefs.folders_first; break;
    case MenuAction::supported_only: prefs.supported_only = !prefs.supported_only; break;
    default: return;
    }
    settings::save_browser_prefs(path, prefs);
    g_videos_dirty = true;
}

void open_sidebar() noexcept
{
    g_sidebar_row = 0;
    g_sidebar_count = 0;
    const bool active = player::active();
    for (const auto &entry : sidebar_entries)
    {
        if (!active && entry.id <= MenuAction::shuffle)
            continue;
        if (entry.id == MenuAction::page_layout && (active || g_place != Place::videos))
            continue;
        if (entry.id >= MenuAction::include_library && entry.id <= MenuAction::erase_playback &&
            (active || !local_folder_page()))
            continue;
        if (!active && g_place == Place::home &&
            (entry.id == MenuAction::bookmark || entry.id == MenuAction::paste || entry.id == MenuAction::home))
            continue;
        g_sidebar_visible[g_sidebar_count++] = entry;
    }
    g_overlay = Overlay::sidebar;
}

void show_log() noexcept
{
    menu_info("Movian log");
    auto *log = std::fopen(MOVIAN_LOG_PATH, "r");
    if (!log)
    {
        copy_text(g_menu_lines[0], sizeof g_menu_lines[0], "No Movian log is available.");
        return;
    }
    if (std::fseek(log, 0, SEEK_END) == 0 && std::ftell(log) > 8192)
    {
        std::fseek(log, -8192, SEEK_END);
        int ch;
        do { ch = std::fgetc(log); } while (ch != EOF && ch != '\n');
    }
    else
        std::rewind(log);
    char line[sizeof g_menu_lines[0]];
    while (std::fgets(line, sizeof line, log))
    {
        line[std::strcspn(line, "\r\n")] = 0;
        for (char *p = line; *p; ++p)
            if (static_cast<unsigned char>(*p) < 32) *p = ' ';
        std::memmove(g_menu_lines, g_menu_lines + 1, sizeof g_menu_lines - sizeof line);
        copy_text(g_menu_lines[menu_info_lines - 1], sizeof line, line);
    }
    std::fclose(log);
}

void reload_current_page() noexcept
{
    if (g_place == Place::videos) {
        network::invalidate();
        g_videos_dirty = true;
    }
    else if (g_place == Place::bittorrent && !g_torrent_job.busy && !g_torrent_descriptor[0] && !g_torrent_hash[0])
        update_torrents(pad::triangle);
    else if (g_place == Place::plugins && !g_plugin_job.busy)
    {
        if (g_plugin_import) scan_plugin_zips();
        else if (g_plugin_depth > 0)
        {
            const int index = g_plugin_depth - 1;
            if (g_plugin_page_repo[index])
                open_plugin_job(PluginJob::Action::repository, g_plugin_page_urls[index], true,
                                g_plugin_repo_offset[index]);
            else open_plugin_page(g_plugin_page_urls[index], true);
        }
        else g_plugin_catalog_loaded = false;
    }
}

void request_quit() noexcept;
void run_sidebar_action(const MenuEntry &entry) noexcept
{
    switch (entry.id)
    {
    case MenuAction::stop:
    case MenuAction::previous:
    case MenuAction::rewind:
    case MenuAction::pause:
    case MenuAction::forward:
    case MenuAction::next:
        if (!player::active())
        {
            menu_info(entry.label, "Start a video to use playback controls.");
            return;
        }
        switch (entry.id)
        {
        case MenuAction::stop: player::close(); g_overlay = Overlay::none; break;
        case MenuAction::previous: (void)play_delta(-1, true); break;
        case MenuAction::rewind: player::seek_step_by(-1); break;
        case MenuAction::pause: player::toggle_pause(); break;
        case MenuAction::forward: player::seek_step_by(1); break;
        case MenuAction::next: (void)play_delta(1, true); break;
        default: break;
        }
        return;
    case MenuAction::queue:
    {
        menu_info(entry.label);
        if (g_queue_n == 0)
            copy_text(g_menu_lines[0], sizeof g_menu_lines[0], "The play queue is empty.");
        const int first = std::max(0, g_queue_i - 1);
        for (int i = first; i < std::min(g_queue_n, first + menu_info_lines); ++i)
            snprintf(g_menu_lines[i - first], sizeof g_menu_lines[0], "%s%s",
                     i == g_queue_i ? "Current: " : "", g_queue_title[i]);
        return;
    }
    case MenuAction::repeat: settings::cycle_repeat(1); return;
    case MenuAction::shuffle: setting_nudge(3, 1); return;
    case MenuAction::bookmark:
    {
        const char *path = player::active() ? player::path() :
            g_place == Place::videos ? g_stack[g_depth].path : "";
        if (!*path || !std::strcmp(path, kSearchUri) || !std::strcmp(path, kBookmarksUri))
            menu_info(entry.label, "Open a folder or video to bookmark it.");
        else
        {
            settings::toggle_bookmark(path, player::active() ? player::title() : g_stack[g_depth].title);
            g_videos_dirty = true;
        }
        return;
    }
    case MenuAction::page_layout:
    case MenuAction::sort_on:
        g_browser_choice = current_browser_choice(entry.id);
        g_overlay = Overlay::browser_choice;
        return;
    case MenuAction::folders_first:
    case MenuAction::supported_only:
        apply_browser_choice(entry.id, 0);
        return;
    case MenuAction::reload: reload_current_page(); g_overlay = Overlay::none; return;
    case MenuAction::volume: setting_nudge(5, 1); return;
    case MenuAction::media:
        menu_info(entry.label, player::active() ? player::title() : "No video is playing.");
        if (player::active())
            snprintf(g_menu_lines[1], sizeof g_menu_lines[1], "%d × %d   %.0f s / %.0f s",
                     player::width(), player::height(), player::time(), player::duration());
        return;
    case MenuAction::system:
        menu_info(entry.label, kTitleId);
        snprintf(g_menu_lines[1], sizeof g_menu_lines[1], "%s   %s", kVersion, hwdec_status());
        return;
    case MenuAction::log: show_log(); return;
    case MenuAction::home:
        if (player::active()) player::close();
        g_overlay = Overlay::none;
        g_place = Place::home;
        return;
    case MenuAction::settings: open_settings(true); return;
    case MenuAction::exit: request_quit(); return;
    default:
        menu_info(entry.label, "This action is planned for a later PS5 build.");
        return;
    }
}

void draw_settings_overlay(screen::Canvas &c) noexcept
{
    const auto &categories = settings_menu::categories;
    const auto &category = categories[g_settings_category];
    c.rect(0, 90, screen::width, 918, Color::bg);
    c.text(76.f, 158.f, "Settings", font::semibold, 42, Color::text);
    c.round_rect(68, 191, 504, 788, 12, Color::surface);
    c.round_rect(602, 191, 1250, 788, 12, Color::surface);
    for (int i = 0; i < static_cast<int>(categories.size()); ++i)
    {
        const int y = 207 + i * 53;
        if (i == g_settings_category)
        {
            c.round_rect(84, y, 472, 47, 8, Color::hover);
            if (!g_settings_in_category) c.rect(84, y + 8, 4, 31, Color::accent);
        }
        c.text(114.f, static_cast<float>(y + 32), categories[i].title, font::medium, 25,
               i == g_settings_category ? Color::text : Color::muted);
    }
    const char *heading = g_settings_subpage ? g_settings_subpage->title : category.title;
    char display_heading[96];
    c.text(642.f, 253.f, fit(heading, 1140, display_heading, sizeof display_heading, font::semibold, 33),
           font::semibold, 33, Color::text);
    constexpr int visible = 10;
    const auto rows = settings_rows();
    const int size = static_cast<int>(rows.size());
    const int first = std::clamp(g_settings_row - visible + 1, 0, std::max(0, size - visible));
    for (int i = first; i < std::min(size, first + visible); ++i)
    {
        const auto &row = rows[i];
        const int y = 276 + (i - first) * 65;
        if (row.kind == settings_menu::Kind::section)
        {
            c.text(655.f, static_cast<float>(y + 39), row.title, font::semibold, 21, Color::accent);
            c.rect(655, y + 52, 1130, 1, Color::line2);
            continue;
        }
        bool selected = g_settings_in_category && i == g_settings_row;
        if (selected)
        {
            c.round_rect(628, y, 1198, 59, 10, Color::hover);
            c.rect(628, y + 10, 4, 39, Color::accent);
        }
        char label[100];
        c.text(655.f, static_cast<float>(y + 38), fit(row.title, 648, label, sizeof label, font::medium, 24),
               font::medium, 24, selected ? Color::text : Color::muted);
        char value[64]{};
        if (row.kind == settings_menu::Kind::page)
            copy_text(value, sizeof value, "Open");
        else if (row.action < 0)
            copy_text(value, sizeof value, "Pending");
        else
            setting_value(row.action, value, sizeof value);
        char shown[64];
        c.text(1330.f, static_cast<float>(y + 38),
               fit(value, 450, shown, sizeof shown, font::regular, 23), font::regular, 23,
               row.action < 0 && row.kind != settings_menu::Kind::page ? Color::muted : Color::accent);
    }
    if (size > visible)
    {
        const int track = 634;
        const int thumb = track * visible / size;
        c.rect(1827, 281, 3, track, Color::line);
        c.rect(1827, 281 + (track - thumb) * first / (size - visible), 3, thumb, Color::accent);
    }
}

const char *pending_setting_message(settings_menu::Kind kind) noexcept
{
    using settings_menu::Kind;
    switch (kind)
    {
    case Kind::toggle: return "This toggle is pending on PS5.";
    case Kind::number: return "This numeric control is pending on PS5.";
    case Kind::text: return "Text entry for this setting is pending on PS5.";
    case Kind::select: return "This selector is pending on PS5.";
    default: return "This action is pending on PS5.";
    }
}

void draw_setting_detail(screen::Canvas &c) noexcept
{
    draw_settings_overlay(c);
    const auto &row = selected_setting();
    const int count = setting_choices(row.action);
    const bool numeric = row.kind == settings_menu::Kind::number && count;
    const int height = numeric ? 260 : count ? 181 + count * 66 : 240;
    const int top = (screen::height - height) / 2;
    c.rect(0, 90, screen::width, 918, Color::bg, 183);
    c.round_rect(524, top, 872, height, 20, Color::surface2);
    c.round_frame(524, top, 872, height, 20, 2, Color::line);
    char title[128];
    c.text(576.f, static_cast<float>(top + 64), fit(row.title, 776, title, sizeof title, font::semibold, 31),
           font::semibold, 31, Color::text);
    if (!count)
    {
        c.text(576.f, static_cast<float>(top + 130), pending_setting_message(row.kind),
               font::regular, 25, Color::muted);
    }
    else if (numeric)
    {
        char value[40], saved[40], current[80];
        choice_label(row.action, g_setting_choice, value, sizeof value);
        choice_label(row.action, current_choice(row.action), saved, sizeof saved);
        snprintf(current, sizeof current, "Current: %s", saved);
        constexpr int x = 576, width = 758;
        c.text(x, static_cast<float>(top + 125), value, font::semibold, 30, Color::text);
        c.text(1150.f, static_cast<float>(top + 125), current, font::regular, 22, Color::muted);
        c.round_rect(x, top + 159, width, 8, 4, Color::line2);
        c.circle(x + static_cast<float>(width * g_setting_choice) / (count - 1), top + 163.f, 9.f, Color::accent);
    }
    else
    {
        const int current = current_choice(row.action);
        for (int i = 0; i < count; ++i)
        {
            const int y = top + 94 + i * 66;
            if (i == g_setting_choice)
                c.round_rect(561, y, 798, 59, 10, Color::hover);
            char value[64]{};
            choice_label(row.action, i, value, sizeof value);
            c.text(585.f, static_cast<float>(y + 40), value, font::medium, 26, Color::text);
            if (i == current)
                c.text(1195.f, static_cast<float>(y + 39), "Current", font::regular, 20, Color::accent);
        }
    }
    c.text(576.f, static_cast<float>(top + height - 24),
           numeric ? "Left / Right adjust    Cross save    Circle cancel" :
           count ? "Cross save    Circle cancel" : "Cross or Circle back",
           font::regular, 20, Color::muted);
}

void draw_resume_prompt(screen::Canvas &c) noexcept
{
    c.rect(0, 0, screen::width, screen::height, Color::bg, 140);
    c.round_rect(540, 300, 840, 396, 18, Color::surface2);
    c.round_frame(540, 300, 840, 396, 18, 2, Color::line);
    c.text(590.f, 363.f, "Resume video playback?", font::semibold, 32, Color::text);
    char title[112];
    c.text(590.f, 410.f, fit(g_resume_title, 750, title, sizeof title, font::regular, 22),
           font::regular, 22, Color::muted);
    double offset = 0;
    (void)settings::resume_for(g_resume_path, &offset, nullptr);
    const auto seconds = static_cast<unsigned>(std::clamp(offset, 0.0, 35999999.0));
    char label[48];
    snprintf(label, sizeof label, "Continue from %u:%02u", seconds / 60, seconds % 60);
    for (int i = 0; i < 2; ++i)
    {
        const int y = 438 + 72 * i;
        if (i == g_resume_choice)
            c.round_rect(572, y, 776, 60, 10, Color::hover);
        c.text(598.f, static_cast<float>(y + 39), i == 0 ? label : "Play from beginning",
               font::medium, 24, Color::text);
    }
    c.text(590.f, 659.f, "Cross play    Circle cancel", font::regular, 20, Color::muted);
}

void draw_torrent_settings_overlay(screen::Canvas &c) noexcept
{
    constexpr const char *labels[torrent_setting_count] = {
        "Cache storage location", "Cache size", "Read-ahead cache", "Preload cache before play",
        "Connections limit", "Concurrent requests per peer", "Peer block timeout", "Retrackers mode"
    };
    const auto &d = settings::get();
    char values[torrent_setting_count][64]{};
    setting_value(15, values[0], sizeof values[0]);
    const auto limit = torrent::cache_limit_mib(static_cast<torrent::CacheStorage>(d.torrent_cache_path));
    if (d.torrent_cache_mib)
        snprintf(values[1], sizeof values[1], "%u MiB", std::min(d.torrent_cache_mib, limit));
    else
        copy_text(values[1], sizeof values[1], "Auto");
    snprintf(values[2], sizeof values[2], "%u%% ahead / %u%% behind", d.torrent_read_ahead,
             100 - d.torrent_read_ahead);
    if (d.torrent_preload < 0)
        copy_text(values[3], sizeof values[3], "Auto (2 pieces)");
    else if (d.torrent_preload == 0)
        copy_text(values[3], sizeof values[3], "Off (first piece only)");
    else
        snprintf(values[3], sizeof values[3], "%d%%", d.torrent_preload);
    snprintf(values[4], sizeof values[4], "%u", d.torrent_connections);
    snprintf(values[5], sizeof values[5], "%u", d.torrent_requests);
    snprintf(values[6], sizeof values[6], "%u seconds", d.torrent_block_timeout);
    copy_text(values[7], sizeof values[7], settings_menu::retracker_labels[d.torrent_retrackers]);

    c.rect(0, 0, screen::width, screen::height, Color::bg, 140);
    c.round_rect(300, 95, 1320, 890, 20, Color::surface);
    c.round_frame(300, 95, 1320, 890, 20, 2, Color::accent, 180);
    c.text(355.f, 164.f, "Torrent settings", font::semibold, 34, Color::text);
    c.text(355.f, 207.f, "Cache and peer limits apply to the next large Matroska transfer.",
           font::regular, 20, Color::muted);
    for (int i = 0; i < torrent_setting_count; ++i)
    {
        const int y = 254 + i * 75;
        const bool selected = i == g_torrent_settings_row;
        if (selected) c.round_rect(350, y, 1220, 66, 11, Color::hover);
        c.text(382.f, static_cast<float>(y + 42), labels[i], font::medium, 24,
               selected ? Color::text : Color::muted);
        c.text(1060.f, static_cast<float>(y + 42), values[i], font::mono_medium, 23, Color::accent);
    }
    if (g_torrent_settings_row == 7)
        c.text(355.f, 891.f, "Retrackers use /data/movian/trackers.txt or the built-in defaults.",
               font::regular, 20, Color::muted);
    c.text(355.f, 932.f, "Up/Down select    Left/Right adjust    Triangle defaults    Circle back",
           font::regular, 20, Color::muted);
}

void draw_action_menu(screen::Canvas &c, const char *title, const char *subtitle,
                      std::span<const MenuEntry> entries, int selection) noexcept
{
    c.rect(0, 0, screen::width, screen::height, Color::bg, 145);
    c.round_rect(1008, 100, 852, 878, 16, Color::surface);
    c.text(1054.f, 169.f, title, font::semibold, 38, Color::text);
    char shown[192];
    c.text(1054.f, 213.f, fit(subtitle, 730, shown, sizeof shown, font::regular, 20),
           font::regular, 20, Color::muted);
    constexpr int visible = 11;
    const int size = static_cast<int>(entries.size());
    const int first = std::max(0, selection - visible + 1);
    for (int i = first; i < std::min(size, first + visible); ++i)
    {
        const auto &entry = entries[i];
        const int y = 244 + (i - first) * 62;
        if (i == selection)
        {
            c.round_rect(1034, y, 800, 57, 10, Color::hover);
            c.rect(1034, y + 8, 4, 41, Color::accent);
        }
        const char *label = entry.label;
        char dynamic_label[64];
        if (entry.id == MenuAction::resume)
        {
            double offset = 0;
            (void)settings::resume_for(g_item_path, &offset, nullptr);
            const auto seconds = static_cast<unsigned>(offset);
            snprintf(dynamic_label, sizeof dynamic_label, "Continue from %u:%02u", seconds / 60, seconds % 60);
            label = dynamic_label;
        }
        char value[64]{};
        if (entry.id == MenuAction::pause && player::active()) label = player::playing() ? "Pause" : "Play";
        if (entries.data() == g_sidebar_visible)
        {
            if (entry.id == MenuAction::repeat) copy_text(value, sizeof value, settings::repeat_label());
            if (entry.id == MenuAction::shuffle) copy_text(value, sizeof value, settings::get().shuffle ? "on" : "off");
            if (entry.id == MenuAction::volume) snprintf(value, sizeof value, "%d%%", settings::get().volume);
            if (entry.id == MenuAction::page_layout || entry.id == MenuAction::sort_on)
                copy_text(value, sizeof value, browser_choices(entry.id)[current_browser_choice(entry.id)]);
            if (entry.id == MenuAction::folders_first || entry.id == MenuAction::supported_only)
            {
                const auto prefs = settings::browser_prefs(g_stack[g_depth].path);
                const bool on = entry.id == MenuAction::folders_first ? prefs.folders_first : prefs.supported_only;
                copy_text(value, sizeof value, on ? "on" : "off");
            }
        }
        c.text(1071.f, static_cast<float>(y + 37), label, font::medium, 25,
               i == selection ? Color::text : Color::muted);
        if (value[0])
            c.text(1633.f, static_cast<float>(y + 36), value, font::medium, 22, Color::accent);
    }
    if (size > visible)
    {
        const int height = visible * 62;
        const int thumb = height * visible / size;
        c.rect(1836, 244, 3, height, Color::line);
        c.rect(1836, 244 + (height - thumb) * first / (size - visible), 3, thumb, Color::accent);
    }
}

void draw_plugin_details(screen::Canvas &c) noexcept
{
    const bool package = g_plugin_detail == PluginDetail::repository;
    const char *title = package ? g_detail_item.title : g_detail_plugin.title;
    const char *synopsis = package ? g_detail_item.detail : g_detail_plugin.synopsis;
    const char *version = package ? g_detail_item.version : g_detail_plugin.version;
    draw_action_menu(c, "Plugin details", title, plugin_detail_actions(), g_plugin_detail_row);
    c.round_rect(50, 100, 920, 878, 16, Color::surface);
    char shown[176];
    c.text(96.f, 181.f, fit(title, 817, shown, sizeof shown, font::semibold, 38),
           font::semibold, 38, Color::text);
    if (*version)
    {
        char value[96];
        snprintf(value, sizeof value, "Version %s", version);
        c.text(96.f, 228.f, value, font::regular, 23, Color::muted);
    }
    if (const auto *installed = package ? installed_by_id(g_detail_item.id) : nullptr)
    {
        char value[96];
        snprintf(value, sizeof value, "Installed version %s", installed->version);
        c.text(96.f, 265.f, value, font::regular, 22, Color::muted);
    }
    if (!draw_selected_artwork(c, 96, 288, 818, 490) && selected_plugin_icon()[0])
        c.text(96.f, 554.f, "Artwork preview", font::regular, 21, Color::muted);
    c.text(96.f, 855.f, fit(synopsis, 817, shown, sizeof shown, font::regular, 21),
           font::regular, 21, Color::muted);
}

void draw_browser_choices(screen::Canvas &c) noexcept
{
    const auto &entry = g_sidebar_visible[g_sidebar_row];
    const auto choices = browser_choices(entry.id);
    c.rect(0, 0, screen::width, screen::height, Color::bg, 145);
    c.round_rect(1010, 124, 850, 160 + static_cast<int>(choices.size()) * 68, 18, Color::surface);
    c.text(1060.f, 192.f, entry.label, font::semibold, 36, Color::text);
    for (int i = 0; i < static_cast<int>(choices.size()); ++i)
    {
        const int y = 229 + i * 68;
        if (i == g_browser_choice)
            c.round_rect(1040, y, 790, 60, 9, Color::hover);
        c.text(1072.f, y + 39.f, choices[i], font::medium, 26, Color::text);
        if (i == current_browser_choice(entry.id))
            c.text(1700.f, y + 38.f, "Current", font::regular, 20, Color::accent);
    }
}

void draw_menu_info(screen::Canvas &c) noexcept
{
    c.rect(0, 0, screen::width, screen::height, Color::bg, 160);
    c.round_rect(370, 208, 1180, 672, 18, Color::surface);
    c.text(422.f, 286.f, g_menu_info_title, font::semibold, 38, Color::text);
    for (int i = 0; i < menu_info_lines; ++i)
    {
        char shown[192];
        c.text(422.f, 350.f + i * 55.f, fit(g_menu_lines[i], 1070, shown, sizeof shown, font::regular, 23),
               font::regular, 23, Color::muted);
    }
}

void draw_status_strip(screen::Canvas &c) noexcept
{
    const std::uint64_t ms = screen::now_ms() - g_started_ms;
    char line[160];
    snprintf(line, sizeof line, "title %s   uptime %llu s   %s   vdec %s", kTitleId,
             static_cast<unsigned long long>(ms / 1000), title_access::mode(), hwdec_status());
    c.text(48.f, 980.f, line, font::mono_medium, 16, Color::faint);
}
// All exit work runs on the UI thread. Workers keep their objects until their
// completion flag is acquired and their thread has joined; no pthread_cancel.
enum class QuitState { idle, draining, requested };
QuitState g_quit = QuitState::idle;
void request_quit() noexcept
{
    if (g_quit != QuitState::idle) return;
    g_quit = QuitState::draining;
    g_torrent_job.cancel.store(true, std::memory_order_release);
    g_plugin_job.dismissed = true;
    g_file_inspection.dismissed = true;
    // close() persists playback resume and releases the decoder's cache reader.
    player::close();
    network::begin_shutdown();
    diagnostics::record("ui", "Quit requested; waiting for owned workers");
}
void advance_quit() noexcept
{
    if (g_quit != QuitState::draining) return;
    auto join_ready = [](auto &job) {
        if (job.busy && job.ready.load(std::memory_order_acquire)) {
            pthread_join(job.thread, nullptr);
            job.busy = false;
        }
        return !job.busy;
    };
    // Evaluate each independently so an unfinished plugin cannot delay joining
    // another completed worker. Do not apply results or start replacement jobs.
    const bool plugin_done = join_ready(g_plugin_job);
    const bool artwork_done = join_ready(g_artwork_job);
    const bool inspection_done = join_ready(g_file_inspection);
    const bool torrent_done = join_ready(g_torrent_job);
    const bool network_done = network::is_quiescent();
    // A native IME owns its buffer until it has finished closing. Discard its
    // result without launching a login, network request or plugin callback.
    const bool keyboard_done = keyboard::poll() != keyboard::State::open;
    if (!plugin_done || !artwork_done || !inspection_done || !torrent_done ||
        !network_done || !keyboard_done) return;
    clear_login(g_plugin_job);
    image::unload(g_artwork_job.result);
    g_torrent_job.stream.reset();
    keyboard::clear();
    g_network_keyboard = -1;
    g_settings_keyboard = g_home_keyboard = g_torrent_peer_input = false;
    g_login_input = LoginInput::none;
    g_plugin_text_key[0] = 0;
    settings::save();
    const bool saved_network = network::save();
    diagnostics::record("ui", "Quit workers joined; network settings saved=%d", saved_network);
    // The successful request may return before the shell closes the title.
    // Remain in the main loop with live globals and never return from main.
    g_quit = QuitState::requested;
    const int result = title_access::request_exit();
    if (result < 0) {
        g_quit = QuitState::idle;
        const bool resumed = network::cancel_shutdown();
        diagnostics::record("ui", "Shell exit failed result=%d; network resumed=%d", result, resumed);
        screen::notify("Could not close Movian. Please try Quit again.");
    }
}
#include "ui_stpp.inc"
} // namespace

void load() noexcept
{
    g_started_ms = screen::now_ms();
    mkdir(MOVIAN_DOWNLOAD0, 0777);
    settings::load();
    network::load();
    if(network::config().ftp_server && !network::restart_server()) screen::notify("FTP server could not start");
    if (settings::get().remote_diagnostics && !diagnostics::enable(true))
    {
        settings::get().remote_diagnostics = false;
        screen::notify("Could not start LAN or FTP diagnostics");
    }
    hwdec_force_software(settings::get().hw_decode ? 0 : 1);
    audio::set_volume(settings::get().volume);
    if (settings::get().debug_log)
        append_log("movian-ps5 playlist start");
    snprintf(g_status, sizeof g_status, "PPSA99456 %s  %s  %s", kVersion, title_access::mode(),
             hwdec_status());
    (void)audio::init();
    enter_videos();
    g_place = Place::home;
    if (settings::get().remote_control) {
        const bool listening=g_property_server.start(42000,"0.0.0.0");
        diagnostics::record("stpp","listen=%d port=42000 error=%s",listening,g_property_server.error().c_str());
        publish_properties();
    }
}

bool handle_overlay(unsigned pressed) noexcept
{
    if (g_overlay == Overlay::none)
        return false;
    if (g_overlay == Overlay::menu_info)
    {
        if (pressed & pad::options) g_overlay = Overlay::none;
        else if (pressed & (pad::cross | pad::circle | pad::left)) g_overlay = g_menu_parent;
        return true;
    }
    if (g_overlay == Overlay::plugin_details)
    {
        const auto entries = plugin_detail_actions();
        if (pressed & (pad::circle | pad::left | pad::options))
            g_overlay = Overlay::none;
        else if (pressed & pad::up)
            g_plugin_detail_row = std::max(0, g_plugin_detail_row - 1);
        else if (pressed & pad::down)
            g_plugin_detail_row = std::min(static_cast<int>(entries.size()) - 1, g_plugin_detail_row + 1);
        else if (pressed & pad::cross)
            run_plugin_detail(entries[g_plugin_detail_row]);
        return true;
    }
    if (g_overlay == Overlay::browser_choice)
    {
        const auto &entry = g_sidebar_visible[g_sidebar_row];
        const int last = static_cast<int>(browser_choices(entry.id).size()) - 1;
        if (pressed & (pad::circle | pad::left))
            g_overlay = Overlay::sidebar;
        else if (pressed & pad::options)
            g_overlay = Overlay::none;
        else if (pressed & pad::cross)
        {
            apply_browser_choice(entry.id, g_browser_choice);
            g_overlay = Overlay::sidebar;
        }
        else if (pressed & pad::up)
            g_browser_choice = std::max(0, g_browser_choice - 1);
        else if (pressed & pad::down)
            g_browser_choice = std::min(last, g_browser_choice + 1);
        return true;
    }
    if (g_overlay == Overlay::sidebar)
    {
        const auto &entry = g_sidebar_visible[g_sidebar_row];
        if (pressed & (pad::circle | pad::options) ||
            ((pressed & pad::left) && entry.id != MenuAction::volume))
            g_overlay = Overlay::none;
        else if (pressed & pad::up) g_sidebar_row = std::max(0, g_sidebar_row - 1);
        else if (pressed & pad::down)
            g_sidebar_row = std::min(g_sidebar_row + 1, g_sidebar_count - 1);
        else if (pressed & pad::cross)
            run_sidebar_action(entry);
        else if (entry.id == MenuAction::volume && (pressed & (pad::left | pad::right)))
        {
            auto &volume = settings::get().volume;
            volume = std::clamp(volume + (pressed & pad::left ? -5 : 5), 0, 100);
            audio::set_volume(volume);
            settings::save();
        }
        return true;
    }
    if(g_overlay==Overlay::network_login) {
        const bool smb=!strncmp(g_login_draft.url,"smb2:",5);
        if(pressed & (pad::circle|pad::options)) { g_overlay=Overlay::none; return true; }
        if(pressed & (pad::up|pad::down|pad::left|pad::right)) {
            const int direction=pressed&(pad::up|pad::left)?-1:1;
            g_login_row=std::clamp(g_login_row+direction,0,5);
            if(!smb && g_login_row==2) g_login_row+=direction;
        } else if(pressed & pad::cross) {
            if(g_login_row<3) {
                const char *value=g_login_row==0?g_login_draft.user:g_login_row==1?g_login_draft.password:g_login_draft.domain;
                if(keyboard::open(g_login_row==0?"Username":g_login_row==1?"Password":"Domain","",value,
                                  g_login_row==1?120:95,g_login_row==1))
                    g_network_keyboard=10+g_login_row;
            } else if(g_login_row==3) g_login_draft.remember=!g_login_draft.remember;
            else if(g_login_row==5) g_overlay=Overlay::none;
            else {
                network::config().connections[g_network_slot]=g_login_draft;
                if(!network::save()) screen::notify("Could not save login preferences");
                g_overlay=Overlay::none;
                if(g_place==Place::videos && !std::strcmp(g_stack[g_depth].path,g_login_path)) g_videos_dirty=true;
                else open_home_folder(g_login_path,g_login_draft.url);
            }
        }
        return true;
    }
    if (g_overlay == Overlay::home_network)
    {
        if(pressed & (pad::circle|pad::options)) g_overlay=Overlay::none;
        else if(pressed & (pad::left|pad::right))
            g_network_slot=(g_network_slot+(pressed&pad::left?network::max_connections-1:1))%network::max_connections;
        else if(pressed & pad::up) g_network_row=std::max(0,g_network_row-1);
        else if(pressed & pad::down) g_network_row=std::min(5,g_network_row+1);
        else if(pressed & pad::cross) {
            auto &c=network::config().connections[g_network_slot];
            if(g_network_row<4) {
                const char *value=g_network_row==0?c.url:g_network_row==1?c.user:g_network_row==2?c.password:c.domain;
                if(keyboard::open(network_rows[g_network_row],g_network_row==0?"smb2://server/share or ftp://server:port/":"",value,
                                  g_network_row==0?120:g_network_row==2?120:95,g_network_row==2))
                    g_network_keyboard=g_network_row;
            } else if(g_network_row==4) {
                if(!network::valid_url(c.url)) screen::notify("Enter an SMB2 share or FTP address first");
                else {
                    char path[32]; network::root_path(g_network_slot,path,sizeof path);
                    network::invalidate(); g_overlay=Overlay::none;
                    open_home_folder(path,c.url);
                }
            } else {
                c=network::Connection{};
                screen::notify(network::save()?"Saved server removed":"Could not save network settings");
            }
        }
        return true;
    }
    if (g_overlay == Overlay::torrent_settings)
    {
        if (pressed & pad::circle)
            g_overlay = Overlay::settings;
        else if (pressed & pad::options)
            g_overlay = Overlay::none;
        else if (pressed & pad::up)
            g_torrent_settings_row = (g_torrent_settings_row + torrent_setting_count - 1) % torrent_setting_count;
        else if (pressed & pad::down)
            g_torrent_settings_row = (g_torrent_settings_row + 1) % torrent_setting_count;
        else if (pressed & (pad::left | pad::right))
            nudge_torrent_setting(g_torrent_settings_row, pressed & pad::left ? -1 : 1);
        else if (pressed & pad::triangle)
        {
            auto &d = settings::get();
            const settings::Data defaults;
            d.torrent_cache_path = defaults.torrent_cache_path;
            d.torrent_cache_mib = defaults.torrent_cache_mib;
            d.torrent_read_ahead = defaults.torrent_read_ahead;
            d.torrent_preload = defaults.torrent_preload;
            d.torrent_connections = defaults.torrent_connections;
            d.torrent_requests = defaults.torrent_requests;
            d.torrent_block_timeout = defaults.torrent_block_timeout;
            d.torrent_retrackers = defaults.torrent_retrackers;
            settings::save();
        }
        return true;
    }
    if (g_overlay == Overlay::resume_prompt)
    {
        if (pressed & pad::circle)
            g_overlay = g_resume_parent;
        else if (pressed & pad::options)
            g_overlay = Overlay::none;
        else if (pressed & pad::cross)
        {
            g_overlay = Overlay::none;
            play_path(g_resume_path, g_resume_title, g_resume_choice == 1);
        }
        else if (pressed & (pad::up | pad::down))
            g_resume_choice = (pressed & pad::down) ? 1 : 0;
        return true;
    }
    if (g_overlay == Overlay::setting_detail)
    {
        const auto &row = selected_setting();
        const int count = setting_choices(row.action);
        if (pressed & pad::options)
            g_overlay = Overlay::none;
        else if (pressed & pad::circle)
            g_overlay = Overlay::settings;
        else if (pressed & pad::cross)
        {
            if (count) apply_setting_choice(row.action, g_setting_choice);
            g_overlay = Overlay::settings;
        }
        else if (count)
        {
            const unsigned prev = row.kind == settings_menu::Kind::number ? pad::left | pad::up : pad::up;
            const unsigned next = row.kind == settings_menu::Kind::number ? pad::right | pad::down : pad::down;
            const int delta = !!(pressed & next) - !!(pressed & prev);
            g_setting_choice = std::clamp(g_setting_choice + delta, 0, count - 1);
        }
        return true;
    }
    if (g_overlay == Overlay::settings)
    {
        if (pressed & pad::options)
            g_overlay = Overlay::none;
        else if (pressed & (pad::circle | pad::left))
        {
            if (g_settings_subpage)
            {
                g_settings_subpage = nullptr;
                g_settings_row = g_settings_parent_row;
            }
            else if (g_settings_in_category) g_settings_in_category = false;
            else g_overlay = g_settings_from_sidebar ? Overlay::sidebar : Overlay::none;
        }
        else if (!g_settings_in_category)
        {
            if (pressed & (pad::up | pad::down))
            {
                const int last = static_cast<int>(settings_menu::categories.size()) - 1;
                g_settings_category = std::clamp(g_settings_category + (pressed & pad::up ? -1 : 1), 0, last);
                g_settings_row = move_setting_row(-1, 1);
            }
            else if (pressed & (pad::cross | pad::right))
                g_settings_in_category = true;
        }
        else if (pressed & (pad::up | pad::down))
            g_settings_row = move_setting_row(g_settings_row, pressed & pad::up ? -1 : 1);
        else if (pressed & (pad::cross | pad::right))
        {
            using namespace settings_menu;
            const auto &row = selected_setting();
            const int count = setting_choices(row.action);
            if (row.kind == Kind::page)
            {
                g_settings_subpage = &row;
                g_settings_parent_row = g_settings_row;
                g_settings_row = move_setting_row(-1, 1);
            }
            else if(row.action==quit) request_quit();
            else if(row.action==network_servers) { g_overlay=Overlay::home_network; }
            else if(row.action==system_name || row.action==ftp_port || row.action==ftp_user || row.action==ftp_password || row.action==custom_repository)
            {
                g_settings_text_action=row.action;
                char port_text[8]; snprintf(port_text,sizeof port_text,"%u",network::config().ftp_port);
                const char *value=row.action==system_name?network::config().system_name:row.action==ftp_port?port_text:
                    row.action==ftp_user?network::config().ftp_user:row.action==ftp_password?network::config().ftp_password:settings::get().custom_repo_url;
                const int limit=row.action==system_name || row.action==ftp_user?95:row.action==ftp_port?5:keyboard::max_length;
                g_settings_keyboard = keyboard::open(row.title, row.action==custom_repository?"https://":"",value,
                                                     limit,row.action==ftp_password);
                if (!g_settings_keyboard) screen::notify("Could not open the repository keyboard");
            }
            else if (row.action == about_info)
            {
                menu_info("Movian for PS5", "");
                snprintf(g_menu_lines[0], sizeof g_menu_lines[0], "Movian HEVC Test %s   PPSA99456", kVersion);
                copy_text(g_menu_lines[1], sizeof g_menu_lines[1], "Movian media center");
            }
            else if (count || row.action < 0)
            {
                g_setting_choice = count ? std::clamp(current_choice(row.action), 0, count - 1) : 0;
                g_overlay = Overlay::setting_detail;
            }
            else
                setting_nudge(row.action, 1);
        }
        return true;
    }
    if (pressed & (pad::circle | pad::options | pad::left))
    {
        g_overlay = Overlay::none;
        return true;
    }
    const auto entries = context_entries();
    if (pressed & pad::up)
        g_item_row = std::max(0, g_item_row - 1);
    else if (pressed & pad::down)
        g_item_row = std::min(g_item_row + 1, static_cast<int>(entries.size()) - 1);
    else if (pressed & pad::cross)
        run_item_action(entries[g_item_row]);
    return true;
}

bool update_local() noexcept
{
    if(network::poll()) {
        g_videos_dirty=true;
        if (g_plugin_import && g_zip_depth && network::is_path(g_zip_stack[g_zip_depth].path))
            scan_plugin_zips();
    }
    const bool file_ready = finish_file_inspection();
    const bool page_ready = finish_plugin_page() || file_ready;
    const bool torrent_ready = finish_torrent_download();
    if(g_network_keyboard>=0) {
        const auto state=keyboard::poll();
        if(state==keyboard::State::open) return (player::active() && player::tick()) || page_ready || torrent_ready;
        if(state==keyboard::State::accepted) {
            const char *text=keyboard::text();
            auto &c=g_network_keyboard>=10?g_login_draft:network::config().connections[g_network_slot];
            const int field=g_network_keyboard>=10?(g_network_keyboard==10?1:g_network_keyboard==11?2:3):g_network_keyboard;
            bool valid=!std::strpbrk(text,"\r\n");
            if(field==0) valid=valid && (!text[0] || network::valid_url(text));
            if(valid) {
                char *dst=field==0?c.url:field==1?c.user:field==2?c.password:c.domain;
                const unsigned cap=field==0?sizeof c.url:field==1?sizeof c.user:field==2?sizeof c.password:sizeof c.domain;
                copy_text(dst,cap,text);
                if(g_network_keyboard<10)
                    screen::notify(network::save()?"Network login saved":"Could not save network login");
            } else screen::notify("Use smb2://server/share or ftp://server:port/folder, without login in the address");
        }
        keyboard::clear(); g_network_keyboard=-1; return true;
    }
    if (g_settings_keyboard)
    {
        const auto state = keyboard::poll();
        if (state == keyboard::State::open)
            return (player::active() && player::tick()) || page_ready || torrent_ready;
        if (state == keyboard::State::accepted) {
            using namespace settings_menu;
            const char *value=keyboard::text();
            if(g_settings_text_action==custom_repository)
                screen::notify(settings::set_custom_repo_url(value)?"Plugin repository saved":"Enter an HTTPS URL without credentials");
            else {
                bool valid=!std::strpbrk(value,"\r\n");
                auto &n=network::config();
                if(g_settings_text_action==ftp_port) {
                    char *end=nullptr; const long port=std::strtol(value,&end,10);
                    valid=valid && value[0] && !*end && port>=1024 && port<=65535;
                    if(valid) n.ftp_port=port;
                } else if(valid) {
                    char *dst=g_settings_text_action==system_name?n.system_name:g_settings_text_action==ftp_user?n.ftp_user:n.ftp_password;
                    copy_text(dst,g_settings_text_action==ftp_password?sizeof n.ftp_password:sizeof n.ftp_user,value);
                }
                if(!valid) screen::notify("Enter valid text or a TCP port from 1024 to 65535");
                else {
                    if(n.ftp_server && !network::restart_server()) { n.ftp_server=false; screen::notify("FTP server could not restart"); }
                    screen::notify(network::save()?"Network setting saved":"Could not save network setting");
                }
            }
        }
        keyboard::clear();
        g_settings_keyboard = false;
        return true;
    }
    if (g_home_keyboard)
    {
        const auto state = keyboard::poll();
        if (state == keyboard::State::open)
            return false;
        g_home_keyboard = false;
        if (state == keyboard::State::accepted)
        {
            const char *query = keyboard::text();
            query += std::strspn(query, " \t\r\n");
            if (*query)
            {
                copy_text(g_home_query, sizeof g_home_query, query);
                std::size_t size = std::strlen(g_home_query);
                while (size && std::strchr(" \t\r\n", g_home_query[size - 1])) --size;
                g_home_query[size] = 0;
                open_home_folder(kSearchUri, "Search");
            }
        }
        keyboard::clear();
        return true;
    }

    if (g_torrent_peer_input)
    {
        const auto state = keyboard::poll();
        if (state == keyboard::State::open)
            return true;
        g_torrent_peer_input = false;
        if (state == keyboard::State::accepted && keyboard::text()[0])
            start_torrent_download(keyboard::text());
        keyboard::clear();
        return true;
    }
    if (g_login_input != LoginInput::none)
    {
        const auto state = keyboard::poll();
        if (state == keyboard::State::open)
            return true;
        const auto field = g_login_input;
        g_login_input = LoginInput::none;
        auto &job = g_plugin_job;
        if (state == keyboard::State::accepted && keyboard::text()[0])
        {
            if (field == LoginInput::username)
            {
                copy_text(job.username, sizeof job.username, keyboard::text());
                keyboard::clear();
                char title[288];
                snprintf(title, sizeof title, "Password for %s", g_login_origin);
                if (keyboard::open(title, "Password", "", 60, true))
                {
                    g_login_input = LoginInput::password;
                    return true;
                }
                append_log("KinoZal.TV password keyboard did not open");
                screen::notify("Password keyboard unavailable");
            }
            else
            {
                copy_text(job.password, sizeof job.password, keyboard::text());
                keyboard::clear();
                open_plugin_job(PluginJob::Action::login, g_login_origin, true);
                if (job.busy)
                {
                    append_log("KinoZal.TV sign-in request queued");
                    return true;
                }
                append_log("KinoZal.TV sign-in request could not start");
            }
        }
        else if (state == keyboard::State::failed)
            append_log("KinoZal.TV sign-in keyboard failed");
        keyboard::clear();
        clear_login(job);
        return true;
    }
    if (g_plugin_text_key[0])
    {
        const auto state = keyboard::poll();
        if (state == keyboard::State::accepted)
        {
            char assignment[320];
            snprintf(assignment, sizeof assignment, "%s%s", g_plugin_text_key, keyboard::text());
            if (plugins::set_option(g_installed_plugins[g_active_plugin], assignment))
                open_plugin_page("#settings", true);
            else
                screen::notify("Could not save plugin setting");
        }
        if (state != keyboard::State::open)
            g_plugin_text_key[0] = 0;
        return true;
    }
    const unsigned pressed = pad::poll() | std::exchange(g_remote_pressed,0u);
    bool dirty = update_plugin_artwork(pressed) || page_ready || torrent_ready || pressed != 0;
    if (g_file_inspection.busy && !g_file_inspection.dismissed) {
        if (pressed & pad::circle) {
            g_file_inspection.dismissed = true;
            g_overlay = Overlay::none;
        }
        return (player::active() && player::tick()) || dirty;
    }
    if (g_place == Place::bittorrent && g_torrent_job.busy)
    {
        static unsigned shown_progress = 0;
        const unsigned progress = g_torrent_job.progress.load(std::memory_order_relaxed);
        dirty |= progress != shown_progress;
        shown_progress = progress;
    }

    if (g_overlay != Overlay::none)
    {
        if (pressed) handle_overlay(pressed);
        return (player::active() && player::tick()) || dirty;
    }

    if (pressed & pad::options)
    {
        open_sidebar();
        return true;
    }

    if (player::active())
    {
        if (!g_auto_advanced && player::ended() &&
            (settings::get().auto_next || settings::get().repeat != 0))
        {
            g_auto_advanced = true;
            if (play_delta(1, settings::get().repeat == 2))
                return true;
            screen::notify("End of folder");
        }
        if ((pressed & pad::l1) && play_delta(-1, true))
            return true;
        if ((pressed & pad::r1) && play_delta(1, true))
            return true;
        dirty = player::handle(pressed, play_delta) || dirty;
        dirty = player::tick() || dirty;
        return dirty;
    }

    if (g_place == Place::home && (pressed & pad::triangle))
    {
        append_log("HEVC test requested after startup");
        constexpr const char *fixture = MOVIAN_APP0 "/assets/hevc-test.mp4";
        if (!play_path(fixture, "1080p HEVC test", true))
            append_log(g_selected_note);
        return true;
    }

    if (g_place == Place::plugins && g_plugin_job.busy && (pressed & (pad::l1 | pad::r1)))
        g_plugin_job.dismissed = true;
    if (pressed & pad::l1)
        g_place = static_cast<Place>((static_cast<int>(g_place) + place_count - 1) % place_count);
    if (pressed & pad::r1)
        g_place = static_cast<Place>((static_cast<int>(g_place) + 1) % place_count);

    if (g_place == Place::home)
        update_home(pressed);
    else if (g_place == Place::videos)
    {
        if (g_videos_dirty)
            reload_videos();
        const bool grid = browser_grid();
        const int stride = grid ? library_columns : 1;
        const int page = grid ? library_grid_page : rows_in_view;
        const int selected = g_stack[g_depth].selected;
        if ((pressed & pad::up) && (!grid || selected >= library_columns))
            move_selection(-stride);
        if ((pressed & pad::down) && (!grid || selected / library_columns < (g_row_count - 1) / library_columns))
            move_selection(stride);
        if (grid && (pressed & pad::left) && selected % library_columns != 0)
            move_selection(-1);
        if (grid && (pressed & pad::right) && selected % library_columns != library_columns - 1)
            move_selection(1);
        if (pressed & pad::l2)
            move_selection(-page);
        if (pressed & pad::r2)
            move_selection(page);
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
            network::invalidate();
            g_videos_dirty = true;
            screen::notify("Reloaded folder");
        }
    }
    else if (g_place == Place::plugins)
    {
        update_plugins(pressed);
    }
    else if (g_place == Place::bittorrent)
        update_torrents(pressed);
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

bool update() noexcept
{
    if (g_quit != QuitState::idle) {
        advance_quit();
        if (g_quit == QuitState::requested && g_property_server.port()) g_property_server.stop();
        return true;
    }
    stpp::Command command;
    bool dirty=false;
    // Bound work per frame even when a controller floods valid commands.
    for(unsigned count=0;count<16 && g_property_server.poll(command);++count) {
        property_command(command); dirty=true;
        if (g_quit != QuitState::idle) break;
    }
    if (g_quit == QuitState::idle) dirty=update_local() || dirty;
    if (g_quit != QuitState::idle) {
        advance_quit();
        if (g_quit == QuitState::requested && g_property_server.port()) g_property_server.stop();
        return true;
    }
    if (!settings::get().remote_control && g_property_server.port()) g_property_server.stop();
    if (settings::get().remote_control && !g_property_server.port()) {
        static std::uint64_t retry=0;
        if(screen::now_ms()-retry>5000) { retry=screen::now_ms(); g_property_server.start(42000,"0.0.0.0"); }
    }
    const auto now=screen::now_ms();
    if(g_property_server.port() && (dirty || now-g_property_published>=200) && now-g_property_published>=100) {
        publish_properties(); g_property_published=now;
    }
    return dirty;
}

void draw(screen::Canvas &canvas) noexcept
{
    if (g_quit != QuitState::idle) {
        canvas.rect(0, 0, screen::width, screen::height, Color::bg);
        canvas.text(72.f, 170.f, "Closing Movian", font::semibold, 38, Color::text);
        canvas.text(72.f, 232.f, g_quit == QuitState::draining ?
                    "Stopping playback and waiting for background tasks..." :
                    "Waiting for the PS5 to close this title...", font::regular, 24, Color::muted);
        return;
    }
    const bool playing = player::active();
    if (playing)
    {
        player::draw(canvas);
        if (g_overlay == Overlay::none) return;
    }
    else
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
        case Place::plugins:
            draw_plugins(canvas);
            break;
        case Place::bittorrent:
            draw_torrents(canvas);
            break;
        default:
            draw_placeholder(canvas, g_place);
            break;
        }
        if (g_place != Place::videos)
            draw_status_strip(canvas);
    }
    if (g_overlay == Overlay::sidebar)
        draw_action_menu(canvas, "Page settings", playing ? player::title() : place_name[static_cast<int>(g_place)],
                         {g_sidebar_visible, static_cast<std::size_t>(g_sidebar_count)}, g_sidebar_row);
    else if (g_overlay == Overlay::plugin_details)
        draw_plugin_details(canvas);
    else if (g_overlay == Overlay::browser_choice)
        draw_browser_choices(canvas);
    else if (g_overlay == Overlay::menu_info)
        draw_menu_info(canvas);
    else if (g_overlay == Overlay::settings)
        draw_settings_overlay(canvas);
    else if (g_overlay == Overlay::setting_detail)
        draw_setting_detail(canvas);
    else if (g_overlay == Overlay::resume_prompt)
        draw_resume_prompt(canvas);
    else if (g_overlay == Overlay::home_network)
        draw_home_network(canvas);
    else if (g_overlay == Overlay::network_login)
        draw_network_login(canvas);
    else if (g_overlay == Overlay::torrent_settings)
        draw_torrent_settings_overlay(canvas);
    else if (g_overlay == Overlay::item)
        draw_action_menu(canvas, "Item actions", g_item_title, context_entries(), g_item_row);
    draw_footer(canvas);
}
} // namespace movian::ui
