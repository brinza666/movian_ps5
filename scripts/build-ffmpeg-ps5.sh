#!/usr/bin/env bash
# Small static FFmpeg for Movian PS5 (H.264 + AAC, no network).
# Does not link into the title until STATUS.md is updated in the same change.
set -euo pipefail
SDK=${PS5_PAYLOAD_SDK:-$HOME/.ps5-starter/ps5-payload-sdk}
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
VER=6.1.2
SHA256=3e0ce06bff50122907c64f952256e3ba627622b4ee837a1ad914781aa81f0a0c
SRC=$ROOT/.deps/ffmpeg-$VER
PREFIX=$ROOT/.deps/ffmpeg-ps5
JOBS=$(nproc 2>/dev/null || echo 2)

[[ -x $SDK/bin/prospero-clang ]] || {
    echo "PS5_PAYLOAD_SDK missing at $SDK" >&2
    exit 2
}

mkdir -p "$ROOT/.deps"
if [[ ! -d $SRC ]]; then
    curl -fsSL -o "$ROOT/.deps/ffmpeg-$VER.tar.xz" \
        "https://ffmpeg.org/releases/ffmpeg-$VER.tar.xz"
    echo "$SHA256  $ROOT/.deps/ffmpeg-$VER.tar.xz" | sha256sum -c -
    tar -C "$ROOT/.deps" -xf "$ROOT/.deps/ffmpeg-$VER.tar.xz"
fi

cd "$SRC"
if [[ ! -f ffbuild/config.mak ]]; then
    ./configure \
        --prefix="$PREFIX" \
        --cc="$SDK/bin/prospero-clang" \
        --ar="$SDK/bin/prospero-ar" \
        --nm="$SDK/bin/prospero-nm" \
        --ranlib="$SDK/bin/prospero-ranlib" \
        --strip="$SDK/bin/prospero-strip" \
        --arch=x86_64 \
        --target-os=freebsd \
        --enable-cross-compile \
        --sysroot="$SDK/target" \
        --enable-static --disable-shared \
        --disable-programs --disable-doc --disable-avdevice --disable-avfilter \
        --disable-network --disable-protocols --enable-protocol=file \
        --disable-muxers --disable-encoders --disable-hwaccels \
        --disable-x86asm --disable-inline-asm --disable-autodetect \
        --disable-everything \
        --enable-demuxer=mpegps,mpegts,mov,matroska,avi,mp3,aac \
        --enable-decoder=mpeg1video,mpeg2video,h264,aac,mp2,mp3,pcm_s16le \
        --enable-parser=mpegvideo,h264,aac,mpegaudio \
        --enable-protocol=file \
        --extra-cflags="-O2 -fPIC" \
        --extra-ldflags="-fPIC"
fi
make -j"$JOBS"
make install
echo "ffmpeg static libs in $PREFIX"
