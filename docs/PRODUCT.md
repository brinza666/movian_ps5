# Product reference (what Movian is)

This port should *feel* like Movian, not like a generic file player.

The full inventory of **what official M7 actually does**, harvested from
a live WSL2 7.0.273 install plus Android 7.0.288, is
[M7_CAPABILITIES.md](M7_CAPABILITIES.md). Read that before inventing UI
or claiming a feature.

Primary sources:

- Live WSL2 M7 7.0.273 on this PC (`wsl -d Ubuntu-24.04 -- showtime`)
- [apps.movian.eu](https://apps.movian.eu/)
- [czz/movian-go USER_GUIDE.md](https://github.com/czz/movian-go/blob/master/USER_GUIDE.md)
- [czz/movian-go PLUGINS.md](https://github.com/czz/movian-go/blob/master/PLUGINS.md)
- Flat skin: [andoma/movian](https://github.com/andoma/movian)

## Look (observed)

Dark wallpaper, **Home** title, search bar, 10-foot **icon grid** of
services: Plugins, Local network, Settings, user folders. Sidebar (`|||`)
for transport + page settings. Global settings is a vertical list
(General, Video, Audio, Subtitles, Languages, Look and feel, …).
Screenshots: [images/m7-wsl-home.png](images/m7-wsl-home.png),
[images/m7-wsl-live.png](images/m7-wsl-live.png).

DualSense **places** (L1/R1) are shortcuts into that same model, not a
different product:

1. **Home** — service tiles + search (this is the real M7 home).
2. **Videos** — folders: USB, `/data/movian`, `/download0`, later SMB.
3. **BitTorrent** — magnets, `.torrent`, play-while-download (built-in).
4. **Plugins** — `https://repo.movian.eu/plugins-v1.json` and/or
   `https://bit.ly/mprepo` → `plugins/repo.json`.

## Controller (target)

| Button | Library | Player |
| --- | --- | --- |
| D-pad | move | seek / row |
| Cross | open / play | pause |
| Circle | back | back |
| Square | item options | audio/sub cycle |
| Triangle | home / extra | tracks |
| L1 / R1 | switch places | prev/next |
| Options | settings | OSD menu |

This mapping is aligned with Movian’s pad keyboard heritage and with
VLC for PS5’s controller-first UI. Refine against M7 once playback
exists; do not copy VLC’s Library/Player tables blindly if they fight
muscle memory from PS3 Movian.

## Plugins

A plugin is a zip with `plugin.json` (`type: ecmascript`, `apiversion: 2`)
and an entry `plugin.js`:

```js
(function (plugin) {
  plugin.createService("My service", "myplugin:main:", "video", true);
  plugin.addURI("myplugin:main:", function (page) {
    page.type = "directory";
    page.metadata.title = "Root";
    page.appendItem("http://host/file.mp4", "video", { title: "A video" });
    page.loading = false;
  });
})(this);
```

Identity is `<plugin-id>@<origin>`. Local zips install as `@local`.
HTTP Control on desktop (`:42000`) is **not** a Phase 0–4 goal.

## Playback expectations (from M7)

Containers: MP4, MKV, MPEG-TS, AVI, others FFmpeg knows.
Video: H.264, HEVC, MPEG-2/4, VP8, …
Audio: AAC, AC-3, E-AC-3, MP3, DTS, TrueHD, Opus, FLAC, PCM.
Subtitles: SRT first, ASS later.
Resume, speed, audio/subtitle tracks.

Do not advertise a codec in the UI until the player actually opens it.
