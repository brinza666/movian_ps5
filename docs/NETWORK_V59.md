# v59 HTTP initialization and desktop tracker controls

Evidence date: 2026-11-23. Isolated title `PPSA99456`; no sandbox-lift.
Installed plugins, saved network connections and the selected RAM cache remain
intact. This work does not replace the peer or cache engine.

## v58 console imports and torrent failure

The user confirmed SMB ZIP installation, then separately confirmed FTP ZIP
installation and FTP torrent file-list opening. Existing v58 logs include
`network plugin ZIP ready`, `installed local plugin ZIP` and `network torrent
file list ready`. The requested runs were user-observed; the retained UI
entries predate the latest Home restart and do not individually timestamp
each new confirmation. Keep that distinction when reporting the result.
The private plugin-directory baseline retained the same 14 directory entries
before and after this check; no plugin removal was performed.

The SMB 19.1 GB torrent reached its file list and allocated 67,108,864 bytes
of RAM successfully. Its HTTP announces then returned status 0 and
`0x8041013d`, with zero verified pieces and zero workers supplying data.
Several real plugins failed with the same HTTP result in 1–2 ms. This run
failed before a decoder could demonstrate hardware playback. Changing RAM
size is not an established fix for this network failure.

Saved PS5 values captured read-only: RAM, 64 MiB cache, 80% read-ahead,
5% preload, 8 connections, 32 requests per peer, 13-second block timeout and
original trackers. The profile was not overwritten. Raw logs and private
directory listings remain in ignored `build/v58-investigation`.

## Native HTTP correction and diagnostics

The public PS5 SDK sample `samples/http2_get/main.c` calls `sceNetInit` before
creating its network pool. The port's HTTP and DNS paths omitted that step.
The inspected local sample SHA-256 is
`9a7d56fc274c2c31dbfff5079bc9c5b3ff66bde35e9d3671e1f77adf560b3bbd`.

v59 adds mutex-protected, process-wide native network initialization before
HTTP or hostname-resolution pools. It accepts an already-initialized result,
retries initialization after failure and does not terminate the shared library
when one request ends. HTTP diagnostics now identify the failed initialization,
pool, SSL, HTTP, template, connection, request, send, status or read operation.
The records omit URLs, headers, credentials and response bodies. Response state
is reset at the start of a request, and allocated handles retain scoped cleanup.

The old trace does not identify the failed native call. Missing initialization
is a concrete API-order defect, but whether it explains the console error
remains a hypothesis until v59 produces a matching HTTP success/failure trace.
Do not claim plugin login or torrent playback fixed from host mocks alone.

## WSL comparison using retained REA identity and live observations

The installed `/usr/bin/showtime` SHA-256 was checked again and matches
`da133c4a6ee509da8f3b74dd2dfd1b445988381c5b3210d57b15a2e3a96ac9b1`.
Retained REA ELF evidence
`ev_b592f41edf83f81bd55356450bda2b3cbc1726efb46946e4fd06b3afd3205bfb`
identifies GNU build ID `cecc8ab5765c085d19ffdc4e5c86649186a62cfa`.
The REA record establishes binary identity. The live property captures establish
the settings and navigation observations below; no old andoma Git source was used.

| WSL BitTorrent control | Observed range/choices | PS5 state after this change |
| --- | --- | --- |
| Max upload speed | 5–500 Mbit/s, step 5 | Backend pending |
| Max usage of free space for caching torrents | 0–95%, step 1 | Backend pending; RAM mode does not use a disk-space percentage |
| Max number of connections | 100–2000, step 100 | Existing native worker limit remains 1–8; desktop range is not implemented |
| Max concurrent requests per peer | 5–50, step 1 | Existing native setting remains bounded to 32 requests |
| Max memory for active transfers | 96–384 MB, step 4 | Separate desktop budget not implemented |
| Max cache size per torrent | 160–4000 MB, step 16 | PS5 has its existing RAM/internal/USB cache budget, not this desktop control |
| Inject additional trackers | Off, Best, Best (IP), All, All (IP) | PS5's existing original/add/replace/remove modes are different; exact desktop policies remain pending |
| Tracker protocols | TCP & UDP, TCP, UDP, in that order | Implemented and persisted in v59 |
| Torrent cache path | Directory request, default value available | Existing RAM/internal/USB chooser remains PS5-specific |
| Clear cache | Action | Desktop-equivalent backend pending |

Tracker protocol choice now filters the effective announce set before either
transfer path starts: TCP permits HTTP/HTTPS; UDP permits UDP trackers. It
preserves torrent privacy and does not toggle DHT or direct-peer transport.
New transfers log the protocol, connection/request limits and cache policy.
No memory, preload, read-ahead or peer limit was silently raised.

Opening the controlled HEVC torrent in WSL produced a directory with one video
row and a `torrentfile://<infohash>//<filename>` URL. This confirms the observed
file-list → torrent-stream input flow. The first reference playback attempt failed because the fixed-port test
server had not started (Windows rejected its port). After correcting the
fixture to allocate available ports and using its new descriptor, the WSL
page became `video`; media state changed from pause at time 0 to play and
advanced from 0.833 to 3.367 seconds. These are runtime property observations;
visible picture, audible sound and seeks were not independently observed. Raw runtime captures are in
ignored `build/v58-investigation/wsl-*`. Cache scheduling details still require
separate runtime observations and the existing TorrServer/pinned-engine reference;
settings labels alone do not establish the scheduler's behavior.

## Validation and deployment

Full self-check passed, including native HTTP initialization/retry and failed
stage/resource cleanup, TCP/UDP tracker filtering, persisted settings, real
SMB2/FTP installs/reads, RAM cache, multi-peer/DHT/private exclusion and Matroska
EOF/seeks. The shared-source UI harness passed ZIP/torrent flows and the three
tracker labels, order, selection and saved reload. These are host checks.

Signed archive `build/PPSA99456-v59.zip`, SHA-256:
`b54442548d0689e75e99c53ff52fa4564c43a21bb40eacb3c2ca7f4743b4b5bf`.
All 23 files were staged with execute permissions and byte-for-byte readback
via FTP 2121. After the user confirmed Close Game, v59 was activated at the
existing test folder and mounted (HTTP 200, status 0). The old v58 folder is
retained outside homebrew. Persistent download0 was untouched.

v59 startup logs confirm main, font/controller initialization and UI load.
The diagnostics listener still cannot bind (error 13); FTP fallback capture
is running at `build/v59-console/capture`. The pending console acceptance is
the feature-length SMB Avanpost run after resolving the hostname failure.
See the measured console follow-up below.


## v59 console follow-up: hostname failure isolated; numeric probe passes

The Avanpost attempts still fail. Native initialization returned 0/ready,
HTTP failed specifically at `send request` with 0x8041013d, UDP trackers logged
DNS failure, and DHT bootstrap found zero nodes. One run ended with 16 effective
trackers, zero peer candidates, zero workers and zero verified pieces. This
falsifies the claim that initialization alone fixes the observed connection
error. The new stage diagnostics work and narrow the next investigation.

A controlled numeric-IP tracker probe was then prepared. Its peer and HTTP
server bind port 0 so Windows chooses available ports; the earlier fixed-port
server had exited with WinError 10013. The numeric tracker returned HTTP 200
on a host probe. Its private `.torrent` was uploaded and readback-verified as
`/data/movian/movian-v59-http-probe.torrent`; only metainfo is stored there,
while video data is served from the laptop.

The PS5 run, captured before the later WSL test, shows:

- `start cache=0` (RAM), protocols 0 (TCP & UDP).
- Native initialization rc 0, tracker status 200, one peer.
- RAM allocation 2,654,208 bytes for the 2,640,741-byte media file.
- First hardware HEVC frame at 1920×1080 and 48 kHz PCM.
- `peer playback started before download completed`, followed by a verified
  completed download. The controlled peer served 162 blocks/2,640,741 bytes.
- Decoder EOF after 180 hardware frames; AudioOut drain submitted 288,768,
  nonzero 288,108, dropped 0, result 0.

This verifies the small-file RAM torrent path and hardware decode through EOF.
It does not establish audibility, sustained 19.1 GB rolling-cache playback or
seeking. The numeric control succeeding while hostname HTTP, UDP and DHT
bootstrap fail strongly points to hostname resolution as the remaining shared
blocker, rather than a global HTTP transport or decoder failure. The precise
DNS configuration/API cause is not yet established. The user was asked for
PS5 primary/secondary DNS and whether they form part of update blocking; no
system network settings or registry data were modified.

Raw evidence is frozen in `build/v59-console/probe-success` before WSL added
its own requests to the fixture service log. Later live captures remain under
`capture/`. The fixed-port fixture's failure must not be counted as a console
playback failure or as evidence against the decoder.


A same-host WSL DNS check resolved router.bittorrent.com, router.utorrent.com
and dht.transmissionbt.com while the PS5 bootstrap attempts yielded no nodes.
The user has a pending request to report PS5 primary/secondary DNS. Standard
FTP-exposed resolv.conf paths were absent; no registry or protected settings
were inspected or modified.
