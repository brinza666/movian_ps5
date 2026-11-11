# Porting plan

Work in vertical slices that always leave a **launchable title**. Copy
patterns from working trees; do not invent a third toolchain.

Reference checkouts (read, do not vendor unless noted):

- https://github.com/ZiZc3/VLC — title build, platform_ps5.c, ps5vdec, sandbox helper
- https://github.com/czz/movian-go — PRODUCT, plugins, BitTorrent, settings
- https://github.com/MiladRTX/ps5-homebrew-starter — renderer, pad, lists, keyboard
- `archive/splicer/src` — local FFmpeg/hwdec/bt experiments
- `C:\Users\brinza\Documents\deep\ps5-homebrew-starter`

## Phase 0 — Native shell

**Done when:** `app/build.sh` produces `build/app/PPSA99455/eboot.bin`,
the icon launches, DualSense switches Home / Videos / BitTorrent /
Plugins, klog shows `EXEC /app0/eboot.bin`, no crash.

- [x] Archive broken trees
- [x] Native C++ title on the starter kit
- [x] Honest docs, credits, new-session handoff
- [x] `app/build.sh` on this workstation produced signed `eboot.bin` (258865 bytes, 2026-11-11)
- [x] Folder deployed to `/data/homebrew/PPSA99455` (2026-11-11); ShadowMount mount OK
- [ ] klog `createApp` / `EXEC /app0/eboot.bin` for this binary (open the icon)

## Phase 1 — Browse

**Done when:** Videos lists real files from `/download0` and at least
one of `/data/movian` or `/mnt/usb0`, Cross selects a row, Circle back.

Steal list + scroll behaviour from
`ps5-homebrew-starter/app/src/page_notes.cpp`. File types: video/audio
extensions only; do not pretend to decode yet. Show a “cannot open
path” row when `opendir` fails (sandbox), never crash.

- [x] Videos lists `/download0`, `/data/movian`, `/mnt/usb0` (64 KiB getdents walker)
- [x] Cross opens a folder or selects a file row; Circle back; Triangle reloads
- [x] Failed `open` becomes a message row (errno), never a crash
- [ ] Hardware confirmation that `/download0` listing is visible on this console (open the icon)
- [ ] USB may stay invisible until a VLC-style Helper.elf — document, do not lie

Optional: VLC-style Helper.elf later if USB stays invisible. Document
the restriction instead of lying.

## Phase 2 — Software playback  ← current

**Done when:** Cross on an MPEG-1 or H.264 file plays video + stereo
audio, pause/seek with the pad, OSD with time.

Path:

1. Cross-compile a **small** FFmpeg (no network, limited demuxers) with
   `prospero-clang`, static `.a`, PacBrew if a recipe exists.
2. Player thread separate from the UI thread (VLC does this; splicer
   `avdec.c` is a local sketch).
3. Blit YUV → RGBA into the existing CPU compositor (slow, honest).
4. `sceAudioOut` 48 kHz stereo. Starter `sound.cpp` is a beep only;
   VLC `platform_ps5.c` and splicer audio are the real references.

Do not enable every codec on day one. MPEG-1 + H.264 + AAC is enough
to prove the pipeline.

- [x] MPEG-1 software path (pl_mpeg) + DualSense pause/seek + OSD time
- [x] `sceAudioOut` 48 kHz stereo ring (starter grain/thread)
- [x] Options settings (resume, seek, volume, clock, debug, show-all, repo URL)
- [x] Continue watching + bookmarks + Square item menu
- [x] Auto-next, L1/R1 prev/next, newest-first sort, file info
- [x] Repeat (off / one / folder) and shuffle
- [ ] H.264 + AAC via a small static FFmpeg
- [ ] Separate decode thread (decode currently on the UI tick)

## Phase 2b — Hardware decode

**Done when:** 1080p H.264/HEVC uses `libSceVideodec2` when the module
loads, software fallback otherwise. HUD shows HW/SW.

References: VLC `ps5/modules/ps5vdec.c`, splicer `hwdec.c`. Link with a
stub `.so` the way VLC stubs `libSceVideodec2`.

## Phase 3 — BitTorrent

**Done when:** a magnet or `.torrent` from Videos/BitTorrent starts
download under `/download0/bt` and the largest video is offered to the
Phase 2 player while pieces arrive.

Reference: movian-go `internal/backend/bittorrent` (magnet →
`torrentfile://` + 40-hex info hash). Splicer `bt.c` is a smaller C
sketch. No DHT required for v1. HTTP+UDP trackers, plaintext peers.

## Phase 4 — Plugins

**Done when:** `plugin.json` + `plugin.js` from a zip in `/download0`
or from `https://bit.ly/mprepo` (`plugins/repo.json`) can
`createService` and `addURI`, and `page.appendItem` video URLs play.

API: movian-go [PLUGINS.md](https://github.com/czz/movian-go/blob/master/PLUGINS.md)
(API v2). Engine: Duktape 2.7 (C, already used by M7) is the realistic
PS5 choice. goja is Go-only.

Host modules to implement first: `native/io`, `native/fs`,
`native/kvstore`, `page`, `plugin.createService`. Defer webpopup/CEF.

## Phase 5 — Network sources

SMB2 (libsmb2), HTTP/HLS (FFmpeg protocols), then UPnP. VLC already
ships libsmb2 on this OS — copy build flags from
`ps5/deps/build-deps.sh`, not from Android JNI stories.

## Phase 6 — Polish

On-screen keyboard (starter `keyboard.cpp` + CommonDialog stub already
in `app/stubs`), SRT, 5.1 downmix, optional Vulkan UI if CPU blit
becomes the bottleneck. Settings file + resume + bookmarks are in
`app/` (`/download0/settings.txt`).

## Explicit non-goals (until the phases above exist)

- Widevine / DRM7 / Google Cast / PiP / WebKit (Android-only M7)
- Paid plugins (bgTV, F1, pluginsBG keys) — host the API, do not vendor content
- Stock Go runtime
- Sony proprietary SDK / retail PKG signing
- Replacing VLC
- Claiming 4K HDR before HW decode works
- HTTP Control :42000 / STPP remote as a shipping feature

Reference behaviour: [M7_CAPABILITIES.md](M7_CAPABILITIES.md). Live
desktop copy: `wsl -d Ubuntu-24.04 -- showtime`.
