// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <sys/stat.h>
#ifdef MOVIAN_HAVE_SMB2
#include "network.hpp"
#endif

namespace movian::input {
// Small descriptors/packages only. The caller owns the returned allocation.
// Network payloads stay in memory; media still uses the seekable stream reader.
inline char *read_bounded(const char *path, std::size_t limit, std::size_t &size) noexcept
{
    size = 0;
    if (!path || !*path || !limit) return nullptr;
#ifdef MOVIAN_HAVE_SMB2
    if (network::is_path(path)) {
        auto *file = network::open(path);
        if (!file) return nullptr;
        const auto length = network::seek(file, 0, 0x10000); // AVSEEK_SIZE
        char *data = length > 0 && static_cast<std::uint64_t>(length) <= limit
                   ? static_cast<char *>(std::malloc(static_cast<std::size_t>(length))) : nullptr;
        std::size_t used = 0;
        while (data && used < static_cast<std::size_t>(length)) {
            const int request = static_cast<int>(std::min<std::size_t>(65536, length - used));
            const int n = network::read(file, reinterpret_cast<unsigned char *>(data + used), request);
            if (n <= 0 || n > request) break;
            used += n;
        }
        network::close(file);
        if (data && used == static_cast<std::size_t>(length)) { size = used; return data; }
        std::free(data);
        return nullptr;
    }
#endif
    struct stat st{};
    if (stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
        static_cast<std::uint64_t>(st.st_size) > limit) return nullptr;
    auto *data = static_cast<char *>(std::malloc(static_cast<std::size_t>(st.st_size)));
    auto *file = data ? std::fopen(path, "rb") : nullptr;
    const bool ok = file && std::fread(data, 1, static_cast<std::size_t>(st.st_size), file) ==
                           static_cast<std::size_t>(st.st_size) && !std::ferror(file);
    if (file) std::fclose(file);
    if (ok) { size = static_cast<std::size_t>(st.st_size); return data; }
    std::free(data);
    return nullptr;
}
} // namespace movian::input
