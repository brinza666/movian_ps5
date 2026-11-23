/*
 * Movian for PS5 — short settings subset, continue-watching, bookmarks.
 * Stored under /download0. DualSense toggles only; no on-screen keyboard.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "fs.hpp"

#include <cstddef>

namespace movian::settings
{
constexpr int max_path = 383;
constexpr int max_title = 95;
constexpr int max_bookmarks = 24;
constexpr int repo_preset_count = 5;
inline constexpr int seek_steps[] = {5, 10, 30, 60};
inline constexpr int volume_presets[] = {50, 75, 100};
inline constexpr char mprepo_short_url[] = "https://bit.ly/mprepo";
inline constexpr char mprepo_direct_url[] =
    "https://drive.google.com/uc?export=download&id=1w6BS1LGx31IExt6fNGsf4HYZWugI_1k8";

struct Data
{
    bool resume = true;
    bool resume_ask = false;
    bool show_extensions = true;
    bool playback_clock = false;
    char custom_repo_url[384]{};
    int seek_step = 10; // 5, 10, 30, or 60 seconds
    bool clock_24h = true;
    bool debug_log = true;
    bool remote_diagnostics = false;
    bool remote_control = true; // Isolated test title; Network settings can disable it.
    bool hw_decode = true; // libSceVideodec2 when the module loads; else software H.264
    bool show_all = false;
    bool auto_next = true;
    bool newest_first = false;
    int repeat = 0; // 0 off, 1 one, 2 folder
    bool shuffle = false;
    int volume = 100; // 50 / 75 / 100
    int repo_preset = 1; // bit.ly/mprepo
    bool torrent_trackers = true;
    bool torrent_progressive = true;
    int torrent_cache_path = 0; // RAM, internal storage, USB
    unsigned torrent_cache_mib = 0; // Auto
    unsigned torrent_read_ahead = 100;
    int torrent_preload = -1; // Auto
    unsigned torrent_connections = 4;
    unsigned torrent_requests = 16;
    unsigned torrent_block_timeout = 6;
    unsigned torrent_retrackers = 1; // Add configured retrackers
    unsigned torrent_tracker_protocols = 0; // TCP & UDP, TCP, UDP (WSL order).
};

struct BrowserPrefs
{
    bool grid = false;
    fs::SortOrder sort = fs::SortOrder::filename;
    bool folders_first = true;
    bool supported_only = true;
};

[[nodiscard]] BrowserPrefs browser_prefs(const char *path) noexcept;
void save_browser_prefs(const char *path, BrowserPrefs prefs) noexcept;

struct Resume
{
    char path[max_path + 1];
    char title[max_title + 1];
    double time = 0;
    double duration = 0;
    bool valid = false;
};

void load() noexcept;
void save() noexcept;
[[nodiscard]] Data &get() noexcept;

void format_clock(char *dst, std::size_t capacity) noexcept;
bool set_custom_repo_url(const char *url) noexcept;
[[nodiscard]] const char *repo_url() noexcept;
[[nodiscard]] const char *repo_label() noexcept;
[[nodiscard]] const char *repo_label(int preset) noexcept;
void cycle_repo(int delta) noexcept;
void cycle_seek(int delta) noexcept;
void cycle_volume(int delta) noexcept;
void cycle_repeat(int delta) noexcept;
[[nodiscard]] const char *repeat_label() noexcept;

[[nodiscard]] Resume last_resume() noexcept;
void remember_resume(const char *path, const char *title, double time, double duration) noexcept;
void clear_resume() noexcept;
[[nodiscard]] bool resume_for(const char *path, double *time, double *duration) noexcept;

[[nodiscard]] int bookmark_count() noexcept;
[[nodiscard]] const char *bookmark_path(int index) noexcept;
[[nodiscard]] const char *bookmark_title(int index) noexcept;
[[nodiscard]] bool is_bookmarked(const char *path) noexcept;
bool toggle_bookmark(const char *path, const char *title) noexcept;
void clear_bookmarks() noexcept;
} // namespace movian::settings
