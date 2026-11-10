# Status

Last updated: 2026-11-10.

Legend: **done** = observed on PS5 or in a shared-source host preview;
**shell** = UI exists, backend does not; **no** = not in the live tree.

| Feature | M7 / movian-go | Live `app/` | Notes |
| --- | --- | --- | --- |
| Home-screen title | n/a | **shell** | `PPSA99455`, Media category; signed `eboot.bin` built 2026-11-10 |
| DualSense navigation | yes | **done** (code) | hardware check pending for this binary |
| Local file browser | yes | **no** | Phase 1 |
| USB | yes | **no** | sandbox may hide `/mnt/usb*` |
| Software video | FFmpeg | **no** | Phase 2 |
| Hardware video | platform | **no** | Phase 2b, `libSceVideodec2` |
| Audio | yes | **no** | |
| BitTorrent | yes | **no** | Phase 3 |
| Plugins API v2 | Duktape / goja | **no** | Phase 4 |
| SMB2 | yes | **no** | Phase 5 |
| UPnP / DLNA | yes | **no** | |
| Subtitles | srt/ass | **no** | |
| Web remote | yes | **no** | |

Historical claims in old READMEs that everything above was already
implemented were **false**. See `archive/`.
