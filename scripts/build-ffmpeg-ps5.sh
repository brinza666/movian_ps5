#!/usr/bin/env bash
# Small static FFmpeg for the Movian PS5 playback test (no network).
# Includes H.264, AAC and E-AC-3. HEVC uses the platform hardware decoder.
set -euo pipefail
SDK=${PS5_PAYLOAD_SDK:-$HOME/.ps5-starter/ps5-payload-sdk}
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
VER=6.1.2
# ffmpeg.org/releases/ffmpeg-6.1.2.tar.xz (content-length 10466152, 2024-08-01)
SHA256=3b624649725ecdc565c903ca6643d41f33bd49239922e45c9b1442c63dca4e38
WORK=${MOVIAN_FFMPEG_BUILD_ROOT:-$ROOT/.deps}
SRC=$WORK/ffmpeg-$VER
TARBALL=$WORK/ffmpeg-$VER.tar.xz
PREFIX=$ROOT/.deps/ffmpeg-ps5
JOBS=$(nproc 2>/dev/null || echo 2)

[[ -x $SDK/bin/prospero-clang ]] || {
    echo "PS5_PAYLOAD_SDK missing at $SDK" >&2
    exit 2
}

mkdir -p "$WORK" "$ROOT/.deps"
if [[ ! -d $SRC ]]; then
    if [[ ! -f $TARBALL ]]; then
        curl -fsSL -o "$TARBALL" \
            "https://ffmpeg.org/releases/ffmpeg-$VER.tar.xz"
    fi
    echo "$SHA256  $TARBALL" | sha256sum -c -
    tar -C "$WORK" -xf "$TARBALL"
fi

cd "$SRC"
# Reconfigure when the parser/bsf set changes. HEVC parser only (no software
# HEVC decoder: that crashed this title). VLC packetizes from extra data;
# FFmpeg still needs the parser to name hev1/hvc1 streams.
if [[ ! -f ffbuild/config.mak ]] || ! grep -q 'CONFIG_EAC3_DECODER 1' config_components.h 2>/dev/null; then
    make distclean >/dev/null 2>&1 || true
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
        --enable-decoder=mpeg1video,mpeg2video,h264,aac,eac3,mp2,mp3,pcm_s16le \
        --enable-parser=mpegvideo,h264,hevc,aac,ac3,mpegaudio \
        --enable-bsf=h264_mp4toannexb,hevc_mp4toannexb \
        --enable-protocol=file \
        --extra-cflags="-O2 -fPIC" \
        --extra-ldflags="-fPIC"
fi
make -j"$JOBS"
make install
echo "ffmpeg static libs in $PREFIX"
