# Current payload

`payload.elf` is the file in [README.md](README.md).

- Color bars run before USB, pad, or audio.
- Pad and audio are opened with `dlopen`. They are not in `DT_NEEDED`.
- MPEG-1 is pl_mpeg. H.264 and HEVC use libSceVideodec2 when it loads, otherwise FFmpeg.
- Audio in those containers is mixed to 48 kHz stereo.
- zlib-compressed Matroska headers are inflated.
- `payload-mpeg1.elf` is the older MPEG-1-only file.
- Not run on a console here.

`movian-go` does not boot yet. See [../movian-ps5-port/README.md](../movian-ps5-port/README.md).
