# Movian PS5 Port - Build Instructions for ps5-native-app-boilerplate

This project uses [ps5-native-app-boilerplate](https://github.com/drakmor/ps5-native-app-boilerplate)
to build a ShadowMount-compatible PS5 native application. The boilerplate builds
without proprietary Sony SDK — it generates its own clean-room `libc.prx` runtime
from source.

## Prerequisites

The build requires **WSL2 with Ubuntu** (or native Linux) with:

```bash
sudo apt update
sudo apt install curl git make pkg-config python3 python3-pip python3-venv tar wget unzip \
  clang-18 clang-format-18 clang-tidy-18 lld-18
```

Optional for `.ffpkg` packaging: .NET SDK 8 or newer (for UFS2Tool).

## Quick Build

```bash
# Navigate to the project
cd F:\rea\ps5-boilerplate\  # Windows path
# or in WSL:
cd /mnt/f/rea/ps5-boilerplate

# Initialize app identity (already configured with PPSA99455)
make init TITLE_ID=PPSA99455 APP_NAME="Movian PS5"

# Check build tools
make doctor

# Fetch dependencies
make deps

# Build the app folder (ShadowMount-compatible)
make

# Or build with all output formats (folder + .ffpkg + .ffpfsc)
make packages
```

## Build Outputs

| Target | Output | ShadowMount Compatible |
|--------|--------|----------------------|
| `make` | `dist/PPSA99455/` folder | ✅ Yes |
| `make ffpkg` | `dist/PPSA99455.ffpkg` (UFS2) | ✅ Yes |
| `make ffpfsc` | `dist/PPSA99455.ffpfsc` (compressed) | ✅ Yes |
| `make packages` | All three formats | ✅ Yes |

The app folder is the most reliable for ShadowMountPlus deployment.

## Deploy to PS5

### Method 1: Folder (Recommended for ShadowMountPlus)

Copy the entire `dist/PPSA99455/` folder to your PS5 via USB or FTP:

```
USB: /data/homebrew/PPSA99455/
  eboot.bin
  sce_module/libc.prx
  sce_sys/param.json
  sce_sys/icon0.png
  assets/banner.txt
  assets/ui/main.js
```

Then launch from the PS5 homebrew menu.

### Method 2: FTP Deploy (requires etaHEN on port 9021)

```bash
make deploy PS5_HOST=192.168.1.100
```

### Method 3: FPKG Package

```bash
make ffpkg
# Copy dist/PPSA99455.ffpkg to USB, install via homebrew installer
```

## Project Structure

```
ps5-boilerplate/
├── src/
│   ├── main.cpp          # C++20 entry point (replaces JNI)
│   ├── video_ps5.c       # sceVideoOut implementation
│   ├── audio_ps5.c       # sceAudioOut implementation
│   ├── input_ps5.c       # scePad controller input
│   ├── filesystem_ps5.c  # PS5 file access
│   ├── smb2_ps5.c        # SMB2 network shares
│   ├── torrent_ps5.c     # BitTorrent client
│   ├── subtitles_ps5.c   # SRT subtitles
│   ├── plugin_ps5.c      # Duktape API v2 plugins
│   ├── ui_ps5.c          # UI, on-screen keyboard, settings
│   ├── media_ps5.c       # FFmpeg media playback
│   ├── paw_platform.c/h  # Platform abstraction
│   └── movian_core.c/h   # Movian core wrapper
├── sce_sys/
│   ├── param.json        # App identity (PPSA99455)
│   ├── icon0.png         # App icon
│   └── pic0.dds          # Background
├── assets/
│   ├── banner.txt
│   └── ui/main.js        # Duktape JS UI
└── dist/                 # Build output
    └── PPSA99455/        # App folder (ShadowMount-ready)
```

## Features

| Feature | Implementation |
|---------|---------------|
| Video output | sceVideoOut, CPU RGBA8 framebuffer |
| Audio output | sceAudioOut, PCM |
| Controller input | scePad (all buttons, sticks, triggers) |
| File system | app0, usb0, host0 access |
| SMB2 network | Via libm7.so exports |
| BitTorrent | Magnet links, .torrent files |
| Subtitles | SRT format, OpenSubtitles API |
| Plugins | Duktape API v2, repository system |
| UI | On-screen keyboard, settings, navigation |
| Media | FFmpeg software decode (H.264, HEVC, etc.) |

## Troubleshooting

**clang-18 not found:** Install clang-18 package or set `CXX` environment variable.

**Missing dependencies:** Run `make deps` to fetch native dependencies.

**Build fails on param.json:** Verify titleId uses `PPSA` + 5 digits format.

**ShadowMount won't mount:** Ensure you deploy the entire folder, not just eboot.bin.

**App closes immediately:** Check that param.json has valid contentId and contentVersion.

## References

- Boilerplate: https://github.com/drakmor/ps5-native-app-boilerplate
- ShadowMountPlus: https://github.com/drakmor/ShadowMountPlus
- PS5 SDK setup: See PS5_SDK_SETUP.md (if using proprietary SDK instead)
