# Deploy script for Movian PS5 port
# Deploys to PS5 via FTP or USB
#
# Usage: ./deploy_ps5.sh [ps5-ip] [ps5-port]
#
# Deploy structure:
# /data/homebrew/PPSA99455/
#   eboot.bin
#   sce_module/libc.prx
#   sce_sys/param.json
#   sce_sys/icon0.png
#   assets/banner.txt
#   assets/ui/main.js

#!/bin/bash
set -e

PS5_IP="${1:-192.168.1.100}"
PS5_PORT="${2:-9021}"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "=== Movian PS5 Deploy ==="
echo "PS5 IP: $PS5_IP"
echo "PS5 Port: $PS5_PORT (etaHEN)"
echo "Project: $PROJECT_DIR"
echo ""

# Check eboot.bin exists
if [ ! -f "$PROJECT_DIR/eboot.bin" ]; then
    echo "ERROR: eboot.bin not found. Run build_ps5.sh first."
    exit 1
fi

# Check required files
for f in sce_module/libc.prx sce_sys/param.json sce_sys/icon0.png assets/banner.txt; do
    if [ ! -f "$PROJECT_DIR/$f" ]; then
        echo "WARNING: $f not found, continuing anyway"
    fi
done

echo "Deploying to PS5 via FTP..."
echo "Target: $PS5_IP:$PS5_PORT"
echo ""
echo "Manual deployment steps:"
echo "1. Copy entire F:\rea\movian-ps5\ folder to PS5 USB"
echo "2. Or use FTP to transfer to /data/homebrew/PPSA99455/"
echo "3. Ensure folder structure matches PPSA99455 template"
echo ""

# FTP deployment (commented out - requires ps5-ftp or similar)
# ftp -n $PS5_IP $PS5_PORT <<EOF
# user anonymous anonymous
# binary
# mkdir -p /data/homebrew/PPSA99455/sce_module
# mkdir -p /data/homebrew/PPSA99455/sce_sys
# mkdir -p /data/homebrew/PPSA99455/assets/ui
# put $PROJECT_DIR/eboot.bin /data/homebrew/PPSA99455/eboot.bin
# put $PROJECT_DIR/sce_module/libc.prx /data/homebrew/PPSA99455/sce_module/libc.prx
# put $PROJECT_DIR/sce_sys/param.json /data/homebrew/PPSA99455/sce_sys/param.json
# put $PROJECT_DIR/sce_sys/icon0.png /data/homebrew/PPSA99455/sce_sys/icon0.png
# put $PROJECT_DIR/assets/banner.txt /data/homebrew/PPSA99455/assets/banner.txt
# put $PROJECT_DIR/assets/ui/main.js /data/homebrew/PPSA99455/assets/ui/main.js
# quit
# EOF

echo ""
echo "=== Deploy Instructions ==="
echo "1. Copy F:\rea\movian-ps5\ to USB drive"
echo "2. Insert USB into PS5"
echo "3. Install homebrew package via shadowMountPlus or similar"
echo "4. Launch Movian PS5 from homebrew menu"
echo ""
echo "For debugging, connect to etaHEN on port $PS5_PORT"
echo ""
