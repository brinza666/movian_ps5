/*
 * Movian for PS5 — 48 kHz stereo AudioOut.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "audio.hpp"
#include "diagnostics.hpp"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <pthread.h>

#ifndef MOVIAN_HOST_PREVIEW
extern "C"
{
    int sceAudioOutInit();
    int sceAudioOutOpen(int user, int type, int index, unsigned grain, unsigned rate,
                        unsigned format);
    int sceAudioOutOutput(int handle, const void *samples);
    int sceAudioOutClose(int handle);
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
pthread_mutex_t g_ring_lock = PTHREAD_MUTEX_INITIALIZER;
struct RingGuard
{
    RingGuard() { pthread_mutex_lock(&g_ring_lock); }
    ~RingGuard() { pthread_mutex_unlock(&g_ring_lock); }
    RingGuard(const RingGuard &) = delete;
    RingGuard &operator=(const RingGuard &) = delete;
};
int g_read = 0;
int g_write = 0;
unsigned g_generation = 0;
bool g_submitting = false;
unsigned long long g_submitted = 0;
unsigned long long g_nonzero = 0;
unsigned long long g_dropped = 0;
int g_output_error = 0;
std::atomic<bool> g_ready{false};
std::atomic<int> g_volume{100};
std::atomic<bool> g_mute{false};
int g_handle = -1;

int next_index(int i) noexcept
{
    return i + 1 >= cap_frames ? 0 : i + 1;
}

#ifndef MOVIAN_HOST_PREVIEW
int pull(std::int16_t *out, int frames, int &nonzero) noexcept
{
    int count = 0;
    for (; count < frames && g_read != g_write; ++count)
    {
        const auto left = g_ring[2 * g_read];
        const auto right = g_ring[2 * g_read + 1];
        out[2 * count] = left;
        out[2 * count + 1] = right;
        nonzero += left != 0 || right != 0;
        g_read = next_index(g_read);
    }
    std::memset(out + 2 * count, 0,
                static_cast<std::size_t>(frames - count) * 2 * sizeof(std::int16_t));
    return count;
}

void *output(void *) noexcept
{
    if (g_handle < 0)
    {
        return nullptr;
    }
    alignas(64) static std::int16_t samples[grain * 2];
    for (;;)
    {
        int nonzero = 0;
        int count;
        unsigned generation;
        {
            const RingGuard lock;
            generation = g_generation;
            count = pull(samples, grain, nonzero);
            g_submitting = count != 0;
        }
        const int result = sceAudioOutOutput(g_handle, samples);
        {
            const RingGuard lock;
            if (result < 0)
            {
                g_output_error = result;
                g_submitting = false;
                g_ready.store(false, std::memory_order_relaxed);
                break;
            }
            if (generation == g_generation)
            {
                g_submitted += count;
                g_nonzero += nonzero;
                g_submitting = false;
            }
        }
    }
    return nullptr;
}
#endif
} // namespace

bool init() noexcept
{
#ifndef MOVIAN_HOST_PREVIEW
    if (g_handle >= 0)
        return g_ready.load(std::memory_order_relaxed);
    const int r = sceAudioOutInit();
    if (r != 0 && static_cast<unsigned>(r) != already_initialized)
        return false;
    g_handle = sceAudioOutOpen(system_user, main_port, 0, grain, rate, s16_stereo);
    if (g_handle < 0)
        return false;
    pthread_t thread;
    g_ready.store(true, std::memory_order_relaxed);
    if (pthread_create(&thread, nullptr, output, nullptr) != 0)
    {
        g_ready.store(false, std::memory_order_relaxed);
        sceAudioOutClose(g_handle);
        g_handle = -1;
        return false;
    }
    pthread_detach(thread);
    return true;
#else
    g_ready.store(true, std::memory_order_relaxed);
    return false;
#endif
}

void push(const float *interleaved, int frames) noexcept
{
    if (!interleaved || frames <= 0 || !g_ready.load(std::memory_order_relaxed))
        return;
    const RingGuard lock;
    const float gain = g_mute.load(std::memory_order_relaxed)
                           ? 0.f
                           : static_cast<float>(g_volume.load(std::memory_order_relaxed)) / 100.f;
    for (int i = 0; i < frames; ++i)
    {
        const int nxt = next_index(g_write);
        if (nxt == g_read)
        {
            g_dropped += frames - i;
            break; // drop rather than overwrite unread audio
        }
        float l = interleaved[2 * i] * gain;
        float rg = interleaved[2 * i + 1] * gain;
        l = l > 1.f ? 1.f : l < -1.f ? -1.f : l;
        rg = rg > 1.f ? 1.f : rg < -1.f ? -1.f : rg;
        g_ring[2 * g_write] = static_cast<std::int16_t>(l * 32767.f);
        g_ring[2 * g_write + 1] = static_cast<std::int16_t>(rg * 32767.f);
        g_write = nxt;
    }
}

void clear() noexcept
{
    const RingGuard lock;
    ++g_generation;
    g_read = g_write = 0;
    g_submitting = false;
    g_submitted = g_nonzero = g_dropped = 0;
}

bool drained() noexcept
{
#ifndef MOVIAN_HOST_PREVIEW
    const RingGuard lock;
    return !g_ready.load(std::memory_order_relaxed) || (g_read == g_write && !g_submitting);
#else
    return true;
#endif
}

void log_drain() noexcept
{
    const RingGuard lock;
    diagnostics::record("audio", "drain submitted=%llu nonzero=%llu dropped=%llu result=%#x",
                        g_submitted, g_nonzero, g_dropped, static_cast<unsigned>(g_output_error));
    if (auto *log = std::fopen("/download0/hevc-test-decode.log", "a"))
    {
        std::fprintf(log, "v52 AudioOut drain submitted=%llu nonzero=%llu dropped=%llu rc=%#x ready=%d\n",
                     g_submitted, g_nonzero, g_dropped, static_cast<unsigned>(g_output_error),
                     g_ready.load(std::memory_order_relaxed));
        std::fclose(log);
    }
}

void set_volume(int percent) noexcept
{
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;
    g_volume.store(percent, std::memory_order_relaxed);
}

int volume() noexcept
{
    return g_volume.load(std::memory_order_relaxed);
}

void set_mute(bool on) noexcept
{
    g_mute.store(on, std::memory_order_relaxed);
    if (on)
        clear();
}

bool muted() noexcept
{
    return g_mute.load(std::memory_order_relaxed);
}
} // namespace movian::audio
