# Environment already running

The splicer does not replace these. They are what the console is already running when `payload.elf` is sent to port 9021.

| File | Role |
| --- | --- |
| etaHEN | Homebrew environment. |
| kstuff | Kernel helper. Mounts titles, USB and PFS. |
| pkg-manager | Installs and removes titled apps. |
| shadowmountplus | Mounts homebrew, USB and titles. Needs kstuff. |

The splicer uses the module calls those payloads already rely on: `sceKernelLoadStartModule`, `sceKernelDlsym`, `sceSysmoduleLoadModule` and `sceSysmoduleLoadModuleInternal`. It opens sysmodule 207 (libSceVideodec2) after the color bars and feeds it Annex-B access units from the FFmpeg demuxer. Pictures come back as NV12 or P010 and are copied into the `sceVideoOut` frames.

If the module does not open, or returns no picture, the same file is decoded with FFmpeg, up to 1920×1088. Hardware accepts up to 3840×2176. MPEG-1 stays on pl_mpeg. The toast after the bars says which path opened.

This was not run on a console here.
