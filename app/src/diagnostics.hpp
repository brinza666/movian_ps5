// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace movian::diagnostics
{
inline constexpr unsigned port = 42000;
// Pass structured diagnostics only. Media paths, URLs and credentials stay local.
void record(const char *area, const char *format, ...) noexcept
    __attribute__((format(printf, 2, 3)));
bool enable(bool enabled) noexcept;
bool lan_enabled() noexcept;
}
