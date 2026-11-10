# Movian for PlayStation 5

<p align="center">
  <img src="app/sce_sys/icon0.png" alt="Movian" width="96">
</p>

<p align="center">
  <b>A native PS5 port of the Movian media center.</b><br>
  Built as a ShadowMountPlus home-screen title, the same way
  <a href="https://github.com/ZiZc3/VLC">VLC for PS5</a> ships.
</p>

<p align="center">
  <img alt="status" src="https://img.shields.io/badge/status-phase%200%20shell-yellow">
  <img alt="title" src="https://img.shields.io/badge/title-PPSA99455-1f1f1f">
  <img alt="license" src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue">
</p>

Movian (formerly Showtime) is Andreas Öman’s media center, continued as
Dean Kasabow’s **M7** on PS3 and as [czz/movian-go](https://github.com/czz/movian-go)
on desktop. This repository ports that product to a jailbroken PS5 as a
**native C++20 application**, not as a Go binary and not as a one-shot ELF
sent to port 9021.

> **Honesty.** Earlier trees in this repository claimed FFmpeg, BitTorrent,
> Duktape plugins, and SMB2 were already working. Those sources were stubs,
> a Go overlay that dies before `main` on this userland, or a separate
> loader experiment. They are kept under [`archive/`](archive/README.md) as
> reference only. The live application is [`app/`](app/).

## What works today (Phase 0)

| Capability | Status |
| --- | --- |
| ShadowMountPlus title `PPSA99455` (Media category) | In tree — build with `app/build.sh` |
| DualSense navigation, 1080p CPU UI | Implemented |
| Four Movian places: Home, Videos, BitTorrent, Plugins | Shell only |
| Video / audio playback | **Not in this binary** (Phase 1–2) |
| BitTorrent client | **Not in this binary** (Phase 3) |
| Plugin host (Movian API v2) | **Not in this binary** (Phase 4) |
| Go `movian-go` ELF | Abandoned for PS5 (see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)) |

Product behaviour we are aiming at is documented from a **live** M7 7.0.273
WSL install in [docs/M7_CAPABILITIES.md](docs/M7_CAPABILITIES.md) and
[docs/PRODUCT.md](docs/PRODUCT.md). The engineering plan is
[docs/PORTING_PLAN.md](docs/PORTING_PLAN.md). Run the reference with
`wsl -d Ubuntu-24.04 -- showtime`.

## Why we copy VLC, not Go

[VLC for PS5](https://github.com/ZiZc3/VLC) is a working native title:

1. Cross-compile C/C++ with [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk).
2. Link a PIE ELF, convert + sign to `eboot.bin` with the
   [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)
   toolchain (VLC uses the same converter via [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan)).
3. Ship `dist/PPSA…/eboot.bin` + `sce_sys/` + `sce_module/libc.prx`.
4. Copy the folder to `/data/homebrew/<TITLE_ID>/` and launch from the home screen.

A stock Go `freebsd/amd64` binary issues FreeBSD syscalls (`SYSCALL` with
FreeBSD numbers). The PS5 title userland does not service that table, so
the process dies before `main`. That is why
[`archive/go-overlay/`](archive/go-overlay) is not the shipping path.

Movian-go remains the **behaviour and plugin-API reference**. We reimplement
its features in native code, the same way VLC reimplemented libvlc on this
platform instead of running the desktop binary.

## Build

On **WSL2 Ubuntu 24.04** (this workstation is already provisioned):

```bash
cd /mnt/c/Users/brinza/Documents/deep/movian_ps5
bash scripts/doctor.sh
bash app/build.sh                          # -> build/app/PPSA99455/
bash app/build.sh deploy 192.168.10.238    # FTP to /data/homebrew
```

Details: [docs/BUILDING.md](docs/BUILDING.md) · [docs/DEPLOY.md](docs/DEPLOY.md).

New contributors and new AI sessions should start at
[docs/NEW_SESSION.md](docs/NEW_SESSION.md).

## Repository layout

```text
app/                  Live native title (sources, param.json, fonts, icon)
scripts/              doctor, deploy, icon helper
docs/                 Architecture, plan, manuals, new-session handoff
website/              Project page (GitHub Pages)
plugins/repo.json     Alternative Movian plugin feed (https://bit.ly/mprepo)
archive/              Frozen experiments — do not build these for shipping
.agents/skills/       Agent skill for this port
```

## Credits

This port stands on public homebrew work. Full attributions:
[docs/CREDITS.md](docs/CREDITS.md).

| Project | Authors | Role here |
| --- | --- | --- |
| [Movian / Showtime](https://github.com/andoma/movian) | Andreas Öman | Original media center |
| M7 | Dean Kasabow / [apps.movian.eu](https://apps.movian.eu/) | PS3 reference |
| [movian-go](https://github.com/czz/movian-go) | czz | Behaviour, plugin API v2, architecture reference |
| [VLC for PS5](https://github.com/ZiZc3/VLC) | ZiZc3 | Native title recipe we follow |
| [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) | BlackBearReloaded | FSELF, `libc.prx`, folder packaging |
| [ps5-homebrew-starter](https://github.com/MiladRTX/ps5-homebrew-starter) | MiladRTX | Verified VideoOut renderer, DualSense, fonts |
| [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk) | John Törnblom | Public headers, Clang target, libc++ |
| [ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus) | drakmor | Home-screen titles from `/data/homebrew` |
| [etaHEN](https://github.com/LightningMods/etaHEN) | LightningMods | Jailbreak runtime |
| [kstuff](https://github.com/EchoStretch/kstuff) | EchoStretch | Kernel patches titles need |
| [Breeze](https://github.com/MiladRTX/Breeze) | MiladRTX | Renderer lineage used by the starter |
| stb | Sean Barrett | `stb_truetype`, `stb_image` |
| Inter / JetBrains Mono | rsms / JetBrains | UI fonts |

PlayStation and PS5 are trademarks of Sony Interactive Entertainment.
This project is independent and is not affiliated with Sony.

## License

Repository-authored code is **GPL-3.0-or-later**. Upstream components keep
their own licenses (see [NOTICE.md](NOTICE.md) and `app/fonts/`).
