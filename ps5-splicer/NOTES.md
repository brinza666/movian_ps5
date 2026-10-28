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

- This binary's decoder is pl_mpeg, so it opens MPEG-1. That was a library
  choice, not a console limit. VLC for PS5, EVO Player, PS-Play and Kodi
  already decode H.264, HEVC, VP9 and ordinary containers through FFmpeg.
  movian-go's pipeline is FFmpeg 9 as well. Linking that is the next native
  step. Do not describe MPEG-1 as the only format the machine can show.
- Not an fpkg. No Sony signature.
- Linked here, not booted on a console. The bars are the test that the
  ELF actually started.
- Toolchain: ps5-payload-sdk 0.45, host clang/lld 18.1.8, target
  `x86_64-sie-ps5`. The SDK is not vendored in this repo.
- Entry is crt `_start`, same as `samples/hello_world`.
- A Go overlay of movian-go does not boot yet. Stock Go issues FreeBSD
  syscalls; Orbis userland does not service those numbers, so a Go ELF dies
  in the runtime before `main`. The media code past that point is already
  the FFmpeg pipeline. The syscall patch and the codec list are different
  problems.

## REA layout

`REA.md` is a static layout of this same `payload.elf` (same sha256),
taken with REA and not by running the file. It confirms the notes above:

- `DT_NEEDED` is video out, system service, `libkernel_web`, libc, and net.
  Pad and audio are pointer slots (`p_scePad*`, `p_sceAudioOut*`) filled by
  this binary's own `dlopen` at `0x5a40`. That matches "bars first, then
  dlopen". A missing pad sprx cannot reject the map.
- The only decode symbols are `plm_*` from `mpeg_dec.c`, entered through
  `plm_create_with_filename`. There is no FFmpeg import. Do not treat a
  future MP4 as a relink that already happened.
- The string `libSceVdecShevc.native.sprx` is in the SDK `sysmodtab`, not
  in `DT_NEEDED`. It is not a decoder this ELF calls.

## Next improvements, in order

1. Confirm on a real console that the bars appear. If they do not, the
   problem is the loader path, not the combiner.
2. If bars appear and USB files do not, the payload process cannot see
   `/mnt/usb0` without a root vnode. Do that only after the bars, and
   only on a firmware the SDK offset table names.
3. H.264 needs a software decoder linked in-process, or a sprx that
   actually exports an AvPlayer the payload is allowed to call. Do not
   claim hardware decode until a frame from an MP4 is on the scanout.
   The replacement for `plm_create_with_filename` is the FFmpeg pipeline
   already described in `movian-ps5-port`.
