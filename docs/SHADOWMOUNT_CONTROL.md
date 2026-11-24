# Remote control of the Movian test title

Observed on 2026-11-24. Keep `PPSA99456` separate from production titles. The user authorized autonomous opening and closing of this test title while away. This document distinguishes filesystem mounting, process lifecycle, and Movian UI control.

## ShadowMountPlus mounts the title; it does not launch it

The installed ShadowMountPlus API reports `1.7beta12`. Its capability list includes `mount_game`, `unmount_game`, `read_debug_log`, and `read_kernel_log`; it has no launch or terminate capability. The served web interface's `appendRuntimeActions` function offers mount/unmount for image-backed games and returns without adding these buttons for folder titles. `titleRuntime` sends only `/api/v1/games/mount` or `/api/v1/games/unmount`.

The official [API documentation](https://github.com/drakmor/ShadowMountPlus/blob/0b56a1c25e8f07584135fd537ce49736562d6b07/docs/api.md) and [router](https://github.com/drakmor/ShadowMountPlus/blob/0b56a1c25e8f07584135fd537ce49736562d6b07/src/sm_api_service.c) expose no title launch/close route. Unknown routes return 404. The [runtime service](https://github.com/drakmor/ShadowMountPlus/blob/0b56a1c25e8f07584135fd537ce49736562d6b07/src/sm_shellcore_service.c#L611) prepares or releases filesystem runtime state; unmount returns `EBUSY` when the title or its source is protected. It does not terminate a running process.

The internal ShellCore protocol's `LAUNCH` event records a launch that ShellCore is already processing. `sm_shellcore_remote` implements process-memory access for that integration. Neither is an external network command to start a game.

Therefore:

- A successful mount is not evidence that Movian has started.
- A successful unmount is not evidence that it closed a running Movian process.
- The presence of a download/profile mount alone does not prove a live title.
- Use process-list evidence and bounded kernel-log lifecycle events to establish whether the test title is running. Do not invent `/launch`, `/terminate`, or `/status` ShadowMount routes.

## An existing PS5Upload Minion service provides lifecycle control

Read-only inspection found a live PS5Upload Minion service on HTTP port `9180`. Its token response reports version `12.6.23`, matching the console's existing helper startup record. The served JavaScript calls the same endpoints as the matching [v12.6.23 API implementation](https://github.com/phantomptr/ps5upload/blob/c5244705b2c62917179fe57e384328c908cd1573/payload/src/webui_api.c).

Every API route below uses `POST` with a JSON body, except token acquisition. Obtain a fresh token in memory using `GET /api/token?page=dev`, then send `Content-Type: application/json` and `X-PS5U-Token: <token>`. Do not print, commit, or persist the token. A transient 503 occurred on the first token request; a later bounded retry succeeded. A rejected request is not evidence that the title changed state.

| Purpose | Route | Body | Interpretation |
| --- | --- | --- | --- |
| Read helper/foreground status | `/api/status` | `{}` | Includes version and `running`; `running: null` alone is insufficient process evidence. |
| Read actual processes | `/api/proc/list` | `{}` | `ok`, `procs`, including `title_id`, `app_id`, `pid`, and `kind`. Select the exact test title. |
| Launch Movian test title | `/api/apps/launch` | `{"title_id":"PPSA99456"}` | Refuses with HTTP 409 `other_game_running` when another game exists. Success acknowledges the request; poll processes/logs for actual startup. |
| Close confirmed Movian test instance | `/api/app/close` | `{"app_id":<confirmed target app_id>}` | Use only the app ID freshly associated with `PPSA99456`. Poll until that instance disappears. |

The source's launch handler checks the process list before requesting launch and treats an unreadable list as running, rather than guessing. Its close route uses the helper's title lifecycle operation, not arbitrary PID killing. See [launch guard](https://github.com/phantomptr/ps5upload/blob/c5244705b2c62917179fe57e384328c908cd1573/payload/src/webui_api.c#L1460), [close handler](https://github.com/phantomptr/ps5upload/blob/c5244705b2c62917179fe57e384328c908cd1573/payload/src/webui_api.c#L1113), and [Host/Origin/token checks](https://github.com/phantomptr/ps5upload/blob/c5244705b2c62917179fe57e384328c908cd1573/payload/src/webui_auth.c).

The first successful read-only process capture returned 84 processes, no `PPSA`/`CUSA` application, and no `PPSA99456`. It identified an existing `elfldr.elf`, Payload Manager, ShadowMountPlus, and PS5Upload. These observations establish the available service and then-current process state; they do not claim a successful Movian launch or STPP connection.

## Existing ELF loader integration

Minion also exposes `POST /api/payload/load` with `{"path":"/path/to/payload.elf"}`. The [implementation](https://github.com/phantomptr/ps5upload/blob/c5244705b2c62917179fe57e384328c908cd1573/payload/src/webui_pkg.c#L748) checks the filename, ELF magic, size, helper-image exclusion, and loader mutex, then streams to `127.0.0.1:9021`. It sends immediately and half-closes to signal EOF. It does not connect silently to probe the loader, because a silent connection can occupy the one-image loader.

No pure loader-status/start endpoint was found in the Minion route table. An `elfldr.elf` process exists even though the external loader port was reported refused; process presence does not establish a working loopback listener. The existing title-access helper may use the loopback loader, but its execution and result must be separately verified. Do not send a title's signed `eboot.bin` to an ELF loader.

## Other observed candidates

Known websrv HTTP `8080`, shsrv TCP `2323`, PS5 Suite TCP `9113`/web `17382`, and VoidShell HTTP `7007` refused connections during this investigation. These were specific known services derived from installed metadata or primary documentation, not a LAN scan. Old suite boot records are not evidence that its server remains live.

The official [websrv README](https://github.com/ps5-payload-dev/websrv) documents `/elfldr` for SDK ELF payloads and a homebrew launcher expecting `eboot.elf`. It does not establish that a signed ShadowMount folder title can be launched directly by that route.

PS5 Suite source contains a launch command, but its failed-launch fallback can close another foreground game. The reachable Minion route's explicit refusal is preferable for this test-title workflow. No Suite commands or new payloads were sent during this investigation.

## Safe deployment and verification sequence

The host CLI `scripts/ps5-title-control.py` implements the observed Minion routes:

```powershell
python scripts/ps5-title-control.py status --host CURRENT_PS5_IP
python scripts/ps5-title-control.py launch --host CURRENT_PS5_IP
python scripts/ps5-title-control.py close --host CURRENT_PS5_IP
```

It requires an explicit verified private IPv4 address and controls only PPSA99456.
Tokens remain in memory. It rejects malformed/ambiguous process state, refuses
launch when another game runs, rechecks the test app ID before closing, and
requires a process transition after acknowledgment. Six host tests cover these
guards, including a changed instance and an acknowledgment without a transition.
They do not prove console launch/close. The first real read-only status attempt
was refused by port9180; no launch or close request was sent.

1. Acquire Minion token, read processes, and identify the exact test title. If another title is running, leave it untouched.
2. If needed, close only the freshly identified `PPSA99456` app ID; verify its process disappears and capture lifecycle logs.
3. Use the existing verified backup/deployment procedure. Mount the test title through ShadowMountPlus and verify the intended build files/hashes.
4. Request Minion launch for `PPSA99456`. Check actual processes and startup logs. A successful HTTP response is insufficient.
5. After Home and STPP listener readiness are established, use WSL Movian's shared property/UI protocol to inspect and drive Movian. Lifecycle control and STPP control are separate acceptance tests.
6. Record the actual result, including launch errors, title-access helper result, listener bind error, and whether WSL received properties and completed a UI action.

## Private evidence

Device addresses, console process lists, and service logs stay under ignored `build/`:

- `build/v62-console/shadowmount-current.html`, version/API and bounded kernel/debug log captures.
- `build/shadowmount-control/service-inventory-private.json`, specific-service passive reachability and installed payload names.
- `build/shadowmount-control/service-9180-private.html`, decompressed served Minion page; `minion-ui/inline.js`, its extracted inline script.
- `build/shadowmount-control/minion-readiness-private.json`, token presence/version and read-only status capture; tokens excluded.
- `build/shadowmount-control/list-private.json`, actual process-list response.
- `build/shadowmount-control/minion-ui-rea.json`, REA static JavaScript report when complete; parsing a UI artifact does not prove its handlers ran.

This investigation performed read-only queries only. The parent deployment/test task owns any later lifecycle requests and must record their results separately.
