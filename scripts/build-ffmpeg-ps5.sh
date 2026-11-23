#!/usr/bin/env bash
# Small static FFmpeg for the Movian PS5 playback test with passive FTP.
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
DAV1D_VER=1.5.4
DAV1D_SHA256=686616b7c69eb88d44459391ab25cac13b6647a3b288835c5784e71c1514a5c5
DAV1D_SRC=$WORK/dav1d-$DAV1D_VER
DAV1D_PREFIX=$WORK/dav1d-ps5
JOBS=$(nproc 2>/dev/null || echo 2)

[[ -x $SDK/bin/prospero-clang ]] || {
    echo "PS5_PAYLOAD_SDK missing at $SDK" >&2
    exit 2
}

mkdir -p "$WORK" "$ROOT/.deps"

if [[ ! -f $DAV1D_PREFIX/lib/libdav1d.a ]]; then
    dav1d_tar=$WORK/dav1d-$DAV1D_VER.tar.xz
    if [[ ! -f $dav1d_tar ]]; then
        curl -fsSL -o "$dav1d_tar" \
            "https://download.videolan.org/pub/videolan/dav1d/$DAV1D_VER/dav1d-$DAV1D_VER.tar.xz"
    fi
    echo "$DAV1D_SHA256  $dav1d_tar" | sha256sum -c -
    if [[ ! -d $DAV1D_SRC ]]; then
        tar -C "$WORK" -xf "$dav1d_tar"
    fi
    cat > "$WORK/dav1d-cross.ini" <<EOF
[binaries]
c = '$SDK/bin/prospero-clang'
ar = '$SDK/bin/prospero-ar'
strip = '$SDK/bin/prospero-strip'

[host_machine]
system = 'freebsd'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
EOF
    meson setup "$WORK/dav1d-build" "$DAV1D_SRC" --cross-file "$WORK/dav1d-cross.ini" \
        --prefix "$DAV1D_PREFIX" --libdir lib --default-library static \
        -Db_staticpic=true -Denable_asm=false -Denable_tools=false -Denable_tests=false
    meson compile -C "$WORK/dav1d-build"
    meson install -C "$WORK/dav1d-build"
fi
if [[ ! -d $SRC ]]; then
    if [[ ! -f $TARBALL ]]; then
        curl -fsSL -o "$TARBALL" \
            "https://ffmpeg.org/releases/ffmpeg-$VER.tar.xz"
    fi
    echo "$SHA256  $TARBALL" | sha256sum -c -
    tar -C "$WORK" -xf "$TARBALL"
fi

python3 "$ROOT/scripts/patch-ffmpeg-network.py" "$SRC"
cd "$SRC"
# Reconfigure when the parser/bsf set changes. HEVC parser only (no software
# HEVC decoder: that crashed this title). VLC packetizes from extra data;
# FFmpeg still needs the parser to name hev1/hvc1 streams.
if [[ ! -f ffbuild/config.mak ]] || ! grep -q '#define CONFIG_FTP_PROTOCOL 1' config_components.h 2>/dev/null || ! grep -q '#define CONFIG_LIBDAV1D_DECODER 1' config_components.h || ! grep -q -- '-D__PROSPERO__' ffbuild/config.mak; then
    make distclean >/dev/null 2>&1 || true
    PKG_CONFIG_LIBDIR="$DAV1D_PREFIX/lib/pkgconfig" ./configure \
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
        --enable-network --disable-protocols \
        --disable-muxers --disable-encoders --disable-hwaccels \
        --disable-x86asm --disable-inline-asm --disable-autodetect \
        --disable-everything \
        --enable-demuxer=mpegps,mpegts,mov,matroska,avi,mp3,aac \
        --enable-libdav1d --pkg-config-flags=--static \
        --enable-decoder=mpeg1video,mpeg2video,mpeg4,h264,vp8,vp9,libdav1d,rawvideo,aac,ac3,eac3,mp2,mp3,opus,vorbis,pcm_s16le \
        --enable-parser=mpegvideo,mpeg4video,h264,hevc,vp8,vp9,av1,aac,ac3,mpegaudio,opus,vorbis \
        --enable-bsf=h264_mp4toannexb,hevc_mp4toannexb \
        --enable-protocol=file,ftp,tcp \
        --extra-cflags="-O2 -fPIC -D__PROSPERO__" \
        --extra-ldflags="-fPIC"
fi
make -j"$JOBS"
make install
install -m 644 "$DAV1D_PREFIX/lib/libdav1d.a" "$PREFIX/lib/libdav1d.a"
echo "ffmpeg static libs in $PREFIX"
