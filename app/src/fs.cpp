/*
 * Movian for PS5 — one-level directory listing.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "fs.hpp"

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

int cmp_entry(const void *lhs, const void *rhs) noexcept
{
    const auto *a = static_cast<const Entry *>(lhs);
    const auto *b = static_cast<const Entry *>(rhs);
    const int ra = entry_rank(a->kind);
    const int rb = entry_rank(b->kind);
    if (ra != rb)
        return ra - rb;
    return cmp_ci(a->name, b->name);
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

int list_dir(const char *path, Entry *out, int cap, int *count, int *err) noexcept
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

    int n = 0;
    int leftover = 0;
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
            if (is_dot_or_dotdot(name))
                continue;

            char full[max_path + 1];
            if (!join_path(full, sizeof full, path, name))
                continue;

            bool is_dir = e->d_type == DT_DIR;
            if (e->d_type == DT_UNKNOWN || e->d_type == DT_LNK)
            {
                struct stat st2{};
                if (stat(full, &st2) == 0)
                    is_dir = S_ISDIR(st2.st_mode);
            }

            const Kind kind = classify_name(name, is_dir);
            if (kind == Kind::other)
                continue;

            if (n >= cap)
            {
                ++leftover;
                continue;
            }
            if (!copy_bounded(out[n].name, sizeof out[n].name, name))
                continue;
            if (!copy_bounded(out[n].path, sizeof out[n].path, full))
                continue;
            out[n].kind = kind;
            ++n;
        }
    }

    free(buf);
    close(fd);
    if (n > 1)
        qsort(out, static_cast<std::size_t>(n), sizeof(Entry), cmp_entry);
    if (count)
        *count = n;
    if (err)
        *err = leftover > 0 ? leftover : 0;
    return 0;
}
} // namespace movian::fs
