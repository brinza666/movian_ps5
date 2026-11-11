# Playback

**This binary plays MPEG-1 program streams** (`.mpg` / `.mpeg` / `.mpv` /
`.m1v`) with pl_mpeg, CPU YUV→RGBA, and 48 kHz stereo AudioOut.
H.264 / MP4 / MKV are **not** decoded until FFmpeg is linked.

## Target formats (Phase 2+)

| Kind | Now | Goal |
| --- | --- | --- |
| MPEG-1 video + MP2 | **in `app/`** | keep |
| H.264 / AAC / MP4 / MKV | no | small static FFmpeg |
| HEVC HW | no | `libSceVideodec2` |
| Other containers | no | FFmpeg demux |
| Subtitles | no | SRT first |
| BitTorrent | no | Phase 3 |

## Hardware decode

PS5 titles can load `libSceVideodec2` (sysmodule 207). VLC’s
`ps5/modules/ps5vdec.c` and `archive/splicer/src/hwdec.c` are the
references. Software fallback must remain.

## Errors

When a file cannot be opened, show the path and `errno` / sce error on
screen and in `/download0/movian-ps5.log`. Never claim “unsupported 4K
HDR” until the HW path is live.
