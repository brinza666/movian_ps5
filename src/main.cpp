/*
 * Movian PS5 - Native Media Center Entry Point
 * Copyright (C) 2026 Movian PS5 Port
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "paw_platform.h"
#include "movian_core.h"
#include "duktape_ps5.h"
#include <cstdio>
#include <cstring>

constexpr char kVersion[] = "0.2.0";
constexpr char kTitleId[] = "PPSA99455";
constexpr double kTargetFrameTime = 1.0 / 60.0;

volatile bool g_running = true;

extern "C" double ps5_now_seconds(void);
extern "C" void ps5_sleep_ms(int ms);

double NowSeconds() noexcept {
    return ps5_now_seconds();
}

bool InitAll() noexcept {
    std::printf("========================================\n");
    std::printf("Movian PS5 v%s\n", kVersion);
    std::printf("TitleID: %s\n", kTitleId);
    std::printf("Firmware: 4.03\n");
    std::printf("========================================\n");

    if (ps5_video_global_init() != 0) {
        std::fprintf(stderr, "FATAL: Video initialization failed\n");
        return false;
    }
    if (ps5_audio_global_init() != 0) {
        std::fprintf(stderr, "WARNING: Audio initialization failed\n");
    }
    if (ps5_input_global_init() != 0) {
        std::fprintf(stderr, "WARNING: Input initialization failed\n");
    }
    if (ps5_media_global_init() != 0) {
        std::fprintf(stderr, "WARNING: Media player initialization failed\n");
    }

    auto* video = ps5_video_get_global();
    if (video && ps5_ui_init(static_cast<int>(video->width),
                              static_cast<int>(video->height)) != 0) {
        std::fprintf(stderr, "WARNING: UI initialization failed\n");
    }

    if (ps5_plugin_init() != 0) {
        std::fprintf(stderr, "WARNING: Plugin system initialization failed\n");
    }

    int speed_val = 100;
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

void RenderFrame() noexcept {
    auto* video = ps5_video_get_global();
    if (!video) return;

    ps5_ui_render(video);

    if (ps5_media_is_playing()) {
        uint8_t* frame_buf = nullptr;
        int frame_w = 0, frame_h = 0;
        if (ps5_media_decode_video_frame(&frame_buf, &frame_w, &frame_h) > 0) {
            // Decoded frame ready
        }

        double current_time = ps5_media_get_time();
        const char* subtitle_text = ps5_subtitle_get_text(current_time);
        if (subtitle_text) {
            std::printf("[Subtitle] %s\n", subtitle_text);
        }

        uint8_t* audio_buf = nullptr;
        int audio_size = 0;
        if (ps5_media_decode_audio_frame(&audio_buf, &audio_size) > 0) {
            auto* audio = ps5_audio_get_global();
            if (audio) {
                ps5_audio_write(audio, audio_buf, audio_size);
            }
        }
    }
}

void HandleInput() noexcept {
    auto* input = ps5_input_get_global();
    if (!input) return;

    ps5_input_poll(input);

    if (ps5_input_button_pressed(input, 0x0001)) {
        ps5_ui_key(0x0D, 1);
    }
    if (ps5_input_button_pressed(input, 0x0002)) {
        ps5_ui_key(0x1B, 1);
    }
    if (ps5_input_button_pressed(input, 0x0004)) {
        ps5_ui_keyboard_show();
    }
    if (ps5_input_button_pressed(input, 0x4000)) {
        ps5_ui_key(0x26, 1);
    }
    if (ps5_input_button_pressed(input, 0x8000)) {
        ps5_ui_key(0x28, 1);
    }
    if (ps5_input_button_pressed(input, 0x10000)) {
        ps5_ui_key(0x25, 1);
    }
    if (ps5_input_button_pressed(input, 0x20000)) {
        ps5_ui_key(0x27, 1);
    }
    if (ps5_input_button_pressed(input, 0x1000)) {
        g_running = false;
    }
}

int main() {
    if (!InitAll()) {
        std::fprintf(stderr, "FATAL: Initialization failed\n");
        ShutdownAll();
        return 1;
    }

    std::printf("Entering main render loop...\n");

    double last_frame = NowSeconds();

    while (g_running) {
        double now = NowSeconds();

        HandleInput();
        ps5_torrent_update();

        if (now - last_frame >= kTargetFrameTime) {
            RenderFrame();
            last_frame = now;
        }

        ps5_sleep_ms(1);
    }

    ShutdownAll();

    std::printf("Movian PS5 exited cleanly\n");
    return 0;
}
