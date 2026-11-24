#!/usr/bin/env bash
# Real shared UI/STPP host acceptance; optional --serve for a native Movian viewer.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
out=${MOVIAN_UI_STPP_BUILD_DIR:-$root/build/ui-stpp}
ffmpeg=${MOVIAN_HOST_NETWORK_FFMPEG_DIR:-/tmp/movian-ffmpeg-v53}
smb=${MOVIAN_HOST_SMB2_DIR:-/tmp/movian-smb2-host}
vendor=${MOVIAN_HOST_PLUGIN_HEADERS:-$root/build/v52-ui/vendor}
mkdir -p "$out"
archives=()
for lib in avformat avcodec swresample swscale avutil; do
    archives+=("$ffmpeg/lib$lib/lib$lib.a")
done
for dependency in "${archives[@]}" "$smb/lib/libsmb2.a" "$vendor/duktape.c"; do
    [[ -f $dependency ]] || { echo "Missing host dependency: $dependency" >&2; exit 2; }
done
cc -ffunction-sections -fdata-sections -c app/src/stb_impl.c -o "$out/stb.o"
objects=()
for source in duktape miniz miniz_zip miniz_tinfl miniz_tdef; do
    cc -ffunction-sections -fdata-sections -I"$vendor" -c "$vendor/$source.c" -o "$out/$source.o"
    objects+=("$out/$source.o")
done
sources=()
for source in app/src/*.cpp; do
    case "$source" in
        app/src/ui.cpp|app/src/player.cpp|app/src/main.cpp|app/src/pad.cpp|app/src/keyboard.cpp) ;;
        *) sources+=("$source");;
    esac
done
c++ -std=c++20 -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -DMOVIAN_HOST_PREVIEW -DMOVIAN_HAVE_DUKTAPE -DMOVIAN_HAVE_SMB2 -DMOVIAN_HAVE_FFMPEG \
    -Iapp/src -I"$vendor" -I.deps/ffmpeg-ps5/include -I.deps/smb2-ps5/include \
    tests/ui_stpp.cpp "${sources[@]}" "${objects[@]}" "$out/stb.o" \
    "${archives[@]}" "$smb/lib/libsmb2.a" -l:libcrypto.so.3 -o "$out/ui-stpp" >"$out/build.log" 2>&1 ||
    { tail -60 "$out/build.log"; exit 1; }
# Every invocation receives disposable state, never the normal title profile.
run=$(mktemp -d "$out/run.XXXXXX")
trap 'rm -rf -- "$run"' EXIT
cd "$run"
python3 - <<'PY'
import json,zipfile
script="""var store=require('movian/store').create('retention');store.marker='saved';
require('movian/service').create('Removal fixture','fixture:start','video',true);
new (require('movian/page').Route)('fixture:start',function(p){p.appendItem('fixture:child','directory',{title:'Child'});});
"""
for name in ('remove-local','remove-remote','keep-fixture'):
    with zipfile.ZipFile(name+'.zip','w',zipfile.ZIP_DEFLATED) as package:
        package.writestr('plugin.json',json.dumps(dict(apiversion=2,id=name,title='Removal fixture',
            author='Local test',synopsis='Disposable plugin management observation',version='1.0.0',file='main.js')))
        package.writestr('main.js',script)
PY
"$out/ui-stpp" "$root/app/fonts/" "$@"
