/*
 * Movian for PS5 — FFmpeg demux + software H.264/AAC, or libSceVideodec2.
 * HEVC is hardware-only (VLC ps5vdec). Software HEVC is refused: it
 * crashed this title on the TV.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace movian::avdec
{
using PcmFn = void (*)(const float *interleaved, int frames, int rate);

[[nodiscard]] bool available() noexcept;
bool open(const char *path, char *why, int why_cap, PcmFn pcm) noexcept;
void close() noexcept;
[[nodiscard]] bool active() noexcept;
[[nodiscard]] bool ended() noexcept;
[[nodiscard]] int width() noexcept;
[[nodiscard]] int height() noexcept;
[[nodiscard]] double time() noexcept;
[[nodiscard]] double duration() noexcept;
[[nodiscard]] const std::uint32_t *rgba() noexcept; // 0xAABBGGRR
[[nodiscard]] const char *codec_name() noexcept;
[[nodiscard]] const char *mode() noexcept; // "HW" or "SW"
bool tick(double dt) noexcept;
void seek(double seconds) noexcept;
} // namespace movian::avdec
