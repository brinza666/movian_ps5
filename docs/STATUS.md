# Status

Last updated: 2026-11-11 (Phase 2 MPEG-1 `v0.6.0` repeat/shuffle, `eboot.bin` 280529 bytes).

Legend: **done** = observed on PS5 or in a shared-source host preview;
**shell** = UI exists, backend does not; **no** = not in the live tree.
Official M7 column is from the live WSL2 7.0.273 install — see
[M7_CAPABILITIES.md](M7_CAPABILITIES.md).

| Feature | Official M7 | Live `app/` | Notes |
| --- | --- | --- | --- |
| Home-screen title | n/a | **shell** | `PPSA99455`, Media category; Phase 1 browse UI |
| DualSense navigation | pad/keyboard | **done** (code) | folder mounted 2026-11-11; open the icon for klog EXEC |
| Home tiles + search | **yes** (live) | **shell** | Continue / Videos / Bookmarks / Settings; no search OSK |
| Local file browser | **yes** | **shell** | Videos lists `/download0` + `/data/movian` + bookmarks |
| USB | **yes** | **shell** | `/mnt/usb0` is attempted; sandbox may hide it |
| Software video | FFmpeg 4.4.4 | **shell** | MPEG-1 via pl_mpeg; resume/OSD; auto-next; H.264 waits for FFmpeg |
| Hardware video | VDPAU / MediaCodec / … | **no** | Phase 2b, `libSceVideodec2` (toggle stored only) |
| Audio | Pulse / AT / passthrough | **shell** | MPEG-1 MP2 → 48 kHz stereo, volume/mute |
| Settings file | **yes** | **shell** | `/download0/settings.txt` DualSense toggles (auto-next, newest-first) |
| Resume / bookmarks | **yes** | **shell** | `/download0/resume.txt` + `bookmarks.txt` |
| HLS / HTTP | **yes** (TS + fMP4) | **no** | Phase 5 |
| BitTorrent | **yes** (built-in) | **no** | Phase 3 |
| Plugins API v2 | Duktape 1.8 | **no** | Phase 4 |
| Official plugin repo | repo.movian.eu | **no** | 14 plugins listed live |
| SMB2 | **yes** (libsmb2) | **no** | Phase 5 |
| UPnP / DLNA | **yes** | **no** | |
| Subtitles | srt/vtt/ass | **no** | SRT in Phase 6 |
| HTTP Control :42000 | **yes** (live) | **no** | not Phase 0–4 |
| DRM / Widevine / Cast | Android only | **no** | explicit non-goal |

Historical claims in old READMEs that everything above was already
implemented were **false**. See `archive/`.
