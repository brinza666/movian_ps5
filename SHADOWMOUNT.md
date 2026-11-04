# Home screen

Movian as a ShadowMountPlus title. Title id `PPSA99455`, name Movian.

The folder is built the same way as PS5MC: a native-app `eboot.bin`, `sce_module/libc.prx`, and `sce_sys/param.json`. It is not the loader ELF. Do not send `eboot.bin` to port 9021.

This build was not launched on a console here.

## What you need

- etaHEN, or another jailbreak that answers the same sandbox request
- kstuff
- ShadowMountPlus
- A USB drive formatted exFAT or FAT32, if the files are on USB

Firmware 4.03 is the one this port is aimed at. The packaging itself is the native-app folder PS5MC uses, which that project has run on later firmwares as well.

## Install

1. Load etaHEN, kstuff and ShadowMountPlus the way you already do.
2. Download [PPSA99455.zip](https://github.com/brinza666/movian_ps5/releases/download/shadowmount/PPSA99455.zip) from the `shadowmount` release.
3. Extract it and copy the `PPSA99455` folder to `/data/homebrew/`, so the file is `/data/homebrew/PPSA99455/eboot.bin`.
4. Restart ShadowMountPlus, or the console, and wait until the home screen shows Movian.
5. Start Movian from the home screen.

The picture is 1080p. The home-screen scanout is tiled, so the frame is swizzled before it is shown. The first picture is color bars and the word MOVIAN.

sha256 of `PPSA99455.zip`:

```
b9cfc8f1c1ef552adec297571a090e33210d5ee6eb5072fa018eb867c0e8aee5
```

## What it does

The first picture is color bars and the word MOVIAN. After that it scans `/mnt/usb0`, `/mnt/usb1`, `/data/video`, `/data/splicer`, `/user/splicer` and `/data`.

A title starts in a sandbox that hides USB and `/data`. On startup Movian writes `{"PID":"<pid>"}` to `/download0/etahen_jailbreak`, the same request PS5MC uses. etaHEN or the Lapy jailbreak daemon consumes that file and opens the sandbox. If nothing is listed yet, press Options to scan again.

Playback is the same player as `payload.elf`: MPEG-1 with audio, and H.264 or HEVC in MP4, MKV and MPEG-TS. Hardware decode is libSceVideodec2 when that module loads, otherwise FFmpeg. AAC, AC-3, E-AC-3, MP3, DTS, TrueHD, Opus, FLAC and PCM are mixed to stereo. Plugins in `/mnt/usb0/plugins` (also usb1 and `/data/movian/plugins`) run inside this process. L2 opens them.

The loader build is still [payload.elf](ps5-splicer/README.md) on port 9021. This folder is the one that appears on the home screen.
