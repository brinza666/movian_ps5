# movian-go overlay

Platform layer for [czz/movian-go](https://github.com/czz/movian-go) (GPL-3.0). A normal `go build` does not boot here.

The ELF that plays video today is [../ps5-splicer](../ps5-splicer). This overlay is not that file.

## Why a stock Go binary stops

`prospero-clang` defines `__FreeBSD__`, but Go’s freebsd/amd64 runtime does not call libc. It puts a FreeBSD syscall number in `%eax` and executes `SYSCALL` (`mmap` 477, `thr_new` 455, `_umtx_op` 454, `kqueue` 362, `kevent` 560). This userland does not service that table, so the process dies before `main`.

`GOOS=linux` is a different ABI. The OpenBSD Go port calls libc, which is the shape to copy, against the SDK libc rather than OpenBSD headers. [ps5-rust](https://github.com/neokuze/ps5-rust) already does that. Details: [runtime/README.md](runtime/README.md).

## What the `ps5` tag does once that runtime exists

- `HOME=/data/movian`, `TMPDIR=/data/movian/tmp`
- File services for `/data`, `/mnt/usb0`, `/mnt/usb1` and `/user` when those directories exist
- Dummy audio. ALSA is not built.
- No GLW skin. A 1920×1080 SDL window on `sceVideoOut` shows a status page.
- DualSense and a USB keyboard: Cross activate, Circle back, Options menu, Triangle home, L1/R1 page, sticks play/pause and stop.
- BitTorrent stays `internal/backend/bittorrent`, already constructed in `wire_core.go`. A magnet opens as `torrentfile://` plus the 40-hex info hash. This overlay does not replace that package.

`scripts/build_ffmpeg_ps5.sh` is the upstream movian-go dependency script. It is not how `payload.elf` was linked.

## Apply

```
git clone https://github.com/czz/movian-go.git
./apply.sh /path/to/movian-go

export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
cd /path/to/movian-go
make ps5-sdl
make ps5 GO_PS5=$HOME/go-ps5/bin/go
```

`make ps5` needs the patched Go from [runtime/README.md](runtime/README.md). Until then the ELF does not survive startup. Check with:

```
prospero-objdump -d movian-go.elf | grep syscall | head
```

Hits inside `runtime` or `syscall` mean the patch is not in the `go` binary you invoked.

`prospero-deploy` sends `movian-go.elf` to the loader already running.

movian-go can also be built with `make windows` and launched under [prospero-win](https://github.com/mpereiraesaa/prospero-win). That is Wine, not this overlay.
