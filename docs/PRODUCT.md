# Product reference (what Movian is)

This port should *feel* like Movian, not like a generic file player.

Primary sources:

- PS3 M7 7.0.273 — [apps.movian.eu](https://apps.movian.eu/)
- [czz/movian-go USER_GUIDE.md](https://github.com/czz/movian-go/blob/master/USER_GUIDE.md)
- [czz/movian-go PLUGINS.md](https://github.com/czz/movian-go/blob/master/PLUGINS.md)
- Flat skin: [andoma/movian](https://github.com/andoma/movian)

## Four places

The home-screen title uses four tabs (L1/R1), matching the old manuals
and the M7 home:

1. **Home** — services (plugins, sources, continue watching).
2. **Videos** — folders: USB, `/data/movian`, `/download0`, later SMB.
3. **BitTorrent** — magnets, `.torrent`, play-while-download.
4. **Plugins** — repository (`https://repo.movian.eu/plugins-v1.json` or
   the alternate feed `https://bit.ly/mprepo` → `plugins/repo.json`).

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
