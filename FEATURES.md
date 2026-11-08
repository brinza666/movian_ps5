# Movian PS5 - Feature Comparison with M7

| Feature | M7 (Reference) | This Port (PS5) | Implementation Status |
|---------|---------------|-----------------|----------------------|
| Local video and audio | FFmpeg, most files | FFmpeg software decode inside payload | Implemented (media_ps5.c) |
| Video codecs | MPEG-1, H.264, HEVC, MPEG-4, MPEG-2, MJPEG, WMV, VC-1, VP8 | Same (via FFmpeg) | Implemented |
| Audio codecs | AAC, AC-3, E-AC-3, MP3, DTS, TrueHD, Opus, FLAC, PCM | Same (via FFmpeg) | Implemented |
| Containers | MP4, MKV, MPEG-TS, AVI, WMV, ASF, FLV, MPEG | Same (via FFmpeg) | Implemented |
| 4K scaling | Software downscale | Software downscale | Planned |
| Play while torrent downloads | Built-in BitTorrent client | C client with magnet/torrent support | Implemented (torrent_ps5.c) |
| Torrent trackers | HTTP, UDP trackers | HTTP, UDP trackers | Planned |
| Peers | Plaintext peers, no DHT | Plaintext peers | Planned |
| Subtitles | srt, ass, sub, vobsub + OpenSubtitles | srt implemented, ass/vobsub planned | Partially (subtitles_ps5.c) |
| Audio/subtitle track choice | Yes | Browser text tracks only (planned) | Planned |
| Resume, seek, playback speed | Yes (0.5x, 1x, 1.5x, 2x, 0.75x, 1.25x) | Seek and pause, speed options | Implemented (media_ps5.c) |
| HLS including live | Yes | Browser-compatible URLs | Planned |
| SMB, NFS, FTP, SFTP, WebDAV | Yes | SMB2 via libm7.so, FTP via etaHEN 127.0.0.1 | Partially (smb2_ps5.c) |
| UPnP/DLNA | Yes | Not implemented | Planned |
| Icecast/SHOUTcast | Yes | HTTP audio URL | Planned |
| Photos | Yes | Planned | Planned |
| DVD/Blu-ray/disc images | Yes | Planned | Planned |
| AC3/DTS passthrough | PS3 | Mixed to stereo | Planned |
| Plugins and repository | Yes (Duktape API v2) | Yes (Duktape API v2) | Implemented (plugin_ps5.c) |
| Repository URL | https://repo.movian.eu/plugins-v1.json | Same (configurable) | Implemented |
| Alternate repository | https://bit.ly/mprepo | Same (configurable) | Implemented |
| Beta passwords | Yes | Configurable | Implemented |
| Auto upgrade | Yes | Configurable | Implemented |
| Install on demand | Yes | Configurable | Implemented |
| Unified search | Yes | Planned | Planned |
| Per-file settings | Yes | Planned | Planned |
| On-screen keyboard | Yes (pad keyboard with shift) | Yes | Implemented (ui_ps5.c) |
| Web remote | Yes | Not implemented | Planned |
| FTP server | Yes | Not implemented | Planned |
| NTFS support | Yes | Not implemented | Planned |

## Architecture Summary

The port replaces Android JNI with native PS5 entry point:
- `main()` instead of `JNI_OnLoad`
- `sceVideoOut` for video (CPU RGBA8 framebuffer)
- `sceAudioOut` for audio (PCM output)
- `scePad` for controller input
- `libm7.so` core library (Duktape JS, FreeType, SMB2, crypto)
- FFmpeg for media decode (software, inside payload)
- BitTorrent client for play-while-download

## Build Requirements

- PS5 SDK with prospero-clang cross-compiler
- PS5 firmware 4.03 with homebrew support
- etaHEN on port 9021 for debugging
- libc.prx module (included from PPSA99455)

## Deployment

1. Build: `./scripts/build_ps5.sh`
2. Deploy: `./scripts/deploy_ps5.sh <ps5-ip>`
3. Launch from PS5 homebrew menu
