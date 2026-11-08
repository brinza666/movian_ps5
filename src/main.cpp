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
#include <cstdarg>

constexpr char kVersion[] = "0.2.0";
constexpr char kTitleId[] = "PPSA99455";
constexpr double kTargetFrameTime = 1.0 / 60.0;

volatile bool g_running = true;

/* Debug logging to file on PS5 (/download0/movian.log) */
static FILE* g_log_file = nullptr;

void ps5_log_init(void) {
    g_log_file = fopen("/download0/movian.log", "w");
    if (g_log_file) {
        fprintf(g_log_file, "=== Movian PS5 Log Start ===\n");
        fprintf(g_log_file, "Version: %s\n", kVersion);
        fprintf(g_log_file, "TitleID: %s\n", kTitleId);
        fflush(g_log_file);
    }
    /* Also log to stderr for serial/debug console */
    fprintf(stderr, "=== Movian PS5 Log Start ===\n");
    fprintf(stderr, "Version: %s\n", kVersion);
    fprintf(stderr, "TitleID: %s\n", kTitleId);
}

void ps5_log_shutdown(void) {
    if (g_log_file) {
        fprintf(g_log_file, "=== Movian PS5 Log End ===\n");
        fclose(g_log_file);
        g_log_file = nullptr;
    }
    fprintf(stderr, "=== Movian PS5 Log End ===\n");
}

void ps5_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    /* Print to stderr (serial console / FTP debug) */
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    
    /* Write to log file */
    if (g_log_file) {
        vfprintf(g_log_file, fmt, args);
        fprintf(g_log_file, "\n");
        fflush(g_log_file);
    }
    va_end(args);
}

#define LOG(fmt, ...) ps5_log("[MOVIAN] " fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) ps5_log("[ERROR] " fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) ps5_log("[WARN] " fmt, ##__VA_ARGS__)

extern "C" double ps5_now_seconds(void);
extern "C" void ps5_sleep_ms(int ms);

double NowSeconds() noexcept {
    return ps5_now_seconds();
}

bool InitAll() noexcept {
    ps5_log_init();

    LOG("========================================");
    LOG("Movian PS5 v%s starting...", kVersion);
    LOG("TitleID: %s", kTitleId);
    LOG("Firmware: 4.03");
    LOG("========================================");

    LOG("Initializing video...");
    if (ps5_video_global_init() != 0) {
        LOG_ERR("FATAL: Video initialization failed");
        return false;
    }
    LOG("Video initialized OK");

    LOG("Initializing audio...");
    if (ps5_audio_global_init() != 0) {
        LOG_WARN("Audio initialization failed (continuing)");
    } else {
        LOG("Audio initialized OK");
    }

    LOG("Initializing input...");
    if (ps5_input_global_init() != 0) {
        LOG_WARN("Input initialization failed (continuing)");
    } else {
        LOG("Input initialized OK");
    }

    LOG("Initializing media player...");
    if (ps5_media_global_init() != 0) {
        LOG_WARN("Media player initialization failed (continuing)");
    } else {
        LOG("Media player initialized OK");
    }

    LOG("Getting video handle...");
    auto* video = ps5_video_get_global();
    if (video) {
        LOG("Video: %ux%u, pitch=%u, format=%u", video->width, video->height, video->pitch, video->format);
        
        LOG("Initializing UI (%ux%u)...", video->width, video->height);
        if (ps5_ui_init(static_cast<int>(video->width), static_cast<int>(video->height)) != 0) {
            LOG_WARN("UI initialization failed (continuing)");
        } else {
            LOG("UI initialized OK");
        }
    } else {
        LOG_ERR("Failed to get video handle!");
    }

    LOG("Initializing plugin system...");
    if (ps5_plugin_init() != 0) {
        LOG_WARN("Plugin system initialization failed (continuing)");
    } else {
        LOG("Plugin system initialized OK");
    }

    LOG("Registering UI settings...");
    int speed_val = 100;
    int subtitle_val = 0;
    int auto_upgrade_val = 1;
    int install_on_demand_val = 1;
    char repo_url[] = "https://repo.movian.eu/plugins-v1.json";

    ps5_ui_register_setting("playback_speed", "Playback Speed", 2, &speed_val);
    ps5_ui_register_setting("subtitle_enabled", "Subtitles Enabled", 0, &subtitle_val);
    ps5_ui_register_setting("repository_url", "Repository URL", 1, repo_url);
    ps5_ui_register_setting("auto_upgrade", "Auto Upgrade", 0, &auto_upgrade_val);
    ps5_ui_register_setting("install_on_demand", "Install on Demand", 0, &install_on_demand_val);

    LOG("Movian PS5 initialized successfully");
    return true;
}

void ShutdownAll() noexcept {
    LOG("Shutting down Movian PS5...");

    ps5_media_stop();
    ps5_plugin_shutdown();
    ps5_subtitle_shutdown();
    ps5_torrent_shutdown();
    ps5_ui_shutdown();
    ps5_media_global_shutdown();
    ps5_input_global_shutdown();
    ps5_audio_global_shutdown();
    ps5_video_global_shutdown();

    ps5_log_shutdown();
    LOG("Movian PS5 shutdown complete");
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
            LOG("[Subtitle] %s", subtitle_text);
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

    if (ps5_input_button_pressed(input, 0x0001)) { // Cross
        ps5_ui_key(0x0D, 1);
    }
    if (ps5_input_button_pressed(input, 0x0002)) { // Circle
        ps5_ui_key(0x1B, 1);
    }
    if (ps5_input_button_pressed(input, 0x0004)) { // Square
        ps5_ui_keyboard_show();
    }
    if (ps5_input_button_pressed(input, 0x4000)) { // D-pad Up
        ps5_ui_key(0x26, 1);
    }
    if (ps5_input_button_pressed(input, 0x8000)) { // D-pad Down
        ps5_ui_key(0x28, 1);
    }
    if (ps5_input_button_pressed(input, 0x10000)) { // D-pad Right
        ps5_ui_key(0x25, 1);
    }
    if (ps5_input_button_pressed(input, 0x20000)) { // D-pad Left
        ps5_ui_key(0x27, 1);
    }
    if (ps5_input_button_pressed(input, 0x1000)) { // Options
        g_running = false;
    }
}

int main() {
    LOG("main() entered");
    
    if (!InitAll()) {
        LOG_ERR("FATAL: Initialization failed");
        ShutdownAll();
        return 1;
    }

    LOG("Entering main render loop...");

    double last_frame = NowSeconds();
    int frame_count = 0;

    while (g_running) {
        double now = NowSeconds();

        HandleInput();
        ps5_torrent_update();

        if (now - last_frame >= kTargetFrameTime) {
            RenderFrame();
            last_frame = now;
            frame_count++;
            
            // Log every 60 frames (~1 second at 60fps)
            if (frame_count % 60 == 0) {
                LOG("Frame %d, running...", frame_count);
            }
        }

        ps5_sleep_ms(1);
    }

    LOG("Main loop exited after %d frames", frame_count);
    ShutdownAll();

    LOG("Movian PS5 exited cleanly");
    return 0;
}