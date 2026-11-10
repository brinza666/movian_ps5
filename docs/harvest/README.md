# Official M7 harvest

Local extracts from the closed-source M7 packages, used to write
[M7_CAPABILITIES.md](../M7_CAPABILITIES.md). **Not** the PS5 port.

Re-install / re-download (this workstation, 11 Nov 2026):

```bash
wsl -d Ubuntu-24.04 -- bash -lc 'showtime --help'
# package: /tmp/movianM7-7.0.273-wsl2-amd64.deb
# https://movian.eu/Linux/movianM7-7.0.273-wsl2-amd64.deb
# Android: https://movian.eu/Android/movianM7-7.0.288-android.apk
```

Git **does not** store `.apk` / `.elf` / `.deb` or the 700 KB
`strings/all.txt`. Skin JS, URI lists, HTTP dumps, and screenshots that
fit are committed under `m7-7.0.273-wsl2/`.
