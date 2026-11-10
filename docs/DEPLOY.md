# Deploy and debug

Console: **192.168.10.238** (this house). Override with `PS5_HOST`.

## Prerequisites on the PS5

Relapse (or your host) → kstuff → etaHEN → ShadowMountPlus → ftpsrv.

| Service | Port | Use |
| --- | --- | --- |
| ftpsrv | 2121 | upload the title folder |
| klogsrv | 3232 | live kernel / app stderr |
| ShadowMountPlus API | 10101 | scan / list / mount |
| Payload Manager | 8084 | payloads only |
| elfldr | 9021 | optional, **not** for `eboot.bin` |

## Upload

```bash
bash app/build.sh deploy 192.168.10.238
```

Or from Windows:

```powershell
C:\Users\brinza\Documents\deep\tools\ps5-ftp.ps1 -Action put `
  -Local C:\Users\brinza\Documents\deep\movian_ps5\build\app\PPSA99455\eboot.bin `
  -Remote /data/homebrew/PPSA99455/eboot.bin
```

The kit deploy uploads under a temp name and publishes `eboot.bin` last.
**Close Movian** before replacing files. ShadowMountPlus scan interval is
about 90 seconds (`stability_wait_seconds=20`). Force a scan:

```bash
curl -sS -X POST http://192.168.10.238:10101/api/v1/scan
curl -sS -X POST http://192.168.10.238:10101/api/v1/games
```

There is no public “launch” RPC. `POST /api/v1/games/mount` with
`{"title_id":"PPSA99455"}` mounts the title; on this console that has
also triggered `launchApp(PPSA99455)`.

## klog

```powershell
# existing monitor writes C:\Users\brinza\Documents\deep\ps5-logs\
```

Success looks like `createApp PPSA99455` then `EXEC /app0/eboot.bin`.
A SIGSEGV immediately after a payload toast is the **SDK CRT `_start`**
bug on `hello.elf`, not a title bug. Titles must **not return from
`main`** — `screen::run` loops forever so the shell can close the app.

## Logs inside the title

`/download0/movian-ps5.log` (FTP get after a run).

## Do not

- Send `eboot.bin` or `payload.elf` to 9021 interchangeably.
- Put videos inside `PPSA99455/` (that folder is the app image).
- Relaunch an `.ffpfsc` of the same path without restarting
  ShadowMountPlus (stale mount). Phase 0 ships a **folder**, not ffpfsc.
