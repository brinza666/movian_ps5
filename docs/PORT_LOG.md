# Playback worktree port log

## 2026-11-22 — v55 SMB2 and FTP

- Active checkout is `C:/Users/brinza/Documents/deep/movian_ps5-playback-publish`,
  branch `test/native-playback`. Existing v54 Settings changes were preserved.
- Inspected WSL M7 7.0.273 Network properties and its FTP login UI. Retained REA
  ELF identity and package inventory evidence under `build/v55-wsl-network`.
  [Network v55](NETWORK_V55.md) distinguishes runtime observations, dependency
  source references and unverified desktop behavior.
- Added eight saved SMB2/FTP connections, separate credentials, Remember me,
  background sorted listings, list/grid folder navigation and seekable network
  input to the existing FFmpeg/Videodec2 player. Added observed Network settings
  and an authenticated FTP server for sandbox-visible title/library/USB roots.
- Fixed FFmpeg FTP reply 125 handling and added Unix LIST fallback. Added PS5
  SO_NBIO handling and numeric address helpers for the static transports.
- The full self-check passed, including the existing torrent RAM-cache,
  multi-peer, tracker, DHT and Matroska EOF/seek regressions. Controlled SMB2/FTP
  tests passed for login rejection, directory types, filtering, long filenames,
  seek/read and private settings. The FTP server passed read/upload, REST,
  listing fallback and path-protection checks. Host UI/player tests exercised
  H.264/AAC playback, forward/backward seek, login editing and Remember me.
- The signed v55 package is `build/PPSA99456-v55.zip`, version `01.000.055`.
  Build and archive verification are local. No sandbox-lift integration is
  included, and production `PPSA99455` and installed plugins were not changed.
- Console status was checked twice. ShadowMount port 10101 was reachable; FTP
  2120 refused connections and diagnostics 42000 was unavailable. v55 was not
  uploaded or mounted. No v55 hardware picture, audible output or controller
  result is claimed. The latest published release remains v53.
- Next check is to upload and mount v55 when FTP 2120 is enabled, then open a
  disposable SMB2 share and an FTP folder, capture hardware decoder and audio
  logs, and exercise forward/backward seeking. Test the title's FTP listener
  separately because the sandbox may deny its control or passive binds.

Automatic discovery, SMB1, share enumeration, WebDAV/FTPS, attribute storage,
remote control and network file-management actions remain open. See the
[UI work list](UI_WORK_LIST.md) for the visible pending controls.

Verified v55 archive SHA-256 is `2959ab59c5c6062bda94414783a2a97e6f5524ba6539dee533ba2e54e33390b5`. The final REA package bundle is
`build/v55-wsl-network/package-verified-rea.json`. Earlier package bundles in
that evidence folder precede the final socket/address dependency rebuild.


## 2026-11-22 — v55 console deployment attempt

- Reverified the saved v55 archive SHA-256 as
  `2959ab59c5c6062bda94414783a2a97e6f5524ba6539dee533ba2e54e33390b5`.
- At `192.168.10.238`, FTP 2120 refused connections or timed out on repeated
  probes. FTP 2121 responded with ftpsrv 1.16-ng-stable. Asked which FTP port to
  use because the user specifically requested 2120; no transfer was attempted
  through 2121 while that choice remained pending.
- ShadowMount's POST games list succeeded. It reported the installed test
  folder as version `01.000.047`, not mounted. The old folder and installed
  plugins were left intact while upload access was unresolved.
- Prepared read-only disposable SMB2 and FTP shares on workstation
  `192.168.10.29`, ports 1445 and 21219. Their media includes a 60-second
  1920x1080 HEVC/AAC Matroska fixture and a nested six-second clip. Local scripts
  and deployment evidence are under `build/v55-console`.
- Inspected the running Chiaki window for direct controller access. It displayed
  a blank white surface and did not provide a usable PS5 view. No PS5 controller
  test, login result, hardware frame or seek result is claimed.
- Next action remains upload and mount of the verified v55 artifact once the
  FTP port is resolved. Sandbox-lift remains excluded.


## 2026-11-22 — v55 uploaded; v56 startup repair

- User authorized FTP 2121. Uploaded v55 after moving the installed v47 folder
  outside `/data/homebrew` for rollback. All 23 package files passed readback
  verification; execute permissions were set. ShadowMount accepted the mount.
- The user reported v55 failed to start. Captured a native SIGSEGV at address
  zero from PID 103. The loaded text extent matched v55. Register values and
  binary disassembly pointed to the first clock-render path; startup file logs
  subsequently confirmed that v55 had entered main and completed UI loading.
- Replaced the v54 clock conversion with the earlier `localtime` path and
  bounded manual 12/24-hour formatting. Added checks for midnight/noon,
  conversion failure, invalid time fields and small/zero output buffers.
- Full self-check passed, including the new clock cases, SMB2/FTP service tests
  and existing RAM-cache, DHT and Matroska regressions. Built and uploaded v56,
  version `01.000.056`, preserving the failing v55 as a separate archive and
  rollback folder. All 23 files passed readback verification; mount succeeded.
- v56 archive SHA-256 is
  `0c8c3f7e488d5e3d2b38a976b2987c7a52e7228c7ee7ada1c2a7d27edcc4bffb`.
  User confirmed v56 reaches Home. Its startup log confirms fonts, pad and UI
  initialization. The diagnostic TCP bind still returns error 13; FTP log
  fallback is available at `/mnt/sandbox/download/PPSA99456` while it runs.
- Configured disposable SMB and FTP connections without existing network
  configuration or real NAS credentials. Moved the read-only fixture services
  from WSL to Windows Python 3.14, using its existing network allow rule; no
  firewall setting was changed. Both Windows services passed local login and
  file-read checks. Installed plugins and existing media settings were kept.
- The first captured v56 hardware decode was the bundled local HEVC clip,
  reporting 1920x1080, hardware frames and 48 kHz PCM. This is a local playback
  result, not yet an SMB or FTP playback result. Network and seek test evidence
  remains pending while the user performs the controller sequence.
- No sandbox-lift integration was added.


## 2026-11-22 — TV camera enabled

- User enabled the laptop's RGB webcam for observing the PS5 television.
  Verified Integrated Webcam through DirectShow at 1280x720 with MJPG. A 720p
  request without MJPG produced a black frame; default 640x480 also worked.
- Observed the physical TV showing Movian Home, with Plugins, Local network,
  Settings, Movian library, Title data, USB, Continue watching and Bookmarks.
  Current brightness is sufficient. No claim of playback smoothness or sound
  follows from a still camera image.
- Added `scripts/ps5-camera.py` for one-shot or timed local snapshots, with
  capture time/resolution metadata and camera release on exit. Instructions
  are in `docs/PS5_CAMERA.md`. Frames stay under ignored `build/camera`.
- The user reported both SMB and FTP playback with sound and forward/backward
  seeking. Record this as a user-reported pass. A subsequent FTP log capture
  from the running title still contained only the earlier bundled local HEVC
  decode, not a network open. Hardware network playback is not yet independently
  corroborated. The actual decoder filename is `hevc-test-decode.log`.
- v56 is the currently uploaded startup repair, with Home confirmed by the user
  and camera. Next network run can be observed with the camera and correlated
  with FTP diagnostics. Sandbox-lift remains excluded.


## 2026-11-22 — Webcam-assisted SMB open crash

- Started a fresh v56 run with the webcam showing the physical TV and prepared
  read-only SMB/FTP fixtures available. The user selected Server 1 and reported
  an error/freeze. The camera then showed PS5 error CE-108255-1.
- Kernel capture identifies PID 113, a listing worker SIGSEGV at address zero,
  with return addresses `0x48d8d4` and `0x48cf85`. No fixture connection from the
  PS5 reached the server. The crash happened before network authentication.
- Mapping against the exact v56 ELF locates the path in `network::connect` →
  `smb2_init_context`. Its first `arc4random_buf` call used a null function
  pointer; the register snapshot matches the eight-byte client-challenge call.
- Added a PS5 adapter using the existing `kern.arandom` sysctl source, bounded
  reads and explicit failure. The pinned SMB dependency now uses this adapter,
  rejects context initialization if entropy fails, and rejects encrypted-PDU
  nonce creation on failure. FFmpeg's `av_random_bytes` uses the same adapter.
  This removes the unsupported entropy import without sandbox-lift.
- Added entropy checks for bounded and partial reads, zero/oversized results,
  failures, clearing partial output, and null/zero-length arguments. Full
  self-check passed, including clock, SMB/FTP service and torrent regressions.
- Preparing v57 for console retest. v56 SMB/FTP success is not claimed; the fresh
  SMB test failed before connection and supersedes the earlier ambiguous
  user-reported pass. Raw evidence is under `build/v56-camera-test`.


## 2026-11-23 — Continue v57 with FTP logs

- User deferred webcam-photo testing in favor of a later visual approach.
  Confirmed no `ps5-camera.py` process is running. Updated camera instructions
  and handoff to keep capture disabled; existing evidence and helper are retained.
- Verified console FTP 2121 and ShadowMount are reachable. The installed
  metadata is `01.000.057`, the test title is mounted, and its startup log shows
  v57 entering main, loading fonts, initializing the controller and reaching UI.
- v57 had been built, uploaded with 23-file readback verification and mounted
  after the entropy fix. Its archive SHA-256 is
  `d1f7315a300db9691a9b4d7430dd4ce1ea671b203b5f2fd99f781542ea1d3d94`.
- Retrieved the current FTP logs into ignored `build/v57-live`. They contain
  network file opens and hardware H.264 frames, plus later torrent playback
  attempts. These mixed historical traces do not establish a protocol-specific
  1080p HEVC pass. Kernel TCP 3232 is currently unavailable.
- Read-only SMB2 1445 and FTP 21219 fixtures on the workstation remain reachable.
  Preserved the user's configured connections and installed plugins. Started
  a fresh incremental FTP log capture and requested a focused SMB browse,
  1080p HEVC, audible-sound and forward/backward seek run before testing FTP.
- No application rebuild or sandbox-lift integration was introduced in this
  continuation. Separate SMB/FTP results remain pending the focused tests.


## 2026-11-23 — v57 network results and v58 network packages

- User separately confirmed SMB browsing/playback, audible sound and seeks,
  then FTP browsing/playback, sound and forward/backward seeking.
- Fresh FTP logs retain successful SMB initialization/authentication, a 1080p
  HEVC hardware first frame with 48 kHz PCM and a separate 720p HEVC EOF run
  with 900 hardware frames. Later FTP listings succeeded, H.264 hardware frames
  appeared and seek targets 10/0 completed. Audible output is user-reported.
  The short HEVC trace does not prove sustained 1080p EOF, and the FTP trace
  does not independently identify the exact requested HEVC fixture. Logs are
  under ignored build/v57-live and build/v58-console/v57-*.
- Fixed local-only stat/fopen paths for plugin ZIPs, torrent metadata and the
  torrent transfer worker's metainfo read. A bounded reader reuses the existing
  authenticated SMB2/FTP File backend, with 8 MiB ZIP and 1 MiB torrent caps.
  Metadata stays in RAM; torrent payload cache policy remains unchanged.
- Network previews now run on a worker. Circle dismisses the result without
  freezing menus; a late response is discarded. ZIP installation uses the
  existing validation and temporary-write/rename flow. Existing plugins are
  retained. Saved servers are available in Plugins → Install from ZIP.
- Fixed remote directory extension filtering and null error-output handling,
  preventing a ZIP picker null access. Added protocol/size-only open diagnostics.
- Full self-check passed: bounded input faults/cleanup, entropy, clock,
  SMB2/FTP login/read/seek, actual remote ZIP installs, invalid/oversize rejection,
  installed-copy preservation, network ZIP filtering, FTP server regressions,
  RAM cache, multi-peer/DHT/private exclusion and Matroska EOF/seek.
  Shared-source UI tests passed remote torrent lists, ZIP preview/install,
  cancellation and network roots for both protocols (build/v58-ui).
- Inspected the installed WSL M7 build using REA ELF identity and live property
  captures. Its discovered: page exposes server rows with SMB2 host URLs.
  One selected server stayed loading, so successful share enumeration/prefill
  is still unverified. Documented the scanner as later work at the user's request;
  no old andoma source or new discovery implementation was used.
- Built and signed v58, contentVersion 01.000.058, isolated PPSA99456.
  build/PPSA99456-v58.zip SHA-256:
  ca72e93f24777fe93db15d6f4fa9fc0a75b5757e7473612af1d78df4078cda13.
  Uploaded and readback-verified all 23 files via FTP 2121, chmod 777, to a
  staging folder outside homebrew. v57 remains mounted. Requested Close Game
  before replacing the active folder and mounting v58. New console import
  tests remain pending. Sandbox-lift excluded; webcam capture disabled.

- Additional host integration passed: the actual transfer worker refetched the
  descriptor over SMB2 and FTP, then downloaded and hash-verified a controlled
  100-byte fixture in RAM on each path without creating download0/torrents.
  This tests the remote-descriptor handoff, not hardware playback. The seed
  process exited after the run; helper is build/v58-ui/peer-test.py.


## 2026-11-23 — v58 activation after Close Game

- User confirmed Movian was closed and requested v58 activation and separate
  SMB/FTP ZIP-install/torrent checks with RAM selected.
- Verified the staged title metadata, moved the old PPSA99456 folder outside
  homebrew as rollback, and renamed the verified v58 staging folder into
  /data/homebrew/PPSA99456. Only the isolated test title was changed. Persistent
  download0, credentials and installed plugins were not modified.
- ShadowMount scan returned HTTP 200. The immediate mount attempt returned
  409 while the scan was settling; after checking the test title state, the
  mount returned HTTP 200 with status 0 and mounted true. Active folder
  metadata is 01.000.058. ShadowMount's cached catalog version still reads
  01.000.047; do not treat that stale field as the installed build version.
- Activation and mount records are retained in ignored build/v58-console.
  The prior archive/readback hash remains unchanged:
  ca72e93f24777fe93db15d6f4fa9fc0a75b5757e7473612af1d78df4078cda13.
- Started fresh FTP diagnostics capture at build/v58-console/capture. Requested
  the user open v58, verify its footer and select RAM, then remain on Home.
  No v58 startup or console import result has been captured yet.
- Prepared controlled files in the existing read-only fixture share Imports:
  movian-network-v58.zip (API v2, distinct plugin ID) and
  movian-v58-ram-playback.torrent (real bundled 1080p HEVC media, private
  descriptor so the fixture does not use public DHT/retrackers). A controlled
  LAN tracker/peer is available for this descriptor only. These fixture reads
  are not a PS5 test result. User connections and existing plugins remain intact.
- Next: confirm Home and saved RAM setting, baseline installed plugin ZIPs,
  test one SMB ZIP install and torrent open/transfer, capture the matching
  logs, then repeat over FTP separately. Sandbox-lift remains excluded.
  Webcam and Chiaki control remain disabled.


## 2026-11-23 — v58 imports confirmed; v59 native HTTP and tracker chooser

- User confirmed v58 Home and RAM, SMB ZIP installation, and separately FTP
  ZIP installation plus FTP torrent file-list opening. Existing v58 UI logs
  contain successful package/import entries, but those entries predate the
  latest Home restart; the new per-protocol confirmations are user reports.
  The before/after private plugin listings retain the same 14 entries.
- Captured the failing SMB 19.1 GB torrent: RAM allocation succeeded at 64 MiB,
  but HTTP announces returned 0x8041013d/status 0 with zero verified pieces.
  Several real plugins returned the same error in 1–2 ms. Hardware playback
  was not demonstrated in that run. Saved RAM/cache/peer settings were read
  without overwriting the user's profile.
- Compared the installed WSL M7 BitTorrent settings through its live property
  API, rechecking the binary hash against retained REA ELF identity. Recorded
  exact numeric ranges, tracker chooser labels and retracker-policy differences
  in docs/NETWORK_V59.md. Opened the controlled reference torrent and observed
  a one-video file list using torrentfile:// input. Reference playback via the
  API remained unverified. No old andoma source was used.
- Fixed the missing sceNetInit-before-pool ordering using the public SDK's
  http2_get sample as the platform reference. Initialization is shared and
  mutex-protected, retries after failure and remains alive across requests.
  Added native HTTP failure-stage diagnostics without URLs or credentials,
  response reset and initialization/resource-cleanup regressions. The old
  error's failing native call is unknown; this remains a console fix candidate.
- Implemented the observed TCP & UDP / TCP / UDP tracker control, persisted
  in settings and passed into both transfer paths. It filters effective
  announces without changing privacy, DHT or direct-peer settings. Added
  effective cache/peer/protocol diagnostics and filter/persistence tests.
- Full self-check and shared-source UI checks passed. Built and signed v59,
  contentVersion 01.000.059, isolated PPSA99456. Archive SHA-256:
  b54442548d0689e75e99c53ff52fa4564c43a21bb40eacb3c2ca7f4743b4b5bf.
  All 23 files were uploaded via FTP 2121 with execute permissions and verified
  byte for byte. After Close Game confirmation, moved the old folder outside
  homebrew, activated v59 and mounted successfully (HTTP 200/status 0).
- v59 startup is captured through FTP: main, fonts, pad and UI load succeeded.
  Remote diagnostics still cannot bind (error 13); FTP fallback remains active.
  Fresh capture is build/v59-console/capture (session 42246). The user was asked
  to select RAM and TCP & UDP, start the same SMB Avanpost torrent and leave the
  title open. Its HTTP-stage, verified-piece and hardware result is pending.
  Installed plugins and private settings preserved; sandbox-lift excluded.


### v59 runtime follow-up — numeric tracker and hardware EOF pass

- Avanpost remains unsuccessful: native init rc=0, failed HTTP stage is send
  request (0x8041013d), UDP tracker DNS failed, DHT bootstrap nodes=0; no peers
  or verified pieces. Initialization alone did not fix hostname requests.
- Corrected test infrastructure: Windows rejected the old fixed peer/tracker
  port (WinError 10013). The replacement binds port 0 and records actual ports.
  Uploaded only a small private metainfo probe to /data/movian, with readback.
- The numeric-IP probe passed on v59: tracker HTTP 200/one peer, RAM cache=0,
  2,654,208-byte RAM allocation, 1080p HEVC hardware first frame, PCM, playback
  before transfer completion and a completed hash-verified transfer. The seed
  served 162 blocks/2,640,741 bytes. Decoder drained at 180 hardware frames;
  PCM drain submitted 288,768/nonzero 288,108/dropped 0/result 0.
- This is a small-file RAM/hardware EOF pass, not a 19.1 GB rolling-cache/seek
  pass. Audibility remains a user observation still to collect. Evidence was
  frozen at build/v59-console/probe-success before the WSL comparison run.
- Corrected WSL reference probe entered a video page, changed pause → play and
  advanced media time 0.833 → 3.367 seconds via its observed torrentfile:// URL.
  No visual picture/audio/seek pass is claimed from these properties alone.
- DNS is the strongest remaining lead: numeric HTTP works, while hostname
  HTTP/UDP/DHT fail. Asked for PS5 primary/secondary DNS and update-blocking
  context. No system network setting or registry was changed. Installed plugins
  remain preserved; sandbox-lift remains excluded.


## 2026-11-23 — Movian-to-Movian STPP investigation

- User deferred DNS work and prioritized reproducing WSL Local network → matv
  remote control for future PS5 automation. Used the running WSL M7 7.0.273
  and actual Android peer whose HTTP root identifies M7 7.0.289. No old andoma
  source was used. The attached 7.0.288 APK was not treated as that live build.
- WSL HTTP now listens on 44456; discovered it from the selected process sockets.
  The matv service has type movian, stpp:id:<instance-id>, and resolves to a
  direct stpp URL. Captured version-3 STPP over WebSocket at /api/stpp.
  Native legacy upgrade omits Host/WS version and sends Sec-WebSocket-Key: 1.
  The raw proxy sample has 440 subscriptions and 509 property notifications.
- Live UDP capture confirmed the 88-byte STPP discovery packet, multicast
  239.255.255.250:42000, unicast replies and advertised HTTP port. Individual
  flag meanings remain static deductions. Device IDs/endpoints/raw frames
  are retained only in ignored build/remote-control-rea.
- Controlled real-TV test: STPP opened General settings, changed Show filename
  extensions 1→0→1, and independent HTTP reads verified both changes. STPP
  opened KinoZal.TV; Back returned to the prior page. Final state is WSL
  connected directly to matv and Android on KinoZal.TV. Preference restored;
  no credential, plugin, update, permission or network setting was changed.
- Live subscriptions advanced the clock by two seconds and delivered log-buffer
  collection growth 43→44 after navigation without polling. Log entry fields
  are prefix/message/severity. Explicit unsubscribe stopped clock updates.
  Android /api/screenshot returned 404; pixel/audio mirroring is not established.
- REA: native-action PCAP 487 packets/complete coverage, evidence
  ev_c3ce5db4623bf60ac9167cb842605e30c93dae9dc7f42591c391b54ff79e50bf;
  discovery six packets/complete,
  ev_ec21ff9e20fcd28de9f714321429b5b52be3f9a7c277930cfda453ed0a63547b;
  selected WebSocket binary frames,
  ev_13fd9109076db098c10a9177a7772395b5d207e3a9f4f17f7c02d34faa7143d0.
  Native-connection capture has partial coverage; see REMOTE_CONTROL_STPP.md.
  Static protocol details came from identity-bound ELF/GNU disassembly after
  Windows REA Ghidra explicitly rejected ELF; no REA decompilation is claimed.
- Added scripts/movian_stpp.py: optional websocket-client dependency, read-only
  default JSON event stream, explicit navigation/actions/int writes and typed
  codec. Six standard-Python wire tests passed, now in scripts/self-check.sh.
  Reusable Client also passed actual Android subscription/unsubscription and
  navigation/Back tests. No PS5 app source or package was changed for this tool.
- Documented exact evidence, protocol and three-part implementation plan in
  REMOTE_CONTROL_STPP.md and static details in STPP_ARTIFACT_FINDINGS.md. Added
  one acceptance test per remote feature to SPEC.md. Next unfinished test is
  real WSL viewer interoperability with our host-preview server/property graph.
  PS5 transport/server remains unimplemented. Existing bind error 13 motivates
  an optional outbound paired bridge rather than sandbox-lift. App relaunch and
  pixel/audio inspection need separate supported paths. Installed PS5 v59 stays.


## 2026-11-23 — v60 STPP server and shared UI callbacks

- Implemented the first STPP server/property slice in test title PPSA99456.
  The queue copies commands into the UI thread; existing settings/navigation
  callbacks share saved state with the local controls. See [v60](STPP_V60.md).
- Unmodified installed WSL M7 7.0.273 rendered eight Home services and
  Settings/General. Actual native toggle tag13 changed extensions On→Off→On;
  the saved file and pushed view agreed. Back returned Home. Repeated this
  after the final vector/pthread server adaptation. [Evidence](STPP_V60_NATIVE.md).
- Full optional host self-check passed before packaging; final core ASan,
  UBSan/leak/socket and real UI callback tests passed again. Native compile,
  link, conversion, signing and integrity passed. SDK C++ strings are linked
  after platform imports; the required system-module set matches v59.
- Version 01.000.060 ZIP is 7,706,570 bytes; SHA-256
  `73b8b7a940703cf24abe6668ee53368ed74a451096b6c82c5e3b8421d419ef19`.
  Eboot SHA-256
  `7befb1d484baf4173beeda810d7950dc5807542802ecea0c255d450b40dab7e3`.
- Latest user request supersedes historical no-lift/preserve-plugin rules for
  this experiment. Added optional one-shot access and fresh-reset requests,
  excluded from public ZIPs. Access preloads modules and retains an open probe
  log/root descriptor; failed title-path restoration exits before the UI.
  Existing filesystem proof never counts as verified credential elevation.
- Archived and independently verified 28 inactive console backup folders, then
  removed only those folders (404,024,807 bytes). Consolidated 44 saved packages
  and the independently verified closed v59 profile image under
  `D:/GPTDEEP/PS5-Movian-Backups/2026-11-23`. Media and production title untouched.
- PS5 went offline before v60 upload: FTP2121 and ShadowMount10101 unavailable.
  v59 remains installed. Fresh-reset/access/listener/console remote tests are
  pending; no v60 PS5 playback or audibility claim. DNS/Avanpost stays deferred.
- GitHub publication and Pages links are recorded after verification below.
