# Playback

**Phase 0 does not play video or audio.** The table below is the
*target* from M7 / movian-go, to be ticked in [STATUS.md](STATUS.md)
only when the player actually opens the file.

## Target formats (Phase 2+)

| Kind | Goal |
| --- | --- |
| Video | H.264, HEVC, MPEG-1/2/4, VP8, … via FFmpeg; HW path for H.264/HEVC |
| Audio | AAC, AC-3, E-AC-3, MP3, DTS, TrueHD, Opus, FLAC, PCM → 48 kHz stereo first |
| Containers | MP4, MKV, MPEG-TS, AVI, … |
| Subtitles | SRT first; ASS/libass later |
| BitTorrent | magnet / `.torrent`, play-while-download |

## Hardware decode

PS5 titles can load `libSceVideodec2` (sysmodule 207). VLC’s
`ps5/modules/ps5vdec.c` and `archive/splicer/src/hwdec.c` are the
references. Software fallback must remain.

## Errors

When a file cannot be opened, show the path and `errno` / sce error on
screen and in `/download0/movian-ps5.log`. Never claim “unsupported 4K
HDR” until the HW path is live.
