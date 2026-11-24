# Status

2026-11-24 follow-up: v61 reaches UI and completes the fresh plugin/config reset.
Recovered v60 log identifies intentional shell exit after failed root-restoration
loader lookups. v61 has filesystem access but no helper credential proof; both
listeners still fail bind with errno13. v62 fixes loader lookup, explicitly uses
the pinned owned helper protocol and captures startup diagnostics without a
worker. Full host checks and signed packaging passed; v62 is verified/mounted,
awaiting an autonomous launch path and the helper/bind/WSL acceptance. User
authorizes further planned-option implementation while away10h.


2026-11-24: Published v60 was uploaded via FTP2121, all 29 package files and
both console-only opt-ins readback-verified, then activated and mounted. A new
closed-profile backup was independently verified before the reset request.
The v59 rollback folder was moved to the consolidated PC backup and removed
from the console. Initial launch, fresh-reset/access execution and native
WSL→PS5 STPP acceptance are pending; no runtime files/listener observed yet.


2026-11-23, v60: STPP server and shared UI callbacks are implemented. Actual WSL
M7 rendered Home/Settings and changed/restored the real extension setting.
Full host/native WSL checks and signed packaging passed. v60 console upload
and access/reset execution are pending because FTP2121/ShadowMount are offline. Latest user request authorizes a one-shot access experiment
and a backed-up fresh plugin/config profile. [v60 scope/results](STPP_V60.md).
Earlier no-lift/preserve-plugin entries below describe historical builds.


2026-11-23, remote-control priority: DNS investigation is deferred by the user.
Verified actual WSL → matv STPP v3 control, restored a boolean preference,
opened KinoZal.TV and observed pushed clock/log updates. A reusable host client
and six wire tests are added; the PS5 server/property adapter is not implemented.
The installed console remains v59. See [STPP findings and three-part build plan](REMOTE_CONTROL_STPP.md).


2026-11-23, v59: user reports confirm v58 SMB and FTP ZIP installs and FTP
metainfo file-list opening. The failing torrent allocated RAM but all HTTP
requests failed with 0x8041013d before verified pieces. v59 adds native network
initialization, precise HTTP-stage logs and the observed WSL TCP & UDP / TCP /
UDP tracker chooser with a working backend. Full host self-check/UI tests and
signed build passed; v59 is uploaded, readback-verified, mounted and startup
is captured. The Avanpost retest still fails at hostname HTTP/UDP/DHT discovery. A numeric
tracker control completed verified RAM transfer, 180 hardware HEVC 1080p frames
through EOF and PCM drain without dropped samples; audibility and large-file
seek/rolling-cache playback remain open. DNS configuration/cause is being checked. Preserve plugins;
no sandbox-lift. See [v59 evidence and limitations](NETWORK_V59.md).


2026-11-23: v57 SMB and FTP browsing/playback, audible sound and seeking were
confirmed separately by the user. FTP diagnostics corroborate authenticated
SMB reads, HEVC hardware frames and PCM, and later FTP listings/H.264 seeks;
the exact FTP 1080p HEVC clip is not independently identified in the trace.
v58 (`01.000.058`) adds bounded SMB2/FTP ZIP installs and torrent metainfo,
background previews with cancellation, and saved servers in the ZIP picker.
Full self-check, shared-source UI checks and signed build passed. All 23 files
were staged and readback-verified via FTP **2121**. After the user closed v57,
v58 was activated and mounted successfully. Its startup and console imports
are pending the user opening the title and confirming RAM.
Sandbox-lift remains excluded; installed plugins are preserved. See
[Network v58](NETWORK_V58.md). Discovery was observed in WSL and scheduled for
later implementation, as requested. Webcam capture remains disabled.

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
v53 is the latest published `PPSA99456` test build. It adds the in-video transport bar,
a seek preview with confirm and cancel, local audio selection, and an embedded
subtitle picker. Host tests passed, including playback continuity, track selection
and seeks. Subtitle rendering and v53 console verification are pending. See the
[UI work list](UI_WORK_LIST.md) for the v48–v53 changes.

v55 added the initial signed network test package. It adds saved SMB2/FTP server
connections, background network browsing, authentication and seekable playback
input, plus an authenticated FTP server and the observed Network settings.
Host service and UI checks passed. PS5 upload and runtime checks remain pending
because FTP 2120 was unavailable. See [Network v55](NETWORK_V55.md) and the
[UI work list](UI_WORK_LIST.md). Sandbox-lift is excluded.

v57 is the currently uploaded and running console test build, using FTP 2121.
It retains v56's startup-clock repair and fixes the SMB context crash caused by
an unavailable `arc4random_buf` import. The verified archive, file readback and
mount succeeded. Console logs show network file opens and hardware H.264 output;
fresh 1080p HEVC, audible sound and seek evidence is being collected separately
for SMB and FTP. Webcam-photo testing is paused at the user's request.
See [the port log](PORT_LOG.md).

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
