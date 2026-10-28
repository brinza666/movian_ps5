# What M7 does, and what this tree does

M7 7.0.273 from [apps.movian.eu](https://apps.movian.eu/) is the player. Its BitTorrent client is `src/backend/bittorrent`. movian-go already ports that package and `wire_core.go` constructs it with `bittorrent.NewBtGlobal`. TorrServer is a different program. It is not used here.

| Capability | M7 | This tree |
| --- | --- | --- |
| Local video and audio | FFmpeg, most files | Preview plays what the browser can. The ELF plays MPEG-1, H.264, HEVC and the audio list in `ps5-splicer/README.md` |
| Play while a torrent downloads | Built-in client. A magnet becomes `torrentfile://` plus the info hash | C client in `payload.elf` (movian-go is not this file). A magnet, a `.torrent` or a plugin magnet plays the largest video while pieces arrive. HTTP and UDP trackers. Plaintext peers, no DHT |
| Subtitles (srt, ass, sub, vobsub) | Yes, plus OpenSubtitles | Browser subtitle tracks only, when the file has them |
| Audio and subtitle track choice | Yes | Browser text tracks. Audio track count is shown, not switched |
| Resume, seek, playback speed | Yes | Seek and pause. Options menu sets speed to 0.5x, 1x, 1.5x or 2x |
| HLS, including live | Yes | A URL the browser can play |
| MPEG-DASH and Widevine (DRM7) | Android and desktop builds | No |
| SMB, NFS, FTP, SFTP, WebDAV | Yes | No |
| UPnP / DLNA | Yes | No |
| Tvheadend | Yes | No |
| Icecast / SHOUTcast | Yes | An HTTP audio URL |
| Photos | Yes | No |
| DVD and disc images | Yes | No |
| AC3 / DTS passthrough | PS3 | ELF mixes to stereo |
| Plugins and another repository | Yes | Packages in `/mnt/usb0/plugins` run in the payload (Duktape, API v2). L2 opens them. Pages use sceHttp2. A popup is accepted. Stream URLs are shown, not decoded |
| Unified search | Yes | Names on screen, plus an `http` or `magnet` line |
| Per-file settings, on-screen keyboard | Yes | Options opens every page and plugin setting (bool, string, integer, multi-choice, action, divider, info). Square opens an item's options. Strings use the pad keyboard, including shift |
| Web remote | Yes | No |
| Google Cast | Android M7 | No |
| FTP server, NTFS, temperature | PS3 package | No |

The ELF was not run on a console here.
