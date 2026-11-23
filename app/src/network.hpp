// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "fs.hpp"
#include <cstdint>

namespace movian::network {
constexpr int max_connections = 8;
struct Connection {
    char url[256]{}; // smb2://host/share or ftp://host:port/folder, never credentials
    char user[96]{};
    char password[121]{};
    char domain[96]{};
    bool remember = true;
};
struct Config {
    char system_name[96] = "movian";
    bool smb_large_buffer = true;
    int smb_xattr = 2; // desktop order Off, SMBv1, SMBv2, On (attribute writes pending)
    bool ftp_server = false;
    unsigned ftp_port = 7777;
    char ftp_user[96]{};
    char ftp_password[121]{};
    Connection connections[max_connections];
};
Config &config() noexcept;
void load() noexcept;
bool save() noexcept;
bool restart_server() noexcept;
bool server_running() noexcept;
bool valid_url(const char *url) noexcept;
bool is_path(const char *path) noexcept;
void root_path(int index, char *out, unsigned cap) noexcept;
// Bounded asynchronous listing, EAGAIN while the worker runs. No UI thread I/O.
int list(const char *path, fs::Entry *out, int cap, int *count, int *error,
         bool other, fs::SortOrder sort, bool folders_first) noexcept;
bool poll() noexcept;
void invalidate() noexcept;
struct File;
File *open(const char *path) noexcept;
int read(File *file, unsigned char *out, int size) noexcept;
std::int64_t seek(File *file, std::int64_t offset, int whence) noexcept;
void close(File *file) noexcept;
} // namespace movian::network
