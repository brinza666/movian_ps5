/*
 * Movian for PS5 — one-level directory listing.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "fs.hpp"
#include "network.hpp"

#include <algorithm>
#include <cstddef>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#endif
#ifndef DT_DIR
#define DT_DIR 4
#endif
#ifndef DT_REG
#define DT_REG 8
#endif
#ifndef DT_LNK
#define DT_LNK 10
#endif

namespace movian::fs
{
namespace
{
char lower_ascii(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool ends_with_ci(const char *name, const char *ext) noexcept
{
    const std::size_t n = strlen(name);
    const std::size_t e = strlen(ext);
    if (n < e)
        return false;
    for (std::size_t i = 0; i < e; ++i)
    {
        if (lower_ascii(name[n - e + i]) != ext[i])
            return false;
    }
    return true;
}

bool copy_bounded(char *dst, std::size_t cap, const char *src) noexcept
{
    if (!src || cap == 0)
        return false;
    const std::size_t n = strlen(src);
    if (n >= cap)
        return false;
    memcpy(dst, src, n + 1);
    return true;
}

bool join_path(char *dst, std::size_t cap, const char *dir, const char *name) noexcept
{
    const std::size_t d = strlen(dir);
    const std::size_t n = strlen(name);
    if (d + 1 + n >= cap)
        return false;
    memcpy(dst, dir, d);
    dst[d] = '/';
    memcpy(dst + d + 1, name, n + 1);
    return true;
}

int entry_rank(Kind k) noexcept
{
    switch (k)
    {
    case Kind::directory:
        return 0;
    case Kind::video:
        return 1;
    case Kind::audio:
        return 2;
    default:
        return 3;
    }
}

int cmp_ci(const char *a, const char *b) noexcept
{
    for (;;)
    {
        const unsigned char ca = static_cast<unsigned char>(lower_ascii(*a));
        const unsigned char cb = static_cast<unsigned char>(lower_ascii(*b));
        if (ca != cb)
            return ca < cb ? -1 : 1;
        if (ca == 0)
            return 0;
        ++a;
        ++b;
    }
}

bool is_dot_or_dotdot(const char *name) noexcept
{
    return name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0));
}
} // namespace

Kind classify_name(const char *name, bool is_dir) noexcept
{
    if (is_dir)
        return Kind::directory;
    if (!name)
        return Kind::other;
    if (ends_with_ci(name, ".zip"))
        return Kind::package;

    static const char *const video[] = {
        ".mpg",  ".mpeg", ".mpv",  ".m1v",  ".m2v", ".mp4", ".m4v",  ".mov",
        ".mkv",  ".ts",   ".m2ts", ".mts",  ".avi", ".webm", ".wmv", ".asf",
        ".flv",  ".vob",  ".ogv",  ".3gp",  nullptr,
    };
    static const char *const audio[] = {
        ".mp3", ".m4a", ".aac", ".flac", ".ogg", ".oga", ".opus", ".wav",
        ".wma", ".ac3", ".eac3", ".dts", ".ape", ".wv",  nullptr,
    };
    for (int i = 0; video[i]; ++i)
        if (ends_with_ci(name, video[i]))
            return Kind::video;
    for (int i = 0; audio[i]; ++i)
        if (ends_with_ci(name, audio[i]))
            return Kind::audio;
    return Kind::other;
}

static int list_directory_impl(const char *path, Entry *out, int cap, int *count, int *err,
                               bool include_other, SortOrder order, bool folders_first,
                               bool legacy_groups, const char *file_extension) noexcept
{
    if (count)
        *count = 0;
    if (err)
        *err = 0;
    if (!path || !path[0] || !out || cap <= 0)
    {
        if (err)
            *err = EINVAL;
        return -1;
    }

    int n = 0;
    int leftover = 0;
    auto before = [&](const Entry &a, const Entry &b) {
        const bool a_dir = a.kind == Kind::directory;
        const bool b_dir = b.kind == Kind::directory;
        if (legacy_groups)
        {
            const int group = entry_rank(a.kind) - entry_rank(b.kind);
            if (group) return group < 0;
        }
        if (folders_first && a_dir != b_dir)
            return a_dir;
        if (order != SortOrder::filename && !(legacy_groups && a_dir) && a.mtime != b.mtime)
            return order == SortOrder::newest ? a.mtime > b.mtime : a.mtime < b.mtime;
        const int name_order = cmp_ci(a.name, b.name);
        return name_order ? name_order < 0 : strcmp(a.name, b.name) < 0;
    };
    auto collect = [&](const char *name, unsigned type) {
        if (is_dot_or_dotdot(name)) return;
        Entry entry{};
        if (!copy_bounded(entry.name, sizeof entry.name, name) ||
            !join_path(entry.path, sizeof entry.path, path, name))
            return;

        struct stat st{};
        const bool have_stat = stat(entry.path, &st) == 0;
        const bool is_dir = have_stat ? S_ISDIR(st.st_mode) : type == DT_DIR;
        entry.kind = classify_name(name, is_dir);
        if ((entry.kind == Kind::other && !include_other) ||
            (!is_dir && file_extension && !ends_with_ci(name, file_extension))) return;
        entry.size = have_stat ? static_cast<long long>(st.st_size) : 0;
        entry.mtime = have_stat ? static_cast<long long>(st.st_mtime) : 0;

        if (n == cap)
        {
            ++leftover;
            if (!before(entry, out[0])) return;
            std::pop_heap(out, out + n, before);
            --n;
        }
        out[n++] = entry;
        std::push_heap(out, out + n, before);
    };

#ifdef MOVIAN_HOST_PREVIEW
    auto *directory = opendir(path);
    if (!directory)
    {
        if (err) *err = errno;
        return -1;
    }
    int read_error = 0;
    for (;;)
    {
        errno = 0;
        const auto *entry = readdir(directory);
        if (!entry)
        {
            read_error = errno;
            break;
        }
        collect(entry->d_name, entry->d_type);
    }
    closedir(directory);
    if (read_error)
    {
        if (err) *err = read_error;
        return -1;
    }
#else
    const int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0)
    {
        if (err)
            *err = errno;
        return -1;
    }

    int buf_cap = 64 * 1024;
    struct stat st{};
    if (fstat(fd, &st) == 0 && st.st_blksize > buf_cap && st.st_blksize <= 1024 * 1024)
        buf_cap = static_cast<int>(st.st_blksize);

    char *buf = static_cast<char *>(malloc(static_cast<std::size_t>(buf_cap)));
    if (!buf)
    {
        if (err)
            *err = ENOMEM;
        close(fd);
        return -1;
    }

    for (;;)
    {
        int got = getdents(fd, buf, buf_cap);
        while (got < 0 && errno == EINVAL && buf_cap < 1024 * 1024)
        {
            const int ncap = buf_cap * 2;
            char *bigger = static_cast<char *>(realloc(buf, static_cast<std::size_t>(ncap)));
            if (!bigger)
                break;
            buf = bigger;
            buf_cap = ncap;
            got = getdents(fd, buf, buf_cap);
        }
        if (got < 0 && (errno == ENOSYS || errno == EPERM))
        {
            long base = 0;
            got = getdirentries(fd, buf, buf_cap, &base);
        }
        if (got < 0)
        {
            if (err)
                *err = errno;
            free(buf);
            close(fd);
            return -1;
        }
        if (got == 0)
            break;

        int off = 0;
        while (off + 8 <= got)
        {
            auto *e = reinterpret_cast<struct dirent *>(buf + off);
            const int rec = static_cast<int>(e->d_reclen);
            if (rec < 8 || off + rec > got)
                break;
            off += rec;
            if (e->d_fileno == 0)
                continue;

            int namelen = static_cast<int>(e->d_namlen);
            if (namelen <= 0 || namelen > 255 || 8 + namelen > rec)
                namelen = static_cast<int>(strnlen(e->d_name, static_cast<std::size_t>(rec - 8)));
            if (namelen <= 0 || namelen > 255)
                continue;

            char name[256];
            memcpy(name, e->d_name, static_cast<std::size_t>(namelen));
            name[namelen] = 0;
            collect(name, e->d_type);
        }
    }

    free(buf);
    close(fd);
#endif
    std::sort_heap(out, out + n, before);
    if (count)
        *count = n;
    if (err)
        *err = leftover > 0 ? leftover : 0;
    return 0;
}
int list_directory(const char *path, Entry *out, int cap, int *count, int *err,
                   bool include_other, SortOrder order, bool folders_first,
                   const char *file_extension) noexcept
{
    if(network::is_path(path)) {
        int n=0, error=0;
        const int rc=network::list(path,out,cap,&n,&error,include_other,order,folders_first);
        if (rc==0 && file_extension) {
            int kept=0;
            for (int i=0;i<n;++i)
                if (out[i].kind==Kind::directory || ends_with_ci(out[i].name,file_extension))
                    out[kept++]=out[i];
            n=kept;
        }
        if(count) *count=n;
        if(err) *err=error;
        return rc;
    }
    return list_directory_impl(path, out, cap, count, err, include_other, order, folders_first, false, file_extension);
}

int list_dir(const char *path, Entry *out, int cap, int *count, int *err,
             bool include_other, bool newest_first) noexcept
{
    return list_directory_impl(path, out, cap, count, err, include_other,
                               newest_first ? SortOrder::newest : SortOrder::filename, true, true, nullptr);
}
} // namespace movian::fs
