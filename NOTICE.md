# Notices

This file records third-party code that ships in, or is required to build,
Movian for PS5. It is not legal advice.

## Native title toolchain

- [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk) — public PS5
  headers, Clang target, libc++ headers. Fetched by the starter kit into an
  ignored `.deps/` cache. Not redistributed by this repository.
- [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)
  by BlackBearReloaded, GPL-3.0-or-later. Vendored by
  [ps5-homebrew-starter](https://github.com/MiladRTX/ps5-homebrew-starter)
  under `kit/`. Our `app/build.sh` copies that kit into
  `~/.ps5-starter/kit-work` and compiles `app/src` against it.
- Independently authored `libc.prx` loader shim from the boilerplate.
  Generated at build time; digest checked by the kit.

## Application sources copied into `app/src`

These files were copied from `ps5-homebrew-starter/app/src` (GPL-3.0-or-later)
and namespaced to `movian::`. They originally come from:

- VideoOut CPU renderer: boilerplate demo renderer (BlackBearReloaded),
  extended in [Breeze](https://github.com/MiladRTX/Breeze) (MiladRTX),
  reused in the starter.
- DualSense (`pad.cpp`, `pad_input.cpp`): starter / Breeze.
- Font rasteriser: starter, using `stb_truetype.h` (public domain / MIT,
  Sean Barrett).
- Image loader: starter, using `stb_image.h` (public domain / MIT).
- `app/stubs/common_dialog.c`: link-only stub, same approach as
  ProsperoStore (BlackBearReloaded).

`app/src/ui.cpp`, `ui.hpp`, `main.cpp`, and `paths.hpp` are original to this
port.

## Fonts (`app/fonts`)

- [Inter](https://rsms.me/inter/) — SIL Open Font License 1.1 (`Inter-LICENSE.txt`).
- [JetBrains Mono](https://www.jetbrains.com/lp/mono/) — SIL Open Font License
  1.1 (`JetBrainsMono-OFL.txt`).

## Reference trees (not linked into the Phase 0 binary)

- [czz/movian-go](https://github.com/czz/movian-go) — GPL, desktop Go port of
  Movian. Plugin API, backends, and UX reference. Not built for PS5.
- [ZiZc3/VLC](https://github.com/ZiZc3/VLC) — GPL-3.0, VLC 3.0 native PS5
  title. Packaging and platform patterns we follow. We do not copy VLC
  sources into this tree.
- [andoma/movian](https://github.com/andoma/movian) — original C Movian.
- Historical experiments in [`archive/`](archive/README.md), including
  Duktape, splicer payload sources, and a Go overlay. Those remain under
  the licenses recorded in the archived `LICENSE` / `README` files.

## Optional later phases

FFmpeg, libass, libsmb2, hardware video decode (`libSceVideodec2`), Vulkan /
RADV via PS5_Vulkan, and a JavaScript engine (Duktape or a native goja port)
will be introduced with their own notices when they are actually linked.
Do not list a library here until `app/build.sh` compiles against it.
