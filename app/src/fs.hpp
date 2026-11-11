/*
 * Movian for PS5 — one-level directory listing.
 *
 * Title libc opendir() uses a buffer smaller than a directory block, so
 * getdents returns EINVAL and the folder looks empty. Same fix as the
 * archived splicer / PS5MC: open the directory and read with a 64 KiB
 * (or larger) buffer.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace movian::fs
{
constexpr int max_name = 95;
constexpr int max_path = 383;

enum class Kind : unsigned char
{
    directory,
    video,
    audio,
    other,
};

struct Entry
{
    char name[max_name + 1];
    char path[max_path + 1];
    Kind kind;
};

// Walks one directory. Hidden names (leading '.') are skipped. Returns 0 when
// the directory opened, -1 when it did not. *err is 0 on success, else errno
// (or a positive leftover count when the listing was truncated).
// include_other lists non-media files too (settings "Show all files").
int list_dir(const char *path, Entry *out, int cap, int *count, int *err,
             bool include_other = false) noexcept;

[[nodiscard]] Kind classify_name(const char *name, bool is_dir) noexcept;
} // namespace movian::fs
