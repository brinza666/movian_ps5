# Archive — do not ship

Frozen experiments from earlier approaches. **The live application is
`../app/`.** Nothing in this folder is built by `app/build.sh`.

| Directory | What it was | Why it is archived |
| --- | --- | --- |
| `ps5-boilerplate-stubs/` | Fork of ps5-native-app-boilerplate with `media_ps5.c`, `torrent_ps5.c`, `plugin_ps5.c`, … | Headers claimed FFmpeg/BitTorrent/Duktape; bodies were stubs or unlinked. Build notes pointed at a proprietary Sony SDK that this project does not use. |
| `native-stubs/` | Duplicate C sources at repo root | Same over-claim as above. |
| `go-overlay/` | `movian-ps5-port` overlay for [czz/movian-go](https://github.com/czz/movian-go) | Stock Go freebsd/amd64 issues FreeBSD syscalls; the process dies before `main`. Keep as a design note, not a build. |
| `splicer/` | PIE ELF for elfldr:9021 (pl_mpeg, FFmpeg sketches, hwdec, bt) | Useful **decode reference**. Not a home-screen title. Binaries were removed; sources remain. |
| `old-scripts/`, `old-sce_sys/`, `old-sce_module/`, `old-assets/` | Previous packaging | Replaced by `app/sce_sys` and the kit-generated `libc.prx`. |

Presentation binaries (`pic0.dds`, `pic1.dds`, source PNGs, `snd0.at9`) and
payload ELFs were deleted from this archive so git stays source-only.

When harvesting an idea from here (for example `splicer/src/hwdec.c` in
Phase 2b), copy the specific file into `app/src` with a provenance
comment. Do not resurrect the whole tree.
