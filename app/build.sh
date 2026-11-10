#!/usr/bin/env bash
# Build Movian for PS5 as a ShadowMountPlus title folder + ZIP.
#
# Layout matches VLC for PS5 (ZiZc3/VLC) and Hello PS5
# (MiladRTX/ps5-homebrew-starter): kit-based native C++20 eboot.bin.
#
#   app/build.sh                         -> build/app/PPSA99455/ and .zip
#   app/build.sh deploy 192.168.10.238   -> also FTP to /data/homebrew
#
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

usage() { echo "usage: app/build.sh [deploy <PS5 IP>]" >&2; exit 2; }
deploy_host=
case ${1:-} in
    "") [[ $# -eq 0 ]] || usage ;;
    deploy) [[ $# -eq 2 && -n $2 ]] || usage; deploy_host=$2 ;;
    *) usage ;;
esac

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(dirname -- "$here")
starter=${PS5_STARTER:-/mnt/c/Users/brinza/Documents/deep/ps5-homebrew-starter}
home=${PS5_STARTER_HOME:-$HOME/.ps5-starter}
kit=$home/kit-work
sdk=${PS5_PAYLOAD_SDK:-$home/ps5-payload-sdk}

[[ -d $starter/kit ]] || {
    echo "starter kit not found: $starter/kit" >&2
    echo "set PS5_STARTER to the ps5-homebrew-starter checkout" >&2
    exit 2
}
[[ -x $sdk/bin/prospero-lld ]] || {
    echo "PS5 payload SDK missing at $sdk" >&2
    echo "export PS5_PAYLOAD_SDK or run tools/setup.sh in the starter" >&2
    exit 2
}

# Copy the vendored boilerplate into a work tree that may contain .deps.
stamp=$(cd "$starter/kit" && find . -type f -print0 | sort -z | xargs -0 sha256sum | sha256sum | cut -c1-16)
if [[ ! -f $kit/.kit-stamp || $(cat "$kit/.kit-stamp") != "$stamp" ]]; then
    echo "Preparing the native-app kit..."
    mkdir -p "$kit"
    find "$kit" -mindepth 1 -maxdepth 1 ! -name .deps -exec rm -rf {} +
    cp -r "$starter/kit/." "$kit/"
    echo "$stamp" > "$kit/.kit-stamp"
fi
kit_sdk=$kit/.deps/native/ps5-payload-sdk
[[ ! -L $kit_sdk || -e $kit_sdk ]] || rm -f "$kit_sdk"
if [[ ! -e $kit_sdk && -x $sdk/bin/prospero-lld ]]; then
    mkdir -p "$kit/.deps/native"
    ln -sfn "$sdk" "$kit_sdk"
fi

rm -rf "$kit/movian"
mkdir -p "$kit/movian/assets/fonts" "$kit/movian/stubs"
cp -r "$here/src" "$here/sce_sys" "$kit/movian/"
cp "$here/fonts/"* "$kit/movian/assets/fonts/"
[[ ! -d $here/assets ]] || cp -r "$here/assets/." "$kit/movian/assets/"
bash "$kit/tools/validate-assets.sh" "$kit/movian/sce_sys"

# Link-only stub: SDK has no libSceCommonDialog (needed later for IME).
PS5_PAYLOAD_SDK=$sdk sh "$kit/tooling/prospero-clang18" -O2 -Wall -Wextra -fPIC \
    -c "$here/stubs/common_dialog.c" -o "$kit/movian/stubs/common_dialog.o"
"$sdk/bin/prospero-lld" --shared -soname libSceCommonDialog.sprx \
    -o "$kit/movian/stubs/libSceCommonDialog.so" "$kit/movian/stubs/common_dialog.o"

vars=(APP_SOURCE_DIR=movian/src APP_PARAM=movian/sce_sys/param.json
      APP_SCE_SYS=movian/sce_sys APP_ASSETS=movian/assets
      APP_IMPORT_STUBS=movian/stubs/libSceCommonDialog.so)
if [[ -n $deploy_host ]]; then
    make -C "$kit" "${vars[@]}" deploy PS5_HOST="$deploy_host"
else
    make -C "$kit" "${vars[@]}"
fi

title=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' \
    "$here/sce_sys/param.json")
rm -rf "$repo/build/app"
mkdir -p "$repo/build/app"
cp -r "$kit/dist/$title" "$kit/dist/$title.zip" "$repo/build/app/"
echo
echo "App folder: $repo/build/app/$title"
echo "ZIP:        $repo/build/app/$title.zip"
echo "Install:    copy the folder to /data/homebrew/$title on the console"
