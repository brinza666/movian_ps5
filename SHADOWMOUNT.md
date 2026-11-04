# Home screen

Movian as a ShadowMountPlus title. Title id `PPSA99455`, name Movian.

The folder is a native-app `eboot.bin`, `sce_module/libc.prx`, and `sce_sys/param.json`. It is not the loader ELF. Do not send `eboot.bin` to port 9021.

## What you need

- etaHEN, or Lapy, loaded before Movian starts
- kstuff
- ShadowMountPlus
- A USB drive formatted exFAT or FAT32, if the files are on USB

Firmware 4.03 is the one this port is aimed at.

You do not change a ShadowMount setting to edit `/data/homebrew/PPSA99455`. That folder is the application. It is not the media library, and it is not where Movian stores settings. Leave the folder as it was extracted. Put videos on the USB stick: the root, `video/`, `usb/video/`, or `data/video/`. Settings the title writes go to `/download0/movian-settings.txt`.

## Install

1. Load etaHEN, kstuff and ShadowMountPlus the way you already do.
2. Download [PPSA99455.zip](https://github.com/brinza666/movian_ps5/releases/download/shadowmount/PPSA99455.zip) from the `shadowmount` release.
3. Close Movian if it is running. Extract the zip and copy the `PPSA99455` folder to `/data/homebrew/`, so the file is `/data/homebrew/PPSA99455/eboot.bin`.
4. Restart ShadowMountPlus, or the console, and wait until the home screen shows Movian.
5. Start Movian from the home screen.

The picture is 1080p and tiled. The first moment is color bars, then the Movian home.

sha256 of `PPSA99455.zip`:

```
4fbcef9fcbefdc6490b1dc48a87d28a5a4a842b9d59fe38b069d7b2204386f03
```

## What it does

The home screen is six tiles: USB 0, USB 1, Data, Videos, BitTorrent, Plugins. Cross opens a tile. Cross on a file plays it full frame. Circle goes back. Options opens settings. L1 scans again.

Plugins opens the catalog. With no settings file yet, that address is `https://bit.ly/mprepo`. An empty saved address is the official repository. On the repository row, Right moves to the next address.

A line at the bottom says what each directory did. `usb0 4` is four videos. `usb0 gone` means that path is not there. `usb0 denied` means the sandbox is still closed. `usb0 listed 12` means the directory opened and nothing in it had a video name. `saw name` is the first name it read.

On startup the title asks etaHEN (`/download0/etahen_jailbreak`, quoted pid) and Lapy (`/download0/elevate_proc`, unquoted pid) to open the sandbox. The daemon has to already be running. Directories are read with `getdents` and a buffer of at least 64 KiB. The system directory call returns nothing on these filesystems when the buffer is smaller than one block.

Playback is the same player as `payload.elf`: MPEG-1 with audio, and H.264 or HEVC in MP4, MKV and MPEG-TS. The names it also lists are AVI, WebM, WMV, FLV and VOB. Hardware decode is libSceVideodec2 when that module loads, otherwise FFmpeg. Packages under `plugins/` on USB, `/data/movian/plugins` or `/download0/movian/plugins` are listed with the same directory read. L2 opens the ones already on disk.

The loader build is still [payload.elf](ps5-splicer/README.md) on port 9021. This folder is the one that appears on the home screen.
