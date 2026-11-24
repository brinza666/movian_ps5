# v64: native plugin details and Uninstall

**Console follow-up, 24 November:** v64 is now uploaded/mounted/running, and
startup logs reach UI initialization. WSL STPP failed because the console
listener cannot bind (errno13). See [the console report](STPP_V64_CONSOLE.md).
The offline/deployment-pending notes below describe the earlier host acceptance.


The reference remains the installed WSL M7 7.0.273, identified by the retained
REA ELF evidence in [planned options](PLANNED_OPTIONS.md). Its shipped flat view
uses title/artwork/synopsis, Status, Author, Installed version, Available version
when known, and one status-dependent button. The PS5 details page now follows
that field order and shows one Install or Uninstall action for the covered
states. Unverified Check for updates and Changelog detail actions were removed.
R3 opens an installed plugin from the PS5 catalog; Square retains plugin settings.
These are controller shortcuts outside the native details action list.

Uninstall runs on the existing plugin worker. It removes the selected installed
ZIP, its settings/stores, imported torrent descriptor and temporary package.
It clears that plugin's cached pages and artwork; KinoZal session data is cleared
when KinoZal is removed. It preserves other plugins and media. Descriptor-relative
operations refuse path escapes, bundled-plugin removal and linked root/package
paths, and never traverse links inside private data. A depth/work budget bounds
cleanup. If cleanup fails, it reports the error and retains the ZIP for retry;
some private files may already have been removed.

STPP exports native `plugin` rows, metadata and status properties, with stable
IDs tied to plugin IDs. The actual native dynamic `uninstall` event calls the
same worker as the local action. A bounded removed-plugin metadata history lets
an already-open remote details page show Not installed, with the installed
version cleared. An old reference cannot target another installed plugin.

## Verification on 24 November 2026

- The full optional self-check passed: isolated removal tests, existing access,
  STPP, RAM cache, tracker/DHT, Matroska EOF/seek and real SMB2/FTP regressions.
- Shared UI tests installed real ZIPs, ran Duktape routes/stores, removed one
  locally and another over a real WebSocket, verified private-data deletion and
  page invalidation, retried an old reference and ran the retained plugin again.
- The unmodified WSL Movian viewer connected to the shared production host UI
  on loopback port 42011. Its own native plugin details screen rendered the
  metadata and Uninstall button. Clicking that button removed the disposable
  remote ZIP/settings/stores, pushed Not installed and cleared Installed version.
  A separate fixture ZIP and settings marker survived. WSL returned to its local
  Home, and the owned host acceptance server was stopped.
- Signed public-SDK packaging and archive integrity passed. Host playback/shell
  stubs do not establish console playback, audibility or lifecycle behavior.

Private evidence: `build/v64-console/{self-check.log,ui-check.log,build.log,
package-verification.json,plugin-details.png}` and `build/v64-native/` native
screenshots, scripts and `result.json`. These new GUI observations are direct
X11/application-protocol evidence, not a new REA runtime trace. An imported old
capture helper initially executed its old navigation sequence; the test copy was
corrected and the final explicit actions above were repeated successfully.

## Open checks

v62 remains mounted on PS5. FTP 2121, ShadowMount 10101 and Minion 9180 timed out
on the latest checks after the user reported the PS5 online. Some requests briefly
returned connection refused. No v62 launch/helper/bind result or v64 console
deployment is claimed. Confirm the current console address/services, then follow
the [autonomous launch procedure](SHADOWMOUNT_CONTROL.md).

Upgrade/version eligibility and reinstall provenance are separate unfinished
tests. The retained remote status does not advertise Install after deleting its
only known ZIP. Full description layout, ZIP-relative remote artwork resources,
native popup focus and complete UI skin parity remain open. The new details
layout is not claimed pixel-identical. The access experiment remains separately
gated, and public archives contain no authorization or reset request files.
