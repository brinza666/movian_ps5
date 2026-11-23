# Movian desktop parity specification

Evidence cut: 2026-11-21. Desktop scope includes the inspected M7 7.0.273 Linux x86-64 build running under WSLg on Windows 11, and the desktop releases and variants listed by the publisher. The Linux ELF has build ID `cecc8ab5765c085d19ffdc4e5c86649186a62cfa`. Read desktop behavior from the running WSL build and retained REA evidence. Use TorrServer and its pinned dependencies as the BitTorrent implementation reference. The [desktop inventory](docs/M7_CAPABILITIES.md) and [retained property captures](docs/harvest/m7-7.0.273-wsl2/) contain the discovered options and their platform notes.

Desktop comparison targets are Linux x86-64, Linux ARM64/Raspberry Pi where offered, macOS Apple silicon and Intel, macOS legacy Intel, Windows via WSL2, and the historical packaged Chrome/NaCl desktop target. A listing or a shared string is evidence of availability or implementation intent. It does not certify behavior on a second binary. Android 7.0.288 and iOS/tvOS are comparison targets for features that vary by platform. Android-only DRM, tunnel mode, and device permissions do not enter the PS5 plan. Older releases and binaries absent from the workspace remain uninspected. [Publisher platform list](https://apps.movian.eu/)

Source priority is a runtime observation of the exact build, the corresponding shipped artifacts, and then upstream source or the release notes. Test one behavior per row and record the version, input, expected output, and result. For each named variant within a row, repeat that row’s test separately. A static observation never counts as a console test.

## 1. Native playback, navigation, and persisted settings

| Desktop feature or option | Acceptance test |
| --- | --- |
| Home services, icon visibility, and service ordering | Toggle one service, restart, and check its presence and order. |
| About, version, system information | Compare the displayed identity with the running package. |
| Keyboard and global open box | Enter and open a known media path using the system keyboard. |
| Skin, UI scale, colors, background image | Change each option and verify its visible effect after restart. |
| Landscape lock, animations, back behavior, loading indicator | Exercise each setting on a compatible desktop and compare the resulting UI event. |
| Language, character encoding, keyboard layout | Open the same non-ASCII title and enter its name under each supported setting. |
| Clock format and offset | Compare the UI clock to a controlled timestamp. |
| Sort ignoring articles, folders first, supported types, newest first | Reorder a fixed directory and compare the expected row order or filter. |
| Local files and removable media | Browse an attached source and open a known video. |
| ZIP and RAR browsing | Open an archive as a directory and select a known file. |
| File deletion and copying | Delete or copy a disposable media fixture and verify the result. |
| Bookmarks and per-file settings | Save a bookmark and a file setting, restart, and reopen the same item. |
| MP4, MKV, WebM, MOV, AVI, MPEG-PS/TS, FLV, ASF, Ogg, MXF, NSV | Open a fixture for each container and verify visible playback to EOF. |
| DVD/VCD/SVCD, VIDEO_TS and ISO | Open a test disc image and select a title. |
| H.264, HEVC, VP8/VP9, AV1, MPEG-1/2/4, DivX/XviD, VC-1, ProRes when available | Decode a fixture for each codec and verify picture dimensions and EOF. |
| Hardware decode and software fallback | Run the same eligible fixture in each available mode and check the reported decoder. |
| 1080p and 4K rendering, HDR metadata, HLG/HDR10/Dolby Vision indicators | Compare output and reported metadata to known fixtures with the relevant metadata. |
| Deinterlacing, upscaling, adaptive format, and match refresh rate | Use a reference pattern and verify the selected video output mode. |
| Video buffers and prebuffer targets | Throttle a fixture connection and measure the startup threshold. |
| OSD, seek bar, status badges, clock, and button behavior | Open the overlay and compare its controls and values to the playing fixture. |
| Resume, restart, mark seen/unseen | Save state, reopen the item, and check each action independently. |
| Seek step and playback speed | Move one step or play at one selected rate and compare timestamps. |
| Play queue, auto-next, shuffle, and repeat | Use a three-file queue to verify the selected ordering rule. |
| MP3, AAC/M4A, FLAC, Vorbis, Opus, WavPack, APE, Musepack, WMA, AC3/E-AC-3, DTS, MIDI, tracker modules | Play a known audio-only fixture for each supported input and verify PCM or documented output. |
| Audio port, PCM, passthrough and IEC61937 | Check the sink’s reported format with a known fixture. |
| Transcode, upmix, downmix, gain, delay, and speaker test | Feed a channel-identification fixture and verify the selected transformation or timing. |
| Per-file audio volume and master volume | Change one setting and measure the level of a test tone. |
| Embedded, sidecar, central-folder, and plugin subtitles | Play an identical timed cue from each source. |
| SRT, WebVTT, ASS/SSA, DVB, PGS and VobSub where available | Render a fixture cue in each supported format. |
| Subtitle language order, priority, style, SDH filter, embedded style override, opacity and position | Compare each setting against two known tracks and a reference cue. |
| JPEG, PNG, SVG, WebP, experimental AVIF/HEIC where shipped | Open a labeled fixture, then advance the slideshow. |
| Screensaver delay and image source | Shorten the delay and verify the selected local test image. |

## 2. Native torrent engine and file access

| Desktop feature or option | Acceptance test |
| --- | --- |
| Local `.torrent`, `torrent:`, `torrent:browse:`, and `torrent:video:` | Open each form with a controlled descriptor and check its file list or playback action. |
| Plugin-delivered inline or fetched metainfo | Invoke the plugin route and compare the resulting info hash to the fixture. |
| Magnet metadata resolution | Open a magnet against a controlled peer and verify its resolved info hash and files. |
| HTTP/HTTPS tracker discovery | Announce to a controlled tracker and connect to the returned peer. |
| UDP tracker discovery | Announce to a controlled UDP tracker and connect to the returned peer. |
| IPv4 DHT peer discovery for public rolling-cache transfers | Bootstrap against a controlled DHT node, obtain peers with no tracker, verify pieces for a distant seek, and confirm a private descriptor sends no DHT queries. |
| Tracker protocol selection | Run the same fixture with TCP, UDP and combined settings and record which tracker was contacted. |
| Additional-tracker injection and its selection modes | Append a configured HTTP/HTTPS retracker for a public fixture and verify a private fixture keeps its original announces. Compare the announced set for each remaining desktop selection mode. |
| Per-torrent and global maximum connections | Offer more peers than the limit and count simultaneous connections. |
| Per-peer requests in flight | Stall the fixture peer and count block requests before it responds. |
| Sliding request window and startup preload | Release one block and withhold the rest until a replacement is requested. Verify playback startup waits for two contiguous verified Matroska pieces when both exist. |
| Upload speed | Measure uploaded bytes per second against a deliberately throttled test peer. |
| Cache directory and PS5 cache location | Select a disposable directory on desktop. On PS5, exercise RAM, internal storage and USB independently and verify where each transfer holds its payload. |
| PS5 cache size, read-ahead and preload | Set a 32 MiB budget with 8 MiB pieces, 50% read-ahead and 75% preload. Verify three pieces are required at startup, playback schedules two forward slots, and older data remains readable until evicted. |
| PS5 connection limit, requests, block timeout and retracker mode | Restart after saving non-default options, check the effective peer policy in diagnostics, and verify tracker replacement and removal preserve private torrents' original announces. |
| Bundled default tracker list | Start without a custom tracker list and verify eight HTTP and four UDP URLs are loaded once. Add a custom list and verify it takes precedence. |
| Free-space percentage, per-torrent cache size and active-memory limit | Exceed each configured budget independently and verify that use stays within it. |
| Clear cache | Clear a fixture cache and verify unrelated files remain present. |
| Multi-peer availability and scheduling | Serve alternate pieces from two peers and verify both supply required data. |
| Hash verification | Corrupt one piece and verify the reader never receives it. |
| Deadline and seek priority | Seek to an uncached offset and verify the requested piece precedes later read-ahead work. |
| Seekable `torrentfile` read and cache eviction | Read across a piece boundary, seek far away, then re-request an evicted piece. |
| Multi-file torrent browse, playback and copy | Play and copy one file whose first piece is shared with a neighboring entry. |
| Play before the download finishes | Throttle peer transfer and verify a hardware first frame precedes completion. |
| Cancellation while buffering | Close a stalled transfer and verify decoder, sockets and cache are released. |
| End of stream | Run a short seeded fixture through decoder drain and PCM drain without a stall. |
| Seeder, leecher, known peers, active peers, bitrate and cache statistics | Compare each reported figure to a controlled tracker and two peers. |
| Torrent diagnostics for tracker, connection, download, upload and disk I/O | Enable each category and check the corresponding event without publishing tracker secrets. |
| Feature-length single-file Matroska | Open the supplied 19.1 GB torrent, start hardware playback, seek forward and back, and confirm audio and picture while storage stays within budget. |

The running M7 build exposes a BitTorrent UDP tracker and torrent diagnostics. Use TorrServer's separate torrent engine, reader, and cache as the implementation reference. The PS5 port has a bounded, seekable verified-piece cache with concurrent peer workers. Discovery, magnets, multi-file scheduling, settings, and feature-length console playback have separate acceptance tests. [TorrServer source](https://github.com/YouROK/TorrServer/tree/MatriX.151/server/torr)

PS5 defaults to RAM for new torrent payloads. The “Torrent cache path” setting also offers explicit internal storage and USB choices. A backend failure leaves the selected mode in effect. The no-storage self-check runs both transfer paths with no title storage directory and verifies that they create no payload files. Verify the feature-length console fixture in RAM mode.

## 3. Plugins, network services, and desktop settings parity

| Desktop feature or option | Acceptance test |
| --- | --- |
| Network-share plugin ZIP installation | Browse a controlled SMB2 or FTP folder, install its API v2 ZIP, restart, and verify that plugin and the pre-existing installed plugins remain available. Repeat separately for each protocol. |
| Network-share torrent metainfo | Open a controlled SMB2 or FTP `.torrent`, compare its file list and info hash to the local fixture, then start the existing peer transfer with RAM selected. Repeat separately for each protocol. |
| Discovered network server selection and prefill | Select a controlled discovered server without typing its host; verify protocol/host/share fields against the WSL observation, authenticate and browse. Discovery and prefill remain a later unfinished slice. |
| API v2 Duktape, ZIP install, ZIP-relative modules | Install a signed-off fixture ZIP and execute a nested import. |
| Plugin catalog, uninstall, autostart, search registration | Exercise each action independently on a disposable plugin. |
| Official and alternate repos, automatic install, version gating | Switch feeds and verify the displayed catalog and compatibility rule. |
| Plugin routes, HTTP, inspectors, redirects, DOM, text and per-plugin stores | Run one deterministic route fixture against each host API and check its output. |
| Plugin settings, consent, account session and item hooks | Toggle an option, accept test consent, authenticate to a mock origin and invoke an item hook. |
| Plugin artwork and images packed in ZIPs | Render a fixture logo over HTTPS and from the archive. |
| Plugin dialogs and properties | Drive one supported native dialog and verify its returned value. |
| Plugin module inventory | Check each module listed in the retained desktop inventory against a versioned API fixture before advertising it. |
| HTTP and HTTPS progressive range reads | Seek in a served file and compare the requested byte range and rendered frame. |
| HLS TS, HLS fMP4, quality and resolution cap | Play both segment types and verify the selected rendition. |
| HLS live edge, chase-play, audio group and WebVTT | Verify each option with a controlled live playlist. |
| Clear DASH via mp4dash | Open a clear DASH fixture and compare timestamps across segment boundaries. |
| Icecast, PLS and M3U | Select a controlled station from each format and verify audio. |
| HTSP, raw UDP and plugin videoparams | Open a fixture for each input and verify the expected route reaches the player. |
| SMBv1, SMB2/3, WSD and NetBIOS | Discover or open a disposable LAN share for each supported protocol. |
| FTP client, FTP server, WebDAV and WebDAVS | Read a fixture through each service, then copy one disposable file where writes are supported. |
| UPnP/DLNA and optional DIDL metadata probe | Browse a controlled server and compare item metadata. |
| HTTP/SOCKS proxy, IPv6, SMB buffer and xattr | Verify traffic or attribute behavior for each controlled option. |
| TMDB, TVDB, IMDb, NFO, original titles and metadata languages | Compare one deterministic metadata result for each source or setting. |
| Last.fm scrobble | Verify a mock account receives one test event. |
| Movian STPP discovery | Decode a controlled peer's 88-byte advertisement and select its advertised endpoint from Local network. |
| Native and modern WebSocket STPP v3 handshake | Connect the unmodified WSL client and a modern client to the same server and verify version and instance identity. |
| Remote live properties and exported references | Subscribe to a page, resolve its child models and verify a pushed change without polling. |
| Remote plugin/page navigation | Open a known plugin through STPP OpenURL and return with Back, comparing the local and remote current page. |
| Remote boolean settings | Change and restore one disposable option; verify the actual saved value and pushed notification through an independent client. |
| Remote numeric settings | Change and restore one bounded number and verify the shared backend accepts its bounds. |
| Remote text settings | Edit and restore a non-sensitive text field and verify UTF-8 text and persistence. |
| Remote dropdown settings | Select another option, cancel a second change, and verify both views agree on the committed selection. |
| Remote playback and seek | Start known media, seek, select a track and stop through the remote model while verifying decoder output. |
| Remote live log buffer | Subscribe before a controlled event and verify a new entry with severity/prefix/message and detectable sequence gaps after reconnect. |
| PS5 outbound remote-control transport | Connect the restricted title through a paired bridge and control its real settings with sandbox-lift excluded. |
| Remote artwork/resources | Fetch a fixture icon through the required resource messages and verify dimensions and cancellation. |
| Remote lifecycle and stale references | Disconnect/reconnect, reject stale exported references, and restore subscriptions without interrupting playback. |
| Remote pixel inspection | Fetch a controlled framebuffer image and compare it to the known rendered fixture. |
| Remote diagnostics | Enable the LAN listener, read `/api/logfile/0` during a torrent run, confirm cache, peer, and seek events, then disable the listener and confirm it closes. |
| System name, upgrade channel, auto-upgrade, general cache and logs | Use test profiles and check the selected identity, update offer, cleared cache or log output separately. |
| TV controls, device-specific capabilities, developer views | Verify each feature only on the platform and hardware that exposes it. |

The 2026-11-23 [STPP investigation and three-part PS5 build plan](docs/REMOTE_CONTROL_STPP.md)
records actual WSL/Android discovery, property subscriptions, plugin navigation,
one restored setting mutation and live clock/log updates. A host controller and
wire self-check exist. These observations do not mark the PS5 server or all
remote setting/playback/resource tests complete.

## Build plan

1. **Playback foundation.** Preserve the last working package and its hashes. Test hardware and software media, seek, audio drain, EOF, switching, local navigation and settings. Use the isolated test title for each console claim.
2. **Native torrent file access.** Inspect desktop behavior and settings, then test in order: verified bounded cache, complementary peers, player read/seek/cancel, supported tracker and magnet sources, and the 19.1 GB console fixture. Require a self-check and separate console evidence for every claim.
3. **Desktop parity surface.** Port only confirmed desktop options, settings and plugin behavior. Run fixture tests for the APIs before testing real plugins. Add playback, network and UI families one at a time. Update the capability matrix and port log after each step.

Run `wsl -d Ubuntu-24.04 -- bash scripts/self-check.sh` from the worktree. The self-check runs the native cache and loopback peer tests, plugin JavaScript syntax, and build script syntax. Console captures are separate.

For the FFmpeg packet and seek comparison, set `MOVIAN_HOST_FFMPEG_DIR` to a native FFmpeg 6.1.2 build with the Matroska demuxer, and `MOVIAN_MATROSKA_FIXTURE` to the generated test MKV. The self-check then compares the growing cache to the same completed file.

Repository workflow, context handoff and terminal verification are described in [AGENTS.md](AGENTS.md). Chat commands, MCP connectors, and assistant lifecycle hooks belong to their chat host.


## v60 first STPP step acceptance (2026-11-23)

Native WSL Home/Settings/shared boolean callbacks passed against the actual UI
adapter, including persisted restoration and pushed local changes. Core and UI
socket tests are in the self-check; full UI checks require the optional host
network/plugin dependencies. See docs/STPP_V60.md for the console boundary.

| Authorized test-only behavior | One acceptance test |
| --- | --- |
| One-shot per-title access | Launch with the separate exact request, capture helper/root/path proof and independently verify bind; relaunch without consuming a second request must not run it again. |
| Fresh plugin/config profile | Back up the closed image, apply the one-time request and verify old installed plugins/settings are absent while media/resume/bookmarks remain. |
| Console backup cleanup | Verify every copied backup file against an independent read before deleting only its scoped inactive console folder. |
