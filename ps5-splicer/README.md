# SPLICER

Native PS5 video combiner. Two pictures, one scanout. No emulator.

`payload.elf` is a PIE x86-64 ELF for an ELF loader you already run
(ps5-payload-elfldr / websrv / jar loader, TCP 9021). It is not an fpkg.
An fpkg has to be signed with Sony's keys. This does not do that.
The loader maps this ELF and jumps to it on the console CPU.

Built with ps5-payload-sdk 0.45 crt and import stubs (`x86_64-sie-ps5`).
This particular ELF was linked with host gcc, because the SDK's
prospero-clang wrapper had no host LLVM here. Scanout is `sceVideoOut`
direct memory, pixel format `A8B8G8R8` (`0x8000000022000000`).
MPEG-1 is pl_mpeg. H.264 and HEVC are demuxed with FFmpeg 7.1.
The picture is decoded with libSceVideodec2 when that module loads,
and with FFmpeg on the CPU when it does not.

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
f0b436bb69864c2e04328f146cb8e0e77b3877b0ca2d0de376eae4a1711e5e2a
```

The previous MPEG-1-only ELF is kept as `payload-mpeg1.elf`
(sha256 `9b62550330fdf1162f0a6b031f67ddcb1eadfc3e0f21f322fdd48d0db8d23d64`)
if the new file does not show the color bars.

The first thing on screen is three seconds of full-frame color bars
(red, green, blue, white, amber) with the word SPLICER counting up.
That happens before USB is scanned and before pad or audio are opened.
A toast says `SPLICER payload started`, then `SPLICER on screen`.

If you do not see those bars, this ELF is not running. Check that the
loader is actually waiting on port 9021 and that the file you sent is
this `payload.elf`. A black screen with no toast means the bytes never
started, not that the combiner drew black.

`main` does not call `kernel_set_proc_rootdir`. SDK startup still runs
`__kernel_init` before `main`. In this file, firmware 4.03 is the
version word `0x4030000`, and that word takes the same offset block as
4.02, 4.50 and 4.51. A version the table does not know fails before the
first frame. The entry point is `_start`. The older loader that looks
for `payload_main` on port 9020 will not run this file. Send it to the
ELF loader on port 9021.

After the bars it loads the hardware decoder, scans USB, and combines
two buses. A toast says `SPLICER hardware decoder` if libSceVideodec2
opened, or `SPLICER software decoder` if it did not. An amber border
stays on the picture so the output cannot collapse to a black frame.

Pad and audio are loaded with `dlopen` after the bars. If either
library refuses to open, the picture keeps running.

There is no `kernel_set_proc_rootdir` call in `main`. An earlier build
called that before the first flip, and it linked pad, audio, and pthread
as required libraries. A fault in any of that exited before a single
frame. That build is not this file. Startup still runs the SDK's
`__kernel_init`, and this binary's table includes 4.03.

## What it plays

REA is not a decoder. Neither are etaHEN, kstuff, the package manager,
or ShadowMountPlus. Those four are the environment already running on
the console. This file uses the module-loading calls they already
import (`sceKernelLoadStartModule`, `sceKernelDlsym`,
`sceSysmoduleLoadModule` / `sceSysmoduleLoadModuleInternal`) to open
sysmodule 207, libSceVideodec2. The call is resolved with `dlsym` after
the color bars, so a missing module does not stop the first frame.
Details of what REA measured are in [PAYLOADS.md](PAYLOADS.md).

MPEG-1, via pl_mpeg (`plm_decode`):

- `.mpg` `.mpeg` `.mpv` `.m1v` `.m2v`
- MPEG-1 Layer II audio is played

H.264 and HEVC. Demux is FFmpeg. The picture is libSceVideodec2 when
the probe succeeds, otherwise `avcodec_send_packet`.

- `.mp4` `.m4v` `.mov` `.mkv` `.ts` `.m2ts` `.mts`
- hardware: 8-bit H.264, 8-bit and 10-bit HEVC, 4:2:0, up to 3840×2176
- software fallback: same codecs, up to 1920×1088
- the HUD marks the bus `HW` or `SW`
- picture only on those containers. Their audio is not decoded
- a Matroska file with zlib-compressed headers will not open
- VP9, AV1 and AVI are not opened
- this ELF was not booted on a console here. Scanout stays `sceVideoOut`.
  If the hardware decoder refuses the process, the toast says software
  and 1080p still plays through FFmpeg.

Paths: `/mnt/usb0`, `/mnt/usb1`, `/data/video`, `/data/splicer`,
`/user/splicer`, and files directly in `/data`.

Optional `/mnt/usb0/splicer.txt`:

```
a=/mnt/usb0/left.mpg
b=/mnt/usb0/right.mpg
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

## Chrome / NaCl

The same M7 version also ships as a Chrome packaged app. REA's read of that
package, and what it could not open, is in [CHROME.md](CHROME.md).

## License

Payload sources: GPL-3.0-or-later (derived from the hbldr scanout sample,
Copyright (C) 2023 John Törnblom).
pl_mpeg: MIT, Dominic Szablewski.
FFmpeg 7.1 libraries linked into payload.elf: LGPL-2.1-or-later.
font8x8: public domain, Daniel Hepper.
