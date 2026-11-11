/*
 * Movian for PS5 — software FFmpeg decode (H.264 + AAC and kin).
 * Compiled always; bodies are stubs unless MOVIAN_HAVE_FFMPEG is set
 * and static libs are linked. MPEG-1 stays on pl_mpeg.
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
bool tick(double dt) noexcept;
void seek(double seconds) noexcept;
} // namespace movian::avdec
