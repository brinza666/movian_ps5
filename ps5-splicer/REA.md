# Layout of the old MPEG-1 payload

This note is `payload-mpeg1.elf`, not the current `payload.elf`. The current file is described in [README.md](README.md). Its sha256 is not the one in the table below.

Measured with REA `inspect-binary-layout` (pwntools 4.15.0, pyelftools 0.33, unicorn 2.1.2). The file was not executed.

| | |
|---|---|
| Evidence | `ev_35073aaeb6145843452dbf30e73c4a408e80032fd1805383235dd96a2b6c16dc` |
| SHA-256 | `9b62550330fdf1162f0a6b031f67ddcb1eadfc3e0f21f322fdd48d0db8d23d64` |
| Size | 220464 bytes |
| Class | ELF64, `EM_X86_64`, little-endian, `ET_DYN` |
| Entry | `0x13260` (linked virtual address, not a proven runtime PC) |

## What the loader must map

`DT_NEEDED`, and nothing else:

- `libSceVideoOut.sprx`
- `libSceSystemService.sprx`
- `libkernel_web.sprx`
- `libSceLibcInternal.sprx`
- `libSceNet.sprx`

`libScePad`, `libSceAudioOut`, and `libSceUserService` are not in that list. The symbol table has storage for the pointers (`p_scePadInit`, `p_scePadOpen`, `p_scePadReadState`, `p_sceAudioOutInit`, `p_sceAudioOutOpen`, `p_sceAudioOutOutput`, `p_sceAudioOutSetVolume`, `p_sceAudioOutClose`) and a defined `dlopen` at `0x5a40`. Those libraries are opened after the process is already running. A missing pad sprx cannot fail the initial map.

Undefined imports that do have to resolve at load are the video-out set (`sceVideoOutOpen`, `sceVideoOutRegisterBuffers2`, `sceVideoOutSubmitFlip`, `sceVideoOutSetFlipRate`, and the flip-queue calls), `sceKernelAllocateMainDirectMemory`, `sceKernelMapDirectMemory`, `sceSystemServiceHideSplashScreen`, plus libc file calls: `fopen`, `fread`, `fseek`, `opendir`, `readdir`, `stat`.

## What decodes a picture

The defined functions are `plm_*` from `mpeg_dec.c`. The ones the combiner actually enters through:

- `plm_create_with_filename` at `0x5ae0`
- `plm_probe` at `0x71c0`
- `plm_decode` at `0x7e90`
- `plm_seek` at `0x92c0`
- `plm_frame_to_rgba` at `0xf8f0` (scanout is `A8B8G8R8`, so the frame is expanded before the flip)

The GOT names `PLM_VIDEO_MACROBLOCK_TYPE_INTRA` and the other pl_mpeg tables. There is no `avcodec`, `avformat`, or FFmpeg symbol, and no `DT_NEEDED` on an FFmpeg sprx.

`libSceVdecShevc.native.sprx` is a string in the file. It sits in the SDK `sysmodtab` (`rtld_sprx.c`), not in `DT_NEEDED` and not in the undefined imports. It is not a decoder this ELF calls.

`scan_media` (`0x4290`) and `scan_dir` (`0x4d30`) are the USB walk. File strings in the image are `/mnt/usb0`, `/mnt/usb1`, `/data/video`, and `Put .mpg files on /mnt/usb0`.

## What this measured

This layout is the MPEG-1 ELF only. Decoding in that file stays `plm_decode`. REA is not a codec and cannot take FFmpeg's place. The current `payload.elf` does link FFmpeg. Do not use the paragraphs below to describe it.

REA 6.8.0 was checked for a video decoder. The commands that contain "decode" are Android resources, Interface Builder nibs, sigrok signal recordings, and one native instruction. None of them turns a media file into frames. Video reverse engineering is an open request on the REA repo, not a command in this version.

So playback is not routed through the `rea` CLI. The decoder in this ELF is the one the symbol table names: `pl_mpeg`, entered at `plm_create_with_filename`. There is still no FFmpeg import, and this note does not add one. REA did not recover function bodies either: no Ghidra, Hopper, IDA, or rizin was available.
