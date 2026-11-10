# Architecture

## Decision: native title, VLC-style

```text
┌─────────────────────────────────────────────────────────────┐
│  PS5 home screen  (ShadowMountPlus)                         │
│    PPSA99455 / Movian                                       │
└──────────────────────────┬──────────────────────────────────┘
                           │ launch
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  /data/homebrew/PPSA99455/                                  │
│    eboot.bin          signed FSELF                          │
│    sce_sys/param.json title metadata (Media, 65536)         │
│    sce_sys/icon0.png  512×512                               │
│    sce_module/libc.prx  clean-room shim                     │
│    assets/fonts       Inter + JetBrains Mono                │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  Process                                                    │
│    CRT (_start, never return from main)                     │
│    movian::screen   sceVideoOut, 1920×1080 CPU compositor   │
│    movian::pad      DualSense                               │
│    movian::ui       Home / Videos / BitTorrent / Plugins    │
│    (later) player, fs, torrent, plugins                     │
└─────────────────────────────────────────────────────────────┘
```

This is the same shape as [ZiZc3/VLC](https://github.com/ZiZc3/VLC):

| VLC (`ps5/build-title.sh`) | Movian (`app/build.sh`) |
| --- | --- |
| `PS5_Vulkan` + RADV + libvlc | starter `kit/` (boilerplate) + our `app/src` |
| `param.json` titleId `PPSA85300` | `PPSA99455` |
| `applicationCategoryType` 65536 (Media) | same |
| signed `eboot.bin` + `libc.prx` | same |
| folder under `/data/homebrew` | same |
| ImGui + Vulkan UI | CPU renderer first (starter); GPU later if needed |
| libvlc + FFmpeg + ps5vdec | Phase 2 player (FFmpeg, then `libSceVideodec2`) |

We start on the CPU renderer because it is **already proven** on this
console (`PPSA99901` Hello PS5). VLC’s Vulkan stack requires
`PS5_Vulkan` and RADV; that is an optional later upgrade, not a
prerequisite for listing files or software decode.

## Rejected: stock Go on PS5

[czz/movian-go](https://github.com/czz/movian-go) is the right *product*
reference (GLFW frontend, FFmpeg 9, goja plugins, BitTorrent backend).
It is the wrong *binary* for this OS.

Go `GOOS=freebsd GOARCH=amd64` does not call libc. It loads a FreeBSD
syscall number into `%eax` and executes `SYSCALL` (`mmap` 477, `thr_new`
455, `_umtx_op` 454, `kqueue` 362, `kevent` 560). The PS5 title
userland does not implement that table. The process is dead before
`main`. `GOOS=linux` is a different ABI and also wrong.

A future Go port would need a libc-calling runtime (OpenBSD-style, or
the approach in [ps5-rust](https://github.com/neokuze/ps5-rust)). That
is a multi-month toolchain, not a Movian feature. Notes from the old
attempt live in [`archive/go-overlay/`](../archive/go-overlay).

## Rejected: payload-only player

`archive/splicer/` is a PIE ELF for **elfldr on TCP 9021**. It is a
useful source of decode experiments (pl_mpeg, FFmpeg, `libSceVideodec2`)
but it is not a home-screen app. Users launch Movian from the icon.
Port 9021 is often down on this console; Payload Manager (8084) is the
payload path, and titles do not use it.

## Layering (target)

```text
ui.cpp            four places, pad, settings  ← look: docs/M7_CAPABILITIES.md
plugin_host       Movian API v2 (js)     ← Phase 4, M7 Duktape + movian-go PLUGINS.md
player            play/pause/seek/osd    ← Phase 2
  ffmpeg_sw       software decode
  hwdec           libSceVideodec2        ← Phase 2b, splicer/VLC reference
fs                usb / download0 / smb  ← Phase 1 / 5
torrent           magnet + .torrent      ← Phase 3, movian-go internal/backend/bittorrent
platform          screen, pad, audio, net
```

Keep `sce*` calls inside platform files. UI and backends should be
host-testable later (the starter’s `STARTER_PREVIEW` path, renamed
`MOVIAN_HOST_PREVIEW`).

## Filesystem (title sandbox)

| Path | Use |
| --- | --- |
| `/app0` | Read-only title image |
| `/download0` | Persistent per-title data (`downloadDataSize` > 0) |
| `/data/movian` | Extra media if the sandbox can see `/data` |
| `/mnt/usb0`… | USB; often hidden until a helper lifts the sandbox (VLC: Helper.elf) |
| `/temp0` | Do not assume it exists (starter notes 0x80020002 on some loaders) |

Log file: `/download0/movian-ps5.log`.
