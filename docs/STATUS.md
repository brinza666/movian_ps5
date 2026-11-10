# Status

Last updated: 2026-11-11.

Legend: **done** = observed on PS5 or in a shared-source host preview;
**shell** = UI exists, backend does not; **no** = not in the live tree.
Official M7 column is from the live WSL2 7.0.273 install — see
[M7_CAPABILITIES.md](M7_CAPABILITIES.md).

| Feature | Official M7 | Live `app/` | Notes |
| --- | --- | --- | --- |
| Home-screen title | n/a | **shell** | `PPSA99455`, Media category; signed `eboot.bin` built 2026-11-10 |
| DualSense navigation | pad/keyboard | **done** (code) | hardware check pending for this binary |
| Home tiles + search | **yes** (live) | **shell** | M7: Plugins / Local network / Settings / folders |
| Local file browser | **yes** | **no** | Phase 1 |
| USB | **yes** | **no** | sandbox may hide `/mnt/usb*` |
| Software video | FFmpeg 4.4.4 | **no** | Phase 2 |
| Hardware video | VDPAU / MediaCodec / … | **no** | Phase 2b, `libSceVideodec2` |
| Audio | Pulse / AT / passthrough | **no** | stereo first |
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
