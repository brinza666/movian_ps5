# Native title (`PPSA99455`)

Sources compiled by `build.sh` through the
[ps5-homebrew-starter](https://github.com/MiladRTX/ps5-homebrew-starter)
copy of [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate).

```text
src/main.cpp          entry
src/ui.cpp            Home / Videos / BitTorrent / Plugins + Options
src/settings.*        /download0 settings, resume, bookmarks
src/fs.*              64 KiB getdents walker, video/audio classify
src/player.*          MPEG-1 software player (pl_mpeg, resume, OSD)
src/avdec.*           FFmpeg H.264/AAC glue (stubs unless MOVIAN_HAVE_FFMPEG)
src/audio.*           48 kHz stereo AudioOut + volume/mute
src/pl_mpeg.h         PL_MPEG (MIT)
src/screen.*          1920×1080 CPU VideoOut (starter / Breeze / boilerplate)
src/pad.*             DualSense
src/font.*            Inter + JetBrains Mono via stb_truetype
sce_sys/param.json    title identity (Media)
sce_sys/icon0.png     512×512
fonts/                OFL fonts
stubs/common_dialog.c link-only IME stub
```

MPEG-1 is always in this binary. FFmpeg objects are linked when
`.deps/ffmpeg-ps5/lib/libavcodec.a` exists (`bash scripts/build-ffmpeg-ps5.sh`).
Do not claim H.264 on the TV until `STATUS.md` says it was observed.
