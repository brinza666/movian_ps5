// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>

namespace movian::diagnostics
{
inline constexpr unsigned port = 42001;
struct Entry
{
    uint64_t sequence;
    char text[200];
};
// Copies the newest records into caller storage in ascending sequence order.
// Sequences remain stable for the session, including across ring wraparound.
std::size_t copy_entries(Entry *entries, std::size_t capacity) noexcept;
// Pass structured diagnostics only. Media paths, URLs and credentials stay local.
void record(const char *area, const char *format, ...) noexcept
    __attribute__((format(printf, 2, 3)));
bool enable(bool enabled) noexcept;
bool lan_enabled() noexcept;
}
