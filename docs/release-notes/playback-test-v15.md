## Movian PS5 video switching test v15

This prerelease builds the separate Movian HEVC Test title `PPSA99456`.
Open it from the media home screen. Cross on Home runs the bundled HEVC
clip. Use Videos for local files.

- Reuse the decoded picture allocation across files and protect the frame
  while the compositor reads it.
- Synchronize audio during file switches and wait for the final AudioOut
  submission before advancing. The logs count submitted, nonzero and
  dropped PCM frames.
- Write FFmpeg warnings to the title log. Select the linked MPEG demuxer
  when an MPEG Program Stream pack header identifies the clip.

Earlier console runs verified repeated 1920×1040 H.264 hardware output
and 48 kHz stereo PCM from a 5.1 E-AC-3 track. v14 recovered from failed
MPEG clip opens and continued playing H.264. The v15 binary was built and
verified on the mounted console. The v15 switch sequence, MPEG playback
and audible E-AC-3 through feature EOF are still being tested.

The title uses standard platform imports and contains no sandbox-lift
code. The executable's SHA-256 is
`4a238d8443f2e2f4d56889d8828a1f51562724bf656c8537ece043dfb8fc4efc`.
`ffmpeg-6.1.2.tar.xz` supplies the source for the linked FFmpeg build.
See the [playback test handoff](https://github.com/brinza666/movian_ps5/blob/playback-test-v15/docs/PLAYBACK_TEST_HANDOFF.md)
for build inputs and console evidence.
