# Movian PS5 v64 — shared Quit and native plugin removal preview

Native ShadowMountPlus test title **PPSA99456**, version **01.000.064**.
This prerelease passed the full host self-check, real WSL Movian remote-viewer
tests and signed packaging. **v64 has not been tested on PS5.** v62 remains
mounted; FTP 2121 and remote lifecycle services were unreachable on the last check.

## Changes since v60

- Fixed fresh-profile cleanup using the PS5 directory ABI. v61 reached Home and
  completed the requested reset on console. Its STPP/diagnostics binds still
  failed with errno 13; filesystem access did not establish helper credentials.
- Recovered v60's intentional shell exit after failed root-restoration lookups.
  v62 corrects loader resolution and the explicit owned helper request. Host
  access regressions pass; the actual console experiment remains unverified.
- General Quit, sidebar Exit and STPP Quit share an orderly shutdown callback.
  It closes playback, cancels torrent work, drains FTP/listing and owned workers,
  waits for IME, saves settings and requests shell exit. Shell refusal restores
  normal operation; the title never returns from main.
- Native-shaped plugin details with Author/status/version fields and one action
  in tested installed/local-package states. Working Uninstall removes package
  and private settings/stores, invalidates cached pages and preserves other data.
  R3 opens an installed plugin; Square opens its settings.
- Stable plugin properties and native dynamic Uninstall over STPP. The real
  unmodified WSL M7 viewer displayed the details, removed a disposable plugin and
  received Not installed. Another fixture's ZIP/settings survived.
- Documented the installed Minion exact-title launch/close API and completed a
  per-setting pending-feature inventory with independent acceptance tests.

## Install and control

Upload the complete extracted `PPSA99456` folder to `/data/homebrew/PPSA99456`
using your console FTP port and retain execute permissions. Verify files, scan
and mount through ShadowMountPlus, then launch and confirm v64/Home. Keep
production PPSA99455 separate. Mounting does not launch the title.

Once the title's listener is actually reachable, open `stpp://PS5_IP:42000` in
desktop Movian. Optional title-access support is included, but **no authorization
request is packaged**. Standard source builds omit that helper support by
default. Installing this ZIP alone does not reset a profile or run the helper.
Do not send signed `eboot.bin` to the payload loader.

## Limits

PS5 startup, helper credential/root proof, listener binding, WSL→PS5 control,
Quit/relaunch and plugin removal need console acceptance. Host tests do not
prove hardware picture or sound. Large Avanpost torrent playback/hostname
discovery, Upgrade/reinstall provenance, full descriptions/artwork resources and
complete desktop UI/settings parity remain unfinished.

[Build and install instructions](https://github.com/brinza666/movian_ps5/tree/test/native-playback)
· [v64 evidence](https://github.com/brinza666/movian_ps5/blob/test/native-playback/docs/UI_V64.md)
· [port log](https://github.com/brinza666/movian_ps5/blob/test/native-playback/docs/PORT_LOG.md)

## Assets

- `PPSA99456-v64.zip`: **7,713,845 bytes**.
- ZIP SHA-256: `422c96f03f906c9686f7e475770fc5c169848dd7cfd04887948c9ab3d82f6bae`.
- Eboot SHA-256: `605a9affb1765d331b1575fa9b4ff08ecdd9e612f83215ff3d0712e492814b1f`.
- `PPSA99456-v64-dependency-sources.tar`: the unchanged pinned upstream dependency
  inputs/notices from the v60 source bundle; new adapter/application changes are
  in this release's Git tag/source archive.
- `SHA256SUMS.txt`: checksums for both binary/source assets.
