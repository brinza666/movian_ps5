# Install

Movian as a ShadowMountPlus title. Title id `PPSA99455`, content id `UP9000-PPSA99455_00-MOVIAN0000000000`.

The zip contains one folder:

```text
PPSA99455/
  eboot.bin
  sce_module/libc.prx
  sce_sys/param.json
  sce_sys/icon0.png
```

That folder is the application. It is not the media library. Do not send `eboot.bin` to port 9021.

## What has to be running first

- kstuff
- etaHEN, or Lapy
- ShadowMountPlus

Firmware 4.03 is the one this port is aimed at. A USB stick, if you use one, should be exFAT or FAT32.

You do not change a ShadowMount setting to unlock `/data/homebrew/PPSA99455`. Leave that folder as it was extracted. Settings the title writes go to `/download0/movian-settings.txt`.

## Steps

1. Load kstuff, etaHEN and ShadowMountPlus the way you already do.
2. Download [PPSA99455.zip](https://github.com/brinza666/movian_ps5/releases/download/shadowmount/PPSA99455.zip).
3. Close Movian if it is on screen. Replacing the folder while it is running does not update the copy that is mounted.
4. Extract the zip and copy the `PPSA99455` folder to `/data/homebrew/`, so the file is `/data/homebrew/PPSA99455/eboot.bin`.
5. Restart ShadowMountPlus, or the console, and wait until the home screen shows Movian.
6. Start Movian from the home screen.

Put videos on the USB stick: the root, `video/`, `usb/video/`, or `data/video/`. Details: [USB and files](usb.md).

The picture is 1080p. The first moment is color bars, then the Movian home. How to move around: [Using Movian](manual.md).

## Checksum

sha256 of `PPSA99455.zip`:

```text
8cc2fbbe6b9459992333264b9065e17358ab0c4a84a3017b2fa3f8b45ea869ff
```

The loader ELF is a separate download. See [Loader ELF](payload.md).
