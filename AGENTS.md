# Movian PS5 playback worktree

At the start of each project chat, report the current provider, model identity, local date, host OS, and build OS from the available session metadata. Mark an undisclosed exact model as unknown. If the user changes the model or provider, report the new values from the updated metadata. Answer identity and verification questions using the same evidence.

Read [SPEC.md](SPEC.md), [STATUS](docs/STATUS.md), the local port log, and the project handoff. Check the index, working tree, branch and saved build artifacts before editing. Preserve unsaved changes. Work on one unfinished acceptance test at a time. Keep title `PPSA99456` separate from production. The latest 2026-11-23 user request authorizes an explicit, default-off title-access experiment for STPP; earlier no-sandbox-lift instructions remain historical and do not override this request. Never package console opt-in request files. The same request authorizes a fresh test-title plugin/config profile after a verified PC backup; do not preserve stale plugins for this test.

For a context handoff, record the active checkout, changed files, last working package and hashes, exact console results, and the next unfinished acceptance test. Use the chat host’s supported compaction and helper agents when available and authorized. Use REA for shipped artifact or runtime evidence. Apply the same rule to external MCP tools and hooks. Keep user data, credentials, private tracker URLs, and access tokens out of checked-in artifacts.

Before packaging, run `wsl -d Ubuntu-24.04 -- bash scripts/self-check.sh` from this worktree. Record which host tests passed and which console checks still need evidence.

## Evidence and implementation sources

- Use the installed desktop Movian build running in WSL Ubuntu 24.04 and captured REA evidence as the reference for desktop features, settings, and behavior. Record the build, the action observed, and the evidence location. Treat behavior as unverified if the installed build does not expose it or the observation does not cover it.
- Do not use the old andoma/Movian Git source to reconstruct desktop behavior or implement the PS5 port.
- Use [YouROK/TorrServer](https://github.com/YouROK/TorrServer) and the exact dependencies pinned by its server build as the implementation reference for BitTorrent, including discovery, peer transport, caching, buffering, and playback while downloading. Record the release and source paths used, then adapt those behaviors to the native PS5 title.
