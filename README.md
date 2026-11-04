# Movian for PS5

PS5 port of Movian. Movian on PS3 is Dean Kasabow’s M7 (7.0.273) from [apps.movian.eu](https://apps.movian.eu/), continued from Andreas Öman’s Movian. The flat skin is the one in [andoma/movian](https://github.com/andoma/movian).

![Home](docs/images/home.jpg)

The picture above is the browser preview. The television build uses the same four places: Home, Videos, BitTorrent, Plugins.

## Download

| Build | What it is | File |
| --- | --- | --- |
| Home screen | ShadowMountPlus title `PPSA99455`. Launch Movian from the console home screen. | [PPSA99455.zip](https://github.com/brinza666/movian_ps5/releases/download/shadowmount/PPSA99455.zip) |
| Loader | `payload.elf` for a loader that is already running on port 9021. Not the home-screen title. | [pre-alpha](https://github.com/brinza666/movian_ps5/releases/tag/pre-alpha) |

Do not send `eboot.bin` to port 9021. Do not put videos inside the `PPSA99455` folder.

Install and the current zip checksum: [docs/install.md](docs/install.md).

## Manuals

- [Install the home-screen title](docs/install.md)
- [Using Movian](docs/manual.md)
- [USB, data, and where files go](docs/usb.md)
- [Plugins](docs/plugins.md) — Russian and English
- [Playback](docs/playback.md)
- [Loader ELF (port 9021)](docs/payload.md)
- [What M7 does, and what this port does](docs/capabilities.md)

## Tree

| Path | Contents |
| --- | --- |
| `docs/` | Manuals and screenshots |
| `plugins/` | Alternative plugin feed. Address `https://bit.ly/mprepo` |
| `ps5-splicer/` | Player sources, and the port-9021 ELF |
| `movian-ps5-port/` | movian-go overlay. It does not boot yet |

## License

[LICENSE](LICENSE). Payload sources are GPL-3.0-or-later.
