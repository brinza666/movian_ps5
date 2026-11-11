# New session handoff — Movian for PS5

Give this file to the next agent or human. It is enough to start Phase 1
without rediscovering the workstation.

## Objective

Port **Movian** to PS5 as a **native ShadowMountPlus title** (`PPSA99455`),
following the **VLC for PS5** packaging model, using **movian-go / M7** as
the product reference. Do not resume the broken Go ELF or the stub
`ps5-boilerplate` tree.

## First commands

```bash
cd /mnt/c/Users/brinza/Documents/deep/movian_ps5
bash scripts/doctor.sh
bash app/build.sh
# optional: bash app/build.sh deploy 192.168.10.238
```

On Windows PowerShell (host tools only):

```powershell
cd C:\Users\brinza\Documents\deep\movian_ps5
# FTP helper
C:\Users\brinza\Documents\deep\tools\ps5-ftp.ps1 -Action ls -Remote /data/homebrew
# klog is TCP 3232 — use the existing monitor if it is running
```

Read, in order:

1. [ARCHITECTURE.md](ARCHITECTURE.md) — why native, why not Go
2. [PORTING_PLAN.md](PORTING_PLAN.md) — phases, done/next
3. [STATUS.md](STATUS.md) — honest capability table
4. [BUILDING.md](BUILDING.md) / [DEPLOY.md](DEPLOY.md)
5. [PRODUCT.md](PRODUCT.md) — what Movian is supposed to feel like
6. [M7_CAPABILITIES.md](M7_CAPABILITIES.md) — live WSL2 + Android harvest of official M7
7. Skill: `.agents/skills/movian-ps5-port/SKILL.md`

## Workstation (verified 10 Nov 2026)

| Item | Value |
| --- | --- |
| Windows | 11 Pro, user `brinza` |
| WSL | Ubuntu-24.04, user `brinza` |
| SDK | `/home/brinza/.ps5-starter/ps5-payload-sdk` (v0.42), also `PS5_PAYLOAD_SDK` in `~/.bashrc` |
| Windows SDK copy | `C:\Users\brinza\Documents\deep\ps5-payload-sdk\ps5-payload-sdk` |
| Starter + kit | `C:\Users\brinza\Documents\deep\ps5-homebrew-starter` (`kit/` = boilerplate) |
| This repo | `C:\Users\brinza\Documents\deep\movian_ps5` → https://github.com/brinza666/movian_ps5 |
| GitHub | `gh` as `brinza666`, HTTPS. `git@github.com` SSH is **not** working; use HTTPS |
| Console IP | `192.168.10.238` |
| FTP | 2121 (`ftpsrv`) |
| klog | 3232 |
| ShadowMount API | 10101 (`POST /api/v1/scan`, `POST /api/v1/games`, `POST /api/v1/games/mount`) |
| Payload Manager | 8084 |
| etaHEN | 12800 |
| elfldr 9021 | often **down** — titles do not need it |
| Hello PS5 test | `PPSA99901` built and launched successfully (no SIGSEGV in the title path) |
| `hello.elf` payload | toast works, then CRT `_start` SIGSEGV on exit — **payload** issue, ignore for titles |
| REA | `rea-agents@6.8.0`, Ghidra 12.1.4, JADX MCP JAR configured |
| DSH profile | `desktop`, GUI `http://127.0.0.1:64482` |
| FTP script | `C:\Users\brinza\Documents\deep\tools\ps5-ftp.ps1` (`-Action ls/get/put`) |
| klog monitor | `C:\Users\brinza\Documents\deep\tools\ps5-monitor.ps1` |
| Official M7 (WSL) | `showtime` 7.0.273 in Ubuntu-24.04; HTTP control `http://127.0.0.1:42000/` |
| M7 data | `\\wsl$\Ubuntu-24.04\home\brinza\.hts\showtime` |

WSL build environment:

```bash
export PS5_PAYLOAD_SDK=/home/brinza/.ps5-starter/ps5-payload-sdk
export PS5_STARTER=/mnt/c/Users/brinza/Documents/deep/ps5-homebrew-starter
export PS5_HOST=192.168.10.238
```

## Deploy loop that already worked for Hello PS5

1. `bash app/build.sh`
2. Upload `build/app/PPSA99455/` to `/data/homebrew/PPSA99455/` (build.sh
   `deploy` or the PowerShell FTP helper).
3. `POST http://192.168.10.238:10101/api/v1/games/mount` with
   `{"title_id":"PPSA99455"}` if the icon is present but not launched.
   There is no generic “launch” API; mount often causes the system to
   start the title.
4. Confirm klog: `createApp PPSA99455`, `EXEC /app0/eboot.bin`.
5. Close the title fully before replacing `eboot.bin`.

## What not to do

- Do not send `eboot.bin` to port 9021.
- Do not `go build` movian-go for PS5 and expect it to boot.
- Do not treat `archive/ps5-boilerplate-stubs` or `archive/native-stubs`
  as implemented FFmpeg/BitTorrent/plugins.
- Do not use Sony’s proprietary SDK. Public payload SDK only.
- Do not collide title IDs: Movian is `PPSA99455`. Hello PS5 is
  `PPSA99901`. VLC is `PPSA85300`.

## Next engineering slice (Phase 1 → 2)

See [PORTING_PLAN.md](PORTING_PLAN.md). Videos already lists
`/download0`, `/data/movian`, and `/mnt/usb0` (message row if the
sandbox hides USB). Cross opens a folder or selects a file; there is
**no decoder**. Next: Phase 2 software playback of MPEG-1 / H.264 + AAC.

Keep the shell launching after every slice. When a feature’s intended
behaviour is unclear, run `wsl -d Ubuntu-24.04 -- showtime` and/or read
[M7_CAPABILITIES.md](M7_CAPABILITIES.md) instead of guessing.

## GitHub Actions / Pages

Workflows live in `.github/workflows/` (`ci.yml`, `pages.yml`). Example
copies remain at [`docs/ci.yml.example`](ci.yml.example) and
[`docs/pages.yml.example`](pages.yml.example). The project page source is
[`website/index.html`](../website/index.html); Pages is deployed by the
`pages` workflow once GitHub Pages is enabled for this repository.
