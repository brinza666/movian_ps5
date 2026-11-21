## Movian PS5 hardware playback test v12

This is an experimental folder title for `PPSA99456`. It can coexist with the
production title `PPSA99455`. Open **Movian HEVC Test** and press Cross at Home
for the bundled six-second fixture. Use Videos to select other files.

- A previous test build played the 1920×1080 HEVC fixture and drained all 180
  hardware-decoded frames. v12 also logged its first hardware frame and AAC PCM.
- v12 logged 1920×1040 H.264 High hardware output and stereo PCM generated from
  the six-channel E-AC-3 track in the MKV regression fixture.
- Audible TV output, full MKV playback and the cause of an earlier intermittent
  null-instruction crash remain open. Later repeated fixture requests also reported
  hardware HEVC unavailable.

Contains no sandbox-lift integration. The console's video library is provided
by the platform. See the [handoff](https://github.com/brinza666/movian_ps5/blob/playback-test-v12/docs/PLAYBACK_TEST_HANDOFF.md)
for build inputs, evidence and next checks.

The release ZIP contains the tested executable with SHA-256
`8bb86ccafcf495a20cebe7ed371b6026ede87a9edca1a2388cf30135c1ffa391`.
`ffmpeg-6.1.2.tar.xz` is the pinned source for the static FFmpeg libraries.
