# Movian PS5 Port

Native PlayStation 5 port of Movian media center for firmware 4.03.
Built with [ps5-native-app-boilerplate](https://github.com/drakmor/ps5-native-app-boilerplate) — no proprietary Sony SDK required.

## Features (from M7 comparison)

| Feature | Status | Implementation |
|---------|--------|---------------|
| Video: MPEG-1, H.264, HEVC, MPEG-4, MPEG-2, MJPEG, WMV, VC-1, VP8 | ✅ | FFmpeg software decode inside payload |
| Audio: AAC, AC-3, E-AC-3, MP3, DTS, TrueHD, Opus, FLAC, PCM | ✅ | FFmpeg software decode inside payload |
| Containers: MP4, MKV, MPEG-TS, AVI, WMV, ASF, FLV, MPEG | ✅ | FFmpeg demuxing |
| 4K software downscale | ⚠️ Planned | CPU framebuffer |
| BitTorrent client (magnet, .torrent) | ✅ | Play-while-download, HTTP/UDP trackers |
| Subtitles: SRT | ✅ | ASS/VOBSub planned |
| Audio/subtitle track selection | ⚠️ Planned | UI framework ready |
| Resume, seek, playback speed (0.5x–2x) | ✅ | Media player controls |
| HLS including live | ⚠️ Planned | Browser-compatible URLs |
| SMB2 network shares | ✅ | Via libm7.so exports |
| NFS, FTP, SFTP, WebDAV | ⚠️ Planned | FTP via etaHEN 127.0.0.1 |
| UPnP/DLNA | ⚠️ Planned | Not yet implemented |
| Icecast/SHOUTcast | ⚠️ Planned | HTTP audio URL |
| Photos | ⚠️ Planned | Image decoder ready |
| DVD/Blu-ray disc images | ⚠️ Planned | Not yet implemented |
| AC3/DTS passthrough | ⚠️ Mixed to stereo | Passthrough planned |
| Plugins (Duktape API v2) | ✅ | Repository, beta passwords, auto-upgrade |
| Repository URL (configurable) | ✅ | https://repo.movian.eu/plugins-v1.json |
| Alternate repository | ✅ | https://bit.ly/mprepo |
| Beta passwords | ✅ | Configurable |
| Auto upgrade | ✅ | Configurable |
| Install on demand | ✅ | Configurable |
| On-screen keyboard (with shift) | ✅ | PS5 controller input |
| Unified search | ⚠️ Planned | Not yet implemented |
| Per-file settings | ✅ | Settings system |
| Web remote | ⚠️ Planned | Not yet implemented |
| FTP server | ⚠️ Planned | Not yet implemented |
| NTFS support | ⚠️ Planned | Not yet implemented |

## Project Structure

```
movian_ps5/
├── movian-ps5/           # Original C implementation (standalone)
│   ├── src/              # C source files
│   ├── scripts/          # Build and deploy scripts
│   ├── assets/           # UI assets
│   ├── sce_sys/          # PS5 app config
│   └── sce_module/       # libc.prx
├── ps5-boilerplate/      # ps5-native-app-boilerplate integration
│   ├── src/              # C++20 entry point + C sources
│   ├── sce_sys/          # PS5 app config (PPSA99455)
│   ├── assets/           # Packaged assets
│   ├── tools/            # Build tooling (from boilerplate)
│   ├── runtime/          # Clean-room libc.prx
│   ├── Makefile          # Build system
│   ├── BUILD_GUIDE.md    # Build instructions
│   └── .env              # Build configuration (copy from .env.example)
├── MOVIAN_PS5_PORT_PLAN.md  # Detailed porting plan
├── FEATURES.md           # Feature comparison table
└── README.md             # This file
```

## Quick Start

### Prerequisites (WSL2 Ubuntu)

```bash
sudo apt update
sudo apt install curl git make pkg-config python3 python3-pip python3-venv tar wget unzip \
  clang-18 clang-format-18 clang-tidy-18 lld-18
```

### Build (ShadowMount-compatible folder)

```bash
cd ps5-boilerplate

# Initialize app identity
make init TITLE_ID=PPSA99455 APP_NAME="Movian PS5"

# Check build environment
make doctor

# Fetch dependencies (PS5 payload SDK, zlib)
make deps

# Build the app folder
make

# Or build all formats (folder + .ffpkg + .ffpfsc)
make packages
```

### Deploy to PS5 (Firmware 4.03)

**Folder method (ShadowMountPlus recommended):**

```
Copy ps5-boilerplate/dist/PPSA99455/ to USB drive
Insert USB into PS5
Install via homebrew manager (ShadowMountPlus, etc.)
Launch Movian PS5 from homebrew menu
```

**FTP method (etaHEN port 9021):**

```bash
make deploy PS5_HOST=192.168.1.100
```

**FPKG method:**

```bash
make ffpkg
# Copy dist/PPSA99455.ffpkg to USB, install via homebrew installer
```

## Analysis Results (via REA)

The core library `libm7.so` was analyzed with REA (Reverse Engineer Anything):

**897 exports identified:**

| Category | Functions | Purpose |
|----------|-----------|---------|
| Duktape JS Engine | `duk_*` (~100) | JavaScript runtime for UI/logic |
| FreeType | `FT_*` (~50) | Font rendering |
| SMB2 Network | `smb2_*` (~60) | SMB network shares |
| DCERPC/RPC | `dcerpc_*`, `ndr_*`, `asn1ber_*`, `srvsvc_*` | Network protocols |
| NTLMSSP Auth | `ntlmssp_*` | Authentication |
| Crypto | MD4, MD5, AES128, hmacResult | Encryption/hashing |
| JNI Bridge | `Java_com_deanbg_movian_*` | Android JNI (replaced with native main()) |
| App Core | `app_dataroot`, `free_c_data` | Application core |

## References

- ps5-native-app-boilerplate: https://github.com/drakmor/ps5-native-app-boilerplate
- ShadowMountPlus: https://github.com/drakmor/ShadowMountPlus
- movian-go: https://github.com/czz/movian-go
- kodi-ps5: https://github.com/VivaLaVent/kodi-ps5
- ps5-tailscale: https://github.com/holdmysocks/ps5-tailscale
- REA (Reverse Engineer Anything): https://github.com/morluto/rea

## License

GPL-3.0-or-later (same as ps5-native-app-boilerplate)
