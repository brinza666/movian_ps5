# movian_ps5

PS5 port notes and the current ELF experiment for Movian. Nothing here was on the remote until this commit. The repository was empty.

This is not a jailbreak and not a signed package. Payloads are for a console that already loads homebrew ELFs.

## What real Movian is

The player people run on PS3 is Dean Kasabow’s M7, currently 7.0.273, from [apps.movian.eu](https://apps.movian.eu/). It is the continuation of Andreas Öman’s Movian (Showtime). The interface is the flat GLW skin in [andoma/movian](https://github.com/andoma/movian) (`glwskins/flat`): Roboto Condensed, accent `#4192ff`, a tiled home of sources, a file list, a right-hand menu, and a play deck. Playback goes through FFmpeg. MKV, MP4, HEVC and the rest are normal files for that player.

`movian-go` ([czz/movian-go](https://github.com/czz/movian-go)) is the same idea in Go, also on FFmpeg 9. A stock Go binary does not boot on PS5: the runtime issues FreeBSD syscall numbers, and Orbis userland does not service them. The process dies before `main`. That is a toolchain problem. It is not a reason to pretend the only format is MPEG-1.

## What is in this tree

- `movian-ps5-port/` — platform overlay for movian-go (build tag `ps5`) and the notes on the libc bridge that has to exist before that ELF survives startup.
- `ps5-splicer/` — a separate scanout experiment. The ELF that is actually linked uses pl_mpeg, so **that binary** opens MPEG-1 only. That was a library choice, not the console and not Movian.

VLC for PS5, EVO Player, PS-Play and Kodi already link FFmpeg (and, for H.264/HEVC, the hardware decoder) on a jailbroken PS5. That is the path for a Movian that plays what the PS3 build plays.

## Not done

- No Go runtime with the FreeBSD syscalls removed, so `movian-go.elf` is not a booting payload yet.
- The splicer ELF has not been relinked against FFmpeg.
- The flat-skin shell used to judge the look is the preview app, not a PS5 binary.
