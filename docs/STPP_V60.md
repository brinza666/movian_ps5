# v60 STPP implementation and console experiment

2026-11-23. Isolated native title **PPSA99456**, metadata **01.000.060**.

The first STPP step exports shared Home/services, Settings/categories, current
page/selection, browser/plugin summaries, playback status and the bounded log
ring. Socket commands are copied into a bounded queue and executed by the UI
thread through the existing settings/navigation/player handlers. Local input
and remote changes use the same state. Publication is throttled to 5–10 Hz.

The server supports native and modern WebSocket handshakes, STPP v3 integer,
float/string decoding, typed writes, native boolean toggle tag13, OpenURL,
actions, property aliases and collection deltas. Only implemented callbacks
accept mutations. Invalid bounds/types are rejected. Limits are four clients,
64 KiB frames, 1 MiB queued output/client, 2048 subscriptions, 65536 exported
references and 256 copied commands. Removed references and reconnect references
cannot mutate a replacement node.

## Verified host behavior

The unmodified installed WSL M7 **7.0.273** rendered all eight Home services,
Settings and General. Native clicks changed Show filename extensions **1→0→1**;
the actual shared UI settings file agreed, and Back returned through Settings
to Home. A separate socket test verified independent local changes push back,
persistence, invalid writes, aliases and selection. See
[the native evidence report](STPP_V60_NATIVE.md).

The full self-check passed with the optional Matroska and real SMB2/FTP fixtures,
including RAM-cache seeks/eviction, complementary peers, UDP/DHT discovery,
private-torrent isolation, growing Matroska EOF/seek, login rejection, ZIP and
torrent-metainfo imports, and FTP server authentication/path protection.
The core socket suite and diagnostics ring also passed sanitizer/concurrency
checks. Final vector/pthread core changes passed the socket sanitizer suite and actual
shared-UI tests again. The unmodified native WSL viewer repeated Home/Settings,
On→Off→On persistence and Back to Home after those changes.
Host playback in the UI harness is inert and proves no PS5 video/audio result.

## Console-only opt-ins

The latest user request authorizes this access experiment and a fresh plugin/
config profile. Standard builds default to `MOVIAN_TITLE_ACCESS_TEST=0`.
The experiment is compiled with `MOVIAN_TITLE_ACCESS_TEST=1` and still requires
an exact separately uploaded request, consumed once in the private profile:

- `/app0/stpp-title-access-test.request`: `PPSA99456\n`.
- `/app0/stpp-fresh-profile.request`: `PPSA99456 v60 fresh\n`.

These request files are **absent from release ZIPs**. The fresh-reset request
removes installed `plugins-v23`, settings, browser preferences, saved network
credentials and the private repository cache once, preserving media, resume and
bookmarks. A per-version done marker prevents repeated resets. The bundled
sample plugin remains application content. An external library repository cache
is not deleted automatically. Reset tests cover invalid markers, one-time
consumption, scoped deletion and a symlink target outside the plugin tree.

Before access changes, the title retains IME/videodec modules, its original root
and an open private probe log. The public helper is exact-source pinned. If roots
change, a public dynamic root-restore probe attempts to keep title paths usable
while retaining credential changes. Failure requests shell exit before the UI.
Existing filesystem access alone is explicitly labelled credentials unverified.
See [the access audit](STPP_TITLE_ACCESS.md) for mechanism and runtime limits.

STPP listens on **TCP 42000** when Network → Allow remote control is enabled.
Diagnostics moved to **TCP 42001**; FTP diagnostic files remain available.
Connect from desktop Open URL using `stpp://PS5_IP:42000`; compatible Python
clients use `ws://PS5_IP:42000/api/stpp`. Compatible LAN discovery is a later
step, so the PS5 does not yet appear automatically as a discovered Movian peer.
The endpoint controls the running test title; relaunch remains a separate step.

## Cleanup and console evidence

28 inactive test-title rollback/staging directories were copied to the single
PC backup folder `D:/GPTDEEP/PS5-Movian-Backups/2026-11-23`. Each file was compared
with a second independent FTP read before its console copy was removed.
**404,024,807 bytes** were removed from those folders. Saved versioned packages
were consolidated there too. The closed **336,789,504-byte** v59 profile image
was independently verified before the fresh-profile request was prepared.
Media and production PPSA99455 were not cleanup targets.

Final signed package: `PPSA99456-v60.zip`, **7,706,570 bytes**, SHA-256
`73b8b7a940703cf24abe6668ee53368ed74a451096b6c82c5e3b8421d419ef19`.
Eboot SHA-256:
`7befb1d484baf4173beeda810d7950dc5807542802ecea0c255d450b40dab7e3`.
Conversion, signing, integrity and request-marker exclusion checks passed.
The native required-module set matches v59; no static kernel_sys dependency
was added. The public SDK string runtime is linked after platform imports,
with unused sections discarded. Its input hashes are in the release build inputs.

The PS5 went offline after backup cleanup/profile verification. FTP 2121 and
ShadowMount 10101 are unreachable. v60 upload/mount, startup/access, fresh-reset
execution and native PS5 transport are **pending**. v59 remains installed.
No v60 PS5 runtime result is claimed. Hostname DNS/Avanpost work is deferred.

## Remaining scope

Complete text/numeric/dropdown and nested settings semantics, dialogs, network
editing, complete plugin navigation, discovery, artwork/resource RPC, framebuffer
capture, track controls, torrent statistics and autonomous relaunch have separate
acceptance tests. The exposed player actions are not console-verified by the
STPP Home/settings result. No claim of complete desktop remote-control parity.
