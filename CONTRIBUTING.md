# Contributing

## Scope

This is a native PS5 title. Patches should move a **PORTING_PLAN.md phase**
forward with something that builds and, when hardware is available, runs.

Do not:

- Mark a feature complete unless it runs on a PS5 title (or a documented
  host preview that shares the same sources).
- Check in `.elf`, `.bin`, `.prx`, or SDK trees.
- Revive `archive/go-overlay` as the shipping path without a libc-calling
  Go runtime that has been shown to survive `_start`.
- Point `PS5_SDK_SETUP.md`-style docs at Sony’s proprietary SDK. This
  project uses the public [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk).

## Development loop

1. WSL2 Ubuntu 24.04.
2. `bash scripts/doctor.sh`
3. Edit `app/src/`.
4. `bash app/build.sh`
5. Deploy with `bash app/build.sh deploy $PS5_HOST` (FTP 2121).
6. Close the title on the console, wait for ShadowMountPlus to rescan
   (~90 s, or `POST http://$PS5_HOST:10101/api/v1/scan`), relaunch.
7. Watch klog on TCP 3232.

## Code style

- C++20, exceptions and RTTI off (boilerplate default).
- 4-space indent.
- Platform calls stay behind `movian::screen` / `movian::pad` / future
  `movian::player` facades. UI code does not call `sce*` directly.
- Logs: `fprintf(stderr, …)` shows up in klog; also append to
  `/download0/movian-ps5.log` for anything a user might attach to a bug.

## Commits

Conventional commits: `feat:`, `fix:`, `docs:`, `build:`, `chore:`.

Every feature commit should update [docs/STATUS.md](docs/STATUS.md) if the
truth table changed.
