/*
 * Movian for PS5 — 48 kHz stereo AudioOut.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "audio.hpp"

#include <atomic>
#include <cstddef>
#include <cstring>

#ifndef MOVIAN_HOST_PREVIEW
#include <pthread.h>

extern "C"
{
    int sceAudioOutInit();
    int sceAudioOutOpen(int user, int type, int index, unsigned grain, unsigned rate,
                        unsigned format);
    int sceAudioOutOutput(int handle, const void *samples);
}
#endif

namespace movian::audio
{
namespace
{
constexpr int cap_frames = rate; // 1 s of stereo
constexpr int system_user = 0xff;
constexpr int main_port = 0;
constexpr int s16_stereo = 1;
constexpr unsigned already_initialized = 0x8026000eu;

std::int16_t g_ring[cap_frames * 2];
std::atomic<int> g_read{0};
std::atomic<int> g_write{0};
std::atomic<bool> g_ready{false};
int g_handle = -1;

int next_index(int i) noexcept
{
    return i + 1 >= cap_frames ? 0 : i + 1;
}

void mix_silence(std::int16_t *out, int frames) noexcept
{
    std::memset(out, 0, static_cast<std::size_t>(frames) * 2 * sizeof(std::int16_t));
}

void pull(std::int16_t *out, int frames) noexcept
{
    int r = g_read.load(std::memory_order_relaxed);
    for (int i = 0; i < frames; ++i)
    {
        const int w = g_write.load(std::memory_order_acquire);
        if (r == w)
        {
            out[2 * i] = 0;
            out[2 * i + 1] = 0;
            continue;
        }
        out[2 * i] = g_ring[2 * r];
        out[2 * i + 1] = g_ring[2 * r + 1];
        r = next_index(r);
    }
    g_read.store(r, std::memory_order_release);
}

#ifndef MOVIAN_HOST_PREVIEW
void *output(void *) noexcept
{
    alignas(64) static std::int16_t samples[grain * 2];
    for (;;)
    {
        if (g_ready.load(std::memory_order_relaxed))
            pull(samples, grain);
        else
            mix_silence(samples, grain);
        if (sceAudioOutOutput(g_handle, samples) < 0)
            break;
    }
    return nullptr;
}
#endif
} // namespace

bool init() noexcept
{
#ifndef MOVIAN_HOST_PREVIEW
    if (g_handle >= 0)
        return true;
    const int r = sceAudioOutInit();
    if (r != 0 && static_cast<unsigned>(r) != already_initialized)
        return false;
    g_handle = sceAudioOutOpen(system_user, main_port, 0, grain, rate, s16_stereo);
    if (g_handle < 0)
        return false;
    pthread_t thread;
    if (pthread_create(&thread, nullptr, output, nullptr) != 0)
        return false;
    pthread_detach(thread);
    g_ready.store(true, std::memory_order_relaxed);
    return true;
#else
    g_ready.store(true, std::memory_order_relaxed);
    return false;
#endif
}

void push(const float *interleaved, int frames) noexcept
{
    if (!interleaved || frames <= 0)
        return;
    int w = g_write.load(std::memory_order_relaxed);
    const int r = g_read.load(std::memory_order_acquire);
    for (int i = 0; i < frames; ++i)
    {
        const int nxt = next_index(w);
        if (nxt == r)
            break; // drop rather than overwrite unread audio
        float l = interleaved[2 * i];
        float rg = interleaved[2 * i + 1];
        l = l > 1.f ? 1.f : l < -1.f ? -1.f : l;
        rg = rg > 1.f ? 1.f : rg < -1.f ? -1.f : rg;
        g_ring[2 * w] = static_cast<std::int16_t>(l * 32767.f);
        g_ring[2 * w + 1] = static_cast<std::int16_t>(rg * 32767.f);
        w = nxt;
    }
    g_write.store(w, std::memory_order_release);
}

void clear() noexcept
{
    g_read.store(0, std::memory_order_relaxed);
    g_write.store(0, std::memory_order_relaxed);
}
} // namespace movian::audio
