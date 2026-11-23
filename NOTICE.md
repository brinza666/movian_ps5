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

`app/src/ui.cpp`, `ui.hpp`, `main.cpp`, `paths.hpp`, `fs.*`, `player.*`,
and `audio.*` are original to this port (audio thread pattern from the
starter).

## Fonts (`app/fonts`)

- [Inter](https://rsms.me/inter/) — SIL Open Font License 1.1 (`Inter-LICENSE.txt`).
- [JetBrains Mono](https://www.jetbrains.com/lp/mono/) — SIL Open Font License
  1.1 (`JetBrainsMono-OFL.txt`).

## Reference trees (not linked into the Phase 0 binary)

- [czz/movian-go](https://github.com/czz/movian-go) — GPL, desktop Go port of
  Movian. Plugin API, backends, and UX reference. Not built for PS5.
- [ZiZc3/VLC](https://github.com/ZiZc3/VLC) — GPL-3.0, VLC 3.0 native PS5
  title. Packaging and platform patterns we follow. The playback test uses the VLC
  Videodec2 link-only stub under GPL-2.0-or-later.
- [andoma/movian](https://github.com/andoma/movian) — original C Movian.
- Historical experiments in [`archive/`](archive/README.md), including
  Duktape, splicer payload sources, and a Go overlay. Those remain under
  the licenses recorded in the archived `LICENSE` / `README` files.

## Linked in this binary

- [PL_MPEG](https://github.com/phoboslab/pl_mpeg) — MIT, Dominic Szablewski.
  MPEG-1 video + MP2 audio. Vendored as `app/src/pl_mpeg.h` from the
  archived splicer copy.

## Optional later phases

FFmpeg, libass, libsmb2, hardware video decode (`libSceVideodec2`), Vulkan /
RADV via PS5_Vulkan, and a JavaScript engine (Duktape or a native goja port)
will be introduced with their own notices when they are actually linked.
Do not list a library here until `app/build.sh` compiles against it.

## Native playback test release

- FFmpeg 6.1.2 is statically linked for demux, format adaptation, software
  video/audio decoding, libswresample and libswscale. Exact inputs and flags are in
  the test build recipe. The release includes the FFmpeg source tarball.
- [VideoLAN dav1d 1.5.4](https://code.videolan.org/videolan/dav1d/-/tree/1.5.4)
  provides software AV1 decoding under the BSD 2-Clause license. Its
  copyright and license are in `app/assets/licenses/dav1d.txt`.
- [Duktape 2.7.0](https://duktape.org/download) is compiled into the
  native plugin host. Its MIT license is packaged as
  `assets/licenses/duktape.txt`.
- [miniz 3.0.2](https://github.com/richgel999/miniz/tree/3.0.2) reads local
  plugin ZIPs. Its license is packaged as `assets/licenses/miniz.txt`.
- [Gumbo Parser 0.14.1](https://codeberg.org/gumbo-parser/gumbo-parser/src/tag/0.14.1)
  parses HTML for plugins under Apache-2.0. The packaged license is
  `assets/licenses/gumbo.txt`. The build applies a local nesting and token limit.
- The [VLC Videodec2 import stub](https://github.com/ZiZc3/VLC/blob/7f104abb5a88dd675b60261769dc86d5dfb0a0d4/ps5/stubs/videodec2_link_stub.c)
  is used at link time. It is not packaged. Videodec2 is supplied by the console.
- The included HEVC test fixture is a generated color pattern and sine tone.

## v60 network and optional title-access test

- The installed public SDK's static libc++, libc++abi and libunwind archives
  supply the required C++ string/runtime symbols. Unused sections are discarded;
  the server itself uses pthreads and bounded vector tables. The LLVM runtime
  license is packaged as `assets/licenses/llvm-runtime.txt`. SDK input hashes
  accompany the release verification; no exact upstream SDK rebuild is claimed.
- libsmb2 commit `ef32cff6cdda24279eaac802f468d9290a2c8b5d` is statically
  linked under LGPL-2.1. Its exact source archive is in the dependency-source
  asset; `scripts/build-smb2-ps5.sh` and the tracked patches reproduce changes.
- Optional test builds copy the GPL public starter elevation client from
  starter commit `c3f939230aee18863da1f359e39eb43bf95a056d`. The exact starter
  kit source is included in the dependency-source asset.
- The optional one-shot Lapy helper is built from commit
  `57fcc19b7f57e46c823d6f2a6c6233ebf3288079`, primary archive SHA-256
  `c87cf02eceb1c004461f268de9657f7f17a367a4bbf3acfa2c6b7f86c90ae152`.
  Its MIT license, manifest and exact source archive accompany the helper;
  upstream component notices remain in that source and the starter notices.
  The per-console authorization file is never included in public packages.
- Current desktop feature reconstruction uses the installed WSL Movian build
  and retained runtime/REA evidence. The historical andoma reference above is
  not an implementation source for this work.
