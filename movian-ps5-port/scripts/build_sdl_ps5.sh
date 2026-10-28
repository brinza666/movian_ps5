#!/bin/sh
# Build the PS5 SDL2 port and install it into the payload SDK sysroot
# so `prospero-pkg-config --libs sdl2` works.
set -eu

SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
CC="$SDK/bin/prospero-clang"
CMAKE_TC=""
if [ -f "$SDK/toolchain/prospero.cmake" ]; then
	CMAKE_TC="$SDK/toolchain/prospero.cmake"
elif [ -f "$SDK/host/toolchain/prospero.cmake" ]; then
	CMAKE_TC="$SDK/host/toolchain/prospero.cmake"
else
	echo "no prospero.cmake under $SDK" >&2
	exit 1
fi

ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/.deps/SDL"
if [ ! -d "$SRC/.git" ]; then
	mkdir -p "$ROOT/.deps"
	git clone --depth 1 --branch release-2.30.x-ps5 \
		https://github.com/ps5-payload-dev/SDL.git "$SRC"
fi

cmake -S "$SRC" -B "$SRC/build-ps5" -G Ninja \
	-DCMAKE_TOOLCHAIN_FILE="$CMAKE_TC" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/user/homebrew \
	-DSDL_SHARED=OFF -DSDL_STATIC=ON \
	-DSDL_VIDEO=ON -DSDL_RENDER=OFF \
	-DSDL_JOYSTICK=ON -DSDL_HAPTIC=OFF

cmake --build "$SRC/build-ps5"
# DESTDIR is the SDK sysroot. prospero.mk defaults it to $SDK/target.
cmake --install "$SRC/build-ps5" --prefix "${DESTDIR:-$SDK/target}/user/homebrew"
echo "sdl2 installed. Check: $SDK/bin/prospero-pkg-config --libs sdl2"
