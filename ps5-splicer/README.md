# SPLICER

Two pictures, one scanout. `payload.elf` is a PIE x86-64 ELF for a loader that is already running (elfldr, websrv, or the jar loader) on TCP port 9021.

Pre-alpha download: [release `pre-alpha`](https://github.com/brinza666/movian_ps5/releases/tag/pre-alpha).

Built with ps5-payload-sdk 0.45 (`x86_64-sie-ps5`). Scanout is `sceVideoOut`, pixel format `A8B8G8R8`. MPEG-1 is pl_mpeg. H.264 and HEVC are demuxed with FFmpeg 7.1 and decoded with libSceVideodec2 when that module loads, otherwise with FFmpeg. AAC, AC-3, E-AC-3, MP3, DTS, TrueHD, Opus, FLAC and PCM play through sceAudioOut.

## Send it

```sh
nc -q 0 PS5_IP 9021 < payload.elf
```

```sh
prospero-deploy -h PS5_IP -p 9021 payload.elf
```

SHA-256:

```
abfbdbeee968f696c8d6ecfd4b2618659b5fd992719a133b0ac86ba64d5e227a
```

The release also has `payload-mpeg1.elf` (sha256 `9b62550330fdf1162f0a6b031f67ddcb1eadfc3e0f21f322fdd48d0db8d23d64`), the older MPEG-1-only file. It is not a build for another firmware.

The first picture is color bars and the word SPLICER, before USB, pad, or audio. Toasts: `SPLICER payload started`, then `SPLICER on screen`. No bars means the file did not start. Port 9020 will not run this ELF. Entry is `_start`.

Startup reads the firmware and picks a block in this same file. There is no separate binary per firmware. An unknown version stops before the first frame.

1.00, 1.01, 1.02, 1.05, 1.10, 1.11, 1.12, 1.13, 1.14, 2.00, 2.20, 2.25, 2.26, 2.30, 2.50, 2.70, 3.00, 3.10, 3.20, 3.21, 4.00, 4.02, 4.03, 4.50, 4.51, 5.00, 5.02, 5.10, 5.50, 6.00, 6.02, 6.50, 7.00, 7.01, 7.20, 7.40, 7.60, 7.61, 8.00, 8.20, 8.40, 8.60, 9.00, 9.05, 9.20, 9.40, 9.60, 10.00, 10.01, 10.20, 10.40, 10.60, 11.00, 11.20, 11.40, 11.60, 12.00, 12.02, 12.20, 12.40, 12.60, 12.70, 13.00, 13.20, 13.40, 13.42, 13.60, 14.00, 14.10.

4.03 (`0x4030000`) uses the same block as 4.00, 4.02, 4.50 and 4.51.

After the bars the toast says `SPLICER hardware decoder` or `SPLICER software decoder`. Pad and audio open after that. If either library does not open, the picture keeps running.

## What it plays

MPEG-1 (pl_mpeg), with Layer II audio:

- `.mpg` `.mpeg` `.mpv` `.m1v` `.m2v`

H.264 and HEVC (FFmpeg demux, libSceVideodec2 or FFmpeg):

- `.mp4` `.m4v` `.mov` `.mkv` `.ts` `.m2ts` `.mts`
- hardware: 8-bit H.264, 8-bit and 10-bit HEVC, 4:2:0, up to 3840×2176
- software: same codecs, up to 1920×1088
- HUD shows `HW` or `SW`
- AAC, AAC-LATM, AC-3, E-AC-3, MP3, DTS, DTS-HD, TrueHD, Opus, FLAC, integer PCM, float PCM, Blu-ray and DVD LPCM, mixed to 48 kHz stereo. 5.1 and 7.1 fold to stereo. Atmos and DTS:X objects are not rendered past the channel bed
- zlib-compressed Matroska headers are inflated. bzip2-compressed tracks and encrypted files do not open
- a `.torrent`, a `.magnet` file, or `magnet:?xt=urn:btih:...` in `splicer.txt` or on a plugin line starts the built-in client. The largest video plays while the rest is still arriving. HTTP and UDP trackers, plaintext peers. No DHT. Piece cache is `/data/splicer/bt` (or USB `splicer-bt` when `/data` is not writable)
- VP9, AV1 and AVI are not opened

Paths: `/mnt/usb0`, `/mnt/usb1`, `/data/video`, `/data/splicer`, `/user/splicer`, and files in `/data`.

Optional `/mnt/usb0/splicer.txt`:

```
a=/mnt/usb0/left.mpg
b=/mnt/usb0/right.mpg
```

Two files: left/top, then right/bottom. One file: both buses, the second starts halfway. No files: two generated reels.

etaHEN and kstuff are the environment already running. The splicer loads libSceVideodec2 (sysmodule 207) through `sceKernelLoadStartModule`, `sceKernelDlsym` and `sceSysmoduleLoadModule`. See [PAYLOADS.md](PAYLOADS.md).

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
| L2 | Plugins. Packages in `/mnt/usb0/plugins` (also usb1 and `/data/movian/plugins`) |
| Options | Rescan USB |
| Hold Options | Quit back to the loader |

L2 lists `plugin.json` packages. Cross runs that package in this payload (Duktape 2.7, Movian API v2) and opens its first service. Pages are fetched with sceHttp2. Circle goes back. A popup is accepted, so a terms gate does not stop the script. Titles are transliterated for the on-screen font. An `http` address is shown. This FFmpeg build has no HTTP protocol, so that address is not decoded. The payload was not run on a console here.

## Rebuild

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
export PATH="$PS5_PAYLOAD_SDK/bin:$PATH"
make
```

## License

Payload sources: GPL-3.0-or-later (hbldr scanout sample, Copyright (C) 2023 John Törnblom).
pl_mpeg: MIT, Dominic Szablewski.
FFmpeg 7.1: LGPL-2.1-or-later.
font8x8: public domain, Daniel Hepper.
