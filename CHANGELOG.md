# Changelog

All notable changes to Movian PS5 will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
using the PS5 `contentVersion` format `NN.NNN.NNN`.

## [01.000.000] - 2026-11-08

### Added
- Initial native PS5 port of Movian media center
- **Video subsystem**: `sceVideoOut` CPU RGBA8 framebuffer (1920×1080)
- **Audio subsystem**: `sceAudioOut` PCM 48kHz stereo output
- **Input subsystem**: `scePad` DualSense controller polling with button mapping
- **Media/FFmpeg stubs**: Software decode framework for H.264/HEVC/VP8, AAC/AC-3/MP3/Opus/FLAC/PCM
- **Filesystem**: POSIX-like wrappers (app0:/, usb0:/, host0:/)
- **SMB2 networking**: Client stubs using libm7.so bindings
- **BitTorrent client**: Magnet/.torrent support, HTTP/UDP trackers, piece priority for play-while-download
- **Subtitles**: SRT parser, OpenSubtitles download stub, timing parser
- **Plugin system**: Duktape JS engine API v2 with repo URLs and beta passwords
- **UI/OSK**: On-screen keyboard, settings registry, UI element tree rendering
- **Duktape JS bindings**: `ps5.video`, `ps5.audio`, `ps5.input`, `ps5.fs`, `ps5.net`, `ps5.torrent`, `ps5.subtitle`, `ps5.plugin`, `ps5.ui`
- **Clean-room runtime**: Reproducible `libc.prx` built from source (no proprietary Sony SDK)
- **Build system**: Make + PowerShell frontend, FSELF signer, FFPKG/FFPFSC packaging

### Fixed
- `std::chrono::steady_clock::now()` and `std::this_thread::sleep_for` undefined symbols — replaced with C-compatible `ps5_now_seconds()` and `ps5_sleep_ms()`
- `subtitle_entry_t` typedef vs `struct subtitle_entry` pointer mismatch
- SMB2 undefined symbols (`smb2_init_context`, `smb2_connect_share`, etc.) — added stub implementations
- Duktape amalgamation build failures — provided minimal stub header/source
- `duk_ret_t` missing, `duk_pop` arity, `duk_error` return value handling
- `ps5_ui_render_element` forward declaration missing

### Build
- Successfully builds on Ubuntu/WSL with Clang 18 / lld 18
- Produces valid ShadowMountPlus-compatible app folder at `dist/PPSA99455/`
- TitleID: `PPSA99455`, contentVersion: `01.000.000`

## [Unreleased]

### Planned
- Real FFmpeg integration (libavformat/avcodec/swscale/swresample)
- Real Duktape 2.7.0 amalgamation (fix `combine_src.py` Python 3 bytes/str issue)
- Real SMB2 implementation via libm7.so
- Hardware-accelerated video decode (VDEC) when available
- Audio resampling and format conversion
- USB storage detection and mounting
- Save data support
- Trophy integration
- Comprehensive test suite on PS5 hardware