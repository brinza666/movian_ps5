/*
 * Movian for PS5 — 48 kHz stereo AudioOut.
 * Grain/thread pattern from ps5-homebrew-starter sound.cpp (MiladRTX /
 * BlackBearReloaded). Used for MPEG-1 PCM, not UI beeps.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace movian::audio
{
constexpr int rate = 48000;
constexpr int grain = 256; // frames per sceAudioOutOutput (~5 ms)

bool init() noexcept;

// Interleaved stereo float -1..1. Extra samples are dropped if the ring is full.
void push(const float *interleaved, int frames) noexcept;
void clear() noexcept;
} // namespace movian::audio
