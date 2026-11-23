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
#include <ctime>
#include <sys/stat.h>

namespace movian::settings
{
namespace
{
constexpr const char *kRepoPreset[repo_preset_count] = {
    "",
    mprepo_short_url,
    "https://raw.githubusercontent.com/brinza666/movian_ps5/main/plugins/repo.json",
    "https://repo.movian.eu/plugins-v1.json",
    mprepo_direct_url,
};
constexpr const char *kRepoLabel[repo_preset_count] = {
    "none (not fetched)",
    "bit.ly/mprepo",
    "github repo.json",
    "repo.movian.eu",
    "mprepo (Google Drive)",
};

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
    for (int s : seek_steps)
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

constexpr char browser_file[] = MOVIAN_DOWNLOAD0 "/browser-views.txt";
constexpr int browser_pref_limit = 48;
struct BrowserEntry
{
    char path[max_path + 1];
    BrowserPrefs prefs;
};
BrowserEntry browser_entries[browser_pref_limit];
int browser_count = 0;

void load_browser_prefs() noexcept
{
    browser_count = 0;
    auto *file = std::fopen(browser_file, "r");
    if (!file) return;
    char line[max_path + 22];
    while (browser_count < browser_pref_limit && std::fgets(line, sizeof line, file))
    {
        const auto end = std::strcspn(line, "\r\n");
        if (line[end] == 0 && !std::feof(file))
        {
            int c;
            do { c = std::fgetc(file); } while (c != '\n' && c != EOF);
            continue;
        }
        line[end] = 0;
        int layout, sort, folders, supported, start = 0;
        if (std::sscanf(line, "%1d,%1d,%1d,%1d|%n", &layout, &sort, &folders, &supported, &start) != 4 ||
            start == 0 || end - start > max_path || layout > 1 || sort > 2 || folders > 1 || supported > 1)
            continue;
        auto &entry = browser_entries[browser_count++];
        copy_text(entry.path, sizeof entry.path, line + start);
        entry.prefs = {layout == 1, static_cast<fs::SortOrder>(sort), folders == 1, supported == 1};
    }
    std::fclose(file);
}

void parse_bool(const char *v, bool *out) noexcept
{
    if (!v || !out)
        return;
    *out = !(v[0] == '0' && v[1] == 0);
}

bool valid_repo_url(const char *url) noexcept
{
    if (!url || std::strlen(url) >= sizeof g_data.custom_repo_url)
        return false;
    if (!url[0])
        return true;
    constexpr char prefix[] = "https://";
    if (std::strncmp(url, prefix, sizeof prefix - 1) != 0)
        return false;
    const char *authority = url + sizeof prefix - 1;
    const auto length = std::strcspn(authority, "/?#");
    if (!length || std::memchr(authority, '@', length))
        return false;
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(authority); *p; ++p)
        if (*p <= 32 || *p >= 127 || *p == '\\')
            return false;
    return true;
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
        else if (std::strcmp(k, "resume_ask") == 0)
            parse_bool(v, &g_data.resume_ask);
        else if (std::strcmp(k, "show_extensions") == 0)
            parse_bool(v, &g_data.show_extensions);
        else if (std::strcmp(k, "playback_clock") == 0)
            parse_bool(v, &g_data.playback_clock);
        else if (std::strcmp(k, "custom_repo_url") == 0 && valid_repo_url(v))
            std::snprintf(g_data.custom_repo_url, sizeof g_data.custom_repo_url, "%s", v);
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
        else if (std::strcmp(k, "repeat") == 0)
        {
            int r = std::atoi(v);
            if (r < 0)
                r = 0;
            if (r > 2)
                r = 2;
            g_data.repeat = r;
        }
        else if (std::strcmp(k, "shuffle") == 0)
            parse_bool(v, &g_data.shuffle);
        else if (std::strcmp(k, "volume") == 0)
            g_data.volume = clamp_volume(std::atoi(v));
        else if (std::strcmp(k, "repo") == 0)
            g_data.repo_preset = clamp_repo(std::atoi(v));
        else if (std::strcmp(k, "torrent_trackers") == 0)
            parse_bool(v, &g_data.torrent_trackers);
        else if (std::strcmp(k, "torrent_progressive") == 0)
            parse_bool(v, &g_data.torrent_progressive);
        else if (std::strcmp(k, "remote_control") == 0)
            parse_bool(v, &g_data.remote_control);
        else if (std::strcmp(k, "remote_diagnostics") == 0)
            parse_bool(v, &g_data.remote_diagnostics);
        else if (std::strcmp(k, "torrent_cache_path") == 0)
        {
            const int path = std::atoi(v);
            if (path >= 0 && path <= 2) g_data.torrent_cache_path = path;
        }
        else if (std::strcmp(k, "torrent_cache_mib") == 0)
        {
            const int size = std::atoi(v);
            if (size >= 0 && size <= 512) g_data.torrent_cache_mib = static_cast<unsigned>(size);
        }
        else if (std::strcmp(k, "torrent_read_ahead") == 0)
        {
            const int percent = std::atoi(v);
            if (percent >= 5 && percent <= 100) g_data.torrent_read_ahead = static_cast<unsigned>(percent);
        }
        else if (std::strcmp(k, "torrent_preload") == 0)
        {
            const int percent = std::atoi(v);
            if (percent >= -1 && percent <= 100) g_data.torrent_preload = percent;
        }
        else if (std::strcmp(k, "torrent_connections") == 0)
        {
            const int limit = std::atoi(v);
            if (limit >= 1 && limit <= 8) g_data.torrent_connections = static_cast<unsigned>(limit);
        }
        else if (std::strcmp(k, "torrent_requests") == 0)
        {
            const int limit = std::atoi(v);
            if (limit >= 1 && limit <= 32) g_data.torrent_requests = static_cast<unsigned>(limit);
        }
        else if (std::strcmp(k, "torrent_block_timeout") == 0)
        {
            const int seconds = std::atoi(v);
            if (seconds >= 3 && seconds <= 30) g_data.torrent_block_timeout = static_cast<unsigned>(seconds);
        }
        else if (std::strcmp(k, "torrent_retrackers") == 0)
        {
            const int mode = std::atoi(v);
            if (mode >= 0 && mode <= 3) g_data.torrent_retrackers = static_cast<unsigned>(mode);
        }
        else if (std::strcmp(k, "torrent_tracker_protocols") == 0)
        {
            const int mode = std::atoi(v);
            if (mode >= 0 && mode <= 2) g_data.torrent_tracker_protocols = static_cast<unsigned>(mode);
        }
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
    load_browser_prefs();
    load_resume_file();
    load_bookmarks_file();
}

BrowserPrefs browser_prefs(const char *path) noexcept
{
    for (int i = 0; i < browser_count; ++i)
        if (std::strcmp(browser_entries[i].path, path) == 0)
            return browser_entries[i].prefs;
    return {false, g_data.newest_first ? fs::SortOrder::newest : fs::SortOrder::filename,
            true, !g_data.show_all};
}

void save_browser_prefs(const char *path, BrowserPrefs prefs) noexcept
{
    if (std::strlen(path) > max_path) return;
    int pos = 0;
    while (pos < browser_count && std::strcmp(browser_entries[pos].path, path)) ++pos;
    if (pos == browser_count)
    {
        if (browser_count == browser_pref_limit) --pos;
        else ++browser_count;
    }
    // Most recently changed folders are retained at the start of the cache.
    std::memmove(browser_entries + 1, browser_entries, pos * sizeof browser_entries[0]);
    copy_text(browser_entries[0].path, sizeof browser_entries[0].path, path);
    browser_entries[0].prefs = prefs;

    char body[browser_pref_limit * (max_path + 22)]{};
    std::size_t used = 0;
    for (int i = 0; i < browser_count; ++i)
    {
        const auto &entry = browser_entries[i];
        if (std::strpbrk(entry.path, "\r\n")) continue;
        const auto &p = entry.prefs;
        const int n = std::snprintf(body + used, sizeof body - used, "%d,%d,%d,%d|%s\n",
                                    p.grid, static_cast<int>(p.sort), p.folders_first, p.supported_only, entry.path);
        if (n < 0 || static_cast<std::size_t>(n) >= sizeof body - used) break;
        used += n;
    }
    write_atomic(browser_file, body);
}

void save() noexcept
{
    char body[2048];
    std::snprintf(body, sizeof body,
                  "resume=%d\nseek=%d\nclock24=%d\ndebug=%d\nhwdec=%d\nshowall=%d\nautonext=%d\nnewest=%d\nrepeat=%d\nshuffle=%d\nvolume=%d\nrepo=%d\ntorrent_trackers=%d\ntorrent_progressive=%d\n"
                  "torrent_cache_path=%d\nremote_diagnostics=%d\ntorrent_cache_mib=%u\ntorrent_read_ahead=%u\ntorrent_preload=%d\n"
                  "torrent_connections=%u\ntorrent_requests=%u\ntorrent_block_timeout=%u\ntorrent_retrackers=%u\ntorrent_tracker_protocols=%u\n"
                  "resume_ask=%d\nshow_extensions=%d\nplayback_clock=%d\nremote_control=%d\ncustom_repo_url=%s\n",
                  g_data.resume ? 1 : 0, clamp_seek(g_data.seek_step), g_data.clock_24h ? 1 : 0,
                  g_data.debug_log ? 1 : 0, g_data.hw_decode ? 1 : 0, g_data.show_all ? 1 : 0,
                  g_data.auto_next ? 1 : 0, g_data.newest_first ? 1 : 0, g_data.repeat,
                  g_data.shuffle ? 1 : 0, clamp_volume(g_data.volume), clamp_repo(g_data.repo_preset),
                  g_data.torrent_trackers ? 1 : 0, g_data.torrent_progressive ? 1 : 0,
                  g_data.torrent_cache_path, g_data.remote_diagnostics ? 1 : 0, g_data.torrent_cache_mib,
                  g_data.torrent_read_ahead, g_data.torrent_preload, g_data.torrent_connections,
                  g_data.torrent_requests, g_data.torrent_block_timeout, g_data.torrent_retrackers,
                  g_data.torrent_tracker_protocols,
                  g_data.resume_ask, g_data.show_extensions, g_data.playback_clock, g_data.remote_control, g_data.custom_repo_url);
    write_atomic(MOVIAN_SETTINGS_PATH, body);
}

Data &get() noexcept
{
    return g_data;
}

void format_clock(char *dst, std::size_t capacity) noexcept
{
    if (!dst || !capacity) return;
    // Keep the time API used by the earlier console build. The UI calls this
    // on its render thread; consume the two fields before any other time call.
    const time_t now = std::time(nullptr);
    const auto *local = std::localtime(&now);
    const int hour = local ? local->tm_hour : -1;
    const int minute = local ? local->tm_min : -1;
    char text[12] = "--:--";
    if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60)
    {
        int used = 0;
        const int h = g_data.clock_24h ? hour : (hour % 12 ? hour % 12 : 12);
        if (g_data.clock_24h || h >= 10) text[used++] = '0' + h / 10;
        text[used++] = '0' + h % 10;
        text[used++] = ':';
        text[used++] = '0' + minute / 10;
        text[used++] = '0' + minute % 10;
        if (!g_data.clock_24h) {
            text[used++] = ' ';
            text[used++] = hour < 12 ? 'A' : 'P';
            text[used++] = 'M';
        }
        text[used] = 0;
    }
    std::size_t i = 0;
    while (i + 1 < capacity && text[i]) { dst[i] = text[i]; ++i; }
    dst[i] = 0;
}

bool set_custom_repo_url(const char *url) noexcept
{
    if (!valid_repo_url(url))
        return false;
    std::snprintf(g_data.custom_repo_url, sizeof g_data.custom_repo_url, "%s", url);
    save();
    return true;
}

const char *repo_url() noexcept
{
    return g_data.custom_repo_url[0] ? g_data.custom_repo_url : kRepoPreset[clamp_repo(g_data.repo_preset)];
}

const char *repo_label() noexcept
{
    return g_data.custom_repo_url[0] ? "Custom HTTPS repository" : repo_label(g_data.repo_preset);
}

const char *repo_label(int preset) noexcept
{
    return kRepoLabel[clamp_repo(preset)];
}

void cycle_repo(int delta) noexcept
{
    int v = clamp_repo(g_data.repo_preset) + delta;
    if (v < 0)
        v = repo_preset_count - 1;
    if (v >= repo_preset_count)
        v = 0;
    g_data.repo_preset = v;
    g_data.custom_repo_url[0] = 0;
    save();
}

void cycle_seek(int delta) noexcept
{
    int idx = 1;
    for (int i = 0; i < 4; ++i)
        if (seek_steps[i] == g_data.seek_step)
            idx = i;
    idx = (idx + delta + 4) % 4;
    g_data.seek_step = seek_steps[idx];
    save();
}

void cycle_volume(int delta) noexcept
{
    int idx = 2;
    for (int i = 0; i < 3; ++i)
        if (volume_presets[i] == g_data.volume)
            idx = i;
    idx = (idx + delta + 3) % 3;
    g_data.volume = volume_presets[idx];
    save();
}

void cycle_repeat(int delta) noexcept
{
    int v = g_data.repeat + (delta == 0 ? 1 : delta);
    if (v < 0)
        v = 2;
    if (v > 2)
        v = 0;
    g_data.repeat = v;
    save();
}

const char *repeat_label() noexcept
{
    switch (g_data.repeat)
    {
    case 1:
        return "one";
    case 2:
        return "folder";
    default:
        return "off";
    }
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
