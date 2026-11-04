# Loader ELF

`payload.elf` is not the home-screen title. Send it to an ELF loader that is already running, TCP port 9021. Do not install it as a package. Do not send `eboot.bin` from the ShadowMount zip to this port.

The file to launch from the console home screen is [PPSA99455.zip](install.md).

## Send

etaHEN, or another loader, already loaded:

```sh
nc -q 0 PS5_IP 9021 < payload.elf
```

```sh
prospero-deploy -h PS5_IP -p 9021 payload.elf
```

Download: [release `pre-alpha`](https://github.com/brinza666/movian_ps5/releases/tag/pre-alpha).

sha256 of `ps5-splicer/payload.elf` in this tree:

```text
abfbdbeee968f696c8d6ecfd4b2618659b5fd992719a133b0ac86ba64d5e227a
```

`payload-mpeg1.elf` is the older MPEG-1-only file (sha256 `9b62550330fdf1162f0a6b031f67ddcb1eadfc3e0f21f322fdd48d0db8d23d64`). It is not a build for another firmware.

A payload sent to port 9021 can open the display and still leave the console home screen on the television. The ShadowMount title is the one that replaces that screen.

## 4.03 test files

[ps5-splicer/payload-403/](../ps5-splicer/payload-403/) and the release `fw403-20261030` are earlier tests, not the current player.

- `splicer-linear.elf` draws linear bars and toasts `LIN`
- `splicer-mpeg1.elf` plays one MPEG-1 file and toasts `M1`

Send one of those ELFs to port 9021. Do not install them.

## What the current ELF file is

The `payload.elf` checked into this tree is the previous loader build. Sources under `ps5-splicer/src/` are ahead of that file: the home-screen title is built from those sources with `MOVIAN_TITLE`. Rebuilding `payload.elf` is a separate step and is not what the `shadowmount` release contains.

Builder notes, codecs as linked into the older ELF, and the pad for the two-bus splicer: [ps5-splicer/README.md](../ps5-splicer/README.md).

## Rebuild the loader

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
export PATH="$PS5_PAYLOAD_SDK/bin:$PATH"
make
```

Run that from `ps5-splicer/`. The ShadowMount folder is not produced by that Makefile.
