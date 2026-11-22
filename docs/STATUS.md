# Status

The isolated `PPSA99456` playback title has a published v15 prerelease
and a mounted v23 test build. v22 console logs show finished catalog loads,
repository ZIP installs, artwork decodes, and playback from local torrent
descriptors. v23 adds plugin compatibility and an empty installed plugin
profile. Direct peer downloads passed host tests for verified delivery and
corruption rejection. v23 console peer playback, live sign-in, and MKV EOF
checks remain open. v26 confirmed 1080p hardware decoding of a local torrent
fixture and identified a peer address entry failure. The mounted v27 build
adds a saved-peer path and PS5 nonblocking socket setup. Its PS5 test
started hardware HEVC playback during two controlled transfers. v28 is
supplied the saved preferred plugin catalog. v29 is mounted with a direct
Google Drive refresh and HTTP/HTTPS compact IPv4 tracker discovery for the
same bounded single-file path. The v31 PS5 log shows tracker replies with
four to eight peers followed by failure to allocate the RAM piece cache.
v32 builds with a 16 MiB flexible-memory cache for the 19.1 GB Matroska,
explicit RAM, PS5 storage and USB choices, and up to 16 block requests per
peer per batch for large files. Host cache, peer and demux tests passed.
v32 is mounted on PS5. The console mapped 16 MiB of torrent RAM and
decoded 53 H.264 hardware frames at 1920×804 during the transfer.
All peer workers stopped after one verified piece. Sustained playback,
audible output and seeking on the feature-length torrent remain open.
v33 mounted and confirmed the RAM map again. The attempted peers failed
at connection or handshake before verifying any torrent pieces.
The PS5 recorded a crash after the user selected USB cache.
v34 is mounted with tracker fallback, the configured public retracker
overlay, KinoZal sign-in using the selected HTTPS domain, and a revised
USB file backend. USB opened on one attempt and a transfer crash was
reported. The RAM run delivered three pieces, 552 hardware H.264 frames
and nonzero audio output before a peer failure. v35 is mounted with
separate torrent interruption handling. Sustained playback, seeks, USB
playback and successful live login are still open. v36 is mounted with
per-torrent saved-peer handling and up to 24 distinct candidates per
announce. A separate 720p RAM torrent started but lost its peers.
v37 retains verified pieces after a swarm stops. Its optional LAN
diagnostics listener failed to start on PS5. v38 is mounted and verified
its live FTP diagnostics fallback after PS5 denied the TCP bind.
The Avanpost trace showed 4.5-second piece waits and a final peer
failure. v39 is mounted with two-piece startup buffering and a sliding
block-request window. Host cache, transport and demux tests passed.
The PS5 verified both startup pieces, played Avanpost in hardware and
then waited 10.1 seconds for the third piece from the remaining worker.
v40 is mounted with persistent controller settings for cache and peer
tuning on new large Matroska torrent transfers. Host checks passed.
v41 is the current mount and adds a bundled retracker list for public
torrents when no custom list is present.
v42 passed host UDP tests but exited during the first PS5 Avanpost run.
v43 added PS5-native DNS and UDP stage logs. Its Avanpost FTP logs were
unavailable. v44 is mounted and uses the existing libkernel import for
transaction randomness, removing the new load-time library dependency.
The v44 PS5 trace confirms UDP announces in RAM. Avanpost received no
verified pieces. v45 is mounted and discovers more candidates when active
workers stop making progress. v45 Avanpost confirmed staggered UDP
discovery and again received no verified pieces. v46 is mounted with
room for all supported fallback trackers and avoids immediate retries
of failed tracker connections. Host checks passed. The v46 console
attempt reached all 27 eligible trackers and tried six distinct peers.
Zero pieces were verified and Avanpost did not start playback.
v47 is mounted with an IPv4 DHT lookup adapted from TorrServer's pinned
engine. Host discovery, private-torrent exclusion, RAM seek and Matroska
checks passed. The Avanpost console retest is pending.
v53 is the latest signed `PPSA99456` test build. It adds the in-video transport bar,
a seek preview with confirm and cancel, local audio selection, and an embedded
subtitle picker. Host tests passed, including playback continuity, track selection
and seeks. Subtitle rendering and v53 console verification are pending. See the
[UI work list](UI_WORK_LIST.md) for the v48–v53 changes.

Production status for `PPSA99455` is below.

Last updated: 2026-11-11 (`v0.7.0` static FFmpeg linked, `eboot.bin` 3381807 bytes).

Legend: **done** = observed on PS5 or in a shared-source host preview;
**shell** = UI exists, backend does not; **no** = not in the live tree.
Official M7 column is from the live WSL2 7.0.273 install — see
[M7_CAPABILITIES.md](M7_CAPABILITIES.md).

| Feature | Official M7 | Live `app/` | Notes |
| --- | --- | --- | --- |
| Home-screen title | n/a | **shell** | `PPSA99455`, Media category; Phase 1 browse UI |
| DualSense navigation | pad/keyboard | **done** (code) | folder mounted 2026-11-11; open the icon for klog EXEC |
| Home tiles + search | **yes** (live) | **shell** | Continue / Videos / Bookmarks / Settings; no search OSK |
| Local file browser | **yes** | **shell** | Videos lists `/download0` + `/data/movian` + bookmarks |
| USB | **yes** | **shell** | `/mnt/usb0` is attempted; sandbox may hide it |
| Software video | FFmpeg 4.4.4 | **shell** | MPEG-1 via pl_mpeg; H.264/AAC **linked** (FFmpeg 6.1.2 static), not yet TV-proven |
| Hardware video | VDPAU / MediaCodec / … | **no** | Phase 2b, `libSceVideodec2` (toggle stored only) |
| Audio | Pulse / AT / passthrough | **shell** | MPEG-1 MP2 → 48 kHz stereo, volume/mute |
| Settings file | **yes** | **shell** | `/download0/settings.txt` DualSense toggles (auto-next, newest-first) |
| Resume / bookmarks | **yes** | **shell** | `/download0/resume.txt` + `bookmarks.txt` |
| HLS / HTTP | **yes** (TS + fMP4) | **no** | Phase 5 |
| BitTorrent | **yes** (built-in) | **no** | Phase 3 |
| Plugins API v2 | Duktape 1.8 | **no** | Phase 4 |
| Official plugin repo | repo.movian.eu | **no** | 14 plugins listed live |
| SMB2 | **yes** (libsmb2) | **no** | Phase 5 |
| UPnP / DLNA | **yes** | **no** | |
| Subtitles | srt/vtt/ass | **no** | SRT in Phase 6 |
| HTTP Control :42000 | **yes** (live) | **no** | not Phase 0–4 |
| DRM / Widevine / Cast | Android only | **no** | explicit non-goal |

Historical claims in old READMEs that everything above was already
implemented were **false**. See `archive/`.
