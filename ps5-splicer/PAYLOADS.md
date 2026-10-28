# Payloads that were measured

REA 6.8.0 (`inspect-binary-layout`, provider pwntools-elf) read the four
files that were supplied. None of them is a video decoder. The long
`libSceVdec*` / `libSceAjm*` string lists are the payload SDK's module
catalog, the same catalog linked into this splicer from `crt1.o`. They
are names, not calls. REA reported no video, demux or FFmpeg symbol in
any of the four.

| File | Evidence | Bytes | What it imports |
| --- | --- | --- | --- |
| etaHEN-2.5B.bin | `ev_0075a64c768565bfac577c467dbb4e5ad54d0cdd360f830423a7719582ace8d6` | 4678728 | libkernel_sys, libSceLibcInternal, libSceNet. Threads, sockets, a notification. |
| kstuff.elf | `ev_796363d177aa8107c5ab98181652a636ca8033b29dfd3c81f55b3cdd99157103` | 1737080 | libSceSystemService, libSceUserService, libSceAppInstUtil, libSceFsInternalForVsh, libkernel_web, libSceLibcInternal, libSceNet. |
| pkg-manager_v1.5.1.elf | `ev_1b50c743fb464a9b5431c254a9be058bac9d075baf0bb197e1e8bf7c808775ce` | 1988168 | Those plus libSceNetCtl. Install and uninstall calls (`sceAppInstUtil*`), sockets. |
| shadowmountplus.elf | `ev_97638e3511e4d9d8d35069d281ab3b12cbf229cb5e030533873f3023f4baf3b8` | 2517288 | libkernel_sys, libSceNotification, libSceHttp, libSceSsl, and the usual libc/net set. `mount`, `nmount`, `unmount`. |

kstuff and etaHEN are the homebrew environment already running on the console. ShadowMountPlus is how disks and `/data/homebrew` get mounted. The package manager installs titled apps. None of that decodes a frame.

What they do unlock, and what this splicer now calls, is code loading:

- `sceKernelLoadStartModule`
- `sceKernelDlsym`
- `sceSysmoduleLoadModule` and `sceSysmoduleLoadModuleInternal`

Those symbols are resolved with `dlsym` after the color bars, from libraries already mapped (`libkernel_web`) or loaded by path (`libSceSysmodule.sprx`, then sysmodule 207). Sysmodule 207 is libSceVideodec2. The splicer then resolves `sceVideodec2QueryComputeMemoryInfo`, `sceVideodec2AllocateComputeQueue`, `sceVideodec2CreateDecoder` and `sceVideodec2Decode`, and feeds them Annex-B access units from the FFmpeg demuxer. Pictures come back as NV12 or P010 and are copied into the existing `sceVideoOut` frames. Direct memory for that decoder is `sceKernelAllocateDirectMemory` / `sceKernelMapDirectMemory`, also resolved at runtime, not added as a new required library.

If any of those symbols is missing, or the decoder returns no picture, the same file is decoded with FFmpeg on the CPU, capped at 1920×1088. Hardware accepts up to 3840×2176. MPEG-1 stays on pl_mpeg either way. No kernel patch was added to this ELF. REA 6.8.0 still has no video decoder. Playback is not sent through `rea`.

This file was not booted on a console here. A titled app is still what VLC and EVO use for the same device. The probe is the attempt to use it from the elfldr payload once kstuff is already running. The toast after the bars says which path opened.

