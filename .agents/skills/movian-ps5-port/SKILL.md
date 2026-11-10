---
name: movian-ps5-port
description: Port Movian to PS5 as a native ShadowMountPlus title (PPSA99455) following VLC for PS5. Use when building, deploying, or extending Movian on this workstation; when the user mentions movian_ps5, PPSA99455, movian-go on PS5, or the VLC-style rewrite.
---

# Movian PS5 port

## Load first

1. `docs/NEW_SESSION.md` — workstation facts, first commands
2. `docs/ARCHITECTURE.md` — native title, not Go, not 9021
3. `docs/PORTING_PLAN.md` — current phase and done-when
4. `docs/STATUS.md` — do not contradict this table
5. `docs/M7_CAPABILITIES.md` — official M7 (live WSL 7.0.273 + Android 7.0.288)

Repo: `C:\Users\brinza\Documents\deep\movian_ps5`
Remote: https://github.com/brinza666/movian_ps5
Console: `192.168.10.238`

## Non-negotiable

- Ship a **folder title** `dist/PPSA99455` / `build/app/PPSA99455`.
- Build in **WSL Ubuntu 24.04** with `bash app/build.sh`.
- Use public **ps5-payload-sdk**, never Sony’s proprietary SDK.
- Do not mark FFmpeg / torrent / plugins **done** unless the live `app/` binary does it.
- Do not `go build` movian-go for the console. Read it as a spec.
- Do not send `eboot.bin` to port 9021.
- `main` must not return (CRT SIGSEGV). `movian::screen::run` loops.

## Build / deploy

```bash
export PS5_PAYLOAD_SDK=/home/brinza/.ps5-starter/ps5-payload-sdk
export PS5_STARTER=/mnt/c/Users/brinza/Documents/deep/ps5-homebrew-starter
export PS5_HOST=192.168.10.238
cd /mnt/c/Users/brinza/Documents/deep/movian_ps5
bash scripts/doctor.sh
bash app/build.sh
bash app/build.sh deploy "$PS5_HOST"
```

Windows FTP: `C:\Users\brinza\Documents\deep\tools\ps5-ftp.ps1 -Action ls -Remote /data/homebrew`

ShadowMount: `POST http://192.168.10.238:10101/api/v1/scan` and `/api/v1/games` (POST). Mount: `POST /api/v1/games/mount` `{"title_id":"PPSA99455"}`.

klog: TCP 3232. Success: `createApp PPSA99455`, `EXEC /app0/eboot.bin`.

## Code map

| Path | Role |
| --- | --- |
| `app/src/main.cpp` | entry |
| `app/src/ui.cpp` | four places |
| `app/src/screen.cpp` | VideoOut compositor (from starter / Breeze / boilerplate) |
| `app/src/pad.cpp` | DualSense |
| `app/sce_sys/param.json` | identity; Media category 65536 |
| `archive/` | frozen failures — harvest files, do not build the tree |
| `plugins/repo.json` | alternate Movian plugin feed |

## References (read, don’t vendor wholesale)

- https://github.com/ZiZc3/VLC — `ps5/build-title.sh`, `src/platform_ps5.c`, `ps5/modules/ps5vdec.c`
- https://github.com/czz/movian-go — `USER_GUIDE.md`, `PLUGINS.md`, `internal/backend/bittorrent`
- `C:\Users\brinza\Documents\deep\ps5-homebrew-starter\app\src\page_notes.cpp` — list UI
- `archive/splicer/src/hwdec.c`, `avdec.c`, `bt.c` — local decode/bt sketches

## Next slice (until PORTING_PLAN says otherwise)

Phase 1: Videos lists real files from `/download0` and best-effort USB.
Keep the title launching after every change. Update `docs/STATUS.md` in
the same commit as a behaviour change.
