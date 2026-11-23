# Movian for PlayStation 5

Native C++20 media-center homebrew, packaged as a signed ShadowMountPlus folder title using the public PS5 payload SDK and the VLC for PS5 build approach.

**Playback test branch: `test/native-playback`, title `PPSA99456`.** The separate production title is `PPSA99455`. Keep their application folders and persistent profiles separate.

As of 23 November 2026, [v60 is the latest STPP preview](https://github.com/brinza666/movian_ps5/releases/tag/playback-test-v60). The signed package passed host and unmodified WSL Movian control tests. **PS5 v60 startup/access/transport are pending because the console went offline before upload.** v59 remains the last installed and startup-verified console build. See the [release notes](docs/release-notes/playback-test-v60.md), [v60 scope](docs/STPP_V60.md) and [port log](docs/PORT_LOG.md).

## Capabilities and evidence

| Area | Evidence and limits |
| --- | --- |
| Local playback | PS5 H.264/HEVC hardware frames, software decoding and 48 kHz stereo PCM recorded on controlled fixtures. Format, track and EOF coverage varies by fixture. |
| UI and controls | Home, files, list/grid views, sidebar, categorized settings, resume/bookmarks, playback bar, seek preview and audio/subtitle pickers. Subtitle rendering and complete desktop parity remain open. |
| SMB2 and FTP | Saved authenticated servers, background browsing and seekable input. The user separately confirmed playback, sound and seeks over both protocols in v57. Automatic server/share discovery is pending. |
| Network imports | v58 supports bounded ZIP plugin installation and torrent-metainfo reads from shares; SMB/FTP ZIP installation and FTP torrent file lists were user-confirmed. |
| Native BitTorrent | Verified pieces, bounded RAM/internal/USB cache choices, HTTP/UDP trackers and IPv4 DHT. v59 completed a small numeric-IP tracker transfer in RAM and decoded 180 HEVC 1080p hardware frames through EOF. Feature-length sustained playback and rolling-cache seeks remain open. |
| Plugin API v2 | Duktape host, catalog, ZIP installation, routes, settings, stores and artwork. Compatibility and live account login depend on each plugin. |
| STPP remote control | v60 server and shared Home/Settings callbacks passed against unmodified WSL M7, including a persisted native toggle and return Home. PS5 listener/startup remain pending; complete resources/settings/player coverage is separate. |

The v59 hostname-based HTTP/UDP/DHT failure remains unresolved; the numeric-IP control succeeded. DNS work is deferred. The v59 diagnostics listener was denied by the title sandbox (bind error 13). The v60 ZIP includes optional one-shot title-access support but no authorization request; there is no verified v60 console result yet. The published v53 package excludes sandbox lift.

## Build in WSL2 Ubuntu 24.04

Install the public [ps5-payload SDK](https://github.com/ps5-payload-dev/sdk) and provision the [ps5-homebrew-starter](https://github.com/MiladRTX/ps5-homebrew-starter) kit first. Host tools include Clang/LLD 18, a C++20 compiler, Make, Python 3, Git, curl, unzip, Meson/Ninja, pkg-config and Node.js. Console builds run in WSL with no proprietary SDK.

```bash
git clone --branch test/native-playback https://github.com/brinza666/movian_ps5.git
cd movian_ps5
export PS5_PAYLOAD_SDK="$HOME/.ps5-starter/ps5-payload-sdk"
export PS5_STARTER="/path/to/ps5-homebrew-starter"
export PS5_HOST="PS5_IP"
bash scripts/doctor.sh
bash scripts/build-ffmpeg-ps5.sh
bash scripts/self-check.sh
bash app/build.sh
# build/app/PPSA99456/ and build/app/PPSA99456.zip
```

`app/build.sh` fetches pinned Duktape/miniz/Gumbo sources, builds libsmb2 as needed, links existing static FFmpeg libraries and signs/packages `eboot.bin` through the kit. Run the FFmpeg step first for a fresh checkout. Generated dependencies and artifacts stay under ignored `.deps/` and `build/`. The title ID comes from `app/sce_sys/param.json`; older production documents use `PPSA99455`.

From PowerShell in this checkout, run the required check with `wsl -d Ubuntu-24.04 -- bash scripts/self-check.sh`. Optional plugin, Matroska and real SMB2/FTP fixtures require the `MOVIAN_HOST_*` paths in [the script](scripts/self-check.sh). A base check does not imply optional fixtures ran. Host results and console results are recorded separately.

## Install and verify on PS5

Use a supported homebrew runtime with ShadowMountPlus and an FTP server. Close the test title, extract the complete `PPSA99456` folder and upload it to `/data/homebrew/PPSA99456/` using your actual FTP port (2121 on the tested setup). Retain executable permissions and verify uploaded bytes before activation. Include `eboot.bin`, `sce_sys`, `sce_module` and assets. Do not send `eboot.bin` to the payload loader on port 9021.

The build tool also provides `bash app/build.sh deploy "$PS5_HOST"` when the kit FTP configuration matches the console. Manual staging supports other FTP ports. Preserve the previous folder until the replacement starts successfully.

```bash
curl -sS -X POST "http://$PS5_HOST:10101/api/v1/scan"
curl -sS -X POST "http://$PS5_HOST:10101/api/v1/games/mount" \
  -H 'Content-Type: application/json' -d '{"title_id":"PPSA99456"}'
```

Launch the mounted icon. A mount alone does not prove startup. Check the version, Home, controller input and startup log; then test picture, audible sound, seeks and EOF on a known local fixture before repeating over SMB/FTP or torrent. klog uses TCP 3232 on the tested setup; diagnostics persist under `/download0`. A remote property update is independent of decoded picture and audible output.

Settings and installed plugins persist in the title's `/download0` profile across app-folder replacements. The requested fresh-profile v60 test has a scoped one-time reset, gated by a separately uploaded request after a verified backup; its console execution is pending. Installing an archive alone does not clear the profile. Keep personal configurations, credentials, device identities and private tracker data out of release assets.

## Remote control and diagnostics

[STPP findings and implementation plan](docs/REMOTE_CONTROL_STPP.md) describes the observed Movian-to-Movian property protocol. Discovery uses UDP 42000; connect to the advertised HTTP port. STPP upgrades at `/api/stpp` and carries navigation, settings and live properties. Pixel/audio inspection requires separate verification.

This controller has been verified against an existing Movian peer. The PS5 title attempts STPP TCP 42000 when Network → Allow remote control is enabled. Diagnostics now use TCP 42001. Console binding is pending verification; use the actual reachable endpoint, not a claimed test pass.

```bash
python3 -m venv build/stpp-client-venv
build/stpp-client-venv/bin/pip install websocket-client==1.8.0
build/stpp-client-venv/bin/python scripts/movian_stpp.py \
  --url ws://PEER_IP:HTTP_PORT/api/stpp \
  --path '["clock","unixtime"]' --seconds 5
```

The CLI defaults to observation and emits JSON events. Explicit `--open-url settings:` and `--action back` request navigation. Integer writes need the correct live property path/reference; exported references expire on reconnect. Protocol-only tests need no WebSocket dependency: `python3 tests/stpp_protocol.py`. An STPP server belongs to a running app and cannot launch a closed/crashed title.

## Changes since published v53

- **v54:** desktop-observed settings structure, filename extensions, clock, alternate HTTPS repository, resume policy and shared torrent limits.
- **v55:** SMB2/FTP browsing and seekable playback, saved authentication, network settings and authenticated FTP server.
- **v56–v57:** startup clock correction and SMB random-source compatibility; subsequent console network playback/sound/seek confirmations.
- **v58:** asynchronous SMB2/FTP plugin ZIP and torrent-metainfo imports, bounded reads and saved servers in the ZIP picker.
- **v59:** native network initialization, precise HTTP-stage diagnostics and persisted TCP & UDP / TCP / UDP tracker selection. Small numeric-IP RAM torrent playback passed through hardware EOF; hostname discovery remains blocked.
- **v60:** STPP server/shared property adapter, native WSL control acceptance and default-off one-shot access/fresh-profile requests. Signed package and hashes are published; PS5 upload/runtime and fresh-profile execution remain pending.

See [Network v55](docs/NETWORK_V55.md), [v58](docs/NETWORK_V58.md), [v59](docs/NETWORK_V59.md) and the [UI work list](docs/UI_WORK_LIST.md) for test details.

## Source and credits

`app/` is the live native application; `scripts/` holds build/test tools; `tests/` holds host fixtures; `website/` is the GitHub Pages source. `archive/` contains superseded experiments. Current desktop behavior comes from the inspected M7 7.0.273 WSL build and retained evidence; TorrServer and its pinned engine inform the torrent implementation. See [SPEC.md](SPEC.md), [desktop inventory](docs/M7_CAPABILITIES.md), [credits](docs/CREDITS.md), [NOTICE.md](NOTICE.md) and [LICENSE](LICENSE).

Repository-authored code is GPL-3.0-or-later; dependencies retain their licenses. This independent project is unaffiliated with Sony. PlayStation and PS5 are Sony Interactive Entertainment trademarks.


The v60 access experiment requires `MOVIAN_TITLE_ACCESS_TEST=1` at build time
and a separate one-shot console request. Standard `bash app/build.sh` omits
helper support. See [the exact opt-in/reset procedure](docs/STPP_V60.md). Public
ZIPs contain no request markers. Host UI acceptance can be rerun with
`bash scripts/test-ui-stpp.sh` after provisioning its documented host dependencies.
