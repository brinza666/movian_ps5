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

On startup Movian asks etaHEN and Lapy to open the sandbox:

- etaHEN reads `/download0/etahen_jailbreak` with a quoted pid
- Lapy reads `/download0/elevate_proc` with an unquoted pid

The daemon has to already be loaded. etaHEN makes `/data` listable. USB is still a separate mount.

Movian then reads, with `getdents` and a buffer of at least 64 KiB:

- `/mnt/usb0` through `/mnt/usb3`
- `/mnt/ext0` and `/mnt/ext1`
- `/data`
- `/mnt/shadowmnt`
- `/user`

If USB did not list anything, it asks etaHEN's FTP server on `127.0.0.1`, ports 2121, 1337, 2122, then 21, and plays files from there. Turn FTP on in etaHEN if you want that path. Lapy, loaded before Movian starts, is the other way to open USB directly.

The title keeps asking for about twelve scans after launch. L1 asks again immediately.

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
