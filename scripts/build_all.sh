#!/bin/bash
# Complete Build Script for Movian PS5 Port
# Cross-compiles for PS5 ARM64 and creates FPKG package
#
# Usage: ./build_all.sh [ps5-sdk-dir]
#
# Steps:
# 1. Cross-compile eboot.bin with prospero-clang
# 2. Convert to FSELF format
# 3. Create FPKG package (optional)

set -e

PS5_SDK_DIR="${1:-$PS5_SDK_DIR}"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

if [ -z "$PS5_SDK_DIR" ]; then
    echo "ERROR: PS5_SDK_DIR not set."
    echo "Usage: ./build_all.sh /path/to/ps5-sdk"
    echo ""
    echo "Or set environment variable:"
    echo "  export PS5_SDK_DIR=/path/to/ps5-sdk"
    exit 1
fi

# Export SDK variables
export PS5_SDK_DIR
export PS5_CLANG="${PS5_CLANG:-prospero-clang}"
export PS5_AR="${PS5_AR:-prospero-ar}"
export PS5_STRIP="${PS5_STRIP:-prospero-strip}"
export PS5_FSELF="${PS5_FSELF:-ps5-make-fself}"
export PS5_FPKG="${PS5_FPKG:-ps5-make-fpkg}"

# Verify SDK tools
echo "=== Verifying PS5 SDK ==="
for tool in $PS5_CLANG $PS5_AR $PS5_STRIP $PS5_FSELF; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: $tool not found in PATH"
        echo "Make sure PS5 SDK bin directory is in your PATH"
        exit 1
    fi
    echo "Found: $tool"
done

echo ""
echo "=== Step 1: Cross-compile eboot.bin ==="
./build_ps5.sh

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed"
    exit 1
fi

echo ""
echo "=== Step 2: Verify eboot.bin ==="
if [ ! -f "$PROJECT_DIR/eboot.bin" ]; then
    echo "ERROR: eboot.bin not produced"
    exit 1
fi
echo "eboot.bin: $(stat -c%s "$PROJECT_DIR/eboot.bin") bytes"

echo ""
echo "=== Step 3: Create FPKG package ==="
./make_fpkg.sh "$PROJECT_DIR"

if [ $? -ne 0 ]; then
    echo "WARNING: FPKG creation failed, but eboot.bin is ready"
    exit 0
fi

echo ""
echo "=== All Build Steps Complete ==="
echo "eboot.bin: $PROJECT_DIR/eboot.bin"
echo "FPKG: $PROJECT_DIR/Movian_PS5.fpkg"
echo ""
echo "Deploy to PS5:"
echo "  USB: Copy Movian_PS5.fpkg to USB, install via homebrew installer"
echo "  FTP: Copy eboot.bin + folder structure to /data/homebrew/PPSA99455/"
echo ""
