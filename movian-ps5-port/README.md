# movian-go on a PS5 you already run homebrew on

This is a platform layer for [czz/movian-go](https://github.com/czz/movian-go)
(GPL-3.0), plus the reason a normal `go build` cannot target Orbis and
what has to be patched in the Go toolchain before the ELF will live
past startup.

Nothing in here jailbreaks a console. The binary is a payload for a
machine that already loads homebrew ELFs.

The splicer ELF beside this tree was measured with REA, not run
(`ps5-splicer/REA.md`). Its defined decoder symbols are `plm_*` only,
entered at `plm_create_with_filename`. There is no FFmpeg import.
This overlay is the replacement for that call, and it still cannot boot
until the Go runtime stops issuing FreeBSD syscalls.

## What works in the scene, late October 2026

Homebrew is built as a **dynamically linked ELF**, not a fake Linux
binary and not a PKG you sign yourself unless you go out of your way.

| Piece | Where it stands |
|---|---|
| Toolchain | [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk) **v0.45** (21 Oct 2026). Clang/LLD, `prospero-clang`, target `x86_64-sie-ps5`, Zen 2, FreeBSD-looking headers. v0.45 added kernel offsets for 14.00 and 14.10. v0.44 made Meson treat the host as `freebsd/x86_64` and added `libSceAgc` stubs. |
| Load it | An ELF loader you already run (elfldr on port 9021, websrv, the jar/lua loaders). The SDK's `prospero-deploy` sends the file. |
| Real home-screen tile | [ps5link](https://github.com/Rufidj/ps5link-sdk) links a title (`eboot`) instead of a payload. Heavier, and signing is a separate step. Payloads are the right first target. |
| Video | [ps5-payload-dev/SDL](https://github.com/ps5-payload-dev/SDL) branch `release-2.30.x-ps5`. The video driver is a **software** surface: `sceVideoOut` plus a tile blit. It is not OpenGL. OSMesa is optional (`SDL_VIDEO_OPENGL_OSMESA`). |
| A language runtime that already did this | [ps5-rust](https://github.com/neokuze/ps5-rust). `std`, tokio, rustls, `std::fs`, tested on a CFI-1215A at firmware 13.60. Syscalls go through the SDK libc. This is the model to copy. |
| Windows games on the metal | [prospero-win](https://github.com/mpereiraesaa/prospero-win) (Oct 2026). 64-bit Windows code runs on the PS5 CPU through Wine. OpenGL goes through Zink. Useful as a bridge, not as a native port. |
| GPU for real titles | `libSceAgc` stubs landed in SDK 0.44. Emulator ports use Vulkan/AGC. Movian's UI is OpenGL, so that does not help until a GL layer exists. |

Practice that actually produces a running ELF:

1. Install the SDK zip. `export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk`.
2. Compile with `prospero-clang` (or the CMake/Meson wrappers). The driver adds crt, PIE and the Sony import libs.
3. Entry for a payload is `main`. Do not invent a kernel exploit in the program.
4. Copy data to `/data` or `/mnt/usb0` over the FTP server you already use. The SDK's homebrew prefix is `/user/homebrew`.
5. Deploy with `prospero-deploy` to the loader you already have.

Stay on a firmware your current loader supports. A public scene status page on 27 Oct 2026 was still telling people not to move past **13.60** if they wanted to keep a jailbreak. New offsets in the SDK are not a new jailbreak.

## Why `GOOS=freebsd GOARCH=amd64` crashes

prospero-clang defines `__FreeBSD__`, so it is tempting to build movian-go as FreeBSD. Don't.

Go's freebsd/amd64 runtime does not call libc. `src/runtime/sys_freebsd_amd64.s` puts a FreeBSD syscall number in `%eax` and executes `SYSCALL`:

- `mmap` = 477
- `thr_new` = 455
- `_umtx_op` = 454
- `kqueue` = 362
- `kevent` = 560

Orbis does not implement that table. The same instructions are in `src/syscall` and in `golang.org/x/sys/unix` (movian-go is on v0.48.0). The process dies in the runtime, before `main`, even if `prospero-clang` linked a pretty ELF.

`GOOS=linux` is the wrong ABI (epoll, different numbers). `GOOS=openbsd` is the right *shape* — that port calls libc — but its cgo files include OpenBSD headers, and prospero-clang is not an OpenBSD compiler.

ps5-rust works because Rust `std` calls libc and the linker is `prospero-clang`. Go needs the same treatment. See [runtime/README.md](runtime/README.md) and `runtime/libc_bridge.c`. Until that toolchain exists, `make ps5` produces an ELF that will not survive startup. Check with:

```
prospero-objdump -d movian-go.elf | grep syscall | head
```

Hits inside `runtime` or `syscall` mean the patch is not in the `go` binary you invoked.

## What this overlay changes in movian-go

movian-go already picks a platform with build tags (`rpi`, `android`, `glfw`, …). The PS5 tag is `ps5`, and the Go OS stays `freebsd` so the future libc port type-checks. `apply.sh` wires that in.

On this target the process:

- Sets `HOME=/data/movian` and `TMPDIR=/data/movian/tmp` (created if missing).
- Publishes `/data`, `/mnt/usb0`, `/mnt/usb1` and `/user` as file services when those directories exist.
- Uses the **dummy audio** driver. FreeBSD is not linux, so the ALSA file is not built. Picture and sound do not go through ALSA.
- Does **not** start the GLW skin. The skin is OpenGL. The PS5 SDL port is a software framebuffer. The main thread locks itself to the OS thread, opens a 1920x1080 SDL window, and draws a status page (the driver flips it with `sceVideoOut`).
- Maps DualSense / SDL gamecontroller and a USB keyboard onto Movian actions: Cross activate, Circle back, Options menu, Triangle home, L1/R1 page, click sticks play/pause and stop.

That is a native shell around the existing core (plugins, HTTP, the media pipeline's software decode once FFmpeg is linked). It is not the GL skin and it is not hardware video decode.

FFmpeg 9.0.1 (the version `scripts/deps.env` pins, sha256 `cf38e0e2…`) is cross-built with `--target-os=freebsd` and hardware accelerators off. There is no VAAPI, NVDEC or `/dev/dri` on a payload.

GLW still references `gl*` symbols. `apply.sh` adds `-lOSMesa` for `freebsd`+`ps5`. If you have not built OSMesa into the sysroot, the link fails there on purpose. Replacing that one line with a Piglet/GLES link is the performance path; the context-creation code in `internal/ui/glw` is still the Raspberry Pi / GLFW one and is not compiled into the `ps5` tag.

## Apply

On a computer, not on the console:

```
git clone https://github.com/czz/movian-go.git
# this directory is the port
./apply.sh /path/to/movian-go

export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
cd /path/to/movian-go
make ps5-sdl
make ps5-ffmpeg
# GO_PS5 must be the patched toolchain from runtime/README.md
make ps5 GO_PS5=$HOME/go-ps5/bin/go
```

`prospero-deploy` from the SDK sends `movian-go.elf` to the loader you already run.

## Bridge that can show the real UI sooner

movian-go already has `make windows` (GLFW + OpenGL + FFmpeg, amd64). prospero-win runs 64-bit Windows binaries on the PS5 CPU and routes OpenGL through Zink. That is Wine, not an Orbis port, and WASAPI audio may not map, but it is the only path that reuses the existing UI without a Go runtime fork. Build `movian-go.exe` on a PC and launch it from prospero-win the same way you launch any other 64-bit program you own.

## Out of scope on purpose

- No exploit, no kernel offset table, no "how to jailbreak".
- No AGC/Piglet context yet. The status page is the display that exists in the public SDL port today.
- No claim that `movian-go.elf` plays video until the Go `SYSCALL` stubs are gone and FFmpeg is in `ffmpeg/ps5`. The codec reach, once that ELF lives past `main`, is FFmpeg's — the same library VLC for PS5, EVO Player and PS-Play already ship. MPEG-1 is not the format limit of the console or of Movian.
