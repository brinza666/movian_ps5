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

Players that do open MKV and HEVC on that setup (VLC, EVO Player) are titled apps. EVO's architecture notes say an elfldr payload does not get `sceVideodec2` or `sceAgc`. Hardware decode stays in those apps. This splicer is still an elfldr payload, so H.264 and HEVC in it are FFmpeg software decode, capped at 1920×1088.

REA 6.8.0 still has no video decoder. Playback is not sent through `rea`.
