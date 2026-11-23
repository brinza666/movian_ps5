# STPP title access and exit audit

2026-11-23. Worktree: `movian_ps5-playback-publish`, branch
`test/native-playback`, test title `PPSA99456`. The latest user instruction
permits trying without sandbox restrictions for this test. It supersedes the
older exclusion in AGENTS/handoff for this experiment only. It does not
authorize changing firmware, global network or update settings.

## Installed helper and scope

Inspected the installed public starter at
`C:/Users/brinza/Documents/deep/ps5-homebrew-starter/kit`:

- `docs/SANDBOX_ELEVATION.md`
- `examples/sandbox-elevation/{elevation.hpp,protocol.hpp,src/elevation.cpp}`
- `tools/{build.sh,build-lapy-helper.py}`
- `docs/CURL.md`, section 6, and `docs/UPDATE_CHECK.md`.

The client first proves PID-specific `/data` create/write/read/remove and
directory listing, then cooperates with a resident Lapy service using
`/download0/elevate_proc`. It waits 1.5 seconds and a one-second cancellation
grace period. A resident claim prevents a competing fallback. Otherwise it
streams `/app0/lapy.elf` to **localhost** TCP 9021 and exchanges the fixed
24-byte request/prepare/prepared/response messages, then repeats the data
proof. It calls `seteuid(geteuid())` for preparation. Transport timeouts are
five seconds. A title ELF is never sent to 9021.

The helper build selects exact title from param.json; manifest requires
`mode=elf-helper`, `max_requests=1`, `service=false` and the
`root_layout_probe_retry` feature, and checks the helper/protocol hashes.
It pins Lapy commit `57fcc19b7f57e46c823d6f2a6c6233ebf3288079` and SDK v0.42.
It is an existing public cooperative helper, not a new kernel-offset backend.

Starter documentation reports five launch/elevate/close cycles on each of
firmware 6.02 and 12.70, UID/GID 0, successful data proofs and two reaped donor
processes per run. This is the starter author's retained evidence, **not** a
qualification of this Movian build or this console. CURL.md describes the
title's root changing from its sandbox to the console root. The client and
packaging code show no firmware, global network or update configuration write.

No upstream cache was originally installed. The owner subsequently authorized
fetching this exact pinned public dependency for static review. The pinned
BlackBearReloaded URL returned 404; the same commit archive was available from
primary upstream `mpereiraesaa/PS5-Lapy-JB-Daemon`. Retained unmodified source:
`build/title-access-audit/PS5-Lapy-JB-Daemon-57fcc19b7f57e46c823d6f2a6c6233ebf3288079`.
No helper was executed. `source/owned_root_daemon.c` SHA-256 is
`61b82d6845f756387f606e7571ddbbf4ab8b95eccfb8e86013ba27a835c4d3e3`.

The reviewed owned-helper build compiles only `owned_root_daemon.c`,
`donor_transaction.c` and `vnode_ref_probe.c` with the public SDK and pinned
logging header. Static mutation audit:

- `run_one` checks exact title/PID again after ptrace stop, private filedesc,
  private cloned credentials and exactly one thread. It requires the existing
  prison pointer to equal prison0; it **does not overwrite prison0 or switch
  the target prison pointer**.
- It updates only the target private credentials: UID/RUID/SVUID and RGID/SVGID
  zero, zero supplementary group count, SYSTEM_AUTHID, full capability bytes
  and one attribute bit. Full original credentials are saved for rollback.
- `lapy_replace_two_roots` transfers two native vnode references from temporary
  donors to the target's filedesc root/jail slots, and transfers old roots into
  donor receiver slots. Donors are normally released/reaped, preserving vnode
  ownership. It does not manually patch global vnode counters.
- The helper clones its own credentials, temporarily changes its own authid
  to gain ptrace authority, and restores that authid. It samples global vnode
  counters for layout/balance checks; those samples are reads.
- No global security-policy/sysent/kernel-text patch, firmware write, system
  network/update setting change, persistent resident service installation or
  reboot-surviving permission change is in the reviewed helper path. The
  success effect is scoped to the target's lifetime plus transient helper and
  donor processes. This conclusion concerns this exact owned-helper path,
  not unrelated legacy/daemon sources in the upstream repository.
- Ambiguous reference ownership, failed credential restoration, or failed
  detach enters `daemon_held` and deliberately sleeps forever retaining any
  potentially owning processes. Do not describe all failures as cleanly
  reversible; watch klog and do not start another helper after such a result.

The installed build script still uses the unavailable fork URL. Packaging needs
the exact pinned source from the available upstream endpoint and the existing
manifest/hash checks; changing a URL must not change the pinned commit or
select an unreviewed helper. No kernel offsets were invented or copied into
Movian source.

## Bind evidence and integration

The existing console diagnostic bind to `0.0.0.0:42000` returned `EACCES` 13.
Root/jail elevation could remove its policy restriction, but the filesystem
proof does not test networking. Neither installed starter evidence nor this
audit proves that the proposed helper makes bind/listen work. The same native
bind/listen and actual remote connection must be tested after startup access.
If port 42000 is occupied after access succeeds, use a free HTTP listener port
and advertise it; do not misclassify `EADDRINUSE` as denied access. Discovery
UDP 42000 is a separate test. On denied access retain a usable local UI and
record the error. A paired outbound bridge remains an alternative transport;
do not automatically run another privileged implementation or change system
network settings.

Thin wrapper added: `app/src/title_access.hpp/.cpp`. Nothing enables it by
default. API: `movian::title_access::request_for_test()` returns `enabled`,
`data_verified`, numeric public-client `status`, diagnostic `path` and
`root_restored`. `active()` becomes true only after the pinned helper path,
data proof, UID 0 and root proof. `mode()` reports existing data access as
credential-unverified rather than claiming restrictions were removed.
`MOVIAN_TITLE_ACCESS_LAPY` compiles support only in native builds. Runtime
authorization is the separately uploaded `/app0/stpp-title-access-test.request`
containing exactly `PPSA99456\n`. The wrapper first creates the private
`/download0/stpp-title-access-test.consumed` with exclusive creation, then
seeds/verifies/removes `/download0/stpp-title-access-test`. Existing `.consumed`
means disabled, even when the app0 request remains. An explicitly authorized
retry requires removal of `.consumed` (and any incomplete pending file).
Consumption happens before module/helper work, so failures do not silently
authorize a second attempt. No request is packaged. Startup calls this once,
before fonts/pad/UI/workers. The wrapper preloads IME `0x0096` and videodec2
`207`; failure skips access. Load/unload wrappers keep successful preloads
available to the existing keyboard/decoder code after access. It initializes
the existing process-wide native network runtime before requesting access. Verify
that module/network initialization has not added a thread: the helper rejects
targets with more than one thread, rather than trying an unsafe fallback.

Implemented build/startup integration in `app/build.sh` and `app/src/main.cpp`:

1. `MOVIAN_TITLE_ACCESS_TEST=1 bash app/build.sh` explicitly compiles/packages
   support; default `0` forces `APP_LAPY_HELPER=0`, even with ambient starter
   environment variables. It rejects any title except PPSA99456 before enabling
   support. Keep `downloadDataSize` positive.
2. The build copies public files into `movian/src/starter_elevation/elevation.hpp`,
   `movian/src/starter_elevation/protocol.hpp` and
   `movian/src/starter_elevation/src/elevation.cpp` preserving copyright/license.
   Its `../` includes require this layout. Existing recursive compilation finds
   the new cpp. APP_LAPY_HELPER alone would only package the ELF.
3. It appends `MOVIAN_TITLE_ACCESS_LAPY`, sets `APP_LAPY_HELPER=1`, and sets
   `APP_WRAP_SYMBOLS` for `sceSysmoduleLoadModule`/`sceSysmoduleUnloadModule`
   only in the opted-in build. The copied starter builder's source acquisition
   uses the reviewed primary codeload archive SHA-256
   `c87cf02eceb1c004461f268de9657f7f17a367a4bbf3acfa2c6b7f86c90ae152`.
   It refuses changed cached source and preserves original SDK/logging download,
   protocol and manifest checks. The exact source archive and original starter
   notices are packaged under assets/licenses, alongside normal helper MIT
   license/manifest. The installed starter itself remains unmodified.
4. The SDK already supplies `libSceNet.so` and `libSceSystemService.so`; kit
   tools/build.sh links and converts against target/lib/*.so. No new custom
   import stub or kernel library is needed by the client/wrapper.

**Compatibility constraint:** public starter CURL.md says that after elevation
`sceHttp`/`sceSsl` requests to public sites fail (`0x8095f00c`); its demonstrated
solution is libcurl. It also says later system-module loads may fail because
paths still refer to the old sandbox. Preload/retain each module this title
needs before access (including videodec2/IME if those functions are tested).
Do not promise existing native HTTP/TLS plugin behavior survives elevation.
The narrowly scoped preloads above cover modules currently loaded by this title;
new module consumers must be audited separately. This test should establish transport access before broader playback/plugin
claims. No blanket module-ID list is invented here.

## Isolated original-root restoration probe

The opt-in wrapper retains an open descriptor for its own original `/` and
records that root's device/inode before calling the public helper. It also
requires an open `/download0/title-access-test.log` descriptor before module
preloads or helper execution. Module, helper, dynamic-loader, restoration and
failure stages are written through that descriptor across root changes; the
descriptor is closed before normal UI entry or requested shell exit. This
evidence path does not depend on klog being available.

After successful data proof, if `/` no longer matches the retained root, it
first dynamically resolves `chroot` and `fchdir` among already loaded modules.
When absent, it dynamically resolves public `sceKernelLoadStartModule` and
`sceKernelDlsym` from the existing libkernel module and tries exact installed
SDK SONAME `libkernel_sys.sprx`. The SDK symbol table confirms that module
exports `chroot`/`fchdir`; ordinary libkernel.so does not list them. No new
libkernel_sys startup import, guessed syscall number, NID, absolute system
path or modified privileged helper backend is used.

The native sequence is `fchdir(saved_root_fd)`, `chroot(".")`, `chdir("/")`.
Native APIs manage this process's vnode references. It leaves elevated
credentials/capabilities untouched and restores title path addressing; it is
an experiment with **broad credentials and original title-root paths**, not
a claim that all filesystem restrictions remain removed. It verifies original
root device/inode plus exact app0 request/download0 consumed files before
entering the UI. Existing filesystem access alone never proves credentials,
capabilities or bind permission and never forces a second helper attempt.

**Runtime unknowns:** whether this console permits basename loading of
libkernel_sys after helper access, resolves these symbols, accepts the native
root restore, and permits network bind with the resulting credentials. These
are not console-proven. A changed root with failed resolution/restoration
writes the private log, requests shell exit and stays in a sleep loop; it
does not enter UI with broken paths. A failed helper with unchanged verified
root closes retained descriptors and continues standard paths. The public
helper's own HELD failure can still prevent it from returning at all.

## Fresh-profile reset boundary

The primary owner reports completing a double-read/hash-verified backup of
the closed test title persistent container, and owns the separate
`profile_reset.hpp/.cpp` implementation. Main calls `profile::reset_requested()`
before title access, while original title paths are still active. Its request
is separately uploaded under `/app0/stpp-fresh-profile.request` with exact
`PPSA99456 v60 fresh\n` content and a one-time private done marker. No reset
was executed by this helper task. Exact currently used paths from source:

- `/download0/settings.txt` and `settings.txt.tmp`
- `/download0/browser-views.txt` and `browser-views.txt.tmp`
- `/download0/network-private.bin` and `network-private.bin.tmp`
- `/download0/plugins-v23/`: installed ZIPs, `<id>.settings/<key>`,
  `<id>.stores/<name>.json`, `<id>.torrent`, repository/temp files
- `/download0/mprepo-cache.json`

`resume.txt`, `bookmarks.txt` and their `.tmp` files are separate user history
and need not be cleared for plugin/config freshness. An external fallback feed
cache at `/data/movian/mprepo-cache.json` can seed repository data again; do not
automatically delete it. Logs, media, torrents and unrelated paths are outside
this reset boundary. No credentials or private config contents are recorded.

## Safe title exit

`movian::title_access::request_exit()` calls the installed public SDK import
`sceSystemServiceLoadExec("exit", nullptr)` and returns its result if the API
returns. Call from the main/UI thread after saving state and stopping workers;
an STPP callback should queue this action. Send any desired remote response
before scheduling closure. Host preview returns -1 and leaves its process up.

Keep the main/UI loop alive after the request, including failure: **never
return from main, call exit(), or remount while the title is still running**.
The shell's completion must be independently observed before replacement or
remount. The starter update-check example uses the same API followed by a
sleep loop. Its documented console result is `Kill for LoadExec ... => 0` and
normal unload with no core dump; that does not yet prove Movian/STPP closure.

## Validation

No console, reset, package, publish or system-setting action was performed by
this audit. Existing changes and saved package preserved. Baseline artifact
SHA-256: eboot `cec76bdce42a4821799fb5c2983bb0a032bd3dc68934b8367939cd111bb7491a`;
ZIP `b54442548d0689e75e99c53ff52fa4564c43a21bb40eacb3c2ca7f4743b4b5bf`.
Passed: build script `bash -n`, generated pinned-source adapter Python compile,
entry-point native compilation, wrapper
host-preview compilation, native opt-in wrapper compilation,
native public-client compilation (C++20, Wall/Wextra/Werror), the starter's
existing resident/one-shot client/exchange/proof test executable, and upstream
`tests/test_owned_credentials.py` (one credential snapshot/restore test).
Build objects and temporary public-client copies stay outside tracked source
under `/tmp/movian-title-access-check`. Bind, STPP and close/remount remain
console acceptance checks. No full packaging/self-check was needed for this
isolated wrapper audit; the owner must run required project self-check before
packaging the integrated title.
