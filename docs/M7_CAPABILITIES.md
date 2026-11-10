# Official M7 capability inventory

Harvested 11 Nov 2026 from a **live** WSL2 install of
`movianM7-7.0.273-wsl2-amd64.deb`, plus
[apps.movian.eu](https://apps.movian.eu/), the Android APK 7.0.288
(REA, not executed), Chrome/NaCl changelog, and
[czz/movian-go](https://github.com/czz/movian-go) as the readable
source map of the same product.

This is the **product we are porting**. The PS5 title is a native
reimplementation (`app/`), not this ELF.

| Artifact | Version | How harvested |
| --- | --- | --- |
| WSL2 `.deb` / `/usr/bin/showtime` | **7.0.273** (2026-08-16) | Installed in Ubuntu-24.04, launched under WSLg |
| Android APK | **7.0.288** (2026-10-22) | Package + JADX manifest only |
| Official plugin feed | live | `https://repo.movian.eu/plugins-v1.json` |
| Chrome/NaCl | 7.0.273 | changelog/readme |
| PS3 PKG | 7.0.273 | listed, not installed |

Screenshots from this workstation:

- [docs/images/m7-wsl-home.png](images/m7-wsl-home.png) — Home
- [docs/images/m7-wsl-live.png](images/m7-wsl-live.png) — Global settings

Raw extracts (not all committed): `docs/harvest/`.

---

## 1. What M7 is

M7 (**Movian 7**) is a **light-weight 10-foot media center**, formerly
Showtime / Movian MOD. It is **not** a generic file player:

- One **home screen of services** (plugins + discovered sources + folders).
- Type a path or URL in the **search bar** to open anything.
- Play local files, LAN shares, HLS/HTTP, torrents, IPTV plugins.
- **Duktape** plugins (`plugin.json` + `plugin.js`, API v2).
- **No account, no analytics, no telemetry** (privacy policy 29 Sep 2026).
  Optional “Bandwidth Sharing / Web Indexing” exists as a partner toggle
  in Settings; leave it off.

Maintainer of current M7 builds: Dean Kasabow (`dean` / pluginsBG).
Original Showtime: Andreas Öman (`lonelycoder`).

Desktop package identity is still **`showtime`**: binary `/usr/bin/showtime`,
data `~/.hts/showtime`, cache `~/.cache/movian`, Start-menu name
**Showtime (Ubuntu)**. Window title is **Movian**.

---

## 2. How it looks (observed on this PC)

Live WSLg window, OpenGL renderer **llvmpipe** (software), Weston WM,
fullscreen via **F11**.

**Home**

- Dark textured wallpaper (`res/static/m7bg.jpg`).
- Title **Home**, hamburger (opens sidebar), clock in the corner.
- Full-width **search bar** (`search:`).
- 10-foot **icon grid** (about 5 columns on a wide window). Observed tiles:
  - **Plugins** (puzzle)
  - **Local network** (hub) — WSD/SMB workgroups already populated
    (`WORKGROUP_DESKTOP-UDHSAS3`, TrueNAS, Home Assistant, …)
  - **Settings** (gear)
  - XDG folders for the Linux user (`brinza` Videos/Music/Pictures)
- Hover scale on the focused tile (GLW `displacement`).

**Global settings** (opened via `settings:`):

General · Video · Audio · Subtitles · Languages · Look and feel ·
Bookmarks · Metadata · (further rows below the fold: Network, BitTorrent,
Search, Developer/Advanced, About, Upgrade).

**Sidebar / page settings** (`|||` / Menu): stop / prev / rewind /
play-pause / ff / next / playqueue / repeat / shuffle, bookmark, paste,
reload, master volume, media info, system info, view log, sleep timer,
Home, Settings, Screensaver, Exit.

**Skin:** bundled **flat** GLW skin only
(`/usr/share/showtime/glwskins/flat/`). Page views include home,
directory, list, grid, movies, video, album, slideshow, searchresults,
playqueue, settings, upgrade, about, stpp (remote), osk, osd.

**Fonts:** Roboto Condensed (UI), Ubuntu Mono, Liberation Sans, Arial
user font. 20 language packs (`lang/*.lang`).

This is the visual language the PS5 shell should converge on — not
VLC’s library, not a four-tab strip unless we keep that as a DualSense
shortcut into the same home.

---

## 3. Platforms (official)

| Platform | Package | Notes |
| --- | --- | --- |
| Android / Android TV / Google TV / Fire TV | APK 7.0.288 | min SDK 21, target 35; armv7/arm64/x86/x86_64 |
| WSL2 Ubuntu 24.04 | `.deb` 7.0.273 | GTK2 + GLX; D3D12 or llvmpipe |
| Linux Ubuntu/Mint 20–24 | `.deb` 7.0.273 | |
| macOS ARM / Intel | `.dmg` 7.0.276 | |
| iOS / tvOS | unsigned IPA 7.0.276 | Sideloadly |
| Chrome pNaCl | zip 7.0.273 | **Chrome ≤ 113 only** |
| PlayStation **3** | `.pkg` 7.0.273 | Install Package Files |
| Raspberry Pi 1/2/3 | STOS image | `curl …:42000/api/replace` |

There is **no official PS4/PS5 build**. That is this repo.

Android extras that **do not exist** in the WSL ELF (do not promise on
PS5): Widevine DRM7, Google Cast, Picture-in-Picture, MediaCodec tunnel
mode, WebKit (7.0.288), Android IME/voice, notification shade, APK
self-update.

---

## 4. Architecture of the real binary

Observed from the WSL ELF + log:

| Layer | Fact |
| --- | --- |
| UI | **GLW** (Showtime widget language), OpenGL 2.x shaders in `res/shaders/glsl/` |
| JS | **Duktape** (M7 7.0.0 bumped to 1.8.0). Host modules under `res/ecmascript/` |
| Decode | **FFmpeg 4.4.4** (`Lavf58.76.100`, `Lavc58.134.100`) statically linked |
| Metadata DB | SQLite `metadb` v18, `kvstore` v2 |
| Audio (Linux) | PulseAudio |
| Discovery | SSDP/UPnP, WSD, NetBIOS/nmb, STPP UDP 42000, BT UDP tracker |
| HTTP control | TCP **42000** (also STPP discovery on the same port) |
| Media HTTP | separate `http-server` (this run: TCP 44364) serving `/api/image/…` |
| Persistent | `~/.hts/showtime` |
| Cache | `~/.cache/movian/bc2` blobcache, default max 1000 MB |

ELF notes (REA): ELF64 x86-64 shared object, GNU build-id
`cecc8ab5765c085d19ffdc4e5c86649186a62cfa`, ABI Linux 3.2.0. Package
depends: GTK2, GL, Pulse, SQLite3, OpenSSL 3, Avahi, VDPAU, fontconfig,
freetype, gdk-pixbuf.

---

## 5. Home services and navigation URIs

Internal URIs (from the ELF + live `/api/prop`):

| URI | Role |
| --- | --- |
| `page:home` | Home |
| `page:about` | About |
| `search:` | Global search / open box |
| `settings:` | Settings root |
| `settings:general` `video` `audio` `subtitles` `i18n` `lookandfeel` `bookmarks` `metadata` `network` `bittorrent` `search` `tv` `dev` | Settings pages |
| `plugin:` `plugin:repo` `plugin:repo:categories` `plugin:start` | Plugin browser / autostart |
| `playqueue:` | Queue |
| `slideshow:` | Image slideshow |

Home tiles are **services** (`$core.services.stable`). Types encoded in
the skin: `tv`, `usb`, `cd`, `dvd`, `bluray`, `network`, `setting`,
`storage`, `plugin`, `movian`.

Live `services/all` on this LAN already included SMB workgroups via WSD.

---

## 6. Playback

### 6.1 Containers / files (desktop MIME + changelog)

Video: MXF, Ogg, MPEG-TS (`video/mp2t`), MP4, MKV, AVI, MOV, WMV/ASF,
FLV, WebM, MPEG-1/2, NSV, DVD/VCD/SVCD, VIDEO_TS folders, DVD **ISO**.

Audio: MP3, AAC/M4A, FLAC, Ogg/Vorbis, Opus, WavPack, APE, Musepack,
WMA, AC3, DTS, MIDI, tracker modules, Icecast/PLS/M3U.

Images: JPEG (screensaver/Bing), PNG, SVG, **WEBP** (7.0.256+), experimental AVIF/HEIC.

Archives browsed as folders: **zip**, **rar**.

### 6.2 Codecs (FFmpeg 4.4.4 + platform HW)

Software (all platforms that ship libav): H.264, HEVC, MPEG-2/4,
VP8/VP9, AV1 (SW where no HW), ProRes (SW, 7.0.274), DivX/XviD/WMV/VC-1
(Android can transcode those to MPEG-2 for TV MEMC).

HDR signalling in UI: HLG, HDR10, Dolby Vision badges; DV profile
detection in MKV/MP4/TS; tone-map to SDR on Android SW path.

**PS5 target:** start with MPEG-1 + H.264 + AAC (Phase 2), then HEVC via
`libSceVideodec2` (Phase 2b). Do not advertise AV1/DV/HDR until HW path
exists.

### 6.3 Streaming protocols

| Scheme | Role |
| --- | --- |
| `http://` `https://` | Progressive, with Range |
| `hls:` `hlsv:` | HLS TS **and** fMP4 (v4); live + VOD; master playlist auto-detect |
| `fmp4://` | Built-in `movian/mp4dash` — DASH→HLS on the fly (clear) |
| `icecast:` | Radio |
| `htsp://host:port` | Tvheadend |
| `udp:` | Raw UDP |
| `videoparams:{json}` | Plugin-constructed player (canonicalUrl, sources, epg, artwork) |

HLS features worth copying later: variant/quality picker, audio-group
AUTOSELECT, WEBVTT subs, live edge / chase-play modes, resolution cap
(SD/720/1080/1440/4K/AVC-only), DD/DD+/DTS in PMT.

**Clear DASH** works on Linux/PS3/NaCl via `mp4dash`. **Widevine DASH/HLS
is Android-only (DRM7)** — PS5 non-goal.

### 6.4 Player UX

- OSD: seek bar, time remaining, title, clock, A/V/sub cycle, SD/HD/FHD/4K and 2.0/5.1/7.1 badges
- Resume / continue from / play from beginning
- Mark seen / unseen (metadb)
- Play until end of list / auto next
- Playqueue, shuffle, repeat
- Playback speed (Android HW+TM; desktop more limited)
- Audio delay, per-file volume, master volume (dB)
- Copy to location… (files and torrent:browse)
- Item menu: Queue, Delete, Bookmark, Copy

### 6.5 Subtitles

Sources: embedded, sidecar same folder, central folder, plugin providers
(`movian/subtitles`). Formats: SRT, WebVTT (incl. MM:SS.MLS), ASS/SSA
(simple), DVB, PGS/VobSub (via DRM7 path on Android).

Settings: language priority, SDH strip, bounding-box opacity, force on
video frame, ignore embedded styling, `{\an1}`–`{\an9}`.

### 6.6 Metadata

TMDB (`api.themoviedb.org/3`), TVDB API v4, IMDb title links, Last.fm
scrobble (`movian/videoscrobbler`), NFO, DLNA DIDL probe (optional).
Bing HP images for screensaver.

---

## 7. Local and LAN files

| Scheme | Role | PS5 note |
| --- | --- | --- |
| `file://` | Filesystem | `/download0`, `/data/movian`, USB |
| `smb://` | SMBv1 | legacy |
| `smb2://` | SMB 2.0–3.11 (libsmb2) | Phase 5; VLC already builds libsmb2 |
| WSD / NetBIOS | Discover Windows 10/11 shares | optional |
| `ftp://` | FTP client (REST, STOR, DELE, APPE) | |
| `webdav://` `webdavs://` | WebDAV | low priority |
| `upnp:` | DLNA/UPnP browse | Phase 5 |
| FTP **server** | Serve USB/SMB to other M7s | skip on PS5 |
| HTTP control `/usb` `/tmp` | Android-only helpers | skip |

File-browser options: sort folders first, show only supported, newest
first, locatedb/Spotlight search (desktop), delete from item menu,
per-file settings on disk, browse archives as folders.

---

## 8. BitTorrent (built-in, not a plugin)

URIs: `magnet:`, `torrent:`, `torrent:browse:`, `torrent:video:`,
`torrentfile://<infohash>/…`.

Settings (from lang + changelog): enable, cache path, max cache per
torrent (up to ~4 GB on recent Android; 2 GB historically), max memory
for active transfers, max free-space %, max peers/connections, tracker
protocol TCP/UDP/both, inject extra trackers (ngosang + newtrackon +
`tr.movian.eu`), hash check, custom tracker UA, play-while-download,
prioritize pieces needed for decode.

Media info shows speed, peers, seeders, cache MB.

**PS5:** Phase 3. HTTP+UDP trackers, plaintext peers, cache under
`/download0/bt`. No DHT required for v1.

---

## 9. Plugin system (the product differentiator)

### 9.1 Official feed (live)

`https://repo.movian.eu/plugins-v1.json` — always merged; alt repo is
additive (`settings:general` “Alternate plugin Repository URL”).

Installed catalog **visible in the live process**
(`/api/prop/global/plugins/nodes`):

| id | Title | Category | What it is |
| --- | --- | --- | --- |
| `pluginsBG` | pluginsBG | video | Paid pack: torrents + subsBG + HBO/MAX helpers |
| `bgTV` | bgTV | tv | Paid BG IPTV + EPG (~140 channels) |
| `skyF1` | Formula 1 | tv | F1TV live/archive (key) |
| `torrents` | torrents | video | tGX + YTS |
| `nebula` | Nebula | video | nebula.tv subscriber |
| `tivix` | Tivix | video-ru | Free channels |
| `tv` | Online TV | tv | M3U/XML playlist loader |
| `shoutcast` | SHOUTcast | music | |
| `somafm` | SomaFM | music | |
| `enigma2` | Enigma2 | tv | Satellite receiver stream |
| `keysBG` `rua_osk` `ell_osk` | OSK layouts | glwosk | Bulgarian / Cyrillic / Greek |
| `glwVIDEO` | glwVIDEO | other | Sample: replace video page view |

Blacklist in the same JSON: old `subsBG`, `pX`, `1337x`.

**Do not ship paid IPTV/F1/HBO keys in the PS5 title.** Support the
**host API** so a user can drop a zip. Optional: later point Plugins at
`https://repo.movian.eu/plugins-v1.json` and/or `plugins/repo.json`.

### 9.2 Plugin shape

Zip with `plugin.json`:

```json
{
  "type": "ecmascript",
  "id": "example",
  "file": "plugin.js",
  "apiversion": 2,
  "category": "video",
  "control": { "uriprefixes": ["example:"] }
}
```

`this` is the plugin object: `createService`, `addURI`, `addSearcher`,
`createSettings`, `createStore`, item hooks, `page.openurl` /
`page.redirect`.

Page item types used by the flat skin: `video`, `audio`, `image`,
`directory`, `plugin`, `action`, `bool`, `integer`, `string`,
`multiopt`, `network`, `font`, `tvchannel`, `tvepisode`, `station`,
`person`, `event`, `location`, `separator`, `settings`, `search`.

### 9.3 Host modules shipped in 7.0.273

JS wrappers in `res/ecmascript/modules/`:

`fs`, `http`, `https`, `url`, `querystring`, `websocket`,
`movian/page`, `movian/http`, `movian/html`, `movian/xml`,
`movian/xmlrpc`, `movian/prop`, `movian/service`, `movian/settings`,
`movian/store`, `movian/sqlite`, `movian/subtitles`,
`movian/itemhook`, `movian/popup`, `movian/mp4dash`,
`movian/videoscrobbler`.

Native `require()` names referenced from those files / api-v1:

`native/fs`, `native/io`, `native/prop`, `native/route`, `native/hook`,
`native/crypto`, `native/string`, `native/popup`, `native/misc`,
`native/metadata`, `native/subtitle`.

Legacy `showtime.*` API still provided (`res/ecmascript/legacy/api-v1.js`).

Android-only plugin routes: `movianDRM:dash:`, `movianDRM:hls:`,
`drm7://`, `m7://`.

**PS5 Phase 4 minimum:** Duktape 2.x, `createService` + `addURI` +
`appendItem(video)` + `native/io` HTTP GET + `native/fs` read. Then
`kvstore`/`store`. Defer popup/CEF, sqlite, xmlrpc, mp4dash.

---

## 10. HTTP Control and remote

TCP **42000** (observed `Server: Movian 7.0.273`):

| Path | Use |
| --- | --- |
| `GET /?url=` | Open a Movian URI (home, settings, file, …) |
| `GET /api/diag` | Version |
| `GET /api/logfile/0` | Live log |
| `GET /api/prop/global/…` | Dump the prop tree |
| `GET /api/open?url=` | Navigate |
| `GET /api/torrents` | Torrent list |
| `GET /api/ecmascript/stats` `…/gc` | Duktape |
| `GET /api/translation` | Upload `.lang` |
| `GET /api/screenshot` | PNG (failed to compress on llvmpipe here) |
| `GET/POST /api/stpp` | Remote-control protocol |
| `POST /api/input/action` `…/utf8` | Inject keys (path present in ELF) |
| ` /api/notifyuser` `/api/restart` `/api/ws/echo` | Present in ELF |
| `POST /api/replace` | Raspberry STOS binary replace |

**STPP:** UDP discovery on 42000; one M7 can remote-control another
(“Allow remote control”). Android companion:
`Movian_Remote_v2.3.1.apk`.

Not a Phase 0–4 PS5 goal. Nice later for debugging.

---

## 11. Settings map (for the PS5 settings page)

From the live Settings list + `bg_BG.lang` (complete English `id:` set):

**General** — system name, upgrade channel, auto upgrade, launch plugin
on start, alternate plugin repo, auto-install plugins, clear cache,
permissions (Android), FTP server enable/port, debug log.

**Video** — HW decode, VDPAU (Linux), tunnel mode (Android), buffer size,
pre-buffer, HLS live mode, HLS resolution limit, deinterlace, upscale,
adaptive format, resume, auto-next, seek step, AFR / match refresh,
clock during playback, up/down key meaning, playback speed.

**Audio** — output port, PCM vs passthrough, AC3/EAC3/DTS/TrueHD
passthrough, transcode to DD/DD+/DTS, stereo upmix, downmix profile,
delay, gain, IEC61937/SPDIF, AudioTrack options (Android), speaker test.

**Subtitles** — languages, source priority, central folder, styling,
SDH, bounding box.

**Languages** — UI lang, keyboard (OSK / hardware / Android IME), clock
12/24, clock offset, charset, sort ignore “The”.

**Look and feel** — skin, UI size, colors, background image, home-screen
icon visibility (Search, USB, Settings, Plugins, Local network, M7 Data),
screensaver delay/source, loading indicator, landscape lock, animation
speed, [BACK] behaviour.

**Bookmarks**, **Metadata** (TMDB/TVDB language, original titles),
**Network** (proxy SOCKS/HTTP, SMB large buffer / xattr, DLNA probe,
IPv6), **BitTorrent** (see §8), **Search**, **Developer/Advanced**,
**About**, **M7 upgrade**.

PS5 should implement a **short** subset first: language, UI scale,
resume on/off, hardware decode on/off, plugin repo URL, debug log.

---

## 12. Android-only (do not silently inherit)

REA on `com.deanbg.movian` 7.0.288:

- Activity `GLWActivity` + DRM `PlayerActivity`
- Leanback launcher + touch optional, **OpenGL ES 3.0 required**
- Foreground `mediaPlayback` service, PiP, Cast app id `1DD3C452`
- URI schemes `m7://`, `drm7://`, `magnet:`, file/content/http(s)
- DRM7 in-process: Widevine HLS + DASH (clear and protected)
- WebKit integration (7.0.288) — brand new, not in 7.0.273 WSL
- Remote APK, voice/IME, notification card

Chrome/NaCl-only: in-process AVC/VP8/VP9 HW via Pepper; Chrome 113 cap.

---

## 13. What “Movian on PS5” should feel like

Keep DualSense **places** as shortcuts, but the **mental model** is M7:

1. **Home** — service tiles + search (not four empty stubs).
2. Open a folder, share, plugin, or pasted URL.
3. Play with a big OSD, resume, audio/sub cycle.
4. Optional: magnet/torrent as a service, not a separate app.
5. Plugins from a zip or repo, each adding a Home tile.

Controller mapping in [PRODUCT.md](PRODUCT.md) stays; refine against this
UI (Menu = sidebar, Options = settings, Search = OSK).

---

## 14. Implementation checklist vs `app/`

Legend: **now** = Phase 0 shell · **P1–P6** = [PORTING_PLAN.md](PORTING_PLAN.md)
· **no** = explicit non-goal.

| Capability | M7 | PS5 plan |
| --- | --- | --- |
| Home tiles + search bar | yes (live) | P1 browse first, then Home lists real sources |
| Local folders | yes | **P1** `/download0`, `/data/movian`, USB best-effort |
| Software video + stereo | FFmpeg 4.4 | **P2** small FFmpeg, H.264/MPEG-1/AAC |
| HW video | VDPAU / MediaCodec / … | **P2b** `libSceVideodec2` |
| OSD pause/seek/resume | yes | P2 |
| SRT subs | yes | P6 (SRT first) |
| BitTorrent + magnet | yes | **P3** |
| Plugin API v2 + Duktape | yes | **P4** |
| HTTP/HLS | yes | P5 (after player) |
| SMB2 | yes | **P5** (copy VLC libsmb2 flags) |
| UPnP/DLNA browse | yes | P5 late |
| FTP client | yes | optional P5 |
| HTTP Control :42000 | yes | debug-only, not required |
| STPP / companion remote | yes | no |
| DRM / Widevine / Cast / PiP | Android | **no** |
| DVD ISO menus | yes | no until player is solid |
| Paid bgTV / F1 / pluginsBG | repo | **no** (host API only) |
| WebKit / CEF popup | Android 7.0.288 / movian-go | **no** |
| Screensaver Bing | yes | polish |

---

## 15. How to run the reference on this workstation

```bash
# already installed 11 Nov 2026
wsl -d Ubuntu-24.04 -- showtime
# or Windows Start → “Showtime (Ubuntu)”
# F11 fullscreen. Search bar: /mnt/c/... to install a plugin zip.
# HTTP control: http://127.0.0.1:42000/
```

Disable `systemd-timesyncd` if audio glitches (done). Data lives in
`\\wsl$\Ubuntu-24.04\home\brinza\.hts\showtime`.
