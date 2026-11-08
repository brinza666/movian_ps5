# Movian PS5 — Native Media Center for PlayStation 5

[![Build](https://github.com/blackbearreloaded/ps5-native-app-boilerplate/actions/workflows/tooling.yml/badge.svg)](https://github.com/blackbearreloaded/ps5-native-app-boilerplate/actions/workflows/tooling.yml)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)

**Movian PS5** is a native port of the Movian media center to PlayStation 5 firmware 4.03, built using the [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate). It runs as a homebrew application under ShadowMountPlus / etaHEN.

## Features Implemented

| Subsystem | Status | Details |
|-----------|--------|---------|
| **Video** | ✅ Stub | `sceVideoOut` CPU RGBA8 framebuffer, 1920×1080 |
| **Audio** | ✅ Stub | `sceAudioOut` PCM 48kHz stereo |
| **Input** | ✅ Stub | `scePad` DualSense controller polling & button mapping |
| **Media/FFmpeg** | ✅ Stub | Software decode: H.264/HEVC/VP8, AAC/AC-3/MP3/Opus/FLAC/PCM |
| **Filesystem** | ✅ Stub | POSIX-like wrappers (app0:/, usb0:/, host0:/) |
| **SMB2** | ✅ Stub | Network share access via libm7.so bindings |
| **Torrent** | ✅ Stub | Magnet/.torrent, HTTP/UDP trackers, piece priority for play-while-download |
| **Subtitles** | ✅ Stub | SRT parser, OpenSubtitles download stub, timing parser |
| **Plugins** | ✅ Stub | Duktape JS engine API v2, repo URLs, beta passwords |
| **UI/OSK** | ✅ Stub | On-screen keyboard, settings registry, element tree rendering |

## Requirements

- **PS5 Firmware**: 4.03
- **Loader**: [ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus) + [etaHEN](https://github.com/LightningMods/etaHEN) (port 9021)
- **Build Host**: Ubuntu/WSL with Clang 18, lld 18, Python 3, Make

## Quick Start

### Build on Linux/WSL

```bash
# Install prerequisites
sudo apt update
sudo apt install git make python3 python3-venv clang-18 lld-18

# Clone and build
git clone https://github.com/brinza666/movian_ps5.git
cd movian_ps5/ps5-boilerplate
make
```

### Build on Windows (PowerShell)

```powershell
git clone https://github.com/brinza666/movian_ps5.git
cd movian_ps5\ps5-boilerplate
.\build.ps1
```

### Deploy to PS5

**USB**: Copy `dist/PPSA99455/` folder → `/data/homebrew/PPSA99455/` on PS5

**FTP** (requires FTP server on PS5, e.g., via etaHEN):
```bash
make deploy PS5_HOST=192.168.1.100
```

## Project Structure

```
ps5-boilerplate/
├── src/
│   ├── main.cpp              # Entry point: InitAll/ShutdownAll/RenderFrame/HandleInput
│   ├── paw_platform.h/.c     # Central PS5 API declarations & stub implementations
│   ├── video_ps5.c/.h        # sceVideoOut wrappers
│   ├── audio_ps5.c/.h        # sceAudioOut wrappers
│   ├── input_ps5.c/.h        # scePad input polling
│   ├── media_ps5.c/.h        # FFmpeg software decode stubs
│   ├── filesystem_ps5.c      # POSIX-like FS wrappers
│   ├── smb2_ps5.c            # SMB2 client stubs
│   ├── torrent_ps5.c         # BitTorrent client stubs
│   ├── subtitles_ps5.c       # SRT parser & subtitle rendering
│   ├── plugin_ps5.c          # Duktape JS plugin system
│   ├── ui_ps5.c              # On-screen keyboard & UI elements
│   ├── duktape_ps5.c/.h      # Duktape JS bindings for PS5 APIs
│   ├── duktape.c/.h          # Minimal Duktape 2.7.0 amalgamation stub
│   └── stdlib_stubs.cpp      # C++ std::chrono/thread stubs (ps5_now_seconds, ps5_sleep_ms)
├── sce_sys/
│   ├── param.json            # TitleID: PPSA99455, version 01.000.000
│   └── icon0.png, pic0.dds, pic1.dds, snd0.at9
├── assets/                   # Packaged assets → /app0/assets/
├── runtime/                  # Clean-room libc.prx (generated at build)
├── tooling/                  # Native build tooling (LLVM, FSELF, runtime builder)
└── tools/                    # Build, deploy, lint, asset scripts
```

## Build Outputs

| Output | Description |
|--------|-------------|
| `dist/PPSA99455/` | Complete directory-style application (for ShadowMountPlus) |
| `dist/PPSA99455.ffpkg` | Uncompressed UFS2 image (requires .NET SDK) |
| `dist/PPSA99455.ffpfsc` | Compressed PFS image (requires PSBrew/MkPFS) |

## Versioning

Version is defined in `sce_sys/param.json`:
- `contentVersion`: `01.000.000` (app version, Git tag, Release name)
- `masterVersion`: `01.00` (compatibility baseline)
- `titleId`: `PPSA99455` (stable identifier)

## Development

### Run tests
```bash
make test
```

### Lint / format
```bash
make lint
make format
```

### Rebuild clean-room runtime
```bash
make libc
```

### Create a release
```bash
# Update contentVersion in sce_sys/param.json
# Commit changes
git tag 01.001.000
git push origin 01.001.000
```

## References & Credits

- **ps5-native-app-boilerplate** — [blackbearreloaded/ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)
- **Movian** — Original media center by Andreas Öman
- **ShadowMountPlus** — [drakmor/ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus)
- **etaHEN** — [LightningMods/etaHEN](https://github.com/LightningMods/etaHEN)
- **PS5 Payload SDK** — [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk)
- **Duktape** — Embedded JavaScript engine [duktape.org](https://duktape.org/)
- **FFmpeg** — Multimedia framework [ffmpeg.org](https://ffmpeg.org/)

## License

GPL-3.0-or-later — see [LICENSE](LICENSE)

This project is independent and not affiliated with or endorsed by Sony Interactive Entertainment. PlayStation and PS5 are trademarks of Sony Interactive Entertainment.