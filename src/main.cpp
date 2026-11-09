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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

constexpr char kVersion[] = "0.2.0";
constexpr char kTitleId[] = "PPSA99455";
constexpr double kTargetFrameTime = 1.0 / 60.0;

volatile bool g_running = true;

/* Direct klog output via sceKernelDebugOutText syscall (works on PS5) */
extern "C" int sceKernelDebugOutText(int channel, const char* text);

static void Klog(const char* text) {
    sceKernelDebugOutText(0, text);
}

static void Klogf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Klog(buf);
}

/* Debug logging to file in /download0 (save data - accessible via FTP) */
static FILE* g_log_file = nullptr;

void ps5_log_init(void) {
    /* Ensure /download0/.movian directory exists */
    mkdir("/download0", 0777);
    mkdir("/download0/.movian", 0777);

    g_log_file = fopen("/download0/.movian/movian.log", "w");
    if (g_log_file) {
        fprintf(g_log_file, "=== Movian PS5 Log Start ===\n");
        fprintf(g_log_file, "Version: %s\n", kVersion);
        fprintf(g_log_file, "TitleID: %s\n", kTitleId);
        fflush(g_log_file);
    }

    /* Also log to klog for immediate visibility */
    Klog("=== Movian PS5 Log Start ===");
    Klogf("Version: %s", kVersion);
    Klogf("TitleID: %s", kTitleId);
}

void ps5_log_shutdown(void) {
    if (g_log_file) {
        fprintf(g_log_file, "=== Movian PS5 Log End ===\n");
        fclose(g_log_file);
        g_log_file = nullptr;
    }
    Klog("=== Movian PS5 Log End ===");
}

/* Make files in a directory tree world-readable/writable for FTP access */
static int OpenUpTree(const char* path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    int failed = 0;
    if (S_ISDIR(st.st_mode)) {
        if (chmod(path, 0777) != 0) ++failed;
        DIR* dir = opendir(path);
        if (!dir) return failed;
        while (struct dirent* e = readdir(dir)) {
            if (!std::strcmp(e->d_name, ".") || !std::strcmp(e->d_name, "..")) continue;
            char subpath[1024];
            std::snprintf(subpath, sizeof(subpath), "%s/%s", path, e->d_name);
            failed += OpenUpTree(subpath);
        }
        closedir(dir);
    } else if (S_ISREG(st.st_mode) && chmod(path, 0666) != 0) {
        ++failed;
    }
    return failed;
}

/* Create directory tree with world-writable permissions */
static bool MakeDirs(const char* path) {
    char tmp[1024];
    std::strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    
    for (size_t pos = 1; pos <= std::strlen(tmp); ++pos) {
        if (pos != std::strlen(tmp) && tmp[pos] != '/') continue;
        tmp[pos] = '\0';
        if (mkdir(tmp, 0777) != 0 && errno != EEXIST) {
            Klogf("[MOVIAN] mkdir %s: errno %d", tmp, errno);
            return false;
        }
        tmp[pos] = '/';
    }
    return true;
}

void ps5_log(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len <= 0) return;

    /* Output to klog (visible in klogsrv on port 3232) */
    Klogf("[MOVIAN] %s", buf);

    /* Write to log file in /download0/.movian/ */
    if (g_log_file) {
        fprintf(g_log_file, "%s\n", buf);
        fflush(g_log_file);
    }
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

    /* Fix file permissions in /download0/.movian for FTP access */
    MakeDirs("/download0/.movian");
    OpenUpTree("/download0/.movian");

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

    /* Fix permissions on shutdown for FTP access */
    OpenUpTree("/download0/.movian");

    ps5_log_shutdown();
    LOG("Movian PS5 shutdown complete");
}

void RenderFrame() noexcept {
    auto* video = ps5_video_get_global();
    if (!video) return;

    /* Clear framebuffer */
    ps5_video_clear(video, 0x1a, 0x1a, 0x2e, 0xFF);

    /* Render UI */
    ps5_ui_render(video);

    /* Render video frame if playing */
    if (ps5_media_is_playing()) {
        uint8_t* frame_buf = nullptr;
        int frame_w = 0, frame_h = 0;
        if (ps5_media_decode_video_frame(&frame_buf, &frame_w, &frame_h) > 0) {
            // Decoded frame ready - would blit to framebuffer
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

    /* Flip and wait for vblank - this paces the frame rate to 60fps */
    ps5_video_flip(video);
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

        /* Small sleep to prevent 100% CPU when not rendering */
        ps5_sleep_ms(1);
    }

    LOG("Main loop exited after %d frames", frame_count);
    ShutdownAll();

    LOG("Movian PS5 exited cleanly");
    return 0;
}