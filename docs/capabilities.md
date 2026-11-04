# What M7 does, and what this port does

M7 7.0.273 from [apps.movian.eu](https://apps.movian.eu/) is the player this port follows. Its BitTorrent client is `src/backend/bittorrent`. movian-go ports that package. TorrServer is a different program and is not used here.

| Capability | M7 | This port |
| --- | --- | --- |
| Local video and audio | FFmpeg, most files | The browser preview plays what the browser can. The home-screen title plays MPEG-1, H.264, HEVC, MPEG-4, MPEG-2, MJPEG, WMV, VC-1 and VP8, with AAC, AC-3, E-AC-3, MP3, DTS, TrueHD, Opus, FLAC and PCM. Containers are MP4, MKV, MPEG-TS, AVI, WMV, ASF, FLV and MPEG. 4K is scaled down in software |
| Play while a torrent downloads | Built-in client. A magnet becomes `torrentfile://` plus the info hash | C client in the home-screen title. BitTorrent starts Sintel, or a `.torrent` file from USB or data. HTTP and UDP trackers. Plaintext peers, no DHT. The browser preview uses WebTorrent and is not this client |
| Subtitles (srt, ass, sub, vobsub) | Yes, plus OpenSubtitles | Browser subtitle tracks only, when the file has them |
| Audio and subtitle track choice | Yes | Browser text tracks. Audio track count is shown, not switched |
| Resume, seek, playback speed | Yes | Seek and pause. Options sets 0.5x, 1x, 1.5x, or 2x. Repository → Playback also stores 0.75x and 1.25x, and the seek step |
| HLS, including live | Yes | A URL the browser can play |
| MPEG-DASH and Widevine (DRM7) | Android and desktop builds | No |
| SMB, NFS, FTP, SFTP, WebDAV | Yes | The title reads etaHEN's FTP on 127.0.0.1 when the sandbox hides USB. It is not a general FTP, SMB, NFS or WebDAV client |
| UPnP / DLNA | Yes | No |
| Tvheadend | Yes | No |
| Icecast / SHOUTcast | Yes | An HTTP audio URL in the browser |
| Photos | Yes | No |
| DVD and disc images | Yes | No |
| AC3 / DTS passthrough | PS3 | Mixed to stereo |
| Plugins and another repository | Yes | Installed packages run in the player (Duktape, API v2). Options → Repository is Settings → General: alternate repository URL, beta passwords, automatic upgrade, install on demand, Browse, and Upgrade. A saved empty URL uses `https://repo.movian.eu/plugins-v1.json`. With no settings file yet the home-screen title starts at `https://bit.ly/mprepo`. L2 opens packages already on disk. A popup is accepted. Stream URLs are shown, not decoded |
| Unified search | Yes | Names on screen, plus an `http` or `magnet` line |
| Per-file settings, on-screen keyboard | Yes | On a plugin page, Options lists every Movian setting type (bool, string, integer, multi-choice, action, divider, info). Square opens an item's options. Strings use the pad keyboard, including shift. The repository URL and beta passwords use a keyboard too |
| Web remote | Yes | No |
| Google Cast | Android M7 | No |
| FTP server, NTFS, temperature | PS3 package | No |

The home-screen folder is `PPSA99455`. Install: [install.md](install.md).
