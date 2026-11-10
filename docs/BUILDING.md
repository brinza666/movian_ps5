# Building

## Host

WSL2 Ubuntu 24.04 with clang-18 / lld-18. Native Windows Make cannot
run the kit’s bash toolchain.

```bash
sudo apt install clang-18 lld-18 llvm-18 make python3 git curl unzip
```

This PC already ran `C:\Users\brinza\Documents\deep\setup-wsl-ps5.sh`.

## Dependencies (not in git)

| Dependency | Where |
| --- | --- |
| ps5-payload-sdk v0.42 | `$HOME/.ps5-starter/ps5-payload-sdk` |
| boilerplate kit | `ps5-homebrew-starter/kit`, copied to `$HOME/.ps5-starter/kit-work` |
| zlib / `libc.prx` | generated inside `kit-work/.deps` on first build |

```bash
export PS5_PAYLOAD_SDK=$HOME/.ps5-starter/ps5-payload-sdk
export PS5_STARTER=/mnt/c/Users/brinza/Documents/deep/ps5-homebrew-starter
```

## Build the title

```bash
cd /mnt/c/Users/brinza/Documents/deep/movian_ps5
bash scripts/doctor.sh
bash app/build.sh
# outputs:
#   build/app/PPSA99455/eboot.bin
#   build/app/PPSA99455.zip
```

`app/build.sh` will:

1. Refresh `~/.ps5-starter/kit-work` from the starter `kit/` if needed.
2. Copy `app/src`, `app/sce_sys`, `app/fonts`, `app/assets` into `kit-work/movian/`.
3. Build a CommonDialog link stub.
4. Invoke the kit `Makefile` with `APP_SOURCE_DIR=movian/src`.
5. Copy `dist/PPSA99455` back into this repo’s `build/app/`.

First kit build compiles zlib and `libc.prx`; later builds are faster.

## Identity

Edit `app/sce_sys/param.json` only when creating a **different** title.
Keep `PPSA99455` for Movian so `/download0` and the home-screen icon stay
stable.

`contentId` must match `^[A-Z]{2}[0-9]{4}-PPSA[0-9]{5}_00-[A-Z0-9]{16}$`
and contain the title id. The kit rejects a 15-character suffix.

## What we do not build yet

VLC’s `ps5/deps/build-deps.sh` (FFmpeg, GnuTLS, libass, …) and
`PS5_Vulkan` are **Phase 2+**. PacBrew static libs can be enabled later
via the kit’s `PACBREW_PACKAGES` once a player exists.
