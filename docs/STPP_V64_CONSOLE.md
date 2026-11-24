# v64 console deployment and WSL STPP test

24 November 2026. The published v64 ZIP is unchanged: version `01.000.064`,
SHA-256 `422c96f03f906c9686f7e475770fc5c169848dd7cfd04887948c9ab3d82f6bae`.

## Observed results

| Check | Result |
| --- | --- |
| FTP 2121 | Connected to ftpsrv 0.19. |
| Profile backup | Closed 336,789,504-byte image copied to PC and independently read twice; SHA-256 `629236b525da72ac11dae020d1ead68b3d06b8d6685097ab7d63cba747f37f46`. |
| Upload | All 29 packaged files uploaded with execute permissions; a final read with FTP SELF decryption disabled matched the exact signed ZIP bytes. |
| Activation/mount | Previous folder moved aside, v64 promoted to `/data/homebrew/PPSA99456`; ShadowMount returned status0, mounted=true. |
| Launch | Existing Minion controller accepted launch; a fresh process list identified the test title. Subsequent checks confirmed it remained running. |
| Startup | Console diagnostics recorded v64 main, fonts, pad and UI initialization. No physical screen or hardware-playback result is inferred from this checkpoint. |
| STPP 42000 | Failed to bind with errno13. Saved `remote_control=1` and `remote_diagnostics=1` were already enabled. |
| Native WSL viewer | Installed M7 7.0.273 opened the actual PS5 STPP URL; its debug log reported “Connection reset by peer.” No handshake, remote navigation or setting mutation passed. |
| End state | v64 remains running on PS5. WSL was returned to its local Home through its application API. |

## Controller issues resolved during deployment

The running ShadowMount was **1.7beta2**, rather than the previously observed
1.7beta12. It ignored the newer sectioned `bind_address` key and logged an API
bind to localhost. After a verified PC backup, the compatible flat
`api_bind_address=0.0.0.0` alias was added alongside the existing intended LAN
setting. Hot reload restored the API. The matching
[1.7beta2 documentation](https://github.com/drakmor/ShadowMountPlus/blob/1.7beta2/README.md#runtime-config-datashadowmountconfigini)
documents that key.

This version refuses mount changes while any title is active. Its initial
Device busy result coincided with Prospero Multi Tools. That utility ceased to
be active before the guarded close attempt; **no close request was sent to it**.
The subsequent test-title unmount/activation/mount succeeded.

The existing PS5Upload 12.6.23 controller was not running. Its already-installed
ELF matched the official release digest
`68a4c2135477aa0d2a9f396565c50cac7d672bbc0f158e718986616a14c6b71b`
and was sent directly to the existing ELF loader, restoring Minion9180. No title
eboot was sent to that loader. The host wrapper initially treated the launch's
empty JSON ACK as rejection, although launch succeeded. It now accepts that ACK
and still requires a process transition. Seven host regressions pass; the
[pinned native handler](https://github.com/phantomptr/ps5upload/blob/c5244705b2c62917179fe57e384328c908cd1573/payload/src/webui_api.c#L1468)
returns HTTP200 with `{}`.

## Why STPP remains blocked

The console already contained `stpp-title-access-test-v62.consumed`. v64 retained
it, skipped the one-shot access experiment and logged `enabled=0`, `path=disabled`.
No fresh-reset request was uploaded, and no new profile wipe occurred.

The recovered earlier v62 log records an owned-helper status0 and directory-read
proof, followed by failure to resolve loaded chroot/fchdir. Its last recorded
stage is root restoration; it has no successful original-root/title-path proof.
That is an incomplete earlier experiment, not a successful v64 credential test.
The consumed marker was not removed to repeat that unchanged path.

Next acceptance is to correct and test the title-root restoration path in a new
build, then explicitly retry the owned access experiment and verify the listener.
Only after an actual listener/handshake succeeds should WSL navigation and a
persisted setting change/restoration be counted as passed.

## Evidence and backups

Private evidence is under `build/v64-console/`: `activation.json`,
`active-signed-readback.json`, `console-result-private.json`, `live/`,
`wsl-native-stpp-private.log`, controller/config snapshots and backup proofs.
No private raw logs or tokens are published.

The inactive v61 and v62 folder backups were copied and independently verified
under `D:/GPTDEEP/PS5-Movian-Backups/2026-11-23`, then removed from the console:
31 files / 15,231,748 bytes and 31 files / 15,229,060 bytes respectively. The FTP
server reports successful DELE as226; after its first such response interrupted
the archival script, all original PC files and remaining remote files were
rechecked before cleanup resumed. The manifest records that recovery. Signed
packages and the current closed-profile backup are retained on PC.
