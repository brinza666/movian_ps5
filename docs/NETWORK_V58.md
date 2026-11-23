# Network imports and discovery follow-up — 2026-11-23

v58 extends the existing authenticated SMB2/FTP input to small files opened
from a network share. It does not add a separate network or torrent engine.
The isolated title remains `PPSA99456`; sandbox-lift is excluded.

## Network files

- Select a `.zip` in Local network, inspect its Movian manifest, then choose
  Install. The existing install worker validates the script and replaces only
  that plugin's ZIP after a successful write. Installed plugins are retained.
- Plugins → Install from ZIP now includes the configured Server entries.
  Its network listing filters ZIP files and retains folders.
- Select a `.torrent` in Local network to read its file list. Cross uses the
  existing tracker/peer path; Square retains direct-peer entry. The transfer
  worker also reads the descriptor through the same authenticated backend.
- Package and descriptor preview run on a worker. Circle dismisses the read;
  the bounded transport operation can finish in the background, and a late
  result is discarded. Cancellation does not forcibly interrupt the socket.
- ZIP input is capped at 8 MiB and torrent metainfo at 1 MiB. Unknown, empty,
  oversized and truncated inputs fail without falling back to disk storage.
  The metainfo read stays in memory. Torrent payload cache selection and the
  RAM-only behavior are unchanged. Installing a plugin still persists its ZIP.
- New file-open diagnostics identify SMB2 or FTP and size without exposing
  the share name, media path or credentials.

The reader is `app/src/input_file.hpp`. ZIP validation, torrent parsing and
peer verification retain their existing implementations. For a remote
descriptor, the UI skips the old completed-local-download probe, which would
otherwise reread the network descriptor synchronously.

## Verification

Full `scripts/self-check.sh` passed with the optional network, plugin and
Matroska fixtures enabled. `tests/input_file.cpp` covers partial reads,
truncation after the size query, transport failure, limits and cleanup.
Real disposable SMB2 and FTP services exercise metainfo parsing, full bounded
reads, ZIP inspection/install, malformed and oversized ZIP rejection,
preservation of the valid installed copy, and the network ZIP filter with an
omitted error-output pointer. The latter fixes a possible picker null access.

The shared-source host UI harness in ignored `build/v58-ui` passed both
protocols: asynchronous torrent list, ZIP details/install, dismissed-result
handling and saved network roots in the ZIP picker. These are host results;
the new imports still require a v58 PS5 test. The existing multi-peer, DHT,
private-torrent exclusion, RAM cache and Matroska EOF/seek checks passed.

The host harness also refetched each remote descriptor through the actual
transfer worker and completed a controlled peer transfer with hash verification
into RAM. Both paths returned the expected 100 fixture bytes, reported complete
and created no `download0/torrents` directory. This small transfer checks the
remote-metainfo handoff; it is not a console or hardware-video test. The local
runner is `build/v58-ui/peer-test.py`; its seed service exited after the test.

## v57 console result

The user confirmed SMB playback, sound and forward/backward seeking, then
separately confirmed FTP browsing, playback, sound and seeking. Fresh FTP
diagnostics were retained under ignored `build/v57-live` and
`build/v58-console/v57-*`.

SMB logs show successful context initialization and share authentication,
1080p HEVC hardware first frame with 48 kHz PCM, and a separate 720p HEVC
sample reaching 900 hardware frames and audio EOF. Later FTP logs show
successful directory listings, H.264 hardware frames and completed seek
targets at 10 and 0 seconds. The short 1080p HEVC trace is not a sustained
playback/EOF measurement, and the available FTP trace does not independently
establish the specifically requested 1080p HEVC clip. Audible sound is the
user's observation; nonzero PCM alone is not proof of audibility. Do not
interpret these runs as a pass for every uploaded format or for Avanpost.

## Discovery reference and later implementation

Reference: installed WSL Movian M7 7.0.273, `/usr/bin/showtime`, SHA-256
`da133c4a6ee509da8f3b74dd2dfd1b445988381c5b3210d57b15a2e3a96ac9b1`.
REA ELF evidence `ev_b592f41edf83f81bd55356450bda2b3cbc1726efb46946e4fd06b3afd3205bfb`
records GNU build ID `cecc8ab5765c085d19ffdc4e5c86649186a62cfa`.
That static evidence identifies the shipped executable; it does not establish
runtime discovery behavior.

The running application's property API was inspected separately. Opening
`discovered:` produces five LAN entries from `global/services/discovered`.
Each observed row has type `server`, origin `discovered`, a display title and
an `smb2://` URL, using either a host name or numeric address. Raw property
captures and REA evidence remain in ignored `build/v58-wsl-network`; host
names and credentials are excluded from this document. Selecting one server
opened a directory model, but it remained loading during this observation.
Successful share enumeration, automatic credential prefill and the precise
discovery transport are not established by this capture. No old andoma Git
source was used.

The user requested this scanner for later, so v58 does not implement it.
Continue in this order:

1. Observe a reachable WSL discovered server opening its shares and login
   prompt; record the exact order, labels and fields without saving secrets.
2. Add server discovery and share enumeration, with explicit empty/error and
   retry states. Keep unobserved transports marked unverified.
3. On selection, prefill the observed host/protocol/share information in the
   existing saved-connection/login flow. Never invent or copy a password.
4. Test with a controlled discovered server: select it without typing a host,
   authenticate, browse, play/seek, install a fixture ZIP and open its torrent.
