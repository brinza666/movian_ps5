# SPLICER

Native PS5 video combiner. Two pictures, one scanout. No emulator.

`payload.elf` is a PIE x86-64 ELF for an ELF loader you already run
(ps5-payload-elfldr / websrv / jar loader, TCP 9021). It is not an fpkg.
An fpkg has to be signed with Sony's keys. This does not do that.
The loader maps this ELF and jumps to it on the console CPU.

Built with ps5-payload-sdk 0.45 (`x86_64-sie-ps5`, prospero-clang).
Scanout is `sceVideoOut` direct memory, pixel format `A8B8G8R8`
(`0x8000000022000000`), the same path as the public hbldr sample.
Decode is in-process MPEG-1 (pl_mpeg). Audio is MPEG-1 Layer II,
resampled to 48 kHz s16 and written to `sceAudioOut`.

## Send it

```sh
nc -q 0 PS5_IP 9021 < payload.elf
```

or, with the SDK deployer:

```sh
prospero-deploy -h PS5_IP -p 9021 payload.elf
```

SHA-256 of this tree's `payload.elf`:

```
9b62550330fdf1162f0a6b031f67ddcb1eadfc3e0f21f322fdd48d0db8d23d64
```

The first thing on screen is three seconds of full-frame color bars
(red, green, blue, white, amber) with the word SPLICER counting up.
That happens before USB is scanned and before pad or audio are opened.
A toast says `SPLICER payload started`, then `SPLICER on screen`.

If you do not see those bars, this ELF is not running. Check that the
loader is actually waiting on port 9021 and that the file you sent is
this `payload.elf` (sha256 below). A black screen with no toast means
the bytes never started, not that the combiner drew black.

After the bars it scans USB and combines two buses. An amber border
stays on the picture so the output cannot collapse to a black frame.

Pad and audio are loaded with `dlopen` after the bars. If either
library refuses to open, the picture keeps running.

There is no kernel memory write in this payload. An earlier build
called `kernel_set_proc_rootdir` before the first flip, and it linked
pad, audio, and pthread as required libraries. A fault in any of that
exited before a single frame. That build is not this file.

## What it plays

The ELF in this folder decodes with pl_mpeg, so **this binary** opens MPEG-1
program streams and elementary video:

- `/mnt/usb0`, `/mnt/usb1` (a few directories deep)
- `/data/video`, `/data/splicer`, `/user/splicer`, files directly in `/data`
- extensions `.mpg` `.mpeg` `.mpv` `.m1v` `.m2v`

That list is the library linked into this file. It is not what a PS5 can
play, and it is not what Movian plays. Movian and movian-go hand the file to
FFmpeg. Native players already running on jailbroken consoles do the same:
[VLC for PS5](https://github.com/ZiZc3/VLC) (H.264, HEVC and VP9 in hardware,
everything else through FFmpeg, including MKV and MP4), EVO Player, PS-Play,
and Kodi. `libSceAvPlayer` is still absent from the public payload SDK, so
this ELF does not call it. Those players got past MPEG-1 by cross-compiling
FFmpeg and linking it.

A stock Go build of movian-go still will not boot. Its runtime issues
FreeBSD syscall numbers that Orbis does not service, so the process dies
before `main`. That is a toolchain problem. It does not shrink the codec list.
The Go media pipeline is already FFmpeg 9.

Optional `/mnt/usb0/splicer.txt`:

```
a=/mnt/usb0/left.mpg
b=/mnt/usb0/right.mpg
```

MP4, MKV, H.264 and HEVC will not open **in this ELF**, because pl_mpeg is
the only decoder linked here. They already open in VLC, EVO and Movian.
To feed this particular binary, convert on a computer:

```sh
ffmpeg -i input.mp4 -vf scale=960:540 -c:v mpeg1video -b:v 4500k -c:a mp2 -b:a 224k -f mpeg reel.mpg
```

Two files: left/top is the first, right/bottom is the second.
One file: both buses play it, the second starts halfway through.
No files: two generated reels, so the combiner is still on screen.

## Pad

| Button | Action |
| --- | --- |
| Cross | Pause |
| Up / Down | Layout: side by side, top/bottom, picture in picture, wipe, difference, screen |
| L1 / R1 | Previous / next clip on the focused bus |
| Square | Focus bus A or B |
| Circle | Audio A+B, A only, B only |
| Triangle | Swap the two buses |
| Left / Right | Seek the focused bus by 5 seconds |
| Right stick | Move the wipe |
| Options | Rescan USB |
| Hold Options | Quit back to the loader |

## Rebuild

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
export PATH="$PS5_PAYLOAD_SDK/bin:$PATH"   # needs host clang/lld 18+
make
```

## License

Payload sources: GPL-3.0-or-later (derived from the hbldr scanout sample,
Copyright (C) 2023 John Törnblom).
pl_mpeg: MIT, Dominic Szablewski.
font8x8: public domain, Daniel Hepper.
