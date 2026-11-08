# Go runtime on Orbis

`movian-go` is ordinary Go. It will not become a PS5 payload by setting
`GOOS` and `GOARCH` alone.

## What the CPU and the SDK actually are

- PS5 userland is **x86-64, Zen 2**. `prospero-clang` targets `x86_64-sie-ps5`,
  `-march=znver2`, the System V ABI, and it defines `__FreeBSD__`.
- The payload SDK (ps5-payload-dev/sdk **0.45**, 21 Oct 2026) ships a libc
  and Sony import stubs. Programs call `mmap`, `kqueue`, `pthread_*`.
  They do not use FreeBSD's `syscalls.master` numbers.
- [ps5-rust](https://github.com/neokuze/ps5-rust) calls that libc and links with `prospero-clang`. Go has to do the same.

## What stock Go does instead

`src/runtime/sys_freebsd_amd64.s` (current master) contains, among others:

```
SYS_mmap              477
SYS_thr_new           455
SYS__umtx_op          454
SYS_kqueue            362
SYS_kevent            560
```

and then the `SYSCALL` instruction. The same pattern is copied in
`src/syscall/asm_freebsd_amd64.s` and in `golang.org/x/sys/unix` (movian-go
requires `golang.org/x/sys v0.48.0`).

Those numbers are FreeBSD's. Orbis will not service them. The process
dies inside the runtime, before `main`.

OpenBSD's Go port is the template to copy, because it already enters the
kernel through libc (`//go:cgo_import_dynamic` / a C ABI trampoline).
On a payload you want **static** libc symbols from the SDK archive, not
`dlopen("libc.so")`. Rust does the static link. Go on OpenBSD does the
dynamic one. The port is "OpenBSD's call shape, SDK's libc".

## Files that have to change in a Go 1.27.3 tree

movian-go's `go.mod` says `go 1.27.3`. Patch that version, not an older one.

| Tree | Change |
|---|---|
| `src/runtime/sys_freebsd_amd64.s` | Replace every `SYSCALL` body with a C ABI call to `libc_bridge.c` |
| `src/runtime/os_freebsd.go` | Keep the Go signatures (`thr_new`, `kqueue`, `kevent`, …). They are the assembly entry points you are replacing |
| `src/syscall/asm_freebsd_amd64.s` | Same treatment. `os` and `net` use this |
| `golang.org/x/sys/unix` asm for freebsd/amd64 | Same treatment, or the first `net` dial crashes after the runtime is fixed |
| cgo `CC` | `prospero-clang`, so the support C files see the SDK's FreeBSD-like headers |

Thread creation has to be correct, not just `mmap`. The runtime's
scheduler calls `thr_new` (FreeBSD threads), `_umtx_op` for locks, and
`kevent` for the network poller. Mapping those three to
`pthread_create`, a libc `umtx` (or a pthread mutex fallback), and
`kqueue`/`kevent` is the whole port. `libc_bridge.c` in this directory
is the mmap/file/clock slice; the thread and kqueue wrappers belong next
to it once you are building the patched toolchain.

`ps5-rust` already bridged the annoying libc gaps (`accept4`, `kevent`
layout, `getrandom`). Expect the same gaps when Go's freebsd syscall
types are compiled against the SDK headers.

## Link line

prospero-clang is the external linker, same as Rust's
`targets/x86_64-sie-ps5-std.json`:

```
CC=$PS5_PAYLOAD_SDK/bin/prospero-clang \
CXX=$PS5_PAYLOAD_SDK/bin/prospero-clang++ \
CGO_ENABLED=1 \
GOOS=freebsd GOARCH=amd64 \
go build -ldflags '-linkmode=external' ...
```

`-linkmode=external` is what makes the SDK crt and the Sony import
libraries end up in the ELF. It does **not** rewrite the SYSCALL
instructions already emitted inside `runtime`.

## Check that a binary is not secretly still using SYSCALL

```
prospero-objdump -d movian-go.elf | grep -n syscall | head
```

Any hit inside `runtime` or `syscall` means the patch did not take.
A real payload's syscalls live only inside the SDK libc.
