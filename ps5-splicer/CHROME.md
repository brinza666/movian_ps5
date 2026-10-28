# Chrome / NaCl variant (M7 7.0.273)

Official package `movianM7-7.0.273-nacl-chrome.zip` from
`https://movian.eu/` (same version number as the PS3 pkg). It is a Chrome
packaged app, not a web page player. The readme says Native Client was
dropped by Google and the last Chrome that still runs it is 113.

## What REA could read

`rea analyze-javascript-application` on `app.js` and `background.js`
(evidence `ev_ea92e6102c9516d694bf864b25d0b8bd6bf094852e25685a89b18d5445b503ad`,
provider `rea-javascript-application` v1). Scripts were parsed, not run.
437 semantic nodes. Named functions: `handleDrop`, `handleDragOver`,
`handleMessage`, `launch`, `relaunch`, `displaycrash`, `cleanup`.

REA's own summary is built for Electron, so it reported zero IPC channels
and zero native bindings. That is a gap in the summary, not a claim that
the calls are absent. The same graph still has `postMessage`,
`webkitPersistentStorage.requestQuota`, and `e.data.msgtype`. The source
those nodes point at does this:

- `background.js` opens a 1280×720 window on `index.html`.
- `launch` inserts `<embed src="app.nmf" type="application/x-pnacl">`.
- The page is a loader. The GLW picture is drawn by the native module.
- Messages the page actually sends or answers: `openurl` (including
  `dragndrop://<name>`), `openfile`, `readfile`, `dndopenreply`,
  `dndreadreply`, `fsinfo`, `rpcreply`, `running`, `panic`.
- `relaunch` asks for 256 MiB of persistent storage, then starts the embed.
- `manifest.json` requires WebGL and the socket permissions
  `tcp-listen`, `tcp-connect`, `udp-bind`, `udp-send-to`, multicast.

`rea inspect-artifact` on `app.pexe` (12 750 048 bytes, magic `PEXE`)
returned `target_unavailable` / `unsupported binary format`. REA did not
recover symbols from the native module. Do not invent any.

## What that leaves open

The Chrome build is a thin host around one native module. The host owns
the window, a dropped file, an URL, a storage quota, and raw sockets.
The module owns the skin and the decode. That split is the useful part
for a PS5 port. The NaCl runtime is not: it needs Chrome 113 and a PNaCl
translator, and it does not exist on Orbis.

A string scan of `app.pexe` (not REA; the format was rejected) shows
Dean's tree `/home/dean/movian_nacl/`, `src/libavcodec/` (H.264, HEVC,
VP9, AC-3, DTS, VC-1), `PPB_OpenGLES2`, `PpapiPluginStart`, GLW
(`showtime:glw`), bittorrent, SMB, and `https://repo.movian.eu/plugins-v1.json`.
Those strings are not a symbol table. They do say this package is full
M7, and that its picture path is Pepper GLES2, not the HTML skin and not
`pl_mpeg`.
