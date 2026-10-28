# Notes for the next pass

## What failed in the first ELF

The first linked `payload.elf` could show nothing, and that was a real
bug, not a "try it again" problem.

Before the first `sceVideoOutSubmitFlip` it did three things that can
kill a payload process with no picture:

1. `kernel_set_proc_rootdir(pid, kernel_get_root_vnode())` from the SDK
   crt. If the offset table does not match the running firmware, that
   faults in `main` and the process is gone. No flip, easy to miss the
   one notification.
2. It walked USB and opened decoders before any pixel was submitted.
   A stuck `opendir` or a bad file meant the screen never opened.
3. It NEEDED `libScePad`, `libSceAudioOut`, `libSceUserService`, and
   unresolved `pthread_*`. If the loader refuses any of those at map
   time, it never calls `main`.

The picture was also near-black (`#12130f`) with a thin highlight, so a
successful flip could still look like a dead TV.

## What this tree does instead

`main` only hides the splash, sends one notification, and enters the
scanout loop copied from the public hbldr `test.c` path.

The loop allocates direct memory in falling sizes (128, 64, 512, 32 MB).
If video out cannot be opened it does not return; it notifies every
two seconds so a dead return cannot be mistaken for a hang.

The first 180 flips are full-frame color bars plus a frame counter.
Only after those flips does it scan `/mnt/usb0` and `/mnt/usb1`, then
`dlopen` pad and audio. Those two are not in `DT_NEEDED`.

Live buses are saturated amber and green, not black, and every combiner
frame has an amber border.

## Still true about this ELF

- Current `payload.elf` is the file described in `README.md`, not the first MPEG-1 build. MPEG-1 is still pl_mpeg. H.264 and HEVC are FFmpeg demux plus libSceVideodec2 when that module loads, otherwise FFmpeg on the CPU. Audio for those containers is FFmpeg (AAC through PCM, including DTS, TrueHD, Opus and FLAC), mixed to the same 48 kHz output. zlib header compression in Matroska is inflated.
- The MPEG-1-only ELF is kept as `payload-mpeg1.elf`. `REA.md` is the layout of that older file, sha256 `9b62550330fdf1162f0a6b031f67ddcb1eadfc3e0f21f322fdd48d0db8d23d64`. It is not a layout of the current payload.
- REA 6.8.0 has no video decoder. Playback is not sent through `rea`.
- Not an fpkg. No Sony signature. No new kernel patch.
- Linked here with host gcc and the ps5-payload-sdk 0.45 crt, not booted on a console. The bars are the test that the ELF actually started.
- A Go overlay of movian-go does not boot yet. Stock Go issues FreeBSD syscalls; Orbis userland does not service those numbers, so a Go ELF dies in the runtime before `main`.

## REA layout

`REA.md` is a static layout of `payload-mpeg1.elf`, not of the current `payload.elf`. Do not read its "no FFmpeg import" line as a description of the file in this directory now.

## Next check

1. Confirm on a real console that the bars appear, then that the toast says hardware or software.
2. If bars appear and USB files do not, the payload process cannot see `/mnt/usb0` without the mounts kstuff and ShadowMount already provide. Do not add a kernel patch for that.
3. A picture from an MP4 or MKV on that console is still the missing test. This tree links the decoders. It has not shown a frame from them on hardware.
