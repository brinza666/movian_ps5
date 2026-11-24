# Planned UI options: evidence and implementation queue

Updated 2026-11-24. This is an inventory of visible PS5 placeholders, not a claim
that the desktop implements the same unfinished behavior. `settings_menu.hpp`
and the three menu dispatchers in `ui.cpp` are the inventory source. A handler
that only stores a preference does not pass its functional acceptance test.

## Reference and evidence

The reference is the installed WSL Ubuntu 24.04 `/usr/bin/showtime`, M7 7.0.273.
Its SHA-256 is `da133c4a6ee509da8f3b74dd2dfd1b445988381c5b3210d57b15a2e3a96ac9b1`;
GNU build ID is `cecc8ab5765c085d19ffdc4e5c86649186a62cfa`.
REA ELF evidence `ev_b592f41edf83f81bd55356450bda2b3cbc1726efb46946e4fd06b3afd3205bfb`
identifies that artifact. Existing native STPP action evidence is
`ev_c3ce5db4623bf60ac9167cb842605e30c93dae9dc7f42591c391b54ff79e50bf`.
Neither record proves the new behavioral observations below; those were observed
directly against the running installed application and its installed view files.
No old andoma source was consulted.

Private raw captures and scripts for this pass are in
`build/planned-options-20261124/`. Existing category/menu captures remain in
`build/v50-wsl-menus`, `build/v52-wsl-plugins`, and `build/v54-wsl-settings`.
Do not publish raw profiles, credentials, tracker responses, or private captures.

### Fresh native observations

1. **General → Starting and stopping → Quit.** Runtime row index 6 has type
   `action`, enabled 1, void `action`, and an `eventSink`. Sending the native
   `Activate` action to that row's event sink closes the process immediately.
   No confirmation was observed. `/usr/share/showtime/glwskins/flat/items/list/action.view`
   dispatches to the row event sink when the action string is absent. The test
   verified the title before activation and verified process termination afterward.
   WSL was restarted successfully with its original profile and returned to Home.
   Evidence: `general-runtime.json`, `quit-runtime.json`.
2. **Plugin details use one status-dependent button.** Installed
   `glwskins/flat/ctxmenu/plugin_details.view` displays title, artwork,
   synopsis/description, Status, Author, Installed version, Available version
   (versions are hidden when absent). The one button is `Install`, else `Upgrade`,
   else `Uninstall`, according to `canInstall`, `canUpgrade`, `canUninstall`.
   It delivers `install:<package>`, `upgrade`, or `uninstall` to the plugin
   property. The Changelog block is commented out in this installed view.
   `items/list/plugin.view` opens this popup using `pluginInfo` with the plugin
   property. This is a shipped view observation, not old repository source.
3. **Uninstall removes the plugin's private data.** A uniquely named disposable
   ECMAScript fixture was installed through a local ZIP. Its active settings
   group and store marker existed, and status was `Installed`, installed/loaded
   1, canUninstall 1, installedVersion `1.0.0`. The actual native popup showed
   only Uninstall (`plugin-detail.png`). Clicking it immediately removed the
   installed ZIP and the entire fixture private folder, including settings and
   stores; its settings group disappeared. Status became `Not installed`,
   installed/loaded 0, canInstall 1, canUninstall 0, installedVersion void.
   No confirmation was observed. Personal plugins were not modified.
   `plugin-after-uninstall-runtime.json`, `plugin-uninstalled.ppm` retain the
   post-action observation. The initial probe used an unsupported `javascript`
   package type and was corrected to the native `ecmascript` type before this
   successful test; do not reuse that failed first package as a native fixture.
4. **Boolean settings.** Installed `items/list/bool.view` toggles `value` on
   Activate, displays On/Off, and delivers `reset` to the value on itemMenu.
   Merely opening a status panel is not matching desktop behavior.

## Three-part build plan

1. **Shared actions and state.** Complete Quit, native-shaped plugin details and
   Uninstall; then seen/unseen/playback-info operations, clipboard, and the file
   deletion gate. Route local UI and STPP through the same callbacks. Use a
   disposable fixture for each destructive action, and reject path escape and
   symlink traversal. Preserve unrelated files. Do not implement arbitrary reset
   actions by deleting the whole profile.
2. **Settings with existing PS5 machinery.** Implement seek-step controls,
   playback threshold, audio delay/gain, subtitle sizing/position, list wrapping,
   debug filters and cache controls one feature at a time. Capture each option's
   actual WSL range, unit, default, reset and visible effect before implementation.
   Persisted settings must affect their consumer and round-trip over STPP.
3. **Remaining desktop capabilities.** Add archive browsing, copy/move,
   library/metadata indexing, providers/languages, screen saver and plugin view
   selection only after their runtime behavior and resource requirements are
   observed. Keep unimplemented rows clickable with an honest explanation.
   Search/Upgrade need new WSL evidence; they were absent from the recorded
   installed WSL Settings index. Never invent desktop defaults for these rows.

## Current completion

P01/B10 pass the v63 host acceptance; console Quit/relaunch is pending. P02's
installed/local details and scoped Uninstall pass v64 shared-UI and real native
WSL remote-viewer acceptance. [v64 evidence and remaining states](UI_V64.md)
records Upgrade/reinstall provenance and visual/resource limits. The catalog
keeps Open (R3) and Settings (Square) as PS5 navigation shortcuts outside the
single-button detail action list.

## Immediate acceptance tests

| ID | Feature | One acceptance test |
| --- | --- | --- |
| P01 | Quit / Exit | Trigger each entry through its shared callback with a live STPP subscriber; observe orderly termination rather than a pending panel; relaunch reaches Home. Native Quit has no observed confirmation. |
| P02 | Plugin details and Uninstall | Install a disposable ZIP with settings/store data; open details and verify native field/button order; uninstall, verify package and its private data disappear, routes unload, and another plugin/media survive. |
| P03 | Plugin Upgrade | Controlled repository advertises a newer fixture version; verify the single button becomes Upgrade, activate, then verify loaded and installed version change and error handling. Native runtime upgrade still needs observation. |
| P04 | Enable file deletion from item menu | With the gate Off, no usable Delete is offered; enable it, delete only a disposable local item, refresh and verify removal. Observe native confirmation before implementing one. |
| P05 | Mark seen / unseen | On a disposable media entry, mark seen then unseen and verify the same change in list decoration and playback metadata after reload. Native exact threshold/metadata still needs observation. |
| P06 | Erase playback information | Seed disposable resume/seen records, invoke erase for that folder, and verify only that scope loses the records. Native scope/confirmation still needs observation. |

### Plugin menu mismatch to correct

The PS5 v52-style details menu exposes Open, Settings, Check for updates,
Uninstall, Changelog for an installed plugin. **Check for updates and Changelog
are not verified desktop detail actions** and must not be counted as desktop
features awaiting implementation. Installed native details instead uses the
single status-dependent action above. Open and Settings should remain reachable
through their observed navigation locations, rather than being assumed to belong
in this popup. Verify unavailable/upgrade states with fixtures before changing
their rules. Uninstall must invalidate active plugin routes/settings after it
removes the package, and must report filesystem failures rather than announce
success for a partial removal.

## Sidebar and item-action inventory

The lists below describe dispatchers before the current in-progress Quit patch.
Each named action is a separate feature and acceptance test. A native screenshot
establishes placement/order only; it does not establish backend semantics.

| ID | Unfinished action | One acceptance test |
| --- | --- | --- |
| B01 | Include in library | Toggle on a disposable folder; verify native-observed indexed membership, persistence and a matching off transition. |
| B02 | Metadata mode | Select each native-observed mode; verify its actual lookup behavior and persisted selection. |
| B03 | Mark all as seen | Mark a disposable folder; verify every eligible child is marked and another folder is unchanged. |
| B04 | Mark all as unseen | Clear a disposable folder's seen state; verify every eligible child and unrelated-folder survival. |
| B05 | Erase all playback info | Erase a seeded disposable scope; verify resume and watched state follow the native-observed scope. |
| B06 | Paste | Copy a fixture through the application clipboard and paste it into a writable fixture destination; verify bytes and no overwrite without native-observed handling. |
| B07 | Activate sleep timer | Choose a native-observed interval and verify elapsed-time action, cancellation and displayed countdown. |
| B08 | Screen saver | Activate and dismiss it; verify the configured native-observed behavior and return focus. |
| B09 | Suspend | Verify the observed desktop action maps to an explicitly supported PS5 lifecycle operation; unavailable platform capability remains explained. |
| B10 | Exit | Same orderly shutdown acceptance test as P01; v63 host acceptance covers this shared route; console acceptance remains pending. |
| I01 | Play until end of list | Start midway through a disposable three-item folder; verify remaining playable items run in order and stop at the end. |
| I02 | Queue item | Queue two fixture items; verify playqueue order and next-item behavior. |
| I03 | Copy to clipboard | Copy a fixture URL and verify the native-observed clipboard representation is consumed by Paste. |
| I04 | Copy to location | Copy a fixture to a selected writable folder; verify byte identity, progress/errors and native overwrite handling. |
| I05 | Delete | Apply P04 to local, SMB and FTP fixtures only where those backends support deletion. |
| I06 | Mark as unseen | Apply P05 and verify persisted state after returning from the item menu. |
| I07 | Mark as seen | Apply P05 and verify persisted state after returning from the item menu. |
| I08 | Video cast | Discover a controlled native-observed target and verify URL/metadata handoff and a clear unsupported-target error. |

Other status-only surfaces: unsupported plugin item types reach a generic
planned-action notification; SMB Extended Attributes stores Off / SMBv1 / SMBv2 /
On but actual attribute storage remains pending. Its one test is a controlled
server fixture that records the expected metadata attribute read/write for each
mode and verifies persistence across application restarts.

## Settings inventory

The following rows have no action identifier in the current settings model.
Every row retains its own test ID. The per-kind tests are deliberately conditional
on fresh native observation: a test cannot pass just because a field can be edited.
Existing functional settings, page headings and pages are excluded. Child-provider
rows are expanded per page. Quit is now assigned an action by the v63
implementation and is tracked in P01 rather than as a pending setting.

For each test, `toggle` means change On/Off, verify the actual consumer, reset and
reopen; `number` means exercise the observed min/max/step/unit and verify consumer,
reset and reopen; `select` means choose every observed option, verify its consumer,
reset and reopen; `text` means save/cancel/edit, verify consumer and persistence;
`action` means observe its native scope/confirmation then verify the concrete effect
with an isolated fixture. These are independent acceptance tests for each row.

<!-- Generated inventory follows; source: app/src/settings_menu.hpp. -->

| ID | Pending setting | One acceptance test |
| --- | --- | --- |
| S001 | General → Enable file deletion from item menu | toggle: observe, exercise consumer, reset/reopen |
| S002 | General → Store per-file settings in filesystem | toggle: observe, exercise consumer, reset/reopen |
| S003 | General → Browse archives as folders | toggle: observe, exercise consumer, reset/reopen |
| S004 | General → Launch plugin on M7 start | text: observe, exercise consumer, reset/reopen |
| S005 | General → Clear cached files | action: observe, exercise consumer, reset/reopen |
| S006 | General → Clear all metadata | action: observe, exercise consumer, reset/reopen |
| S007 | General → Forget remembered passwords | action: observe, exercise consumer, reset/reopen |
| S008 | Video → OpenGL Driver | select: observe, exercise consumer, reset/reopen |
| S009 | Video → OpenGL Adapter | select: observe, exercise consumer, reset/reopen |
| S010 | Video → UI Refresh | select: observe, exercise consumer, reset/reopen |
| S011 | Video → Duration | number: observe, exercise consumer, reset/reopen |
| S012 | Video → Title | toggle: observe, exercise consumer, reset/reopen |
| S013 | Video → Description | toggle: observe, exercise consumer, reset/reopen |
| S014 | Video → Audio & Subtitles | toggle: observe, exercise consumer, reset/reopen |
| S015 | Video → Count video as played when reaching | number: observe, exercise consumer, reset/reopen |
| S016 | Video → Step when seeking backward | number: observe, exercise consumer, reset/reopen |
| S017 | Video → Step when seeking forward | number: observe, exercise consumer, reset/reopen |
| S018 | Video → Up / Down during video playback controls | select: observe, exercise consumer, reset/reopen |
| S019 | Video → Video zoom | number: observe, exercise consumer, reset/reopen |
| S020 | Video → Vertical scale | number: observe, exercise consumer, reset/reopen |
| S021 | Video → Vertical pan | number: observe, exercise consumer, reset/reopen |
| S022 | Video → Horizontal scale | number: observe, exercise consumer, reset/reopen |
| S023 | Video → Horizontal pan | number: observe, exercise consumer, reset/reopen |
| S024 | Video → Stretch video to widescreen | toggle: observe, exercise consumer, reset/reopen |
| S025 | Video → Stretch video to fullscreen | toggle: observe, exercise consumer, reset/reopen |
| S026 | Video → Video frame interpolation | toggle: observe, exercise consumer, reset/reopen |
| S027 | Video → Buffer Size | number: observe, exercise consumer, reset/reopen |
| S028 | Video → Pre-buffer | number: observe, exercise consumer, reset/reopen |
| S029 | Video → Adaptive Format | select: observe, exercise consumer, reset/reopen |
| S030 | Audio → Audio gain | number: observe, exercise consumer, reset/reopen |
| S031 | Audio → Audio delay | number: observe, exercise consumer, reset/reopen |
| S032 | Audio → Play test signal | toggle: observe, exercise consumer, reset/reopen |
| S033 | Audio → Test signal type | select: observe, exercise consumer, reset/reopen |
| S034 | Audio → Front Left | toggle: observe, exercise consumer, reset/reopen |
| S035 | Audio → Center | toggle: observe, exercise consumer, reset/reopen |
| S036 | Audio → Front Right | toggle: observe, exercise consumer, reset/reopen |
| S037 | Audio → LFE | toggle: observe, exercise consumer, reset/reopen |
| S038 | Audio → Surround Left | toggle: observe, exercise consumer, reset/reopen |
| S039 | Audio → Surround Right | toggle: observe, exercise consumer, reset/reopen |
| S040 | Audio → Rear Left | toggle: observe, exercise consumer, reset/reopen |
| S041 | Audio → Rear Right | toggle: observe, exercise consumer, reset/reopen |
| S042 | Subtitles → Subtitle size | number: observe, exercise consumer, reset/reopen |
| S043 | Subtitles → Force subtitles to reside on video frame | toggle: observe, exercise consumer, reset/reopen |
| S044 | Subtitles → Subtitle position | select: observe, exercise consumer, reset/reopen |
| S045 | Subtitles → Subtitle vertical displacement | number: observe, exercise consumer, reset/reopen |
| S046 | Subtitles → Subtitle horizontal displacement | number: observe, exercise consumer, reset/reopen |
| S047 | Subtitles → Color | text: observe, exercise consumer, reset/reopen |
| S048 | Subtitles → Shadow color | text: observe, exercise consumer, reset/reopen |
| S049 | Subtitles → Shadow offset | number: observe, exercise consumer, reset/reopen |
| S050 | Subtitles → Outline color | text: observe, exercise consumer, reset/reopen |
| S051 | Subtitles → Outline size | number: observe, exercise consumer, reset/reopen |
| S052 | Subtitles → Bounding box opacity | number: observe, exercise consumer, reset/reopen |
| S053 | Subtitles → Ignore embedded styling | toggle: observe, exercise consumer, reset/reopen |
| S054 | Subtitles → Remove SDH commentary | toggle: observe, exercise consumer, reset/reopen |
| S055 | Subtitles → Path to central folder | text: observe, exercise consumer, reset/reopen |
| S056 | Subtitles → Subtitles embedded in video file → Enabled | toggle: observe, exercise consumer, reset/reopen |
| S057 | Subtitles → Subtitles embedded in video file → Automatically select from this source | toggle: observe, exercise consumer, reset/reopen |
| S058 | Subtitles → Subtitles embedded in video file → Source priority | number: observe, exercise consumer, reset/reopen |
| S059 | Subtitles → Subtitles with matching filename in same folder as video → Enabled | toggle: observe, exercise consumer, reset/reopen |
| S060 | Subtitles → Subtitles with matching filename in same folder as video → Automatically select from this source | toggle: observe, exercise consumer, reset/reopen |
| S061 | Subtitles → Subtitles with matching filename in same folder as video → Source priority | number: observe, exercise consumer, reset/reopen |
| S062 | Subtitles → Subtitles with matching filename in central folder → Enabled | toggle: observe, exercise consumer, reset/reopen |
| S063 | Subtitles → Subtitles with matching filename in central folder → Automatically select from this source | toggle: observe, exercise consumer, reset/reopen |
| S064 | Subtitles → Subtitles with matching filename in central folder → Source priority | number: observe, exercise consumer, reset/reopen |
| S065 | Subtitles → Any subtitle in same folder as video → Enabled | toggle: observe, exercise consumer, reset/reopen |
| S066 | Subtitles → Any subtitle in same folder as video → Automatically select from this source | toggle: observe, exercise consumer, reset/reopen |
| S067 | Subtitles → Any subtitle in same folder as video → Source priority | number: observe, exercise consumer, reset/reopen |
| S068 | Subtitles → Any subtitle in central folder → Enabled | toggle: observe, exercise consumer, reset/reopen |
| S069 | Subtitles → Any subtitle in central folder → Automatically select from this source | toggle: observe, exercise consumer, reset/reopen |
| S070 | Subtitles → Any subtitle in central folder → Source priority | number: observe, exercise consumer, reset/reopen |
| S071 | Languages → Language | select: observe, exercise consumer, reset/reopen |
| S072 | Languages → Primary audio language code | text: observe, exercise consumer, reset/reopen |
| S073 | Languages → Secondary audio language code | text: observe, exercise consumer, reset/reopen |
| S074 | Languages → Tertiary audio language code | text: observe, exercise consumer, reset/reopen |
| S075 | Languages → Primary subtitle language code | text: observe, exercise consumer, reset/reopen |
| S076 | Languages → Secondary subtitle language code | text: observe, exercise consumer, reset/reopen |
| S077 | Languages → Tertiary subtitle language code | text: observe, exercise consumer, reset/reopen |
| S078 | Languages → Default character set | select: observe, exercise consumer, reset/reopen |
| S079 | Languages → Ignore 'The' at beginning of words when sorting | toggle: observe, exercise consumer, reset/reopen |
| S080 | Look and feel → Fonts | action: observe, exercise consumer, reset/reopen |
| S081 | Look and feel → Font and icon size | number: observe, exercise consumer, reset/reopen |
| S082 | Look and feel → Interface horizontal shrink | number: observe, exercise consumer, reset/reopen |
| S083 | Look and feel → Interface vertical shrink | number: observe, exercise consumer, reset/reopen |
| S084 | Look and feel → Wrap when reaching beginning/end of lists | toggle: observe, exercise consumer, reset/reopen |
| S085 | Look and feel → Emulate Up/Down buttons with mouse wheel | toggle: observe, exercise consumer, reset/reopen |
| S086 | Look and feel → Loading Indicator | select: observe, exercise consumer, reset/reopen |
| S087 | Look and feel → Custom background image | text: observe, exercise consumer, reset/reopen |
| S088 | Look and feel → Screensaver delay | number: observe, exercise consumer, reset/reopen |
| S089 | Look and feel → Use Bing images of the day | toggle: observe, exercise consumer, reset/reopen |
| S090 | Look and feel → Folder for screensaver images | text: observe, exercise consumer, reset/reopen |
| S091 | Look and feel → Seconds per image | number: observe, exercise consumer, reset/reopen |
| S092 | Look and feel → Background | select: observe, exercise consumer, reset/reopen |
| S093 | Look and feel → Loading screen | select: observe, exercise consumer, reset/reopen |
| S094 | Look and feel → Screen saver | select: observe, exercise consumer, reset/reopen |
| S095 | Look and feel → Home page | select: observe, exercise consumer, reset/reopen |
| S096 | Look and feel → On Screen Keyboards | select: observe, exercise consumer, reset/reopen |
| S097 | Look and feel → Audio tracks | select: observe, exercise consumer, reset/reopen |
| S098 | Look and feel → Album | select: observe, exercise consumer, reset/reopen |
| S099 | Look and feel → List of albums | select: observe, exercise consumer, reset/reopen |
| S100 | Look and feel → Artist | select: observe, exercise consumer, reset/reopen |
| S101 | Look and feel → TV channels | select: observe, exercise consumer, reset/reopen |
| S102 | Look and feel → Images | select: observe, exercise consumer, reset/reopen |
| S103 | Look and feel → Movies | select: observe, exercise consumer, reset/reopen |
| S104 | Bookmarks → Add new entry | action: observe, exercise consumer, reset/reopen |
| S105 | Metadata → thetvdb.com → Enabled | toggle: observe, exercise consumer, reset/reopen |
| S106 | Metadata → thetvdb.com → Language (ISO 639-1 code) | text: observe, exercise consumer, reset/reopen |
| S107 | Metadata → themoviedb.org → Enabled | toggle: observe, exercise consumer, reset/reopen |
| S108 | Metadata → themoviedb.org → Language (ISO 639-1 code) | text: observe, exercise consumer, reset/reopen |
| S109 | Metadata → themoviedb.org → Use original title | toggle: observe, exercise consumer, reset/reopen |
| S110 | Metadata → last.fm → Enabled | toggle: observe, exercise consumer, reset/reopen |
| S111 | BitTorrent → Max upload speed | number: observe, exercise consumer, reset/reopen |
| S112 | BitTorrent → Max usage of free space for caching torrents | number: observe, exercise consumer, reset/reopen |
| S113 | BitTorrent → Max memory for active transfers | number: observe, exercise consumer, reset/reopen |
| S114 | BitTorrent → Max cache size per torrent | number: observe, exercise consumer, reset/reopen |
| S115 | BitTorrent → Clear cache | action: observe, exercise consumer, reset/reopen |
| S116 | Developer → Disable HTTP connection reuse | toggle: observe, exercise consumer, reset/reopen |
| S117 | Developer → SW Create video thumbnails | select: observe, exercise consumer, reset/reopen |
| S118 | Developer → HLS SD width limit | select: observe, exercise consumer, reset/reopen |
| S119 | Developer → Enable indexer option | toggle: observe, exercise consumer, reset/reopen |
| S120 | Developer → Debug all HTTP requests | toggle: observe, exercise consumer, reset/reopen |
| S121 | Developer → Debug various ecmascript engine things | toggle: observe, exercise consumer, reset/reopen |
| S122 | Developer → Log AV-diff stats | toggle: observe, exercise consumer, reset/reopen |
| S123 | Developer → Debug HLS | toggle: observe, exercise consumer, reset/reopen |
| S124 | Developer → Debug FTP Client | toggle: observe, exercise consumer, reset/reopen |
| S125 | Developer → Debug FTP Server | toggle: observe, exercise consumer, reset/reopen |
| S126 | Developer → Debug directory listings | toggle: observe, exercise consumer, reset/reopen |
| S127 | Developer → Debug library indexer | toggle: observe, exercise consumer, reset/reopen |
| S128 | Developer → Debug SMB/CIFS (Windows File Sharing) | toggle: observe, exercise consumer, reset/reopen |
| S129 | Developer → Debug read/writes to URL key/value store | toggle: observe, exercise consumer, reset/reopen |
| S130 | Developer → Debug icecast streaming | toggle: observe, exercise consumer, reset/reopen |
| S131 | Developer → Debug image loading and decoding | toggle: observe, exercise consumer, reset/reopen |
| S132 | Developer → Debug metadata lookups | toggle: observe, exercise consumer, reset/reopen |
| S133 | Developer → Debug settings store/load from disk | toggle: observe, exercise consumer, reset/reopen |
| S134 | Developer → Debug threads | toggle: observe, exercise consumer, reset/reopen |
| S135 | Developer → Debug UPNP | toggle: observe, exercise consumer, reset/reopen |
| S136 | Developer → Debug Bittorrent general events | toggle: observe, exercise consumer, reset/reopen |
| S137 | Developer → Debug Bittorrent tracker communication | toggle: observe, exercise consumer, reset/reopen |
| S138 | Developer → Debug Bittorrent peer connections | toggle: observe, exercise consumer, reset/reopen |
| S139 | Developer → Debug Bittorrent peer downloading | toggle: observe, exercise consumer, reset/reopen |
| S140 | Developer → Debug Bittorrent peer uploading | toggle: observe, exercise consumer, reset/reopen |
| S141 | Developer → Debug Bittorrent disk I/O | toggle: observe, exercise consumer, reset/reopen |
| S142 | Developer → Debug input events | toggle: observe, exercise consumer, reset/reopen |
| S143 | Search → Search sources | action: observe, exercise consumer, reset/reopen |
| S144 | Search → Search history | action: observe, exercise consumer, reset/reopen |
| S145 | Upgrade → Check for updates | action: observe, exercise consumer, reset/reopen |
| S146 | Upgrade → Upgrade channel | select: observe, exercise consumer, reset/reopen |
| S147 | Upgrade → Automatic upgrades | toggle: observe, exercise consumer, reset/reopen |
