# Movian PS5 playback test handoff

Updated 2026-11-21. Release tag `playback-test-v15` and branch
`test/native-playback` track the isolated native test title `PPSA99456`.
The production folder title is `PPSA99455`. Keep the test and production
title IDs separate. Make source changes in this branch when reproducing a
test result.

## Verified and open

v15 keeps the single picture allocation across file opens, synchronizes its
use by the renderer, and drains the AudioOut queue before auto-advance. It
captures FFmpeg diagnostics in `/download0/hevc-test-ffmpeg.log` and selects
the MPEG demuxer for a Program Stream pack header in an `.mpg` or `.mpeg`.
v14 successfully switched among hardware H.264 files and returned from
failed MPEG opens. v15's MPEG replay and the full E-AC-3 feature playback
are awaiting console observations.

| Case | Evidence | Status |
| --- | --- | --- |
| Bundled 6-second 1920×1080/30 HEVC Main fixture, AAC | v10 user saw the clip; console reported 180 hardware frames and clean decoder drain | Pass on the test console |
| H.264 High MKV, 1920×1040 at 23.98 fps | v11 and v12 logged Videodec2 initialization and first decoded hardware frame | Hardware first frame verified; full playback, seek, and EOF still open |
| E-AC-3 5.1 track in the H.264 MKV | v12 logged six input channels, 48,000 Hz output and 1,536 stereo PCM frames | Decode and stereo conversion verified; audible output and uninterrupted playback still open |
| 360×640 H.264 MP4 and bundled HEVC with AAC | v12 logged first hardware frames and PCM blocks | First frame and PCM verified |
| Repeated playback and reopen | Later v12 fixture requests returned Hardware HEVC unavailable after earlier successes | Open; identify failing probe, decoder, or buffer-allocation stage |
| Intermittent SIGSEGV at instruction address zero | v11 kernel trace after a local MPEG clip selection had an empty backtrace | Open; source call site and relationship to the MPEG clip are unknown |

No sandbox-lift code is included. Use the title's current permissions and
the platform's standard module-loading and memory APIs. Support for codecs
outside the static FFmpeg recipe needs its own test. Full Dolby bitstream
passthrough and multichannel AudioOut are outside this stereo build.

## Where to resume

1. Start v15 on the PS5. Cross on Home starts the built-in
   HEVC fixture. To test H.264 plus E-AC-3, go to Videos and open a known
   local MKV with both streams. Confirm that picture and tone/dialogue
   continue past 15 seconds. Seek once, then test a short fixture through EOF.
2. Capture klog starting before launch. Retrieve these title-local files
   while the app is open: `/download0/hevc-test-startup.log`,
   `/download0/hevc-test-probe.log`, `/download0/hevc-test-decode.log`, and
   `/download0/movian-ps5.log`. The app's temporary log directory may
   disappear from FTP after close. Keep evidence associated with the
   executable SHA-256, title version, codec and sample.
3. For the intermittent crash, capture steps separately for first boot,
   HEVC → H.264 switch, the MPEG-1 file, Stop/Back, seek, and Close. Capture
   the faulting PC and backtrace before changing code. Match the executable
   hash to an archived `llvm-pie.elf`. For a title PC based at `0x400000`,
   symbolize the relative address with `llvm-addr2line-18 -f -C -e llvm-pie.elf`.
   A zero instruction address with an empty backtrace gives insufficient
   evidence to name a failing call. Add phase breadcrumbs if it repeats.
4. Reproduce repeated HEVC/H.264 opens and close cycles. Compare the final successful decoder-query and create log with the next failed request. Log release return codes and count owned buffers across close before changing resource budgets.
5. Exercise sample-rate changes and stereo/mono AAC alongside E-AC-3, since
   v12 routes all FFmpeg audio through libswresample. Record the audio sink
   result separately from PCM production. Check decoded frame counts and
   end-of-stream drain for short H.264 and HEVC clips.

## Implementation map

- `app/src/avdec.cpp` owns demux, Annex B packet filtering, FFmpeg audio decode,
  libswresample stereo downmix and rate conversion, hardware/SW video
  selection and worker lifetime. `g_video_bsf` selects FFmpeg's
  `h264_mp4toannexb` or `hevc_mp4toannexb`. The output time base follows the
  filter. EOF drains the filter and decoder, and seek resets both.
- `app/src/hwdec.c` owns Videodec2 configuration, GPU input/output surfaces,
  picture reuse, cropping and hardware drain. Uses public SDK sysmodule
  loading and normal link imports. The video import stub is link-only.
  The title uses HEVC Main/Main10 according to stream bit depth and
  supports eligible 8-bit H.264 profiles with a software fallback at open.
- `app/src/player.cpp` manages playback controls, MPEG-1/PL_MPEG, and presentation.
  Its fallback resampler uses 256-frame stereo batches. A 64 KiB local
  buffer previously overran the FFmpeg decoder thread stack.
- `app/src/audio.cpp` is the 48 kHz stereo AudioOut ring. v12 records first
  decoded stereo PCM, which alone does not establish audible TV output.
- `scripts/build-ffmpeg-ps5.sh` sets the static codec allowlist. A firmware
  decoder or system module goes through normal imports. A software decoder
  must be present in the FFmpeg build before it can be selected.

## Build and provenance

Build in Ubuntu 24.04 WSL with clang/lld 18, public PS5 Payload SDK v0.42, and
[ps5-homebrew-starter at c3f9392](https://github.com/MiladRTX/ps5-homebrew-starter/tree/c3f939230aee18863da1f359e39eb43bf95a056d).
From this branch's root, export `PS5_PAYLOAD_SDK` and `PS5_STARTER` as local
absolute paths. Use a dedicated `PS5_STARTER_HOME` to preserve production
build outputs. Place the included FFmpeg tarball in the selected build
root or let the recipe fetch and verify the pinned FFmpeg 6.1.2 source.

```bash
export PS5_STARTER_HOME="$HOME/.movian-playback-test"
export MOVIAN_FFMPEG_BUILD_ROOT="$PS5_STARTER_HOME/ffmpeg"
bash scripts/build-ffmpeg-ps5.sh
bash app/build.sh
sha256sum build/app/PPSA99456/eboot.bin
```

The packaged v12 eboot hash is
`8bb86ccafcf495a20cebe7ed371b6026ede87a9edca1a2388cf30135c1ffa391`.
The release ZIP installs as the distinct folder `PPSA99456`. Archive the
symbol-bearing `$PS5_STARTER_HOME/kit-work/build/llvm-pie.elf` per release.
Uninstall any old test registration and verify the running title is closed
before replacing a deployed test folder.

Source references reviewed 2026-11-21. See [Kodi PS5](https://github.com/VivaLaVent/kodi-ps5/tree/83a7be649914535d0cb7837eea65f6f7e6eaaa21)
for profile selection, FFmpeg filtering and audio layouts, and
[VLC PS5](https://github.com/ZiZc3/VLC/tree/7f104abb5a88dd675b60261769dc86d5dfb0a0d4)
for title imports, access-unit handling and surface lifetimes.
