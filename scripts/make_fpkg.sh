#!/bin/bash
# FPKG Package Script for Movian PS5
# Creates a fake package (.fpkg) that can be installed on PS5
#
# Usage: ./make_fpkg.sh [output_dir]
# Output: Movian_PS5.fpkg

set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUTPUT_DIR="${1:-$PROJECT_DIR}"
FPKG_NAME="Movian_PS5"
FPKG_FILE="$OUTPUT_DIR/${FPKG_NAME}.fpkg"

# Check for ps5-make-fpkg
FPKG_TOOL="${PS5_FPKG:-ps5-make-fpkg}"
if ! command -v "$FPKG_TOOL" >/dev/null 2>&1; then
    echo "ERROR: $FPKG_TOOL not found. Install PS5 SDK first."
    echo "See PS5_SDK_SETUP.md for instructions."
    exit 1
fi

# Check for eboot.bin
if [ ! -f "$PROJECT_DIR/eboot.bin" ]; then
    echo "ERROR: eboot.bin not found. Run build_ps5.sh first."
    exit 1
fi

echo "=== Movian PS5 FPKG Packager ==="
echo "Project: $PROJECT_DIR"
echo "Output: $FPKG_FILE"
echo ""

# Create staging directory
STAGING_DIR="$PROJECT_DIR/build_fpkg"
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR/$FPKG_NAME/sce_module"
mkdir -p "$STAGING_DIR/$FPKG_NAME/sce_sys"
mkdir -p "$STAGING_DIR/$FPKG_NAME/assets/ui"

# Copy files
echo "Copying files to staging..."
cp "$PROJECT_DIR/eboot.bin" "$STAGING_DIR/$FPKG_NAME/"
cp "$PROJECT_DIR/sce_module/libc.prx" "$STAGING_DIR/$FPKG_NAME/sce_module/"
cp "$PROJECT_DIR/sce_sys/param.json" "$STAGING_DIR/$FPKG_NAME/sce_sys/"
cp "$PROJECT_DIR/sce_sys/icon0.png" "$STAGING_DIR/$FPKG_NAME/sce_sys/"
cp "$PROJECT_DIR/assets/banner.txt" "$STAGING_DIR/$FPKG_NAME/assets/"
cp "$PROJECT_DIR/assets/ui/main.js" "$STAGING_DIR/$FPKG_NAME/assets/ui/"

# Create param.sfo (required for FPKG)
echo "Creating param.sfo..."
cat > "$STAGING_DIR/$FPKG_NAME/sce_sys/param.sfo" << 'EOF'
__SCE__param_sfo
APP_VER=01.00
ATTRIBUTE=0
BOOTABLE=1
CATEGORY=HG
CONTENT_ID=UP9000-PPSA99455_00-MOVIANPS5000000
PARENTAL_LEVEL=0
PS3_SYSTEM_VER=0000000000000000
TITLE=Movian PS5
TITLE_ID=PPSA99455
VERSION=01.00
EOF

# Create pkg.conf
echo "Creating pkg.conf..."
cat > "$STAGING_DIR/pkg.conf" << EOF
title_id=PPSA99455
content_id=UP9000-PPSA99455_00-MOVIANPS5000000
title=Movian PS5
version=01.00
package_type=HG
bootable=true
source_dir=$STAGING_DIR/$FPKG_NAME
output_file=$FPKG_FILE
license_type=free
content_type=application
addon_content_id=
category=game
required_system_version=0x0000000000000000
sdk_version=0x0000000000000000
EOF

# Build FPKG
echo "Building FPKG..."
$FPKG_TOOL --config "$STAGING_DIR/pkg.conf" || {
    echo "ERROR: FPKG creation failed"
    exit 1
}

# Cleanup staging
rm -rf "$STAGING_DIR"

echo ""
echo "=== FPKG Build Complete ==="
echo "Output: $FPKG_FILE"
echo "Size: $(stat -c%s "$FPKG_FILE" 2>/dev/null || echo unknown) bytes"
echo ""
echo "Install on PS5:"
echo "1. Copy $FPKG_FILE to USB drive"
echo "2. Insert USB into PS5"
echo "3. Install via homebrew installer (ShadowMountPlus, etc.)"
echo "4. Launch Movian PS5 from homebrew menu"
echo ""
