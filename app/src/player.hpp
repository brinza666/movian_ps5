/*
 * Movian for PS5 — software MPEG-1 player (pl_mpeg).
 * MPEG-1 always uses pl_mpeg. H.264/AAC play only when this binary was
 * linked with static FFmpeg (MOVIAN_HAVE_FFMPEG).
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
    ffmpeg, // H.264/AAC when static FFmpeg is linked
    later,  // recognised video, no decoder in this binary
};

[[nodiscard]] Kind classify(const char *path) noexcept;

// False when the file is not MPEG-1 or cannot be opened. *why is filled.
// When from_start is false and resume is enabled, playback continues from
// the last saved position for this path (if any).
bool open(const char *path, char *why, int why_cap, bool from_start = false) noexcept;
void close() noexcept;
[[nodiscard]] bool active() noexcept;
[[nodiscard]] bool playing() noexcept;
[[nodiscard]] const char *path() noexcept;
[[nodiscard]] const char *title() noexcept;
[[nodiscard]] double time() noexcept;
[[nodiscard]] double duration() noexcept;
[[nodiscard]] bool ended() noexcept;
[[nodiscard]] int width() noexcept;
[[nodiscard]] int height() noexcept;

// DualSense while the player is up. Returns true if the scene should redraw.
bool handle(unsigned pressed) noexcept;
bool tick() noexcept;
void draw(screen::Canvas &c) noexcept;
} // namespace movian::player
