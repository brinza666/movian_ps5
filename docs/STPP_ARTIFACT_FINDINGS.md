# STPP shipped-artifact findings

Date: 2026-11-23. Scope: static inspection of the installed WSL M7 7.0.273
executable copy, without old andoma/Movian source or repository lookups. This
report does **not** establish runtime behavior. Wire observations belong in the
separate live-capture report. No app source or running WSL/Android app was changed
by this inspection.

## Artifact identity and analysis boundary

- ELF: `build/wsl-movian-dht-rea/showtime`, 9,974,376 bytes, SHA-256
  `da133c4a6ee509da8f3b74dd2dfd1b445988381c5b3210d57b15a2e3a96ac9b1`.
- REA `inspect_artifact` Evidence
  `ev_fe0354cbabe13437c613d0d6015144a9c396c1a9f632e83c38b0e60473dee520`;
  nested inventory Evidence
  `ev_41974b1968962d5b5e8bc0f5c8b7550fe8e868e159111651ada9d6276bacc97a`.
  Saved as `build/remote-control-rea/artifact/elf-artifact.json`. REA establishes
  root-byte identity only, not the protocol semantics below.
- Optional APK was inventoried only: SHA-256
  `0c0f9db8b56289ad2d96cb2c8ebb7b45b732ef2664d54d461f5c2536cfde3194`,
  REA parent Evidence
  `ev_8901ec08eed75630f2ade1ee19fbc471b890cc4997521f27fa69c5df16e50db8`,
  inventory Evidence
  `ev_c0ec150264231cb1f2c01195b29975c0eeffedfcddf997fc84a9f1d12df947b3`.
  Its 7.0.288 version is not the live Android 7.0.289 build. No APK code semantics
  are claimed here.
- REA 6.8.0 sees existing Windows Ghidra 12.1.4/JDK 27, but refuses this ELF:
  `capability_unavailable / target_format_unsupported`, specifically **Windows
  Ghidra P0 accepts native PE and explicitly selected raw 6502 targets only**.
  Saved error: `artifact/ghidra-inspect.json`. No unchanged retry, registration
  change, tool installation, or unsupported Ghidra bypass was made.
- REA's Windows `inspect_with_objdump` also could not launch configured
  `objdump`, returning `ERR_REA_WINDOWS_NATIVE`; saved `artifact/elf-symbols.json`.
  The following findings are supplemental manual static inferences from the
  exact identity-bound ELF using existing WSL GNU objdump, not REA decompilation
  Evidence. ELF is stripped apart from dynamic symbols. Printed labels such as
  `MD5Init-...`/`atan@plt+...` are nearest-symbol labels and have no semantic value.

Raw data is under `build/remote-control-rea/artifact/`: full `elf-disassembly.txt`,
focused `stpp-server.asm`, `stpp-client.asm`, `stpp-event-sender.asm`,
`stpp-subscribe-and-event.asm`, `stpp-discovery.asm`, `elf-stpp-strings.txt`, and
`identity-and-data.txt`, and `action-name-table.json`. The identity file records GNU tool versions, the input hash,
and raw dispatch-table/string bytes. Addresses below are ELF virtual addresses.

## WebSocket transport and version

Static client HTTP request at `0x749494`, referenced by `0x1b8d61`:

```http
GET /api/stpp HTTP/1.1
Connection: Upgrade
Upgrade: websocket
Sec-WebSocket-Key: 1

```

The shipped native client checks the first response line for `HTTP/1.1 101 `,
then consumes headers through the empty line (`0x1b8e39`). Its request does not
include a Host header or Sec-WebSocket-Version. A compatible PS5 HTTP server
must account for this exact old native-client request as well as modern clients.
WebSocket opcode 2 carries binary STPP; opcode 1 takes a separate JSON path.

After upgrade the native client constructs binary payload `00 03 00`
(`0x1b8ee8`). Server opcode 0 accepts a payload of at least three bytes, responds
with 19 bytes `00 03 <16-byte instance ID> 00`, and marks the connection initialized
(`0x11b072`, builder `0x11a63a`). Other binary commands are ignored until that
state is set (`0x11b093`). The client checks response byte 1 equals 3 and rejects
an instance ID equal to its own (`0x1b9890`). The final zero's meaning is not
recovered; preserve it. This is STPP version 3, distinct from WebSocket version.

## Property addressing and subscriptions

Notation used below:

- `u32le`, `u16le`: unaligned little-endian values, established by direct x86
  loads/stores without byte swaps and corroborating writer helpers.
- `path`: repeated `u8 byte_length + exact name bytes`, ending at `00` or the
  end of the available payload. This is a sequence of names, not a dot-delimited
  string. When an address precedes event/value data, an explicit `00` is needed
  to delimit its path; standalone subscribe paths can run to frame end.
- `address`: `u32le base_property_ref + path`.
- `string`: `u8 length + bytes` for lengths 0..254; `ff + u32le length + bytes`
  for longer strings. These event strings differ from path encoding and from
  notification strings.

The native subscribe writer `0x1ba8b1..0x1baa9c` omits a final path zero and uses
message size `11 + sum(1 + name_length)`. Path decoder `0x11a9cb`, property resolver `0x11aa50`, root-reference resolver
`0x11a101`, string decoder `0x117f9d`, string writer `0x1b88da`, address writer
`0x1b8867` provide the boundaries. Base ref 0 calls the root getter
`0x1b2c5f`; nonzero refs resolve per-connection exported child properties.
The root is consistent with the global property namespace used by the nearby
`global/app/systemname` setup (`0x11a539`); verify actual paths live.

Client command dispatch is a 12-entry relative jump table at `0x724108`, selected
by first-byte opcode minus one at `0x11b09d`.

| Opcode | Remaining binary payload | Static result / confidence |
| --- | --- | --- |
| 0 | `03 00` in native writer | Initialize versioned binary connection; high |
| 1 | `u32le subscription_id, u32le base_ref, u16le flags, path` | Subscribe (`0x11b0be`); high |
| 2 | `u32le subscription_id` | Unsubscribe (`0x11b119`); high |
| 3 | `address, u8 value_type, value_payload` | Set/change property value (`0x11b17e`); high |
| 5 | `address, u8 event_type, event_payload` | Send property event (`0x11b132`); high |
| 6 | `u32le child_ref [, u32le before_ref]` | Move/reorder child; semantic inference from parent/before logic (`0x11b23a`, `0x1b1385`) |
| 7 | `u32le subscription_id` | Invokes subscriber-side continuation/request function (`0x11b27e`); precise semantic name unresolved |
| 8 | `address` | Invokes another property mutation (`0x11b2ad`); precise semantic name unresolved |
| 9 | at least 16 remaining header bytes then data | Image/resource request path, not analyzed fully |
| 12 | `u32le request_id` | Cancels matching resource request (`0x11b370`) |
| 4, 10, 11 | any | Unknown-command logging in this client-to-server direction |

Subscribe flags are forwarded as the low 16 bits to the internal subscriber,
with server-added internal bit `0x80000` (`0x11a24c..0x11a285`). Flag-bit names
and their effect remain unresolved. Start with live-native flags; zero is a
test candidate, not established sufficient for every UI subscription.
The native subscribe writer copies its local subscriber's word at offset +0x6e
directly to wire bytes 9..10 (`0x1ba9ed`).
Subscription IDs and exported property IDs are distinct spaces. Exported refs
must remain valid for later relative subscriptions, events, and mutations.

## Property value mutation

Opcode 3 consumes `address` first, then uses a second table at `0x724138`.

| Value type | Remaining payload | Inferred operation |
| --- | --- | --- |
| 0 | empty | Clear/void (`0x11b20d`) |
| 1 | exactly 4 bytes, signed integer bits | Integer set (`0x11b1ed`) |
| 2 | exactly 4 bytes, IEEE binary32 bits | Float set (`0x11b219`) |
| 3 | `u8 string_kind, bytes to frame end` | String set (`0x11b1d5`) |
| 13 | empty | Additional property operation (`0x11b201`), semantic name unresolved |

Other value tags up to 13 do nothing in this path. String kind is an extra byte,
not a length. Do not reuse the event string length prefix here.

## Server property notifications

The binary subscriber callback starts at `0x1196a7`. All property notifications
have `04 + u8 subtype + u32le subscription_id + subtype_payload`. Common header
and WebSocket send are at `0x11a0b2`. Client decoder `0x1b97c5` handles opcode 4,
reads subtype at byte 1 and subscription ID at bytes 2..5; its subtype jump table
is at `0x749660`.

| Subtype | Payload after six-byte header | Static interpretation |
| --- | --- | --- |
| 0 | empty | Void/clear |
| 1 | 4 integer bytes | Integer value |
| 2 | 4 binary32 bytes | Float value (server narrows internal double) |
| 3 | `u8 string_kind, bytes to frame end` | String value |
| 5 | empty | Directory/container |
| 6 | one or more `u32le child_ref` | Add child vector |
| 7 | `u32le before_ref, u32le child_ref...` | Add vector before existing child |
| 8 | `u32le child_ref` | Delete child |
| 9 | `u32le child_ref [,u32le before_ref]` | Move/reorder child |
| 10 | `u32le child_ref` | Child selection (name inferred from internal event) |
| 11 | `u32le child_ref` | Adds child with internal flag 1; likely selected add |
| 12 | `u32le property_ref` | Replaces special retained value-property ref; semantic name unresolved |
| 14 | empty | Internal notification event 23; semantic name unresolved |
| 15 | empty | Internal notification event 24; semantic name unresolved |

Subtypes 4/13 are not handled by the native client's table. Vector lengths come
from the remaining frame length, not a count field. Child IDs become addressable
property refs. Structural notifications require retaining child order and each
connection's ref mapping, not just exposing a read-only JSON tree.

## Events: navigation, OpenURL and track selection

Opcode 5 is `05 + address + event_type + event_payload`. Server decoder
`0x11ab16` and native sender `0x1ba5be` agree on these branches:

| Event type | Payload | Static interpretation |
| --- | --- | --- |
| 1 | repeated `u8 length + action-name bytes` through frame end | Action vector: names converted to internal actions, then dispatched |
| 14 | `u8 presence_mask`, optional slots below | OpenURL-shaped event |
| 15 | `u8 flags, address track, [address source if flags bit0], [u8 mode]` | Play/select-track-shaped event |
| 18 / 19 | `u8 boolean_flag, string` | Named string events; semantic names unresolved |
| 27 | raw text to frame end | Dynamic/text event; semantic name unresolved |

For event 1, path terminator precedes the event byte; the action-name vector
itself needs no trailing zero in the native sender. Do not transmit internal
numeric action enums. A 76-entry table at `0x969b60` maps exact names to internal
actions. Decoder `0x14b69d` compares through `strcasecmp`, so matching is case
insensitive; writer `0x14b676` looks up canonical spelling. Full extracted table
is `artifact/action-name-table.json`. Relevant canonical names are:

```text
Up Down Left Right Activate Click Enter Submit Ok Cancel Backspace Delete
MoveUp MoveDown MoveLeft MoveRight Forward Back FocusNext FocusPrev PageUp
PageDown Top Bottom Increase Decrease Stop PlayPause Play Pause Eject Record
PreviousTrack NextTrack SeekForward SeekReverse SeekForwardFrame SeekReverseFrame
VolumeUp VolumeDown VolumeMuteToggle Menu ItemMenu LogWindow Select MediaStats
Home Reset Copy Paste ChangeView FullscreenToggle Channel+ Channel- ZoomUI+
ZoomUI- ZoomUIReset ReloadUI Restart Quit Standby PowerOff Reboot Shuffle Repeat
EnableScreenSaver AudioTrack SubtitleTrack ReloadData Playqueue Sysinfo SwitchUI
Hypstart Hypstop Hypwarm Hypcold
```

This establishes statically accepted action names, not which target property
receives them or their resulting UI behavior. For example, a candidate event to
the global event sink is `05 00000000 09 "eventSink" 00 01 04 "Back"`, where
quoted strings mean raw bytes and `00000000` means a four-byte zero ref. The
`eventSink` path itself must be corroborated by the live model before sending.

Event 14 presence slots are serialized in this order, with absent slots omitted:

| Mask bit | Encoding | Structure slot copied by constructor `0x14b802` |
| --- | --- | --- |
| 1 | `string` | event +0x48; URL inferred |
| 2 | `string` | event +0x50; view/type candidate |
| 4 | `address` | event +0x58; model/item property candidate |
| 8 | `address` | event +0x60; parent/source property candidate |
| 16 | `string` | event +0x68; semantic name unresolved |
| 32 | `string` | event +0x70; semantic name unresolved |

The first-string URL interpretation is strong but the optional slot names are
not established from recovered symbols. Wire order and encodings are explicit
in sender `0x1ba6d5..0x1ba78e` and matching decoder `0x11ac41..0x11ad77`.
Event 15 required/optional property refs and trailing mode are explicit in
`0x1ba790..0x1ba7c7` and `0x11ad7c..0x11ae09`.

## JSON WebSocket path

Opcode-1 WebSocket text frames enter separate JSON parsing at `0x11aefc`.
The static handler supports subscribe `[1, subscription_id, base_ref, path]`,
unsubscribe `[2, subscription_id]`, and property value set
`[3, base_ref, path, value]`. The JSON subscriber emits literal templates
`[4,id,value]`, `[5,id,before,[child...]]`, `[6,id,[child...]]`, and
`[7,id,child,before]` in the shipped string region `0x723d53..0x723f29`.
Exact path representation and unsupported JSON cases still require live checks.
Do not assume the binary event operation exists in the JSON path: its dispatch
handles only these three top-level commands.

## Discovery

Static UDP advertisement builder `0x118262` and receiver `0x11a72b` use an
88-byte fixed packet:

| Offset | Size | Content |
| --- | --- | --- |
| 0 | 4 | ASCII `STPP` |
| 4 | 16 | Instance ID bytes |
| 20 | 1 | Version 3 |
| 21 | 1 | Flags: bit0 response/request role; bit1 remote control enabled |
| 22 | 2 | Advertised HTTP listener port, big-endian |
| 24 | 32 | First fixed text field; friendly name inferred |
| 56 | 32 | Second fixed text field; exact name unresolved |

Receiver rejects lengths <=87, wrong magic/version and self ID. Missing bit1
removes the discovered service. IDs form `stpp:id:` plus a hex encoding, while
resolved URLs are formatted `stpp://a.b.c.d:port` (`0x11a89e`, `0x1181dd`).
Source address supplies the IP; the packet supplies the HTTP port.
Discovered entries expire after 60 seconds (`0x11a915`), while one local timer
uses 20 seconds (`0x118437`).

The multicast IP bytes are `ef ff ff fa` = 239.255.255.250. The executable has a
1900 local socket-port constant at `0x1186a8` and a destination structure at
`0x9689b0` containing port 42000 (`04 00 10 a4 ef ff ff fa`). The exact source,
multicast destination and unicast destination port roles must be settled with
live traffic; neither the advertised HTTP port nor the multicast port should be
assumed from a currently configured endpoint.

## Remaining acceptance work

Answered statically: version handshake, subscribe/unsubscribe layout, scalar
set layout, scalar/structural notification layouts, generic action-vector and
optional OpenURL/track event encodings, discovery packet fields.

Partially answered: notification semantic names beyond common scalar/child
operations; subscribe flag semantics; OpenURL optional-field names; which
property receives settings/navigation events; discovery
port roles. Unresolved: runtime masking/fragmentation compatibility, changes
between WSL 7.0.273 and Android 7.0.289, property adapter/UI model required for
full remote rendering, resource/image commands, all console behavior.

For PS5, the evidence supports implementing a WebSocket endpoint with separate
per-connection subscriptions and exported refs, a version-3 handshake, an ordered
property model, scalar mutation, and typed events. Such an implementation remains
a design proposal until controlled live subscriptions and UI actions corroborate
the layouts and until console tests pass.
