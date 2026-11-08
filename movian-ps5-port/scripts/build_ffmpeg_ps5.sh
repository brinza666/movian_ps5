#!/bin/sh
# Cross-build FFmpeg 9.0.1 (the revision movian-go pins) with prospero-clang.
# Software decode only: the payload SDK has no libdrm, VAAPI or NVDEC.
set -eu

SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
VER=9.0.1
SHA256=cf38e0e28c7e5605942c4a77755349b0145804a397af37eb1fb4c77cb237f635
SRC="$ROOT/.deps/ffmpeg-$VER"
PREFIX="$ROOT/ffmpeg/ps5"

if [ ! -d "$SRC" ]; then
	mkdir -p "$ROOT/.deps"
	curl -fsSL -o "$ROOT/.deps/ffmpeg-$VER.tar.xz" \
		"https://ffmpeg.org/releases/ffmpeg-$VER.tar.xz"
	echo "$SHA256  $ROOT/.deps/ffmpeg-$VER.tar.xz" | sha256sum -c -
	tar -C "$ROOT/.deps" -xf "$ROOT/.deps/ffmpeg-$VER.tar.xz"
fi

cd "$SRC"
# --target-os=freebsd matches the SDK headers (kqueue, __FreeBSD__).
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
	--disable-programs --disable-doc --disable-avdevice \
	--disable-x86asm --disable-inline-asm \
	--disable-hwaccels --disable-vulkan --disable-vaapi --disable-vdpau \
	--disable-libdrm --disable-ffplay --disable-ffprobe \
	--extra-cflags="-O2 -fPIC" \
	--extra-ldflags="-fPIC"

make -j"$(nproc 2>/dev/null || echo 2)"
make install
echo "ffmpeg static libs in $PREFIX"
