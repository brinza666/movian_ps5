# v63: orderly Quit from local and remote controls

The installed WSL M7 7.0.273 General page exposes Quit as an action. Activating
its event sink exits immediately without a confirmation. The installed view
and runtime evidence are recorded in [planned options](PLANNED_OPTIONS.md).
The PS5 implementation routes General → Quit, sidebar Exit and STPP Quit through
one callback.

The callback closes playback, requests torrent cancellation, stops accepting new
network jobs and drains existing work. It acquires each worker's completion flag
before joining that thread, waits for the native keyboard to release its buffer,
then saves settings and requests the shell to close this title. A successful
asynchronous request leaves the main loop alive until the shell terminates it.
A rejected shell request restores normal input and the configured FTP server,
with an error notice. It never returns from the title's main function.

## Validation

The shared production UI is exercised through real loopback STPP sockets.
The shell, playback and IME boundaries are host fakes, so these tests make no
hardware, sound or console lifecycle claims. Tests cover local Quit, native
event-sink Activate, sidebar Exit, navigation Quit, settings persistence,
shell-refusal recovery, four owned worker threads, torrent cancellation, IME
ownership, an actual FTP partial command and an actual asynchronous FTP listing.
While draining, new listings are rejected and UI updates remain nonblocking.
The persisted FTP-enabled preference survives shutdown.

The v62 console package remains mounted. The console stopped responding before
its autonomous launch request: FTP 2121, ShadowMount 10101 and Minion 9180 now
time out. No v62 helper credential, listener bind or WSL→PS5 result is claimed.
The [remote lifecycle investigation](SHADOWMOUNT_CONTROL.md) identifies the
installed Minion launch/close API for when it becomes reachable. ShadowMount
mount/unmount is a separate filesystem operation.

## Next acceptance

Launch the mounted v62 through the existing Minion service, verify its access
log and STPP listener, and complete WSL control. Then deploy v63 and repeat Quit
and relaunch on the console. Continue P02 plugin details/Uninstall independently
using the disposable native fixture evidence; do not invent plugin detail actions.

Signed package `build/PPSA99456-v63.zip`: 7707847 bytes, SHA-256
`416dc0e6332c94c959046933960f892153f1dfb76630a641ef05f9bf994e6e14`. Eboot SHA-256
`70225f7753097d9dbbb8e61c2129e49269f873fca28780bf3728ca1b83a33436`. All 29 files passed ZIP integrity; console opt-in
request files are absent. The access experiment remains separately gated.
