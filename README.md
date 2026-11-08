# Movian for PS5

PS5 port of Movian. Movian on PS3 is Dean Kasabow's M7 (7.0.273) from [apps.movian.eu](https://apps.movian.eu/), continued from Andreas Öman's Movian. The flat skin is the one in [andoma/movian](https://github.com/andoma/movian).

![Home](docs/images/home.jpg)

The picture above is the browser preview. The television build uses the same four places: Home, Videos, BitTorrent, Plugins.

## Download

| Build | What it is | File |
| --- | --- | --- |
| Home screen | ShadowMountPlus title `PPSA99455`. Launch Movian from the console home screen. | [PPSA99455.zip](https://github.com/brinza666/movian_ps5/releases/download/shadowmount/PPSA99455.zip) |
| Loader | `payload.elf` for a loader that is already running on port 9021. Not the home-screen title. | [pre-alpha](https://github.com/brinza666/movian_ps5/releases/tag/pre-alpha) |

Do not send `eboot.bin` to port 9021. Do not put videos inside the `PPSA99455` folder.

Install and the current zip checksum: [docs/install.md](docs/install.md).

## Manuals

- [Install the home-screen title](docs/install.md)
- [Using Movian](docs/manual.md)
- [USB, data, and where files go](docs/usb.md)
- [Plugins](docs/plugins.md) — Russian and English
- [Playback](docs/playback.md)
- [Loader ELF (port 9021)](docs/payload.md)
- [What M7 does, and what this port does](docs/capabilities.md)

## Tree

| Path | Contents |
| --- | --- |
| `docs/` | Manuals and screenshots |
| `plugins/` | Alternative plugin feed. Address `https://bit.ly/mprepo` |
| `ps5-splicer/` | Player sources, and the port-9021 ELF |
| `movian-ps5-port/` | movian-go overlay. It does not boot yet |
| `movian-ps5/` | Native C port (this project) — replaces Android JNI with native PS5 `main()` |
| `ps5-boilerplate/` | ps5-native-app-boilerplate integration — ShadowMount-compatible builds |

## Movian PS5 Native Port

This is a native PS5 port of Movian built with [ps5-native-app-boilerplate](https://github.com/drakmor/ps5-native-app-boilerplate) — no proprietary Sony SDK required.

### Features (from M7 comparison)

| Feature | Status | Implementation |
|---------|--------|---------------|
| Video: MPEG-1, H.264, HEVC, MPEG-4, MPEG-2, MJPEG, WMV, VC-1, VP8 | ✅ | FFmpeg software decode inside payload |
| Audio: AAC, AC-3, E-AC-3, MP3, DTS, TrueHD, Opus, FLAC, PCM | ✅ | FFmpeg software decode inside payload |
| Containers: MP4, MKV, MPEG-TS, AVI, WMV, ASF, FLV, MPEG | ✅ | FFmpeg demuxing |
| BitTorrent client (magnet, .torrent) | ✅ | Play-while-download |
| Subtitles: SRT | ✅ | ASS/VOBSub planned |
| Resume, seek, playback speed (0.5x–2x) | ✅ | Media player controls |
| SMB2 network shares | ✅ | Via libm7.so exports |
| Plugins (Duktape API v2) | ✅ | Repository, beta passwords, auto-upgrade |
| On-screen keyboard (with shift) | ✅ | PS5 controller input |

### REA Analysis

The core library `libm7.so` was analyzed with [REA](https://github.com/morluto/rea) — **897 exports** identified:
- Duktape JS Engine (`duk_*`) — JavaScript runtime
- FreeType (`FT_*`) — Font rendering
- SMB2 Network (`smb2_*`) — SMB shares
- DCERPC/RPC — Network protocols
- NTLMSSP Auth — Authentication
- Crypto — MD4, MD5, AES128, HMAC

### Build (WSL2 Ubuntu)

```bash
cd ps5-boilerplate

make doctor          # Verify build environment
make deps            # Fetch dependencies
make                 # Build ShadowMount-compatible folder
make packages        # Build folder + .ffpkg + .ffpfsc
```

### Deploy

| Format | Output | ShadowMount Compatible |
|--------|--------|----------------------|
| Folder | `dist/PPSA99455/` | ✅ Yes |
| `.ffpkg` | `dist/PPSA99455.ffpkg` | ✅ Yes |
| `.ffpfsc` | `dist/PPSA99455.ffpfsc` | ✅ Yes |

**Folder method:**
```
Copy dist/PPSA99455/ → USB → PS5 /data/homebrew/PPSA99455/
Launch from homebrew menu
```

**FTP method (etaHEN port 9021):**
```bash
make deploy PS5_HOST=192.168.1.100
```

## License

[LICENSE](LICENSE). Payload sources are GPL-3.0-or-later.
