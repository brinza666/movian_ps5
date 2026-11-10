#!/usr/bin/env bash
# Upload build/app/<TITLE_ID> to /data/homebrew on the console (FTP 2121).
set -euo pipefail
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(dirname -- "$here")
host=${1:-${PS5_HOST:-192.168.10.238}}
title=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' \
    "$repo/app/sce_sys/param.json")
src=$repo/build/app/$title
[[ -f $src/eboot.bin ]] || { echo "build first: bash app/build.sh" >&2; exit 2; }
exec bash "$repo/app/build.sh" deploy "$host"
