# movian_ps5

PS5 port of Movian, and the plugin repository used with it.

Movian on PS3 is Dean Kasabow’s M7 (7.0.273) from [apps.movian.eu](https://apps.movian.eu/), continued from Andreas Öman’s Movian. The flat skin is the one in [andoma/movian](https://github.com/andoma/movian).

## Plugins

How to point Movian at the alternative repository: [PLUGINS.md](PLUGINS.md).

```
https://bit.ly/mprepo
```

The feed itself is [plugins/repo.json](plugins/repo.json).

## Payload

Pre-alpha ELF: [release `pre-alpha`](https://github.com/brinza666/movian_ps5/releases/tag/pre-alpha).

Send `payload.elf` to an ELF loader that is already running, port 9021. Codecs and firmware list: [ps5-splicer/README.md](ps5-splicer/README.md).

## Tree

- `plugins/` — alternative Movian plugin repository.
- `ps5-splicer/` — the ELF that is linked today.
- `movian-ps5-port/` — movian-go overlay. It does not boot yet. Stock Go issues FreeBSD syscalls, and this userland does not service them.

## Limits

- The splicer was not run on a console here.
- bzip2-compressed Matroska tracks and encrypted files do not open.
- Atmos and DTS:X objects are not rendered past the channel bed.
