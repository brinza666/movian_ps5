/*
 * Movian PS5 - Native Media Center Entry Point
 * Copyright (C) 2026 Movian PS5 Port
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * C++20 entry point for the PS5 native port of Movian.
 * Replaces Android JNI bridge with native PS5 main().
 *
 * Build: ps5-native-app-boilerplate (clang++-18, lld, FSELF output)
 * Deploy: ShadowMountPlus-compatible folder or .ffpkg image
 */

#include "paw_platform.h"
#include "movian_core.h"
#include "duktape_ps5.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

// Application version
constexpr char kVersion[] = "0.2.0";
constexpr char kTitleId[] = "PPSA99455";

// Frame timing
constexpr double kTargetFrameTime = 1.0 / 60.0; // 60 FPS

namespace {

// Get current time in seconds
double NowSeconds() noexcept {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double>(duration).count();
}

// Initialize all Movian PS5 subsystems
bool InitAll() noexcept {
    std::printf("========================================\n");
    std::printf("Movian PS5 v%s\n", kVersion);
    std::printf("TitleID: %s\n", kTitleId);
    std::printf("Firmware: 4.03\n");
    std::printf("========================================\n");

    // Video output (sceVideoOut)
    if (ps5_video_global_init() != 0) {
        std::fprintf(stderr, "FATAL: Video initialization failed\n");
        return false;
    }

    // Audio output (sceAudioOut)
    if (ps5_audio_global_init() != 0) {
        std::fprintf(stderr, "WARNING: Audio initialization failed\n");
    }

    // Controller input (scePad)
    if (ps5_input_global_init() != 0) {
        std::fprintf(stderr, "WARNING: Input initialization failed\n");
    }

    // Media player (FFmpeg software decode)
    if (ps5_media_global_init() != 0) {
        std::fprintf(stderr, "WARNING: Media player initialization failed\n");
    }

    // UI system
    auto* video = ps5_video_get_global();
    if (video && ps5_ui_init(static_cast<int>(video->width),
                              static_cast<int>(video->height)) != 0) {
        std::fprintf(stderr, "WARNING: UI initialization failed\n");
    }

    // Plugin system (Duktape API v2)
    if (ps5_plugin_init() != 0) {
        std::fprintf(stderr, "WARNING: Plugin system initialization failed\n");
    }

    // BitTorrent client (initialized on demand via torrent_add)

    // Register default settings
    int speed_val = 100; // 1.00x * 100
    int subtitle_val = 0;
    int auto_upgrade_val = 1;
    int install_on_demand_val = 1;
    char repo_url[] = "https://repo.movian.eu/plugins-v1.json";

    ps5_ui_register_setting("playback_speed", "Playback Speed", 2, &speed_val);
    ps5_ui_register_setting("subtitle_enabled", "Subtitles Enabled", 0, &subtitle_val);
    ps5_ui_register_setting("repository_url", "Repository URL", 1, repo_url);
    ps5_ui_register_setting("auto_upgrade", "Auto Upgrade", 0, &auto_upgrade_val);
    ps5_ui_register_setting("install_on_demand", "Install on Demand", 0,
                           &install_on_demand_val);

    std::printf("Movian PS5 initialized successfully\n");
    return true;
}

// Shutdown all subsystems
void ShutdownAll() noexcept {
    std::printf("Shutting down Movian PS5...\n");

    ps5_media_stop();
    ps5_plugin_shutdown();
    ps5_subtitle_shutdown();
    ps5_torrent_shutdown();
    ps5_ui_shutdown();
    ps5_media_global_shutdown();
    ps5_input_global_shutdown();
    ps5_audio_global_shutdown();
    ps5_video_global_shutdown();

    std::printf("Movian PS5 shutdown complete\n");
}

// Render one frame
void RenderFrame() noexcept {
    auto* video = ps5_video_get_global();
    if (!video) return;

    // Render UI
    ps5_ui_render(video);

    // If media is playing, decode and display video frame
    if (ps5_media_is_playing()) {
        uint8_t* frame_buf = nullptr;
        int frame_w = 0, frame_h = 0;
        if (ps5_media_decode_video_frame(&frame_buf, &frame_w, &frame_h) > 0) {
            // Decoded frame ready - in real implementation, copy to framebuffer
            // using swscale output
        }

        // Get subtitle text for current time
        double current_time = ps5_media_get_time();
        const char* subtitle_text = ps5_subtitle_get_text(current_time);
        if (subtitle_text) {
            std::printf("[Subtitle] %s\n", subtitle_text);
        }

        // Write audio
        uint8_t* audio_buf = nullptr;
        int audio_size = 0;
        if (ps5_media_decode_audio_frame(&audio_buf, &audio_size) > 0) {
            auto* audio = ps5_audio_get_global();
            if (audio) {
                ps5_audio_write(audio, audio_buf, static_cast<uint32_t>(audio_size));
            }
        }
    }
}

// Handle controller input
void HandleInput() noexcept {
    auto* input = ps5_input_get_global();
    if (!input) return;

    ps5_input_poll(input);

    if (ps5_input_button_pressed(input, 0x0001)) { // CROSS
        ps5_ui_key(0x0D, 1); // Enter
    }
    if (ps5_input_button_pressed(input, 0x0002)) { // CIRCLE
        ps5_ui_key(0x1B, 1); // Escape
    }
    if (ps5_input_button_pressed(input, 0x0004)) { // SQUARE
        ps5_ui_keyboard_show();
    }
    if (ps5_input_button_pressed(input, 0x4000)) { // DPAD_UP
        ps5_ui_key(0x26, 1); // Up arrow
    }
    if (ps5_input_button_pressed(input, 0x8000)) { // DPAD_DOWN
        ps5_ui_key(0x28, 1); // Down arrow
    }
    if (ps5_input_button_pressed(input, 0x10000)) { // DPAD_LEFT
        ps5_ui_key(0x25, 1); // Left arrow
    }
    if (ps5_input_button_pressed(input, 0x20000)) { // DPAD_RIGHT
        ps5_ui_key(0x27, 1); // Right arrow
    }
    if (ps5_input_button_pressed(input, 0x1000)) { // PS button
        g_running = false;
    }
}

} // namespace

// Global running flag
volatile bool g_running = true;

int main() {
    // Parse command-line arguments for media/torrent
    // (In PS5 homebrew, argv may be limited; check for --media/--magnet)

    // Initialize all subsystems
    if (!InitAll()) {
        std::fprintf(stderr, "FATAL: Initialization failed\n");
        ShutdownAll();
        return 1;
    }

    std::printf("Entering main render loop...\n");

    auto last_frame = NowSeconds();

    // Main render loop
    while (g_running) {
        double now = NowSeconds();

        HandleInput();

        // Update BitTorrent state
        ps5_torrent_update();

        // Render frame (capped at 60 FPS)
        if (now - last_frame >= kTargetFrameTime) {
            RenderFrame();
            last_frame = now;
        }

        // Small sleep to prevent CPU spinning (~1ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Clean shutdown
    ShutdownAll();

    std::printf("Movian PS5 exited cleanly\n");
    return 0;
}
