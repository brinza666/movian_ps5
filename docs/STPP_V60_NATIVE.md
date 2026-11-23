# v60 native Movian viewer acceptance

2026-11-23. The installed WSL Ubuntu 24.04 **M7 7.0.273** rendered and controlled
both the standalone STPP fixture and the actual shared UI adapter. These are host
interoperability results; PS5 console acceptance is not established.

## Viewer identity and method

The running viewer's HTTP root identified `movian - M7 7.0.273`. A fresh hash of
`/usr/bin/showtime` was
`da133c4a6ee509da8f3b74dd2dfd1b445988381c5b3210d57b15a2e3a96ac9b1`,
matching the retained shipped ELF. Its GNU build ID is
`cecc8ab5765c085d19ffdc4e5c86649186a62cfa`. The matching REA artifact identity is
`ev_fe0354cbabe13437c613d0d6015144a9c396c1a9f632e83c38b0e60473dee520`
with inventory
`ev_41974b1968962d5b5e8bc0f5c8b7550fe8e868e159111651ada9d6276bacc97a`.
See [artifact findings](STPP_ARTIFACT_FINDINGS.md).

The viewer was opened through its own `/api/open?url=stpp://127.0.0.1:42010`
route, then operated through the supported Computer Use skill. A transparent
TCP proxy retained the exact native HTTP/WebSocket bytes. Local HTTP properties
also confirmed the viewer page type `stpp` and remote URL. The viewer HTTP port
was 42000 after relaunch; it must be rediscovered rather than assumed from the
earlier 44456 session. No old andoma source was used.

## Standalone fixture

`tests/stpp_viewer.cpp` publishes Home, a Settings service, navigator/currentpage
aliases, and a General page with a bound integer boolean. Actual native actions
produced:

- Remote banner and Home Settings tile rendered.
- Clicking the tile sent OpenURL `settings:general` and rendered General.
- Clicking Show filename extensions changed the fixture 1 → 0 and the viewer
  to Off; another click restored 0 → 1 and On.
- Native Back reached the fixture callback and returned the viewer to Home.

Frozen session 1 contains 121 native subscriptions, 194 client frames,
138 server frames, and four control commands. These counts describe the retained
sample, not a claim of protocol completeness.

## Shared UI adapter

The host acceptance harness uses actual `ui.cpp`, `ui_stpp.inc`, settings and
their callbacks, with inert playback/input. The installed native viewer:

- Rendered Home and all eight service tiles.
- Navigated by clicking Settings, then General.
- Changed Show filename extensions 1 → 0: the disposable profile persisted
  `show_extensions=0` and the viewer displayed Off.
- Restored 0 → 1: the profile persisted `show_extensions=1` and the viewer
  displayed On.
- Navigated Back from General to Settings, then Home.

Frozen session 2 contains 603 native subscriptions, 1,134 client frames,
843 server frames and six commands: the two OpenURLs, two toggles and two Backs.
Playback, audio, remote video pixels, other setting types and console behavior
were not part of this acceptance.

## Native boolean command recovered

The shipped installed `items/list/bool.view` calls `toggle($self.value)`.
The fixture capture recorded the exact binary payload:

```text
03 04 00 00 00 05 76 61 6c 75 65 00 0d
```

That is opcode 3, exported property ref 4, path `value`, path terminator,
**value-operation tag 13**, with no remaining payload. It is not an absolute
integer set. The server now queues `Command::Kind::ToggleInteger`; the UI thread
resolves the latest value, preserving two toggles queued before publication.
The focused TCP/WebSocket regression passed, including the two-command case.
The native OpenURL also carried mask 5 (URL plus service-property address), which
the server accepted while routing the URL to the UI callback.

## Evidence and remaining check

Private evidence is under ignored `build/v60-stpp-native/`:
`acceptance-summary-private.json`, `viewer-version-final-private.txt`, frozen
wire bytes with SHA-256 hashes, per-frame JSON, fixture callback/state logs,
the two persisted profile snapshots, and `native-*.png`/`shared-ui-*.png` images.
The persistent fixture remains available on port 42012. The recording proxy on
42010 routes to the shared UI host listener on 42011 when that listener is active.

The earlier real-peer REA native connection Evidence
`ev_e1dce4dc2c4e1e7a57f69bd7fe4d70a846ed2fd643b47a6eca7a6c865aa6276b`
and controlled-action Evidence
`ev_c3ce5db4623bf60ac9167cb842605e30c93dae9dc7f42591c391b54ff79e50bf`
remain the packet-level protocol reference, as recorded in
[remote-control evidence](REMOTE_CONTROL_STPP.md). They do not attest to the
new host implementation. Loopback/any tcpdump attempts for this host run captured
zero matching packets; this report claims application-layer proxy and UI
evidence, not a new REA packet observation. REA's Windows Ghidra ELF limitation
still applies to the static analysis route.

**Final-core repeat passed:** after replacing collection/threading internals
for the PS5 runtime, the same unmodified WSL viewer connected again and rendered
Home → Settings → General. Native clicks changed extensions On→Off→On; each
state agreed with the actual saved settings file. Back returned through Settings
to Home. Final sanitizer/socket and shared-UI acceptance tests also passed.
Private repeat evidence is under `build/v60-stpp-native/finalcore-*` and the
app-layer recording proxy. Final signed-package and PS5 tests remain separate.
