/*
 * Movian for PS5 — settings, resume, and bookmarks on /download0.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "settings.hpp"

#include "paths.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

namespace movian::settings
{
namespace
{
constexpr const char *kRepoPreset[repo_preset_count] = {
    "",
    "https://bit.ly/mprepo",
    "https://raw.githubusercontent.com/brinza666/movian_ps5/main/plugins/repo.json",
    "https://repo.movian.eu/plugins-v1.json",
};
constexpr const char *kRepoLabel[repo_preset_count] = {
    "none (not fetched)",
    "bit.ly/mprepo",
    "github repo.json",
    "repo.movian.eu",
};
constexpr int kSeekSteps[] = {5, 10, 30, 60};
constexpr int kVolumes[] = {50, 75, 100};

Data g_data{};
Resume g_resume{};
struct Bookmark
{
    char path[max_path + 1];
    char title[max_title + 1];
};
Bookmark g_bookmarks[max_bookmarks];
int g_bookmark_n = 0;

void copy_text(char *dst, int cap, const char *src) noexcept
{
    if (!dst || cap <= 0)
        return;
    std::snprintf(dst, static_cast<std::size_t>(cap), "%s", src ? src : "");
}

bool same_path(const char *a, const char *b) noexcept
{
    if (!a || !b)
        return false;
    return std::strcmp(a, b) == 0;
}

const char *basename_of(const char *path) noexcept
{
    const char *s = std::strrchr(path, '/');
    return s ? s + 1 : path;
}

int clamp_seek(int v) noexcept
{
    for (int s : kSeekSteps)
        if (v == s)
            return v;
    return 10;
}

int clamp_volume(int v) noexcept
{
    if (v < 0)
        return 0;
    if (v > 100)
        return 100;
    return v;
}

int clamp_repo(int v) noexcept
{
    if (v < 0)
        return 0;
    if (v >= repo_preset_count)
        return repo_preset_count - 1;
    return v;
}

bool write_atomic(const char *path, const char *body) noexcept
{
    if (!path || !body)
        return false;
    char tmp[max_path + 8];
    std::snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = std::fopen(tmp, "w");
    if (!f)
        return false;
    const int n = std::fprintf(f, "%s", body);
    const int ok = std::fflush(f) == 0;
    std::fclose(f);
    if (n < 0 || !ok)
    {
        std::remove(tmp);
        return false;
    }
    std::remove(path);
    if (std::rename(tmp, path) != 0)
    {
        std::remove(tmp);
        return false;
    }
    return true;
}

void parse_bool(const char *v, bool *out) noexcept
{
    if (!v || !out)
        return;
    *out = !(v[0] == '0' && v[1] == 0);
}

void load_settings_file() noexcept
{
    FILE *f = std::fopen(MOVIAN_SETTINGS_PATH, "r");
    if (!f)
        return;
    char line[512];
    while (std::fgets(line, static_cast<int>(sizeof line), f))
    {
        char *nl = std::strchr(line, '\n');
        if (nl)
            *nl = 0;
        char *cr = std::strchr(line, '\r');
        if (cr)
            *cr = 0;
        if (line[0] == 0 || line[0] == '#')
            continue;
        char *eq = std::strchr(line, '=');
        if (!eq)
            continue;
        *eq = 0;
        const char *k = line;
        const char *v = eq + 1;
        if (std::strcmp(k, "resume") == 0)
            parse_bool(v, &g_data.resume);
        else if (std::strcmp(k, "seek") == 0)
            g_data.seek_step = clamp_seek(std::atoi(v));
        else if (std::strcmp(k, "clock24") == 0)
            parse_bool(v, &g_data.clock_24h);
        else if (std::strcmp(k, "debug") == 0)
            parse_bool(v, &g_data.debug_log);
        else if (std::strcmp(k, "hwdec") == 0)
            parse_bool(v, &g_data.hw_decode);
        else if (std::strcmp(k, "showall") == 0)
            parse_bool(v, &g_data.show_all);
        else if (std::strcmp(k, "autonext") == 0)
            parse_bool(v, &g_data.auto_next);
        else if (std::strcmp(k, "newest") == 0)
            parse_bool(v, &g_data.newest_first);
        else if (std::strcmp(k, "volume") == 0)
            g_data.volume = clamp_volume(std::atoi(v));
        else if (std::strcmp(k, "repo") == 0)
            g_data.repo_preset = clamp_repo(std::atoi(v));
    }
    std::fclose(f);
}

void load_resume_file() noexcept
{
    g_resume = Resume{};
    FILE *f = std::fopen(MOVIAN_RESUME_PATH, "r");
    if (!f)
        return;
    char line[768];
    if (!std::fgets(line, static_cast<int>(sizeof line), f))
    {
        std::fclose(f);
        return;
    }
    std::fclose(f);
    char *nl = std::strchr(line, '\n');
    if (nl)
        *nl = 0;
    char path[max_path + 1] = "";
    char title[max_title + 1] = "";
    double time = 0, duration = 0;
    if (std::sscanf(line, "%383[^\t]\t%lf\t%lf\t%95[^\t]", path, &time, &duration, title) < 3)
        return;
    if (!path[0] || time < 0)
        return;
    copy_text(g_resume.path, static_cast<int>(sizeof g_resume.path), path);
    copy_text(g_resume.title, static_cast<int>(sizeof g_resume.title),
              title[0] ? title : basename_of(path));
    g_resume.time = time;
    g_resume.duration = duration;
    g_resume.valid = true;
}

void load_bookmarks_file() noexcept
{
    g_bookmark_n = 0;
    FILE *f = std::fopen(MOVIAN_BOOKMARKS_PATH, "r");
    if (!f)
        return;
    char line[512];
    while (std::fgets(line, static_cast<int>(sizeof line), f) && g_bookmark_n < max_bookmarks)
    {
        char *nl = std::strchr(line, '\n');
        if (nl)
            *nl = 0;
        if (line[0] == 0)
            continue;
        char *tab = std::strchr(line, '\t');
        Bookmark &b = g_bookmarks[g_bookmark_n];
        if (tab)
        {
            *tab = 0;
            copy_text(b.path, static_cast<int>(sizeof b.path), line);
            copy_text(b.title, static_cast<int>(sizeof b.title), tab + 1);
        }
        else
        {
            copy_text(b.path, static_cast<int>(sizeof b.path), line);
            copy_text(b.title, static_cast<int>(sizeof b.title), basename_of(line));
        }
        if (b.path[0])
            ++g_bookmark_n;
    }
    std::fclose(f);
}

void save_resume_file() noexcept
{
    if (!g_resume.valid)
    {
        std::remove(MOVIAN_RESUME_PATH);
        return;
    }
    char body[768];
    std::snprintf(body, sizeof body, "%s\t%.3f\t%.3f\t%s\n", g_resume.path, g_resume.time,
                  g_resume.duration, g_resume.title);
    write_atomic(MOVIAN_RESUME_PATH, body);
}

void save_bookmarks_file() noexcept
{
    char body[max_bookmarks * 512];
    body[0] = 0;
    std::size_t used = 0;
    for (int i = 0; i < g_bookmark_n; ++i)
    {
        char row[512];
        const int n = std::snprintf(row, sizeof row, "%s\t%s\n", g_bookmarks[i].path,
                                    g_bookmarks[i].title);
        if (n <= 0)
            continue;
        if (used + static_cast<std::size_t>(n) + 1 >= sizeof body)
            break;
        std::memcpy(body + used, row, static_cast<std::size_t>(n) + 1);
        used += static_cast<std::size_t>(n);
    }
    write_atomic(MOVIAN_BOOKMARKS_PATH, body);
}
} // namespace

void load() noexcept
{
    mkdir(MOVIAN_DOWNLOAD0, 0777);
    g_data = Data{};
    load_settings_file();
    load_resume_file();
    load_bookmarks_file();
}

void save() noexcept
{
    char body[512];
    std::snprintf(body, sizeof body,
                  "resume=%d\nseek=%d\nclock24=%d\ndebug=%d\nhwdec=%d\nshowall=%d\nautonext=%d\nnewest=%d\nvolume=%d\nrepo=%d\n",
                  g_data.resume ? 1 : 0, clamp_seek(g_data.seek_step), g_data.clock_24h ? 1 : 0,
                  g_data.debug_log ? 1 : 0, g_data.hw_decode ? 1 : 0, g_data.show_all ? 1 : 0,
                  g_data.auto_next ? 1 : 0, g_data.newest_first ? 1 : 0, clamp_volume(g_data.volume),
                  clamp_repo(g_data.repo_preset));
    write_atomic(MOVIAN_SETTINGS_PATH, body);
}

Data &get() noexcept
{
    return g_data;
}

const char *repo_url() noexcept
{
    return kRepoPreset[clamp_repo(g_data.repo_preset)];
}

const char *repo_label() noexcept
{
    return kRepoLabel[clamp_repo(g_data.repo_preset)];
}

void cycle_repo(int delta) noexcept
{
    int v = clamp_repo(g_data.repo_preset) + delta;
    if (v < 0)
        v = repo_preset_count - 1;
    if (v >= repo_preset_count)
        v = 0;
    g_data.repo_preset = v;
    save();
}

void cycle_seek(int delta) noexcept
{
    int idx = 1;
    for (int i = 0; i < 4; ++i)
        if (kSeekSteps[i] == g_data.seek_step)
            idx = i;
    idx = (idx + delta + 4) % 4;
    g_data.seek_step = kSeekSteps[idx];
    save();
}

void cycle_volume(int delta) noexcept
{
    int idx = 2;
    for (int i = 0; i < 3; ++i)
        if (kVolumes[i] == g_data.volume)
            idx = i;
    idx = (idx + delta + 3) % 3;
    g_data.volume = kVolumes[idx];
    save();
}

Resume last_resume() noexcept
{
    return g_resume;
}

void remember_resume(const char *path, const char *title, double time, double duration) noexcept
{
    if (!path || !path[0] || time < 5.0)
        return;
    if (duration > 0 && time + 2.0 >= duration)
    {
        clear_resume();
        return;
    }
    copy_text(g_resume.path, static_cast<int>(sizeof g_resume.path), path);
    copy_text(g_resume.title, static_cast<int>(sizeof g_resume.title),
              title && title[0] ? title : basename_of(path));
    g_resume.time = time;
    g_resume.duration = duration;
    g_resume.valid = true;
    save_resume_file();
}

void clear_resume() noexcept
{
    g_resume = Resume{};
    std::remove(MOVIAN_RESUME_PATH);
}

bool resume_for(const char *path, double *time, double *duration) noexcept
{
    if (!g_resume.valid || !path || !same_path(g_resume.path, path))
        return false;
    if (g_resume.time < 5.0)
        return false;
    if (g_resume.duration > 0 && g_resume.time + 2.0 >= g_resume.duration)
        return false;
    if (time)
        *time = g_resume.time;
    if (duration)
        *duration = g_resume.duration;
    return true;
}

int bookmark_count() noexcept
{
    return g_bookmark_n;
}

const char *bookmark_path(int index) noexcept
{
    if (index < 0 || index >= g_bookmark_n)
        return "";
    return g_bookmarks[index].path;
}

const char *bookmark_title(int index) noexcept
{
    if (index < 0 || index >= g_bookmark_n)
        return "";
    return g_bookmarks[index].title;
}

bool is_bookmarked(const char *path) noexcept
{
    if (!path || !path[0])
        return false;
    for (int i = 0; i < g_bookmark_n; ++i)
        if (same_path(g_bookmarks[i].path, path))
            return true;
    return false;
}

bool toggle_bookmark(const char *path, const char *title) noexcept
{
    if (!path || !path[0])
        return false;
    for (int i = 0; i < g_bookmark_n; ++i)
    {
        if (!same_path(g_bookmarks[i].path, path))
            continue;
        for (int j = i; j + 1 < g_bookmark_n; ++j)
            g_bookmarks[j] = g_bookmarks[j + 1];
        --g_bookmark_n;
        save_bookmarks_file();
        return false;
    }
    if (g_bookmark_n >= max_bookmarks)
        return false;
    Bookmark &b = g_bookmarks[g_bookmark_n++];
    copy_text(b.path, static_cast<int>(sizeof b.path), path);
    copy_text(b.title, static_cast<int>(sizeof b.title), title && title[0] ? title : basename_of(path));
    save_bookmarks_file();
    return true;
}

void clear_bookmarks() noexcept
{
    g_bookmark_n = 0;
    std::remove(MOVIAN_BOOKMARKS_PATH);
}
} // namespace movian::settings
