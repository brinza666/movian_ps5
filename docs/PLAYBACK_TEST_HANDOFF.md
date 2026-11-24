# Publication and remote lifecycle checkpoint — 24 November 2026

v64 release is public and all three asset digests match local files:
https://github.com/brinza666/movian_ps5/releases/tag/playback-test-v64
Source tag points to 318631e. Branch website update is 36e9a9e; main publication
is 24fda04, followed by main README link correction ed167fa. Pages workflow
45435801322 succeeded on attempt2 after a transient deployment-backend failure;
HTTP200 page now contains v64 at https://brinza666.github.io/movian_ps5/.
Unrelated camera prototypes remain untracked and unpublished.

New guarded host script scripts/ps5-title-control.py targets only PPSA99456,
with six tests in tests/ps5_title_control.py and a self-check entry. Use status
first against the current verified console IP, then the documented Minion
launch/close sequence. Host tests pass; actual console status failed at the token
GET with connection refusal, so no launch/close mutation was sent. Both Windows
and WSL currently refuse saved-address ports2121/10101/9180. The async current-IP
question remains unanswered. No console-only one-shot opt-in was changed.
No existing browser console-control tab supplied a newer address. WSL is local
Home; the owned v64 host server is stopped. Do not restart webcam/Chiaki.

# Latest continuation — v64, 24 November 2026

The user has just reported "PS5 is up". Rechecks at saved 192.168.10.238 on FTP2121,
ShadowMount10101, Minion9180 and STPP42000 still time out (some HTTP attempts
briefly refused). Laptop Wi-Fi is 192.168.10.29/24. The known console MAC remains
in a stale neighbor entry. An async question asks the current PS5 IP; continue
from its answer without losing the console STPP objective. No launch request or
replacement upload occurred. v62 remains mounted.

v64 is implemented and packaged: build/PPSA99456-v64.zip, 7,713,845 bytes, SHA-256
422c96f03f906c9686f7e475770fc5c169848dd7cfd04887948c9ab3d82f6bae.
Eboot SHA-256 605a9affb1765d331b1575fa9b4ff08ecdd9e612f83215ff3d0712e492814b1f.
Full optional self-check, signed integrity and actual unmodified WSL remote
plugin-details/Uninstall all PASS. Host fixture evidence: build/v64-console and
build/v64-native. WSL is restored to its LOCAL Home; the owned host server was
stopped. No v64 console result is claimed. See docs/UI_V64.md and release notes.

P02 now has a real backend (plugin_storage.hpp / plugins::uninstall), asynchronous
local/UI dispatch, native plugin metadata/status and dynamic STPP Uninstall.
Metadata/status survive in a bounded removed-plugin history so an open native
popup updates to Not installed. R3 opens a plugin from the catalog, Square opens
settings. Full description/remote artwork, Upgrade and reinstall provenance are
still unfinished. Next desktop feature is P03 only after controlled native
Upgrade observation. Do not count it complete just because Install replaces ZIPs.

First console action remains launch mounted v62 via the documented Minion API,
collect helper/root/bind logs and test WSL against the ACTUAL PS5, then close only
its confirmed app ID before deploying v64. Do not infer a process from its PFS
mount or send eboot to9021. Use a fresh verified closed profile backup before any
new reset request. No repeated profile wipe is needed. Console-only opt-ins stay
out of public packages. All earlier authorization/scope below remains applicable.

# Autonomous work handoff — 2026-11-24, v63

Active checkout: `C:/Users/brinza/Documents/deep/movian_ps5-playback-publish`,
branch `test/native-playback`, origin `brinza666/movian_ps5`. The user is away
for 10 hours and authorizes autonomous test-title launch/close and further
planned-option work. Do not ask for physical Cross or Close Game. No webcam or
Chiaki. The explicit default-off access experiment and backed-up fresh profile
are authorized; never package request files. Production PPSA99455 stays separate.

## Saved builds and console state

- v62 remains mounted at `/data/homebrew/PPSA99456`, metadata `01.000.062`.
  Its 29 files were readback-verified. ZIP SHA-256:
  `2757c2c476427786a863c5c909af6e9ca0f8c6fa2dd70c8637315f03c99f33ae`.
  Eboot: `b6e4fe60c66e90935f36c5bc72a6cc99c6a3f6cceb4bd7208f961cd82d64af4c`.
  It has not been launched or accepted on console.
- v63 is built and host-tested, not uploaded. `build/PPSA99456-v63.zip` is
  7,707,847 bytes; SHA-256
  `416dc0e6332c94c959046933960f892153f1dfb76630a641ef05f9bf994e6e14`.
  Eboot: `70225f7753097d9dbbb8e61c2129e49269f873fca28780bf3728ca1b83a33436`.
  See `build/v63-console/self-check.log`, `build.log`, `package-verification.json`.
- v61 actually reached Home and completed the fresh reset; old plugins absent.
  Filesystem access was `path=existing`, not helper credential proof. STPP42000
  and diagnostics42001 both failed bind with errno13.
- Recovered v60 access log proves deliberate shell exit after failed root-restore
  loader lookup. v62 fixes direct loader imports/module lookup and explicitly
  requests the unchanged pinned owned helper protocol. Full host regressions
  passed; actual helper/root/listener acceptance remains open.

## Next unfinished acceptance

FTP2121, ShadowMount10101 and Minion9180 initially worked, then refused and now
all time out before any autonomous launch request. Recheck after meaningful work.
When reachable, use the existing Minion exact-title API, documented in
`docs/SHADOWMOUNT_CONTROL.md`: acquire a token only in memory, read actual processes,
launch PPSA99456 only when no other game runs, and close only its freshly confirmed
app ID. Never infer a process from a profile mount. Mount/unmount is not launch/close.

First launch mounted v62; read startup/access logs, verify helper status, root
proof and listener42000. Then use actual WSL Movian against the PS5 endpoint to
navigate Settings and change/restore a real setting with pushed updates. A host
preview on42011 is not console proof. Afterward deploy v63, verify local/remote
Quit and relaunch. Never send a signed eboot.bin to9021.

Private console scripts/evidence are in `build/v62-console`; current closed-image
backup proof is in `current-profile-proof.txt`. PC backups are consolidated under
`D:/GPTDEEP/PS5-Movian-Backups/2026-11-23`. The v61-before-v62 rollback folder still
needs verified archival. Do not run the old archive script while any needed staged
build exists: it archives every `/data/.PPSA99456-*` folder. Media remains intact.

## Planned-option progress

v63 implements General Quit, sidebar Exit and STPP Quit via one callback. Full
optional self-check, worker/IME/FTP/listing drain tests, refusal recovery and signed
build pass. Shell/playback/IME host boundaries are fakes; no PS5/audio claim.
`docs/UI_V63.md` describes it. `docs/PLANNED_OPTIONS.md` inventories each pending
setting with its own test; regenerate its table using `scripts/planned-settings.cpp`.

Next independent feature is P02 plugin details/Uninstall. Actual installed WSL
M7 7.0.273 and shipped flat view show ONE status-dependent Install/Upgrade/Uninstall
button. Changelog is commented out; the current PS5 five-action menu is incorrect.
Native Uninstall immediately removes installed ZIP and plugin settings/stores,
without observed confirmation, unloads settings/routes and retains other plugins.
Disposable fixture evidence is under `build/planned-options-20261124`. No backend
uninstall exists yet. Avoid introducing invented desktop actions/defaults.

The installed WSL reference `/usr/bin/showtime` SHA-256 is
`da133c4a6ee509da8f3b74dd2dfd1b445988381c5b3210d57b15a2e3a96ac9b1`.
Use installed WSL + retained REA evidence for behavior, never old andoma source.
Use pinned YouROK/TorrServer for torrents. WSL native HTTP42000 and old host preview
42011 may be running; rediscover before testing. Research fixture was fully removed.

Three prior helpers stopped on workspace credits; no implementation work remains
owned by a running agent. Preserve unrelated untracked camera prototypes and do
not publish them. Latest public release is v60; it is not a verified working
console STPP release. Avanpost/DNS/scanner work remains deferred.

# v60 console deployment — 2026-11-24

v60 is now uploaded/readback-verified, activated and mounted. Version/hash are
unchanged from the published v60 below. All 29 package files and two separate
console-only opt-ins have execute permissions. New closed 336,789,504-byte
profile backup was double-read verified in the same PC backup root/profiles;
its proof path is private build/v60-console/current-profile-proof.txt. Do not
reuse yesterday's profile proof: content changed before today's backup.

Next unfinished action: initial manual PS5 launch, then inspect runtime files
and TCP42000 before attempting WSL STPP control. User launch question is pending.
No runtime path/listener has been observed yet. The helper's loader9021 and
klog3232 endpoints are closed. Do not claim fresh reset or access succeeded.
The title itself consumes /app0 opt-ins once and writes reset/access evidence.

Activation is recorded in build/v60-console/activation.json; mounted source is
/data/homebrew/PPSA99456. Old v59 folder was archived/readback-verified and
removed from console (14,881,877 bytes). Media and production title remain intact.
FTP capture runs from build/v60-console/live_capture.py, saves under
build/v60-console/live-20261124 (30-minute bounded run); inspect events, private
runtime listing, startup/access logs and reset/consumed markers. Reconnect WSL
to actual PS5 endpoint 42000, not the still-running host preview 42011/proxy 42010.
The installed WSL viewer runs HTTP 42000 locally; rediscover processes/ports.

# Active v60 implementation — 2026-11-23

Current request authorizes the STPP property server, shared callbacks, native
WSL acceptance, PS5 integration, one-shot title-access experiment, fresh test
plugins/config after verified backup, console backup cleanup and GitHub release.
This supersedes older no-lift/preserve-plugin instructions below for this test.
Read docs/STPP_V60.md, docs/STPP_V60_NATIVE.md and docs/STPP_TITLE_ACCESS.md first.
Active branch test/native-playback in movian_ps5-playback-publish. Preserve media
and production PPSA99455. No webcam/Chiaki. DNS/Avanpost remains deferred.

Published source/tag: 44cdefb / playback-test-v60. Release:
https://github.com/brinza666/movian_ps5/releases/tag/playback-test-v60
Main landing/docs commit 95b21c5; Pages metadata-wait fix 995307c;
Pages run 45241805249 successful. Repo description and homepage updated.
Release assets and GitHub digests verified. The camera prototype files remain
untracked/preserved and were excluded from this publication.

Final v60 ZIP: build/PPSA99456-v60.zip, 7,706,570 bytes,
SHA73b8b7a940703cf24abe6668ee53368ed74a451096b6c82c5e3b8421d419ef19.
Eboot SHA7befb1d484baf4173beeda810d7950dc5807542802ecea0c255d450b40dab7e3.
Final core sanitizer/socket and shared-UI/native WSL tests passed; signed package
integrity passed. Next unfinished test is PS5 upload/mount/startup/access.
Console went offline before staging; v59 still installed. PC backups are in
D:/GPTDEEP/PS5-Movian-Backups/2026-11-23 (28 removed console folders,44 packages,
closed verified profile). Fresh reset is prepared, not executed on PS5.

The host actualUI server (finalcore) listens loopback42011 with disposable profile
build/ui-stpp-final/native-serve. WSL viewer HTTP42000, native connection via
recording proxy42010. Rediscover ports/processes rather than assume PIDs survive.
Raw evidence/backup manifests are private/ignored. No per-console request file
is packaged; provision only after the saved closed-profile backup is verified.

# Current priority: native Movian remote control — 2026-11-23

User explicitly deferred DNS/Avanpost work. Do not continue it or repeat the DNS
question until asked. New priority is WSL Movian controlling another Movian,
then implementing the same mechanism on PS5 so tests can run without controller
hand-offs. Read docs/REMOTE_CONTROL_STPP.md, docs/STPP_ARTIFACT_FINDINGS.md and
latest PORT_LOG.md first. Keep sandbox-lift out; preserve plugins, private
settings and all uncommitted work. No webcam or Chiaki control.

Actual research DONE: WSL M7 7.0.273 Local network discovered matv (live Android
7.0.289); type movian, stpp:id:<id>, direct stpp://<peer>:<port>. WSL HTTP port
is currently 44456, not 42000. Peer/ID details stay in ignored captures. Real
STPP controls opened Settings and KinoZal.TV; changed Show filename extensions
1→0→1 with independent HTTP verification; Back restored navigation. Live clock
and log additions were pushed, and unsubscribe stopped updates. Final state
is WSL directly connected to matv and Android showing KinoZal.TV.

New tracked files: scripts/movian_stpp.py (host client/codec),
tests/stpp_protocol.py (six passing wire tests), docs/REMOTE_CONTROL_STPP.md,
docs/STPP_ARTIFACT_FINDINGS.md. Added wire test to scripts/self-check.sh and
acceptance rows in SPEC.md. The reusable Client was verified against the actual
TV for subscription/unsubscription and navigation/Back. Optional network package
websocket-client==1.8.0 is installed under ignored build/remote-control-rea/vendor;
codec tests need no dependency. No native PS5 source change/package was made in
this remote-control investigation. Current PS5 remains v59/hash recorded below.

Raw evidence: build/remote-control-rea. Native WebSocket handshake is legacy:
GET /api/stpp; Connection Upgrade; Upgrade websocket; Sec-WebSocket-Key: 1;
no Host or WS version. Binary STPP hello 00 03 00; server 00 03 +16-byte ID+00.
Root ref0 is exposed global tree. Subscribe opcode1: LE32 sub ID, LE32 base,
LE16 flags, byte-length-prefixed path (native sender omits terminator). Embedded
addresses terminate path with00. Notify opcode4 includes subtype+sub ID. Int
write opcode3 address+01+LE32 signed value. Event5 OpenURL type14/mask1/string
to navigators/current/eventSink; Back type1/action name worked. Exported child
references are per-connection; do not reuse after reconnect. See full layouts,
remaining unknown subtypes/flags and artifact limitations in reports.

Live discovery: 88-byte STPP UDP to 239.255.255.250:42000; opaque ID @4,
version3 @20, flags3 @21, big-endian HTTP port @22, name @24 and text @56.
Unicast replies use server source port42000 and peer ephemeral destination.
The 44456 WSL HTTP port was correctly advertised. Static bit meanings are not
fully live-tested. All PCAPs/IDs and app raw data stay ignored/private.

REA packet work runs inside WSL with Node24.11.1 locally extracted under
build/remote-control-rea/tools/node, checksum verified from official Node
SHASUMS, and Windows-installed REA scripts at
/mnt/c/Users/brinza/AppData/Roaming/npm/node_modules/rea-agents/scripts/rea.mjs.
WSL tcpdump/tshark installed. Root is available via wsl -u root; no sudo password
or MCP registration was changed. REA evidence IDs and exact limitations are in
REMOTE_CONTROL_STPP.md. Native action/discovery coverage complete; initial
connection capture partial. Windows Ghidra rejects ELF, so artifact protocol
semantics were supplemental GNU disassembly, not REA decompilation. Helper
agent stpp_artifact completed its static report without runtime/source edits.

NEXT unfinished acceptance (part1 of documented three-part plan): implement
compatible STPP server/property graph in HOST PREVIEW; connect the unmodified
WSL client and render a Home/Settings page, change/restore one disposable option,
and confirm pushed state with the independent Python client. Then part2 maps
shared UI callbacks and safe PS5 transport. Inbound PS5 listener bind currently
fails error13; use public SDK tests, then an opt-in outbound paired bridge if
needed, preserving the STPP wire protocol and no sandbox-lift. Part3 covers
playback/torrents/logs/resources, bounded queues/reconnect/gap recovery and agent
tool integration. STPP only controls a running app; relaunch needs a separate
supported path. Android screenshot endpoint404 and pixel/audio mirroring not
validated, so do not promise STPP alone sees the hardware video pixels.

No CUA app input was sent: Movian window was minimized and binding failed;
protocol/API work achieved the observations. Do not resume Chiaki. Scoped
packet captures are complete; transparent proxy was temporary and WSL was
restored to the direct matv endpoint. Do not reuse an expired recording proxy
as the target. Current connection and exact private endpoint are recorded in
build/remote-control-rea/final-state-private.json.

---

# Historical v59 playback continuation

# Current v59 continuation — 2026-11-23

Active checkout C:/Users/brinza/Documents/deep/movian_ps5-playback-publish,
branch test/native-playback, base b2265f5. Preserve all uncommitted v54–v59
work, installed plugins and private settings. No sandbox-lift. No webcam or
Chiaki control. Latest published release remains v53; no new release was asked
for during this continuation.

v59 is activated and mounted at /data/homebrew/PPSA99456. User confirmed Close
Game before replacing v58. Activation.json and mount-result.json are under
build/v59-console; mount returned HTTP 200/status 0. Old v58 rollback is outside
homebrew. Signed archive build/PPSA99456-v59.zip SHA-256:
b54442548d0689e75e99c53ff52fa4564c43a21bb40eacb3c2ca7f4743b4b5bf.
All 23 files were readback-verified byte for byte via FTP 2121 and chmod 777.
Startup main/fonts/pad/UI is confirmed in live logs.

Read docs/NETWORK_V59.md and the latest PORT_LOG.md section. v58 SMB ZIP install
and FTP ZIP install/torrent file-list opening are user-confirmed. Existing logs
support imports, but the most recent confirmations do not have separate freshly
timestamped protocol traces. Plugin directory entries remained 14 before/after;
no installed plugin removal or private config overwrite occurred.

v59 changes native HTTP/DNS pool startup to call process-wide mutex-protected
sceNetInit, adds exact native HTTP failure stages, and implements the observed
WSL Tracker protocols chooser: TCP & UDP / TCP / UDP, persisted and passed into
the effective tracker filter. Source additions include network_runtime.hpp and
tests/plugin_http_native.cpp. Full self-check and shared-source UI harness passed.
No cache/peer-engine rewrite or memory increase was made. See NETWORK_V59.md
for the exact desktop ranges and still-unimplemented differences.

The Avanpost/public-hostname problem remains. Native init returns 0/ready, but
HTTP fails specifically at send request with 0x8041013d; UDP tracker DNS fails;
DHT bootstrap has zero nodes. No public-torrent peer/verified piece was supplied.
The initial native-init fix did not resolve these hostname requests.

A corrected numeric-IP control PASSED ON PS5: tracker status 200, one peer,
RAM cache=0 and 2,654,208-byte allocation for 2,640,741-byte HEVC media. Hardware
1920x1080 HEVC first frame and 48 kHz PCM began before transfer completion;
all 162 blocks/2,640,741 bytes were served and verified. Decoder EOF reached
180 hardware frames; audio drain submitted 288,768/nonzero 288,108/dropped 0/
result 0. Audibility still requires user confirmation. This is the small-file
contiguous RAM path, not a 19.1 GB rolling-cache/seek pass.

The numeric descriptor is /data/movian/movian-v59-http-probe.torrent, file row
HEVC-1080p-v59-probe.mp4. Only small metainfo was uploaded to storage; payload
comes from the laptop. BitTorrent root needs Triangle refresh after uploading
new descriptors. The old local movian-v22 fixture playing is not this probe.

Important fixture correction: build/v58-console/serve-peer.py uses fixed ports
which Windows now rejects (WinError 10013). Do not reuse that broken server.
Use build/v59-console/probe-peer.py: it binds available ports dynamically,
records probe-fixture.json and rewrites the descriptor. On restart, upload the
new descriptor with FTP, readback-check it and refresh the UI. Current session
5180 runs the controlled peer/tracker for up to 15 minutes (ports were 60884 HTTP
and 60885 peer for this run). Restart if expired. Do not count local host probes
as PS5 evidence.

Console success evidence was frozen BEFORE the WSL probe in
build/v59-console/probe-success. Live FTP capture session 42246 writes
build/v59-console/capture (15-minute bound; restart if needed). Sandbox log path
is /mnt/sandbox/download/PPSA99456, sometimes absent briefly around startup.
Alternate /mnt/sandbox/PPSA99456_000/download0. TCP diagnostics bind is still
rejected with error 13, so use FTP fallback; klog 3232 is reachable again.

Reference comparison: installed WSL M7 hash still matches retained REA identity
(see NETWORK_V59.md). Captured live BitTorrent settings/options under ignored
build/v58-investigation/wsl-*. With the corrected numeric fixture, WSL opens a
video file list using torrentfile://, then pauses for buffering and media time
advances 0.833 to 3.367 seconds in play state. No independent reference visual,
audible or seek pass was obtained. The first WSL attempt failed because the
fixed-port seed never started. Three public bootstrap names resolve on WSL.

NEXT unfinished acceptance: isolate/fix PS5 hostname resolution, then retest
Avanpost in RAM without changing its cache/peer policy during the network test.
A pending async question asks for the PS5 primary/secondary DNS and whether they
form part of update blocking (Settings → Network → Connection Status → View
Connection Status). No reply yet. No system DNS/registry/network settings were
changed. The numeric control shows native HTTP and hardware work; it does not
by itself distinguish bad console DNS from a native-resolver adapter defect.
If DNS settings are sound, add native DNS stage/result diagnostics to distinguish
pool/create/start failure, initialize the DHT/UDP resolver through the same helper
(it currently has its own pool creation path), and use the public SDK/TorrServer
references rather than inventing a new torrent stack. Respect any update-blocking
setup. Keep all credentials, media paths and tracker URLs out of tracked evidence.

---

# Historical v58 continuation

# Current v58 network-import continuation — 2026-11-23

Active worktree `C:/Users/brinza/Documents/deep/movian_ps5-playback-publish`,
branch `test/native-playback`, base `b2265f5`. Preserve all uncommitted v54–v58
changes, installed plugins and private settings. No sandbox-lift. Do not use
the webcam or resume Chiaki control. The latest published release remains v53.

The user separately confirmed SMB and FTP browsing/playback, sound and seeks
on v57. Read [Network v58](NETWORK_V58.md) for exact log coverage and limits;
do not promote these reports to an independently measured FTP 1080p HEVC EOF
pass or an Avanpost pass.

v58 source is complete, full self-check and UI fixture tests passed, and the
signed package is `build/PPSA99456-v58.zip`, SHA-256
`ca72e93f24777fe93db15d6f4fa9fc0a75b5757e7473612af1d78df4078cda13`.
It adds authenticated network ZIP/torrent reads (8 MiB/1 MiB caps), worker
previews and dismissal, remote ZIP install, and saved server roots in the
ZIP picker. Payload cache policy is unchanged. New imports are host-tested,
not yet PS5-tested. New bounded input and service regressions are in `tests/`.
The host UI harness additionally completed hash-verified RAM peer transfers
from both remote descriptors. Its 100-byte fixture is an input/transfer test,
not hardware playback. Runner: `build/v58-ui/peer-test.py` (seed service exited).

All 23 files were uploaded to a staging folder **outside** homebrew with
execute permissions and byte readback checks, through FTP **2121**. The exact
folder is in ignored `build/v58-console/staging-path.txt`, verification in
`staging-verification.json`, staging helper `stage.py`, matching `reference.elf`.
The user confirmed Close Game. v58 is now active at
`/data/homebrew/PPSA99456` and mounted (HTTP 200, status 0). The staging path
was renamed into place and no longer exists. The prior v57 rollback location
is recorded in `build/v58-console/activation.json`. Installed metadata is
`01.000.058`; ShadowMount catalog version 47 is stale. Persistent plugins and
download0 were untouched. A pending question asks the user to open v58,
confirm the footer and RAM cache, and remain on Home. No v58 startup or
console import test is verified yet. Live FTP capture is session 16700,
`build/v58-console/capture.py`, output `capture/` (15-minute bounded run).

Next unfinished acceptance: on v58, browse a configured SMB folder, inspect
and install a known API v2 ZIP, then open its `.torrent` and start the existing
peer path with RAM selected. Repeat over FTP as a separate check. Controlled
host fixtures are under ignored `build/v55-console/media/Imports`; existing
read-only fixture services still run on workstation SMB2 1445 and FTP 21219.
Do not overwrite the user's saved network config.
New controlled fixture files are `Imports/movian-network-v58.zip` (distinct
API v2 plugin ID) and `Imports/movian-v58-ram-playback.torrent` (actual bundled
1080p HEVC media). Seed/tracker helper `build/v58-console/serve-peer.py` is
running in session 42144 for up to 15 minutes on ports 49168/49169; it accepts
only this private fixture. Start it again if expired. This is prepared test
infrastructure, not evidence of a PS5 transfer. First ask whether the user
can reach the fixture share or will use known ZIP/torrent files on their own
configured server; do not replace their saved connections. Raw v57 evidence is in
`build/v57-live` and `build/v58-console/v57-*`. Remote diagnostics bind is
denied on PS5; use FTP `/mnt/sandbox/download/PPSA99456` logs while running.

Network discovery is deliberately later work. WSL M7's `discovered:` page
showed five server rows with SMB2 URLs; one selection stayed loading. REA
identity and private raw property captures are in `build/v58-wsl-network`.
Do not use old andoma Git code. The follow-up plan and evidence limitations
are in [Network v58](NETWORK_V58.md) and [UI work list](UI_WORK_LIST.md).

---

# Historical v57 continuation — 2026-11-23

The installed and running test version is `01.000.057`, verified through FTP
2121. v57 removes the null `arc4random_buf` import which crashed v56 while
initializing SMB. Both SMB and FFmpeg now use the existing `kern.arandom`
adapter. Self-check, signed build, 23-file readback and mount passed. Archive
`build/PPSA99456-v57.zip` has SHA-256
`d1f7315a300db9691a9b4d7430dd4ce1ea671b203b5f2fd99f781542ea1d3d94`.

**The user has stopped webcam-photo testing. Do not capture more camera frames.**
A different visual approach will be supplied later. Continue with FTP logs and
controller observations. Do not resume Chiaki computer control either.

Latest logs are under `build/v57-live`. They show v57 starts and opens network
media, plus hardware H.264 decoding. A fresh, protocol-specific 1080p HEVC
playback and seek run is pending. The user has been asked to run SMB first,
with the clip name, server number, audible sound and any error reported.
Read-only fixture services still answer at workstation `192.168.10.29`, SMB2
1445 and FTP 21219; preserve the user's configured servers and plugins.

---

# Current console and camera state — 2026-11-22

The active test title is now v56 (`01.000.056`), uploaded through FTP **2121**.
v55 was uploaded and mounted but crashed during the initial clock render.
v56 restores the earlier time conversion and uses bounded clock formatting;
its full self-check passed, the upload was verified, and the user confirmed
Home. The camera now also shows Movian Home on the physical TV.

The v56 archive is `build/PPSA99456-v56.zip`, SHA-256
`0c8c3f7e488d5e3d2b38a976b2987c7a52e7228c7ee7ada1c2a7d27edcc4bffb`.
The user reported SMB and FTP picture, sound and seeking, but the collected
hardware trace currently proves the bundled local clip only. Continue with
network correlation before claiming an independently verified hardware pass.
Decoder output is `/download0/hevc-test-decode.log`. While running, the title's
FTP-accessible data path is `/mnt/sandbox/download/PPSA99456`. Diagnostics TCP
bind is denied with error 13, so use the FTP fallback. Preserve installed plugins.

For visual feedback use [the local camera helper](PS5_CAMERA.md). Windows
Python `C:/Python314/python.exe scripts/ps5-camera.py` captures the RGB webcam
with MJPG at 1280x720 to ignored `build/camera/latest.png`. Capture only during
requested TV tests and stop while the user is positioning the laptop. Computer
Use through Chiaki was stopped by the user; do not resume that route without
a new request. Camera capture was separately authorized and is working.

Read [the port log](PORT_LOG.md) for deployment, crash and camera evidence.
The older v55 state below is historical. Preserve all uncommitted changes.

---

# Current UI and network handoff — 2026-11-22

Active checkout `C:/Users/brinza/Documents/deep/movian_ps5-playback-publish`, branch
`test/native-playback`, base commit `b2265f5`. Uncommitted v54 Settings and v55
network changes are present and must be preserved. The local signed packages
are `build/PPSA99456-v54.zip` and `build/PPSA99456-v55.zip`. The latest published
release is v53. Read [STATUS](STATUS.md), [the port log](PORT_LOG.md),
[Network v55](NETWORK_V55.md) and [the UI work list](UI_WORK_LIST.md) first.

v55 implements saved SMB2/FTP connections, login, background directory listing,
seekable player input and an authenticated FTP server. Host service, UI/player
and full self-check results passed. Console FTP 2120 is unavailable while
ShadowMount is reachable, so v55 remains unuploaded and console playback is
unverified. Next is a real SMB2 and FTP hardware playback/seek run after upload.
Keep sandbox-lift excluded and preserve installed plugins. Dependency scripts
pin libsmb2 and FFmpeg and contain the PS5 socket/address adaptations.

Local test evidence, commands, screenshots and REA bundles are under
`build/v55-wsl-network`. WSL Movian is `/usr/bin/showtime` on Ubuntu-24.04;
its diagnostics and the host diagnostics test both use TCP 42000. Stop the
verified desktop executable before full self-check and restore it afterward.
The desktop reference evidence covers Network settings and FTP authentication;
controlled desktop fixture listing did not complete. Source paths and further
limitations are recorded in Network v55. No old andoma Git source was used.

Verified v55 archive SHA-256 is `2959ab59c5c6062bda94414783a2a97e6f5524ba6539dee533ba2e54e33390b5`. The final REA package bundle is
`build/v55-wsl-network/package-verified-rea.json`. Earlier package bundles in
that evidence folder precede the final socket/address dependency rebuild.

---

## Historical playback handoff

# Movian PS5 playback test handoff

Updated 2026-11-21. Release tag `playback-test-v15` and branch
`test/native-playback` track the isolated native test title `PPSA99456`.
The production folder title is `PPSA99455`. Keep the test and production
title IDs separate. Make source changes in this branch when reproducing a
test result.

## Verified and open

v15 keeps the single picture allocation across file opens, synchronizes its
use by the renderer, and drains the AudioOut queue before auto-advance. It
captures FFmpeg diagnostics in `/download0/hevc-test-ffmpeg.log` and selects
the MPEG demuxer for a Program Stream pack header in an `.mpg` or `.mpeg`.
v14 successfully switched among hardware H.264 files and returned from
failed MPEG opens. v15's MPEG replay and the full E-AC-3 feature playback
are awaiting console observations.

| Case | Evidence | Status |
| --- | --- | --- |
| Bundled 6-second 1920×1080/30 HEVC Main fixture, AAC | v10 user saw the clip; console reported 180 hardware frames and clean decoder drain | Pass on the test console |
| H.264 High MKV, 1920×1040 at 23.98 fps | v11 and v12 logged Videodec2 initialization and first decoded hardware frame | Hardware first frame verified; full playback, seek, and EOF still open |
| E-AC-3 5.1 track in the H.264 MKV | v12 logged six input channels, 48,000 Hz output and 1,536 stereo PCM frames | Decode and stereo conversion verified; audible output and uninterrupted playback still open |
| 360×640 H.264 MP4 and bundled HEVC with AAC | v12 logged first hardware frames and PCM blocks | First frame and PCM verified |
| Repeated playback and reopen | Later v12 fixture requests returned Hardware HEVC unavailable after earlier successes | Open; identify failing probe, decoder, or buffer-allocation stage |
| Intermittent SIGSEGV at instruction address zero | v11 kernel trace after a local MPEG clip selection had an empty backtrace | Open; source call site and relationship to the MPEG clip are unknown |

No sandbox-lift code is included. Use the title's current permissions and
the platform's standard module-loading and memory APIs. Support for codecs
outside the static FFmpeg recipe needs its own test. Full Dolby bitstream
passthrough and multichannel AudioOut are outside this stereo build.

## Where to resume

1. Start v15 on the PS5. Cross on Home starts the built-in
   HEVC fixture. To test H.264 plus E-AC-3, go to Videos and open a known
   local MKV with both streams. Confirm that picture and tone/dialogue
   continue past 15 seconds. Seek once, then test a short fixture through EOF.
2. Capture klog starting before launch. Retrieve these title-local files
   while the app is open: `/download0/hevc-test-startup.log`,
   `/download0/hevc-test-probe.log`, `/download0/hevc-test-decode.log`, and
   `/download0/movian-ps5.log`. The app's temporary log directory may
   disappear from FTP after close. Keep evidence associated with the
   executable SHA-256, title version, codec and sample.
3. For the intermittent crash, capture steps separately for first boot,
   HEVC → H.264 switch, the MPEG-1 file, Stop/Back, seek, and Close. Capture
   the faulting PC and backtrace before changing code. Match the executable
   hash to an archived `llvm-pie.elf`. For a title PC based at `0x400000`,
   symbolize the relative address with `llvm-addr2line-18 -f -C -e llvm-pie.elf`.
   A zero instruction address with an empty backtrace gives insufficient
   evidence to name a failing call. Add phase breadcrumbs if it repeats.
4. Reproduce repeated HEVC/H.264 opens and close cycles. Compare the final successful decoder-query and create log with the next failed request. Log release return codes and count owned buffers across close before changing resource budgets.
5. Exercise sample-rate changes and stereo/mono AAC alongside E-AC-3, since
   v12 routes all FFmpeg audio through libswresample. Record the audio sink
   result separately from PCM production. Check decoded frame counts and
   end-of-stream drain for short H.264 and HEVC clips.

## Implementation map

- `app/src/avdec.cpp` owns demux, Annex B packet filtering, FFmpeg audio decode,
  libswresample stereo downmix and rate conversion, hardware/SW video
  selection and worker lifetime. `g_video_bsf` selects FFmpeg's
  `h264_mp4toannexb` or `hevc_mp4toannexb`. The output time base follows the
  filter. EOF drains the filter and decoder, and seek resets both.
- `app/src/hwdec.c` owns Videodec2 configuration, GPU input/output surfaces,
  picture reuse, cropping and hardware drain. Uses public SDK sysmodule
  loading and normal link imports. The video import stub is link-only.
  The title uses HEVC Main/Main10 according to stream bit depth and
  supports eligible 8-bit H.264 profiles with a software fallback at open.
- `app/src/player.cpp` manages playback controls, MPEG-1/PL_MPEG, and presentation.
  Its fallback resampler uses 256-frame stereo batches. A 64 KiB local
  buffer previously overran the FFmpeg decoder thread stack.
- `app/src/audio.cpp` is the 48 kHz stereo AudioOut ring. v12 records first
  decoded stereo PCM, which alone does not establish audible TV output.
- `scripts/build-ffmpeg-ps5.sh` sets the static codec allowlist. A firmware
  decoder or system module goes through normal imports. A software decoder
  must be present in the FFmpeg build before it can be selected.

## Build and provenance

Build in Ubuntu 24.04 WSL with clang/lld 18, public PS5 Payload SDK v0.42, and
[ps5-homebrew-starter at c3f9392](https://github.com/MiladRTX/ps5-homebrew-starter/tree/c3f939230aee18863da1f359e39eb43bf95a056d).
From this branch's root, export `PS5_PAYLOAD_SDK` and `PS5_STARTER` as local
absolute paths. Use a dedicated `PS5_STARTER_HOME` to preserve production
build outputs. Place the included FFmpeg tarball in the selected build
root or let the recipe fetch and verify the pinned FFmpeg 6.1.2 source.

```bash
export PS5_STARTER_HOME="$HOME/.movian-playback-test"
export MOVIAN_FFMPEG_BUILD_ROOT="$PS5_STARTER_HOME/ffmpeg"
bash scripts/build-ffmpeg-ps5.sh
bash app/build.sh
sha256sum build/app/PPSA99456/eboot.bin
```

The packaged v12 eboot hash is
`8bb86ccafcf495a20cebe7ed371b6026ede87a9edca1a2388cf30135c1ffa391`.
The release ZIP installs as the distinct folder `PPSA99456`. Archive the
symbol-bearing `$PS5_STARTER_HOME/kit-work/build/llvm-pie.elf` per release.
Uninstall any old test registration and verify the running title is closed
before replacing a deployed test folder.

Source references reviewed 2026-11-21. See [Kodi PS5](https://github.com/VivaLaVent/kodi-ps5/tree/83a7be649914535d0cb7837eea65f6f7e6eaaa21)
for profile selection, FFmpeg filtering and audio layouts, and
[VLC PS5](https://github.com/ZiZc3/VLC/tree/7f104abb5a88dd675b60261769dc86d5dfb0a0d4)
for title imports, access-unit handling and surface lifetimes.
