# USB, data, and where files go

The title image (`/data/homebrew/PPSA99455`) is **not** the media
library. Putting videos there mixes the app with user data and makes
updates dangerous.

| Path | Who writes it | Purpose |
| --- | --- | --- |
| `/app0` | build | Read-only title (code, fonts, icon) |
| `/download0` | the title | Settings, log, torrent cache |
| `/data/movian` | you (FTP) | Extra media if the sandbox can see `/data` |
| `/mnt/usb0` … `/mnt/usb7` | USB | Films; often **hidden** from a normal title |
| `/user/download/PPSA99455/download0.dat` | system | Host backing for `/download0` — do not open this path from the app |

## Sandbox

ShadowMount native titles run in an application sandbox. VLC for PS5
documents the same limit and ships an optional `Helper.elf` to lift it.
Until we have an equivalent helper:

- Prefer copying a test clip into `/download0` over FTP while Movian is
  **closed**.
- If `open("/mnt/usb0")` fails, Videos shows a “cannot open path”
  row (errno), not a crash. Triangle reloads the current folder.

USB sticks should be exFAT or FAT32.

## etaHEN FTP from inside the title

The archived splicer used `127.0.0.1` FTP as a workaround when USB was
invisible. That is a debug trick, not a product filesystem. Phase 1
should try real paths first.
