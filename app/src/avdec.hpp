/*
 * Movian for PS5 — FFmpeg demux + software H.264/AAC, or libSceVideodec2.
 * HEVC is hardware-only (VLC ps5vdec). Software HEVC is refused: it
 * crashed this title on the TV.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
namespace movian::torrent { class VerifiedFile; }

namespace movian::avdec
{
using PcmFn = void (*)(const float *interleaved, int frames, int rate);

[[nodiscard]] bool available() noexcept;
bool open(const char *path, char *why, int why_cap, PcmFn pcm,
          const torrent::VerifiedFile *stream = nullptr) noexcept;
[[nodiscard]] bool progressive() noexcept;
[[nodiscard]] bool seekable() noexcept;
void close() noexcept;
[[nodiscard]] bool active() noexcept;
enum class EndState { pending, eof, torrent_error };
[[nodiscard]] EndState end_state() noexcept;
[[nodiscard]] int width() noexcept;
[[nodiscard]] int height() noexcept;
[[nodiscard]] double time() noexcept;
[[nodiscard]] double duration() noexcept;
// The returned pixels are valid for the duration of the callback.
using PictureFn = void (*)(void *, const std::uint32_t *, int, int);
void with_picture(PictureFn draw, void *context) noexcept;
[[nodiscard]] const char *codec_name() noexcept;
[[nodiscard]] const char *mode() noexcept; // "HW" or "SW"
struct Track
{
    int stream;
    char title[80];
    char language[24];
    char format[32];
    bool supported;
};
enum class TrackKind { audio, subtitle };
[[nodiscard]] int track_count(TrackKind kind) noexcept;
bool track(TrackKind kind, int row, Track &value) noexcept;
[[nodiscard]] int selected_audio_track() noexcept;
bool select_audio_track(int stream) noexcept;
bool tick(double dt) noexcept;
void seek(double seconds) noexcept;
} // namespace movian::avdec
