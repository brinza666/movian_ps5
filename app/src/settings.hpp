/*
 * Movian for PS5 — short settings subset, continue-watching, bookmarks.
 * Stored under /download0. DualSense toggles only; no on-screen keyboard.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace movian::settings
{
constexpr int max_path = 383;
constexpr int max_title = 95;
constexpr int max_bookmarks = 24;
constexpr int repo_preset_count = 4;

struct Data
{
    bool resume = true;
    int seek_step = 10; // 5, 10, 30, or 60 seconds
    bool clock_24h = true;
    bool debug_log = true;
    bool hw_decode = false; // stored for Phase 2b; this binary is software MPEG-1
    bool show_all = false;
    bool auto_next = true;
    bool newest_first = false;
    int volume = 100; // 50 / 75 / 100
    int repo_preset = 1; // cycle; nothing is fetched in this build
};

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

[[nodiscard]] const char *repo_url() noexcept;
[[nodiscard]] const char *repo_label() noexcept;
void cycle_repo(int delta) noexcept;
void cycle_seek(int delta) noexcept;
void cycle_volume(int delta) noexcept;

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
