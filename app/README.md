# Native title (`PPSA99455`)

Sources compiled by `build.sh` through the
[ps5-homebrew-starter](https://github.com/MiladRTX/ps5-homebrew-starter)
copy of [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate).

```text
src/main.cpp          entry
src/ui.cpp            Home / Videos / BitTorrent / Plugins
src/screen.*          1920×1080 CPU VideoOut (starter / Breeze / boilerplate)
src/pad.*             DualSense
src/font.*            Inter + JetBrains Mono via stb_truetype
sce_sys/param.json    title identity (Media)
sce_sys/icon0.png     512×512
fonts/                OFL fonts
stubs/common_dialog.c link-only IME stub
```

Do not add FFmpeg objects here until Phase 2 has a working static library
and `STATUS.md` is updated in the same change.
