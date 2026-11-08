# USB, data, and where files go

## Where to put videos

On the USB stick, exFAT or FAT32:

- the root of the stick
- `video/`
- `usb/video/`
- `data/video/`

Also under `/data` on the console, once that directory can be listed.

Do not put videos in `/data/homebrew/PPSA99455`. That folder is the application. ShadowMount mounts it as the title. Replacing files inside it does not add them to the library, and editing `eboot.bin` there does not change a title that is already running.

Settings are `/download0/movian-settings.txt`, not a file inside the title folder.

## What ShadowMount does with USB

ShadowMountPlus scans internal storage and every USB port for other homebrew folders:

- `/data/homebrew`
- `/data/etaHEN/games`
- `/mnt/usb0/homebrew` through `/mnt/usb7/homebrew`
- `/mnt/usb0/etaHEN/games` through `/mnt/usb7/etaHEN/games`
- `/mnt/ext0` and `/mnt/ext1`, the same two names

It mounts those folders as titles. It does not give Movian the console's USB mounts. Seeing a drive in ShadowMount's own screens is not the same as Movian being able to list it.

## What the title does

Movian asks for that only after the picture is on screen and the pad, audio and video modules have loaded. Kodi does the same: a request any earlier loses the race, and once the sandbox is open a system module will not load. The request is the file Kodi writes, `{"PID":"<pid>"}` in `/download0/etahen_jailbreak`. Lapy also reads `/download0/elevate_proc`. The title waits until the etaHEN file is removed, then reads the drives with `getdents` and a buffer of at least 64 KiB:

- `/mnt/usb0` through `/mnt/usb7`
- `/mnt/ext0` and `/mnt/ext1`
- `/data`
- `/mnt/shadowmnt`
- `/user`

If the file was taken and USB is still hidden, it asks once more, then checks again every two seconds. etaHEN's FTP on `127.0.0.1` (ports 2121, 1337, 2122, then 21) is tried once after that, and plays files from there. Turn FTP on in etaHEN if the line says `ftp off`. Lapy, loaded before Movian starts, is the other way to open USB directly. L1 asks again immediately.

## The bottom line

| Text | Meaning |
| --- | --- |
| `usb0 4` | Four videos on that path |
| `usb0 gone` | The path is not there |
| `usb0 denied` | The sandbox is still closed |
| `usb0 listed 12` | The directory opened. Nothing in it had a video or torrent name |
| `ftp 3` | etaHEN FTP listed files the sandbox itself could not |
| `ftp off` | Nothing is listening on the FTP ports above |
| `saw name` | The first name that was read |

`/data` can be usable while `usb0 denied` is still on the line. That is etaHEN opening data and not USB. Load Lapy before launch, or enable etaHEN FTP.
