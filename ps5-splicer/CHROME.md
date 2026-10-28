# Chrome package

`movianM7-7.0.273-nacl-chrome.zip` from [movian.eu](https://movian.eu/) is the same M7 version as the PS3 package, packaged as a Chrome app. Native Client was dropped by Google. The last Chrome that runs it is 113.

The page loads `app.nmf` / `app.pexe`. The native module draws the GLW skin and decodes. The host handles the window, dropped files, URLs and sockets. Its plugin feed string is `https://repo.movian.eu/plugins-v1.json`.

That package is not a PS5 binary. The alternative feed for this tree is [../PLUGINS.md](../PLUGINS.md).
