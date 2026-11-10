# Build script for Movian PS5 port
# Cross-compiles for PS5 ARM64 using PS5 SDK (prospero-clang)
#
# Usage: ./build_ps5.sh
# Output: F:\rea\movian-ps5\eboot.bin

#!/bin/bash
set -e

# Configuration
PS5_SDK_DIR="${PS5_SDK_DIR:-/path/to/ps5-sdk}"
PS5_CLANG="${PS5_CLANG:-prospero-clang}"
PS5_AR="${PS5_AR:-prospero-ar}"
PS5_STRIP="${PS5_STRIP:-prospero-strip}"
PS5_FSELF="${PS5_FSELF:-ps5-make-fself}"

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
OUTPUT_DIR="$PROJECT_DIR"

mkdir -p "$BUILD_DIR"

# Compiler flags for PS5 ARM64
CFLAGS="-target aarch64-sce-elf \
    -mcpu=cortex-a72 \
    -I\"$PROJECT_DIR/src\" \
    -I\"$PS5_SDK_DIR/include\" \
    -D__PS5__ \
    -O2 \
    -Wall \
    -Wno-unused-parameter \
    -fno-exceptions \
    -fno-rtti \
    -ffunction-sections \
    -fdata-sections"

LDFLAGS="-L\"$PS5_SDK_DIR/lib\" \
    -L\"$PROJECT_DIR/sce_module\" \
    -lc \
    -lvideoout \
    -laudioout \
    -lpthread \
    -lavformat \
    -lavcodec \
    -lswscale \
    -lswresample \
    -lnostartfiles \
    -Wl,--gc-sections \
    -Wl,-Ttext,0x100000"

# Source files
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

echo "=== Movian PS5 Build ==="
echo "Project: $PROJECT_DIR"
echo "Output: $OUTPUT_DIR/eboot.bin"
echo "SDK: $PS5_SDK_DIR"
echo ""

# Compile each source file
for src in $SRC_FILES; do
    src_path="$PROJECT_DIR/$src"
    obj_name=$(echo "$src" | tr '/' '_')
    obj_path="$BUILD_DIR/${obj_name%.c}.o"

    echo "Compiling $src..."
    $PS5_CLANG $CFLAGS -c "$src_path" -o "$obj_path" || {
        echo "ERROR: Failed to compile $src"
        exit 1
    }
done

# Collect object files
OBJ_FILES=""
for src in $SRC_FILES; do
    obj_name=$(echo "$src" | tr '/' '_')
    OBJ_FILES="$OBJ_FILES $BUILD_DIR/${obj_name%.c}.o"
done

# Link into ELF
echo "Linking eboot.elf..."
$PS5_CLANG $CFLAGS $LDFLAGS \
    $OBJ_FILES \
    -o "$BUILD_DIR/eboot.elf" || {
    echo "ERROR: Linking failed"
    exit 1
}

# Strip symbols
echo "Stripping symbols..."
$PS5_STRIP --strip-unneeded "$BUILD_DIR/eboot.elf" -o "$BUILD_DIR/eboot_stripped.elf" || {
    echo "WARNING: Strip failed, using unstripped ELF"
    cp "$BUILD_DIR/eboot.elf" "$BUILD_DIR/eboot_stripped.elf"
}

# Convert to FSELF format for PS5
echo "Converting to FSELF..."
if command -v "$PS5_FSELF" >/dev/null 2>&1; then
    $PS5_FSELF "$BUILD_DIR/eboot_stripped.elf" "$OUTPUT_DIR/eboot.bin" || {
        echo "ERROR: FSELF conversion failed"
        exit 1
    }
else
    echo "WARNING: ps5-make-fself not found, copying ELF as eboot.bin"
    echo "       For real PS5 deployment, convert ELF to FSELF format"
    cp "$BUILD_DIR/eboot_stripped.elf" "$OUTPUT_DIR/eboot.bin"
fi

echo ""
echo "=== Build Complete ==="
echo "Output: $OUTPUT_DIR/eboot.bin"
echo "Size: $(stat -c%s "$OUTPUT_DIR/eboot.bin" 2>/dev/null || echo unknown) bytes"
echo ""
echo "Features included:"
echo "  - Video output (sceVideoOut, CPU RGBA8)"
echo "  - Audio output (sceAudioOut, PCM)"
echo "  - Controller input (scePad)"
echo "  - File system (app0, usb0, host0)"
echo "  - SMB2 network shares"
echo "  - BitTorrent client (magnet, .torrent)"
echo "  - Subtitles (srt, ass, vobsub)"
echo "  - Plugin system (Duktape API v2)"
echo "  - Media playback (FFmpeg software decode)"
echo "  - UI with on-screen keyboard"
echo ""
echo "Deploy to: /data/homebrew/PPSA99455/ on PS5"
echo "Firmware: 4.03 (compatible with any)"
echo ""
