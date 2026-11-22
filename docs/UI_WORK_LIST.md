# PS5 UI work list

The reference is the installed Movian M7 7.0.273 build on WSL Ubuntu 24.04.5. Use the [running-build inventory](M7_CAPABILITIES.md), the [retained Home and Plugins captures](harvest/m7-7.0.273-wsl2/), and the REA ELF identity in `build/wsl-movian-dht-rea`. The desktop Settings navigation starts with General, Video, Audio, Subtitles, Languages, Look and feel, Bookmarks and Metadata. Network, BitTorrent, Search, Developer / Advanced, About and Upgrade follow. The PS5 maintains its own palette, type and artwork.

| Order | UI surface | Change and acceptance check | State |
| --- | --- | --- | --- |
| 1 | Settings index | Expose all 14 observed category names in order. Use D-pad to change focus, Cross to enter, and Circle to leave one level at a time. | Implemented in v48 |
| 2 | Settings list and value chooser | Scroll to every row. Open a pending control and show its state. Cancel a value change, then commit another and reload it. | Initial v48 implementation |
| 3 | Home | Map the search affordance and the Plugins, Local network, Settings and local source tiles from the WSL layout. Test focus across rows and return focus from each source. | Implemented in v49 and checked in the host preview. PS5 check pending. |
| 4 | Global sidebar and context actions | Add playback and queue controls, bookmarks, page reload, volume, media and system info, log viewer, sleep timer, Settings and exit flow. Open and close from content and playback. | v50 sidebar and file menus verified in host preview. Pending actions open status panels. PS5 check pending. |
| 5 | Library and files | Map directory list and grid, sorting, metadata and file action flows. Confirm PS5 media and USB entries open through the existing paths. | v51 list, grid, sorting and folder navigation verified in the host preview. PS5 check pending. |
| 6 | Plugins and repositories | Align catalog, installed and local-install navigation, plugin settings, item details and artwork states. Verify catalog return focus and installed-plugin selection. | v52 navigation and package flows verified with host fixtures. PS5 check pending. |
| 7 | Player overlays and pickers | Map transport, seek, audio and subtitle selectors, resume prompt, media info and queue. Test playback continuity when opening and cancelling overlays. | v53 transport, seek and track pickers checked on host. Subtitle rendering and PS5 check pending. |
| 8 | Settings completion | Compare every category against the live build, then add missing rows and state types. Check toggle, numeric, text, selector, confirmation and unavailable states with a controller. | Pending |
| 9 | PS5 visual verification | Capture Home, Settings, a value chooser, Plugins and active playback from the console. Compare navigation, focus and 1080p readability against the WSL observations. | Pending |

## v48 scope

Options and the Home Settings card open the categorized view. Existing settings keep their handlers and persistence. Cross opens a value selector for Repeat, Seek step, Volume, Plugin repository and Cache location. Circle cancels and Cross commits. The existing torrent tuning panel opens from BitTorrent and returns to its parent. Pending rows open a detail panel. The inventory is based on the retained categories and settings evidence and still needs a row-by-row comparison.

The shared-source host preview exercised category navigation, opening a pending option, cancelling a choice, saving and reloading a cache location, and returning from the torrent tuning panel. Screenshots are in `build/v48-ui`. The signed `build/PPSA99456-v48.zip` passed the full self-check and archive verification. Console display and DualSense input still need a PS5 run.

## v49 Home

- WSL M7 displayed a full-width Search bar followed by Plugins, Local network, Settings and user folders. The live Home → Search → Home requests were accepted. The installed Home view places search before the focusable service grid. REA confirmed the running binary's GNU build ID against the captured ELF. Evidence is in `build/v49-wsl-home` and the [retained Home capture](harvest/m7-7.0.273-wsl2/ui-home.png).
- The PS5 Home row starts with Plugins, Local network and Settings. Movian library, Title data and USB are the PS5 local sources. Continue watching and Bookmarks remain reachable. D-pad moves between rows and the search bar. Returning from a destination preserves the Home focus. Local network opens a status panel until its backend is implemented.
- Search opens the system keyboard and filters file and folder names at the top level of available local sources. It uses the existing Videos browser for results. It retains the last search and preserves focus when the keyboard is cancelled. Plugin and network search remain open work.
- The shared-source host check exercised grid movement, Plugins, Settings, local-folder and Bookmarks return focus, the Local network panel, keyboard cancel/submit and a case-insensitive search. See `build/v49-ui` for rendered frames. The signed package is `build/PPSA99456-v49.zip`. The system keyboard and physical DualSense still need a console run. Sandbox-lift remains excluded.

## v50 sidebar and context menus

- Opened Page settings and right-clicked the MPEG-1 test video in running WSL M7. The video menu started with Play from beginning, Continue from 0:00 and Play until end of list. The installed views establish the remaining order and Back, Cancel and Left behavior. REA identified the ELF running under WSL. Live and shipped-view evidence is in `build/v50-wsl-menus`.
- Options opens Page settings from Home, browsing and playback. D-pad selects rows, Cross activates, and Circle returns. Left closes the sidebar except on Master volume, where Left and Right adjust the value. Settings and information panels return to the selected sidebar row. During playback, Triangle opens the existing playback settings.
- Sidebar playback controls appear while playing, in the desktop order. The queue viewer, repeat, shuffle, bookmarks, reload, volume, media and system information, log and navigation use existing PS5 state. Selecting Home stops playback and returns to the Home tiles. Paste, sleep, screensaver, suspend and exit open status panels.
- Square opens the selected item menu in Videos. Video and general items have different action orders based on the installed M7 views. File details and path have readable panels. Unfinished item actions such as copy, delete and cast open status panels. Returning from a panel keeps the item action selected.
- A shared-source host check covered action order, pending actions, return focus, volume, the log viewer and context menus. Actual MPEG-1 playback on the host advanced with the sidebar open, paused and resumed through its controls, and kept playing when the sidebar closed. Host audio output and physical PS5 input remain unverified. The signed `build/PPSA99456-v50.zip` passed self-check and archive checks. Sandbox-lift remains excluded.

## v51 library and file browsing

- In running WSL M7, opened a fixture folder, inspected the Page layout and Sort on selectors, switched to Grid, opened a child folder and went back. The installed grid view uses five columns and four visible rows on the wide display. REA confirmed the WSL executable identity. Live list, grid, chooser and folder captures are in `build/v51-wsl-library`.
- Options exposes Page layout, Include in library, Sort on, Sort folders first, Show only supported files, Metadata mode and playback-history actions in the observed order. List and Grid, Filename, Newest first, Oldest first, folders-first and the file filter work. The last 48 adjusted pages retain their preferences in `/download0/browser-views.txt`.
- D-pad moves between tiles, L2/R2 page through entries, and Circle restores the focused folder in the parent. Changing the sort or layout retains focus by path. The directory reader retains the first entries in the selected sort even when a folder exceeds the display cap.
- Library indexing, metadata and bulk playback-history actions open a status panel. Grid tiles currently use file and folder icons. Desktop media thumbnails are pending.
- The shared-source host fixture checked the actual filesystem reader, all three sort modes, the two filters, view persistence, focus across reorders, grid paging, folder return and the simulated USB path. The signed `build/PPSA99456-v51.zip` passed self-check and archive validation. Physical USB, controller input and console rendering still need a PS5 run. Sandbox-lift remains excluded.

## v52 Plugins and repositories

- In running WSL M7, opened Plugins from Home and Browse available plugins from the hub. The installed list follows the catalog link. Captured the category grid and a local ZIP displayed by its manifest name and version. REA confirmed the WSL executable identity. The installed plugin detail view establishes metadata, artwork, package actions and changelog. Evidence is under `build/v52-wsl-plugins`.
- The PS5 hub starts with Browse available plugins, then Installed plugins and Install from ZIP. The catalog uses the WSL category order, plus All plugins for alternate feeds. It keeps the configured repository, supports category filtering and returns to the same category and catalog row. Package details show available and installed versions where present, and require selecting Install.
- The ZIP picker browses Movian library, Title data and USB folders. ZIP filtering happens before the display limit. Selecting a ZIP in Videos also opens details. Installed entries offer Open and Settings. Square retains the direct settings shortcut. Installed and local packages can display their archive artwork, and catalog and service pages can display HTTPS artwork for the focused item.
- Changelog, uninstall, update checks and unimplemented plugin actions open status panels. The host fixture used the actual plugin engine with controlled ZIPs and HTTP responses to test categorization, ZIP and repository installation, a saved plugin setting, pending actions, focus retention and both artwork sources. The signed `build/PPSA99456-v52.zip` excludes sandbox-lift. Live repository requests and console input still need a PS5 test.

## WSL testing access

Movian is available in Ubuntu 24.04 as `/usr/bin/showtime`. Its diagnostics are at `http://127.0.0.1:42000/api/logfile/0`. Settings, torrent, tracker, peer connection, peer download and FTP server logging were already enabled. The built-in FTP server uses `127.0.0.1:7777` from WSL. Confirmed that the stored login succeeded and an incorrect password received FTP 530. Credentials stay in `~/.hts/showtime/settings/ftpserver` with file mode `0600`. Stop WSL Movian before running the host diagnostics self-check, which also uses port 42000.


## v53 Player overlays and pickers

- In running WSL M7, opened the bundled test clip, exposed the playdeck, then opened the Audio / Video and Subtitles pickers. The timeline sits above the control row, with Audio on the left, Previous / Rewind / Play-Pause / Seek Forward / Next in the middle, and Subtitles / Settings on the right. The pickers replace the deck and return focus to their originating controls. REA confirmed GNU build ID `cecc8ab5765c085d19ffdc4e5c86649186a62cfa`. Captures and ELF evidence are under `build/v53-wsl-player`.
- Triangle opens the PS5 playdeck. Use Up to focus the seek bar, Left and Right to preview a position, Cross to commit, and Circle to cancel. Previous and Next use the existing queue. Options opens the global sidebar with media info and the current queue. Existing Continue watching and Play from beginning actions retain their behavior.
- The audio picker displays the container's title, language, format and current selection. Supported local audio tracks can be switched during playback. The subtitle picker shows Off and embedded tracks. Selecting an embedded subtitle displays its pending status. Subtitle rendering, external subtitle providers, torrent audio switching, and a separate resume prompt remain open.
- The host test remuxed a local H.264 fixture with two AAC tracks and one SRT track. It exercised playback while a picker was open, local audio selection, subtitle activation, forward and backward seek, cancellation, focus return, and pause/resume. PS5 audio output and DualSense behavior still need console verification. The signed package excludes sandbox-lift.
