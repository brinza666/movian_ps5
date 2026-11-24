// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "image.hpp"
#include <cstddef>

namespace movian::plugins
{
constexpr int max_items = 16;
constexpr int max_installed = 16;

struct Plugin
{
    char id[64];
    char title[96];
    char synopsis[160];
    char version[24];
    char archive[384];
    char icon[512];
    char author[128];
};

struct Item
{
    char title[96];
    char url[1024];
    char type[16];
    char detail[160];
    char icon[512];
    char id[64];
    char version[24];
    char author[128];
};

struct Page
{
    char title[96];
    char notice[1536];
    Item items[max_items];
    int count;
};

enum class Category { tv, video, music, subtitles, keyboards, regional_tv, other, all };
inline constexpr const char *category_names[] = {
    "Online TV", "Video streaming", "Music streaming", "Subtitles",
    "On Screen Keyboards", "TV Video (RU)", "Uncategorized", "All plugins"
};

int catalog(Plugin (&entries)[max_installed]) noexcept;
bool inspect_package(const char *path, Plugin &plugin) noexcept;
bool install(const char *path, char *error, int capacity) noexcept;
bool uninstall(const Plugin &plugin, char *error, int capacity) noexcept;
bool open(const Plugin &plugin, const char *url, Page &page, char *error, int capacity) noexcept;
bool torrent_path(const Plugin &plugin, char *path, std::size_t capacity) noexcept;
bool set_option(const Plugin &plugin, const char *assignment) noexcept;
bool repository(const char *url, unsigned offset, Page &page, char *error, int capacity,
                bool refresh = false, Category category = Category::all) noexcept;
bool install_remote(const char *url, char *error, int capacity) noexcept;
bool artwork(const Plugin &plugin, const char *url, image::Image &out) noexcept;
bool kinozal_login_origin(char (&origin)[264]) noexcept;
bool kinozal_login(const char *origin, const char *username, const char *password,
                   char *error, int capacity) noexcept;
} // namespace movian::plugins
