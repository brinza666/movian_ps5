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
8cc2fbbe6b9459992333264b9065e17358ab0c4a84a3017b2fa3f8b45ea869ff
```

## What it does

The screen matches the preview's four places: Home, Videos, BitTorrent, Plugins. Up and Down move. On Home, Up and Down also move between USB 0, USB 1 and Data. Cross opens the highlighted row. Cross on a file plays it full frame. Circle goes back. Options opens settings. L1 scans again.

Videos is every video file the scan kept. BitTorrent opens Sintel and any `.torrent` files on USB or in data. Plugins opens the catalog. With no settings file yet, that address is `https://bit.ly/mprepo`. An empty saved address is the official repository. On the repository row, Right moves to the next address.

A line at the bottom says what each directory did. `usb0 4` is four videos. `usb0 gone` means that path is not there. `usb0 denied` means the sandbox is still closed. `usb0 listed 12` means the directory opened and nothing in it had a video name. `ftp 3` means etaHEN's FTP server listed files the sandbox itself could not. `ftp off` means nothing is listening on 127.0.0.1 ports 2121, 1337, 2122 or 21. `saw name` is the first name it read.

ShadowMountPlus mounts this folder as the application and scans USB for other homebrew folders. It does not give this title the console's USB mounts. etaHEN makes `/data` listable. USB is read directly when the sandbox allows it (`/mnt/usb0` through `/mnt/usb3`, `/mnt/ext0`, `/mnt/ext1`). If USB is still denied, Movian asks etaHEN's FTP server on the console and plays those files from there. Turn FTP on in etaHEN if the line says `ftp off`. Lapy, loaded before launch, is the other way to open USB directly. The title asks etaHEN (`/download0/etahen_jailbreak`, quoted pid) and Lapy (`/download0/elevate_proc`, unquoted pid) on startup. Directories are read with `getdents` and a buffer of at least 64 KiB.

Playback covers MPEG-1, H.264, HEVC, MPEG-4, MPEG-2, MJPEG, WMV, VC-1 and VP8 in MP4, MKV, MPEG-TS, AVI, WMV, ASF, FLV and MPEG. Pictures above 1080p are scaled down in software. Hardware decode is libSceVideodec2 for H.264 and HEVC when that module loads. A file that still will not open stays on the list and the status line says why (`no supported video`, `cannot open`, `picture format`). Packages under `plugins/` on USB, `/data/movian/plugins` or `/download0/movian/plugins` are listed with the same directory read. L2 opens the ones already on disk.

The loader build is still [payload.elf](ps5-splicer/README.md) on port 9021. This folder is the one that appears on the home screen.
