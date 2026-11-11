/*
 * Movian for PS5 — software MPEG-1 player (pl_mpeg).
 * H.264/HEVC wait for a later FFmpeg slice. Honest: this binary plays
 * MPEG program streams (.mpg/.mpeg/.mpv/.m1v), not MP4/MKV.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "screen.hpp"

namespace movian::player
{
enum class Kind
{
    none,
    mpeg1,
    later, // recognised video, no decoder in this binary
};

[[nodiscard]] Kind classify(const char *path) noexcept;

// False when the file is not MPEG-1 or cannot be opened. *why is filled.
bool open(const char *path, char *why, int why_cap) noexcept;
void close() noexcept;
[[nodiscard]] bool active() noexcept;
[[nodiscard]] bool playing() noexcept;

// DualSense while the player is up. Returns true if the scene should redraw.
bool handle(unsigned pressed) noexcept;
bool tick() noexcept;
void draw(screen::Canvas &c) noexcept;
} // namespace movian::player
