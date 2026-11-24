# Movian PS5 v60 — STPP remote-control preview

Native ShadowMountPlus test title **PPSA99456**, version **01.000.060**.
This is a historical host-tested preview. **The first v60 console launch exited
before Home after a root-restoration loader lookup failed.** A subsequent v61
reached Home and completed the requested reset, but STPP/diagnostics binds failed
with errno13. v62 corrects loader/owned-helper handling and remains mounted,
awaiting console acceptance. See the newer [v64 preview](https://github.com/brinza666/movian_ps5/releases/tag/playback-test-v64)
for shared Quit, native plugin removal and current verification limits.

## What changed

- STPP v3 property server with native/modern WebSocket handshakes, stable exported
  references, subscriptions, collection deltas, aliases, native boolean toggles,
  OpenURL and a bounded UI-thread command queue.
- Shared Home/services, Settings/categories, current page, browser/plugin
  summaries, playback status and bounded live log properties. Real local and
  remote setting changes use the same callbacks and saved state.
- Network → Allow remote control. The running title attempts TCP **42000**;
  diagnostics moved to **42001**. Automatic Movian discovery is still pending.
- Optional, explicitly gated one-shot title-access support. The public ZIP has
  **no authorization request**; helper execution is disabled until separately
  authorized on a console. Standard source builds omit helper support by default.
- Optional one-time fresh plugin/config reset for this requested console test,
  also requiring a separate exact request. Public installation alone does not
  erase a profile. The reset preserves media, resume and bookmarks.
- Includes previously unpublished v54–v59 desktop settings, SMB2/FTP saved login,
  browsing and seekable playback, network ZIP/torrent-metainfo imports, native
  network initialization and tracker-protocol settings.

## Verified

The **unmodified WSL M7 7.0.273** viewer connected to the final shared UI server,
rendered eight Home services and Settings → General, changed filename extensions
**On→Off→On**, and returned Home. The saved settings agreed at each step.
Local mutations also push to independent clients. Invalid bounds/types,
stale references, reconnects and bounded queues are tested. Core ASan/UBSan and
leak checks passed. Full self-check includes real SMB2/FTP and growing Matroska
fixtures; host player stubs do not establish console picture/audio.

Earlier user confirmations cover v57 SMB/FTP picture, sound and seeking, and
v58 network plugin ZIP installs/torrent file lists. v59's small numeric-IP RAM
torrent produced verified pieces and 180 hardware HEVC 1080p frames through EOF.
Feature-length Avanpost playback and hostname discovery remain unresolved;
this release does not claim they are fixed.

## Install and use

Close the title. Upload the complete extracted `PPSA99456` folder to
`/data/homebrew/PPSA99456` through your console FTP server, preserving execute
permissions. Scan and mount it through ShadowMountPlus, then launch its icon.
Verify v60/Home before claiming runtime success. Keep production PPSA99455 separate.

After a reachable listener has been verified, open `stpp://PS5_IP:42000` in desktop
Movian. Compatible controllers connect to `ws://PS5_IP:42000/api/stpp`. The title
must already be running. Root/path restoration and native HTTP/TLS compatibility
remain part of the access experiment, not assumed capabilities.

Complete build instructions and precise scope: [development README](https://github.com/brinza666/movian_ps5/tree/test/native-playback),
[v60 implementation](https://github.com/brinza666/movian_ps5/blob/test/native-playback/docs/STPP_V60.md),
[native WSL evidence](https://github.com/brinza666/movian_ps5/blob/test/native-playback/docs/STPP_V60_NATIVE.md),
[access audit](https://github.com/brinza666/movian_ps5/blob/test/native-playback/docs/STPP_TITLE_ACCESS.md).

## Assets

- `PPSA99456-v60.zip`: **7,706,570 bytes**.
- ZIP SHA-256: `73b8b7a940703cf24abe6668ee53368ed74a451096b6c82c5e3b8421d419ef19`.
- Eboot SHA-256: `7befb1d484baf4173beeda810d7950dc5807542802ecea0c255d450b40dab7e3`.
- `PPSA99456-v60-dependency-sources.tar`: upstream FFmpeg, dav1d, Duktape, miniz,
  Gumbo, libsmb2, exact starter kit and Lapy helper source inputs/notices.
- `SHA256SUMS.txt`: checksums for both assets.

Discovery, complete setting/dialog semantics, artwork/resources, framebuffer
capture, track/torrent remote controls and autonomous relaunch remain separate
steps. No private configurations, authorization markers, captures or backup
images are included.
