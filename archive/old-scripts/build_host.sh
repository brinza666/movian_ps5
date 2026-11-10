#!/bin/bash
# Host Build Script for Movian PS5 Port
# Compiles a Windows host version for testing without PS5 SDK
#
# Usage: ./build_host.sh
# Output: movian-ps5-host.exe

set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build_host"
OUTPUT_DIR="$PROJECT_DIR"

mkdir -p "$BUILD_DIR"

# Use system clang or gcc
CC="${CC:-clang}"
CFLAGS="-I\"$PROJECT_DIR/src\" \
    -D_HOST_BUILD_ \
    -O2 \
    -Wall \
    -Wno-unused-parameter \
    -fno-exceptions \
    -fno-rtti"

LDFLAGS="-lm -lpthread"

SRC_FILES="
    src/main.c
    src/duktape_ps5.c
    src/paw_platform.c
    src/movian_core.c
    src/video_ps5.c
    src/audio_ps5.c
    src/input_ps5.c
    src/filesystem_ps5.c
    src/smb2_ps5.c
    src/torrent_ps5.c
    src/subtitles_ps5.c
    src/plugin_ps5.c
    src/ui_ps5.c
    src/media_ps5.c
"

echo "=== Movian PS5 Host Build ==="
echo "Project: $PROJECT_DIR"
echo "Output: $OUTPUT_DIR/movian-ps5-host.exe"
echo ""

# Check for Duktape source
DUKTAPE_DIR="$PROJECT_DIR/src/duktape"
if [ ! -d "$DUKTAPE_DIR" ]; then
    echo "Downloading Duktape..."
    mkdir -p "$DUKTAPE_DIR"
    curl -sL https://github.com/svaarala/duktape/releases/download/2.7.0/duktape-2.7.0.tar.xz | tar xJ -C "$DUKTAPE_DIR" --strip-components=1
fi

CFLAGS="$CFLAGS -I\"$DUKTAPE_DIR\""
SRC_FILES="$SRC_FILES $DUKTAPE_DIR/duktape.c"

# Compile each source file
for src in $SRC_FILES; do
    src_path="$PROJECT_DIR/$src"
    if [ ! -f "$src_path" ]; then
        echo "WARNING: $src_path not found, skipping"
        continue
    fi
    obj_name=$(echo "$src" | tr '/' '_')
    obj_path="$BUILD_DIR/${obj_name%.c}.o"

    echo "Compiling $src..."
    $CC $CFLAGS -c "$src_path" -o "$obj_path" || {
        echo "ERROR: Failed to compile $src"
        exit 1
    }
done

# Collect object files
OBJ_FILES=""
for src in $SRC_FILES; do
    src_path="$PROJECT_DIR/$src"
    if [ ! -f "$src_path" ]; then continue; fi
    obj_name=$(echo "$src" | tr '/' '_')
    OBJ_FILES="$OBJ_FILES $BUILD_DIR/${obj_name%.c}.o"
done

# Link
echo "Linking movian-ps5-host.exe..."
$CC $CFLAGS $LDFLAGS $OBJ_FILES -o "$OUTPUT_DIR/movian-ps5-host.exe" || {
    echo "ERROR: Linking failed"
    exit 1
}

echo ""
echo "=== Host Build Complete ==="
echo "Output: $OUTPUT_DIR/movian-ps5-host.exe"
echo "Size: $(stat -c%s "$OUTPUT_DIR/movian-ps5-host.exe" 2>/dev/null || echo unknown) bytes"
echo ""
echo "Run: ./movian-ps5-host.exe"
echo ""
