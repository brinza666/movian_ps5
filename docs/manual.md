# Using Movian (Phase 2 MPEG-1 + options)

The television UI has four places, switched with **L1 / R1**:

| Place | Now | Later |
| --- | --- | --- |
| Home | Continue watching, Videos, Bookmarks, Settings | Live services and search OSK |
| Videos | Lists folders; Cross plays MPEG-1 `.mpg` | H.264 / FFmpeg |
| BitTorrent | Placeholder | Magnets and play-while-download |
| Plugins | Placeholder | API v2 packages |

Copy a **MPEG-1** `.mpg` into `/download0` over FTP **while Movian is closed**.
USB often stays invisible until a sandbox helper exists — Videos then
shows a “cannot open path” row instead of crashing. MP4/H.264 files list
but do not play yet.

## Controller

| Button | Library | Player |
| --- | --- | --- |
| L1 / R1 | Previous / next place | previous / next MPEG-1 in the folder |
| D-pad | Move Home tiles / Videos rows | seek / OSD row |
| Cross | Open a folder, play MPEG-1, or toggle a setting | pause |
| Circle | Back one folder, then Home; close a menu | stop |
| Square | Item options (play, from start, bookmark, path) | bookmark |
| Triangle | Reload the current folder | unused |
| L2 / R2 | Page the Videos list | seek 60s |
| Options | Settings | OSD menu |

## Settings

**Options** opens a DualSense list stored in `/download0/settings.txt`:

- Resume playback
- Play next when a file ends
- Repeat: off / one / folder
- Shuffle folder
- Seek step (5 / 10 / 30 / 60 s)
- Volume (50 / 75 / 100 %)
- 24-hour clock
- Debug log (`/download0/movian-ps5.log`)
- Hardware decode (stored for Phase 2b; this binary stays software MPEG-1)
- Show all files
- Newest files first
- Plugin repository URL (cycled, **not fetched** in this build)

Continue watching is `/download0/resume.txt`. Bookmarks are
`/download0/bookmarks.txt` (Square on a row, or Home → Bookmarks).
