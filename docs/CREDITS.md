# Credits and references

Movian for PS5 is possible because other people published working code,
headers, and loaders. This page is the canonical attribution list.

## Movian

| Work | People | How we use it |
| --- | --- | --- |
| [andoma/movian](https://github.com/andoma/movian) (Showtime) | Andreas Öman | Original media center, flat skin, plugin model |
| M7 7.0.273 | Dean Kasabow — [apps.movian.eu](https://apps.movian.eu/) | PS3 reference player this port follows |
| [czz/movian-go](https://github.com/czz/movian-go) | czz | Idiomatic Go rewrite: plugin API v2, BitTorrent backend, settings, USER_GUIDE. **Behaviour reference, not the PS5 binary** |
| Alternate plugin feed | community packagers | `plugins/repo.json`, advertised as https://bit.ly/mprepo |

Code we copied or adapted from movian-go for a future native host must
keep GPL notices in the file header and a pointer here.

## Native PS5 titles (the packaging we copy)

| Work | People | How we use it |
| --- | --- | --- |
| [ZiZc3/VLC](https://github.com/ZiZc3/VLC) | ZiZc3, with [ryancallstrom](https://github.com/ryancallstrom) and others | Working native title: `ps5/build-title.sh`, `platform_ps5.c`, static plugins, Media `param.json`, Helper.elf sandbox lift, hardware decode via `ps5vdec.c` |
| [VideoLAN / VLC](https://www.videolan.org/) | VideoLAN contributors | Upstream player VLC-for-PS5 ports; we do **not** ship libvlc |
| [FFmpeg](https://ffmpeg.org/) | FFmpeg developers | Decode stack for Phase 2 (not linked yet) |

VLC sources are **not** vendored. We copy *process*: public SDK → signed
`eboot.bin` → `/data/homebrew/<TITLE_ID>` → ShadowMountPlus.

## Toolchain and loaders

| Work | People | Role |
| --- | --- | --- |
| [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk) | John Törnblom | Headers, `prospero-clang` / lld wrappers, libc++ |
| [ps5-payload-dev/pacbrew-repo](https://github.com/ps5-payload-dev/pacbrew-repo) | ps5-payload-dev | Optional static ports (FFmpeg etc.) later |
| [ps5-payload-dev/elfldr](https://github.com/ps5-payload-dev/elfldr) | ps5-payload-dev | Port 9021 payload loader (not used by the title) |
| [ps5-payload-dev/ftpsrv](https://github.com/ps5-payload-dev/ftpsrv) | ps5-payload-dev | FTP 2121 |
| [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) | BlackBearReloaded | FSELF writer, `libc.prx`, folder / ffpkg / ffpfsc |
| [ps5-homebrew-starter](https://github.com/MiladRTX/ps5-homebrew-starter) | MiladRTX | Verified Hello PS5 title, kit vendor, DualSense, fonts, lists |
| [Breeze](https://github.com/MiladRTX/Breeze) | MiladRTX | Renderer lineage used by the starter |
| [ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus) | drakmor | Home-screen icons from `/data/homebrew` |
| [etaHEN](https://github.com/LightningMods/etaHEN) | LightningMods | Jailbreak runtime, toolbox |
| [kstuff](https://github.com/EchoStretch/kstuff) | EchoStretch | Kernel patches titles need |
| [Payload Manager](https://github.com/itsPLK/ps5-payload-manager) | itsPLK | Web UI to start payloads |
| [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) | Mihawk-99 | RADV / Vulkan on PS5 — optional later UI/video path |
| [LibProsperoPKG](https://github.com/SvenGDK) / UFS2Tool | SvenGDK | Optional ffpkg (not Phase 0) |
| [MkPFS](https://github.com/PSBrew/MkPFS) | PSBrew | Optional ffpfsc (not Phase 0) |

## Libraries already in `app/src`

| Work | License | File |
| --- | --- | --- |
| stb_truetype / stb_image (Sean Barrett) | public domain / MIT | `app/src/stb_*.h` |
| Inter (rsms) | OFL-1.1 | `app/fonts/Inter-*` |
| JetBrains Mono | OFL-1.1 | `app/fonts/JetBrainsMono-*` |
| CPU compositor + DualSense | GPL-3.0-or-later | copied from the starter, namespaced `movian::` |

## Analysis tools on this workstation (not shipped)

| Work | People | Role |
| --- | --- | --- |
| [REA](https://github.com/morluto/rea) | morluto | Reverse engineering MCP (Ghidra / JADX) |
| Ghidra 12.1.4 | NSA | Native analysis |
| JADX 1.5.6 | skylot | Android (historical M7 JNI) |
| DeepSeek Harness | DeepSeek | Agent environment used to drive this port |

## House rules for new code

1. If you paste more than a trivial snippet from VLC, movian-go, splicer,
   or the starter, name the file and the project in the header.
2. Do not add a library to README feature tables until `app/build.sh`
   links it.
3. Sony trademarks stay attributed as unaffiliated.
