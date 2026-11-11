# Using Movian (Phase 1 browse)

The television UI has four places, switched with **L1 / R1**:

| Place | Now | Later |
| --- | --- | --- |
| Home | Tiles that jump into places | Live services and continue watching |
| Videos | Lists `/download0`, `/data/movian`, `/mnt/usb0` | Playback (Phase 2) |
| BitTorrent | Placeholder | Magnets and play-while-download |
| Plugins | Placeholder | API v2 packages |

Copy a test clip into `/download0` over FTP **while Movian is closed**.
USB often stays invisible until a sandbox helper exists — Videos then
shows a “cannot open path” row instead of crashing.

## Controller

| Button | Action |
| --- | --- |
| L1 / R1 | Previous / next place |
| D-pad | Move Home tiles / Videos rows |
| Cross | Open a folder, or select a file (no decoder yet) |
| Circle | Back one folder, then Home |
| Triangle | Reload the current folder |
| L2 / R2 | Page the Videos list |
| Options | System toast with build status |

This mapping will grow a player OSD in Phase 2 without breaking library
navigation. See [PRODUCT.md](PRODUCT.md) for the target table.

## Settings

Not implemented. Planned file: `/download0/settings.txt`.
