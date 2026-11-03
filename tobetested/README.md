# tobetested – screen takeover + video test

Uploaded original: H.264 480x854 portrait ~10s glitter snail (not playable by the MPEG-1-only payload).

Generated: glitter_snail_test.mpg – MPEG-1 640x360 24fps ~10s, suitable for splicer-mpeg1.elf.

## Important
Background payloads sent to port 9021 open sceVideoOut and flip but leave the home UI on top. No picture on the TV.

To see anything you need a launch from the home screen:
- ShadowMountPlus / ItemzFlow folder under /data/homebrew/
- or an fPKG that stays open (previous RedBars pkgs closed after ~1s on 4.03)

## Suggested test
1. Copy the MPEG-1 file to /mnt/usb0/ or /data/video/ on the PS5.
2. For screen takeover, use a known-working homebrew folder launch if you have one, or the HomebrewTest pkg from the earlier v2 pack.
3. Once something stays on screen, the linear / mpeg1 scanout can be adapted into that title.

Previous fPKG tests on 4.03 installed but exited almost immediately (likely libc / packaging issue). The HL folder path is the one that has a better chance.

Videos are not in this commit (binary size). Download the original from the chat attachment and generate the .mpg with:
ffmpeg -i original.mp4 -vf "scale=640:360:force_original_aspect_ratio=decrease,pad=640:360:(ow-iw)/2:(oh-ih)/2" -c:v mpeg1video -b:v 2000k -r 24 -an glitter_snail_test.mpg
