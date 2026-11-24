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
title_access_test=${MOVIAN_TITLE_ACCESS_TEST:-0}
[[ $title_access_test == 0 || $title_access_test == 1 ]] || {
    echo "MOVIAN_TITLE_ACCESS_TEST must be 0 or 1" >&2; exit 2;
}
title=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' \
    "$here/sce_sys/param.json")
if [[ $title_access_test == 1 && $title != PPSA99456 ]]; then
    echo "Title access support is restricted to test title PPSA99456" >&2; exit 2
fi

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
# STPP uses the public SDK's string implementation. Keep the kit allocation
# runtime, use pthreads in the server, and discard unused SDK library sections.
mkdir -p "$kit/movian-cxx"
for library in libc++ libc++abi libunwind; do
    cp "$sdk/target/lib/$library.a" "$kit/movian-cxx/${library//+/x}.a"
done
python3 - "$kit/tools/build.sh" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]); text=p.read_text()
text=text.replace('--eh-frame-hdr \\\n', '--eh-frame-hdr --gc-sections \\\n')
if '--eh-frame-hdr --gc-sections -L' not in text:
    text=text.replace('--eh-frame-hdr --gc-sections',
                      '--eh-frame-hdr --gc-sections -L "$sdk_root/target/lib"')
# Resolve existing platform C/ABI symbols before taking SDK archive members.
# This matches the public SDK's application link order and avoids pulling a
# separate unwinder into the kit's entry/runtime.
tail='--as-needed "${stub_paths[@]}" "$sdk_root"/target/lib/*.so'
if tail+' "$root"/movian-cxx' not in text:
    text=text.replace(tail,tail+' "$root"/movian-cxx/libcxx.a "$root"/movian-cxx/libcxxabi.a "$root"/movian-cxx/libunwind.a')
needle='ninja_edge LINK "$build/llvm-pie.elf"'
if 'ninja_inputs+=("$root"/movian-cxx/libcxx.a' not in text:
    text=text.replace(needle,'ninja_inputs+=("$root"/movian-cxx/libcxx.a "$root"/movian-cxx/libcxxabi.a "$root"/movian-cxx/libunwind.a)\n'+needle)
p.write_text(text)
PY
# Console-only authorization files must never enter the public title or ZIP.
if find "$here/sce_sys" "$here/assets" -type f \
    \( -name 'stpp-title-access-test.request' -o -name 'stpp-fresh-profile.request' \) \
    -print -quit 2>/dev/null | grep -q .; then
    echo "Remove console-only STPP request files from packaged assets" >&2; exit 2
fi
cp "$here/fonts/"* "$kit/movian/assets/fonts/"
[[ ! -d $here/assets ]] || cp -r "$here/assets/." "$kit/movian/assets/"

if [[ $title_access_test == 1 ]]; then
    client=$kit/movian/src/starter_elevation
    mkdir -p "$client/src"
    cp "$starter/kit/examples/sandbox-elevation/elevation.hpp" \
       "$starter/kit/examples/sandbox-elevation/protocol.hpp" "$client/"
    cp "$starter/kit/examples/sandbox-elevation/src/elevation.cpp" "$client/src/"
    python3 "$repo/scripts/adapt-title-access-client.py" "$client/src/elevation.cpp"
    # The installed builder's fork URL is unavailable. Retain its pinned SDK,
    # logging header, protocol and manifest checks; replace source acquisition
    # with the reviewed exact-commit primary archive and its SHA-256.
    python3 - "$starter/kit/tools/build-lapy-helper.py" "$kit/tools/build-lapy-helper.py" <<'PY'
from pathlib import Path
import sys
text = Path(sys.argv[1]).read_text()
start = text.index("def fetch_lapy():")
end = text.index("\ndef fetch_sdk():", start)
replacement = '''def fetch_lapy():
    archive = CACHE / "lapy-57fcc19b7f57e46c823d6f2a6c6233ebf3288079.zip"
    download("https://codeload.github.com/mpereiraesaa/PS5-Lapy-JB-Daemon/zip/" + LAPY_COMMIT,
             archive, "c87cf02eceb1c004461f268de9657f7f17a367a4bbf3acfa2c6b7f86c90ae152")
    prefix = "PS5-Lapy-JB-Daemon-" + LAPY_COMMIT
    with zipfile.ZipFile(archive) as source:
        for member in source.infolist():
            parts = Path(member.filename).parts
            if not parts or parts[0] != prefix or ".." in parts:
                raise RuntimeError("unexpected pinned Lapy archive member")
            if member.is_dir():
                continue
            target = LAPY.joinpath(*parts[1:])
            expected = source.read(member)
            if target.exists():
                if not target.is_file() or target.read_bytes() != expected:
                    raise RuntimeError("pinned Lapy source has local changes: " + str(target))
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(expected)

'''
Path(sys.argv[2]).write_text(text[:start] + replacement + text[end:])
PY
fi

duk_archive=$repo/.deps/duktape-2.7.0.tar.xz
duk_sha=90f8d2fa8b5567c6899830ddef2c03f3c27960b11aca222fa17aa7ac613c2890
mkdir -p "$repo/.deps" "$kit/movian/assets/licenses"
if [[ ! -f $duk_archive ]]; then
    curl -fsSL https://duktape.org/duktape-2.7.0.tar.xz -o "$duk_archive"
fi
echo "$duk_sha  $duk_archive" | sha256sum -c -
tar -xf "$duk_archive" -C "$kit/movian/src" --strip-components=2 \
    duktape-2.7.0/src/duktape.c duktape-2.7.0/src/duktape.h duktape-2.7.0/src/duk_config.h
# The PS5 SDK lacks strptime. Duktape still parses ECMAScript ISO dates.
sed -i '/^#define DUK_USE_DATE_PRS_STRPTIME$/s/define/undef/' "$kit/movian/src/duk_config.h"
sed -i \
    -e 's/^#undef DUK_USE_INTERRUPT_COUNTER$/#define DUK_USE_INTERRUPT_COUNTER/' \
    -e 's/^#undef DUK_USE_EXEC_TIMEOUT_CHECK$/#ifdef __cplusplus\nextern "C" int movian_plugin_timed_out(void *);\n#else\nextern int movian_plugin_timed_out(void *);\n#endif\n#define DUK_USE_EXEC_TIMEOUT_CHECK(data) movian_plugin_timed_out(data)/' \
    "$kit/movian/src/duk_config.h"
tar -xOf "$duk_archive" duktape-2.7.0/LICENSE.txt > "$kit/movian/assets/licenses/duktape.txt"

miniz_archive=$repo/.deps/miniz-3.0.2.tar.gz
miniz_sha=c4b4c25a4eb81883448ff8924e6dba95c800094a198dc9ce66a292ac2ef8e018
if [[ ! -f $miniz_archive ]]; then
    curl -fsSL https://codeload.github.com/richgel999/miniz/tar.gz/refs/tags/3.0.2 -o "$miniz_archive"
fi
echo "$miniz_sha  $miniz_archive" | sha256sum -c -
tar -xf "$miniz_archive" -C "$kit/movian/src" --strip-components=1 --wildcards \
    'miniz-3.0.2/miniz*.c' 'miniz-3.0.2/miniz*.h'
cat > "$kit/movian/src/miniz_export.h" <<'EOF'
#define MINIZ_EXPORT
#define MINIZ_NO_ZLIB_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
EOF
# The release C runtime does not export the debug assert helper.
sed -i '/^#define MZ_ASSERT(x) assert(x)$/c\#define MZ_ASSERT(x) ((void)0)' "$kit/movian/src/miniz_common.h"
tar -xOf "$miniz_archive" miniz-3.0.2/LICENSE > "$kit/movian/assets/licenses/miniz.txt"

gumbo_archive=$repo/.deps/gumbo-parser-0.14.1.tar.gz
gumbo_sha=ba5d13b9b508ec693613b3b61518163aced38f8e885f7e28dc047348a4e61365
if [[ ! -f $gumbo_archive ]]; then
    curl -fsSL https://codeberg.org/gumbo-parser/gumbo-parser/archive/0.14.1.tar.gz -o "$gumbo_archive"
fi
echo "$gumbo_sha  $gumbo_archive" | sha256sum -c -
gumbo_src=$kit/movian/src/gumbo
mkdir -p "$gumbo_src"
tar -xf "$gumbo_archive" -C "$gumbo_src" --strip-components=2 --wildcards \
    'gumbo-parser/src/*.c' 'gumbo-parser/src/*.h'
patch -d "$gumbo_src" -p1 < "$here/patches/gumbo-limits.patch"
for file in "$gumbo_src/"*.c "$gumbo_src/"*.h; do
    sed -i '/^#include <assert.h>$/i #define NDEBUG 1' "$file"
done
tar -xOf "$gumbo_archive" gumbo-parser/doc/COPYING > "$kit/movian/assets/licenses/gumbo.txt"
bash "$kit/tools/validate-assets.sh" "$kit/movian/sce_sys"

# Link-only stub: SDK has no libSceCommonDialog (needed later for IME).
PS5_PAYLOAD_SDK=$sdk sh "$kit/tooling/prospero-clang18" -O2 -Wall -Wextra -fPIC \
    -c "$here/stubs/common_dialog.c" -o "$kit/movian/stubs/common_dialog.o"
"$sdk/bin/prospero-lld" --shared -soname libSceCommonDialog.sprx \
    -o "$kit/movian/stubs/libSceCommonDialog.so" "$kit/movian/stubs/common_dialog.o"

cp "$sdk/target/lib/libSceSysmodule.so" "$kit/movian/stubs/libSceSysmodule.so"
cp "$sdk/target/lib/libSceSsl.so" "$kit/movian/stubs/libSceSsl.so"

PS5_PAYLOAD_SDK=$sdk sh "$kit/tooling/prospero-clang18" -O2 -Wall -Wextra -fPIC \
    -c "$here/stubs/videodec2_link_stub.c" -o "$kit/movian/stubs/videodec2_link_stub.o"
"$sdk/bin/prospero-lld" --shared -soname libSceVideodec2.prx \
    -o "$kit/movian/stubs/libSceVideodec2.so" "$kit/movian/stubs/videodec2_link_stub.o"

ffmpeg_prefix=$repo/.deps/ffmpeg-ps5
vars=(APP_SOURCE_DIR=movian/src APP_PARAM=movian/sce_sys/param.json
      APP_SCE_SYS=movian/sce_sys APP_ASSETS=movian/assets
      APP_LAPY_HELPER="$title_access_test"
      APP_IMPORT_STUBS="movian/stubs/libSceCommonDialog.so movian/stubs/libSceSysmodule.so movian/stubs/libSceVideodec2.so movian/stubs/libSceSsl.so")
definitions="MOVIAN_HAVE_DUKTAPE MOVIAN_HAVE_GUMBO"
if [[ $title_access_test == 1 ]]; then
    definitions+=" MOVIAN_TITLE_ACCESS_LAPY"
    vars+=(APP_WRAP_SYMBOLS="sceSysmoduleLoadModule sceSysmoduleUnloadModule")
    # Keep the exact reviewed source in the public artifact alongside existing
    # helper MIT license/manifest and original GPL client headers.
    python3 "$kit/tools/build-lapy-helper.py" "$title" "$kit/build/lapy-helper/$title"
    cp "$kit/.deps/lapy/lapy-57fcc19b7f57e46c823d6f2a6c6233ebf3288079.zip" \
        "$kit/movian/assets/licenses/lapy-source-57fcc19.zip"
    cp "$starter/kit/THIRD_PARTY_NOTICES.md" "$kit/movian/assets/licenses/starter-notices.txt"
fi
if [[ -f $ffmpeg_prefix/lib/libavcodec.a && -f $ffmpeg_prefix/lib/libavformat.a && -f $ffmpeg_prefix/lib/libavutil.a ]]; then
    echo "Linking static FFmpeg from $ffmpeg_prefix"
    rm -rf "$kit/ffmpeg-ps5"
    mkdir -p "$kit/ffmpeg-ps5"
    cp -a "$ffmpeg_prefix/include" "$kit/ffmpeg-ps5/include"
    mkdir -p "$kit/ffmpeg-ps5/lib"
    cp -a "$ffmpeg_prefix/lib/"*.a "$kit/ffmpeg-ps5/lib/"
    archives="ffmpeg-ps5/lib/libavformat.a ffmpeg-ps5/lib/libavcodec.a ffmpeg-ps5/lib/libavutil.a"
    archives+=" ffmpeg-ps5/lib/libswresample.a ffmpeg-ps5/lib/libswscale.a"
    [[ ! -f $kit/ffmpeg-ps5/lib/libdav1d.a ]] || archives+=" ffmpeg-ps5/lib/libdav1d.a"
    definitions+=" MOVIAN_HAVE_FFMPEG MOVIAN_TORRENT_DHT"
    vars+=(APP_INCLUDE_PATHS=ffmpeg-ps5/include
           APP_STATIC_ARCHIVES="$archives")
fi
smb_prefix=$repo/.deps/smb2-ps5
if [[ ! -f $smb_prefix/lib/libsmb2.a ]]; then
    PS5_PAYLOAD_SDK=$sdk bash "$repo/scripts/build-smb2-ps5.sh"
fi
mkdir -p "$kit/smb2-ps5"
cp -a "$smb_prefix/include" "$smb_prefix/lib" "$kit/smb2-ps5/"
cp "$smb_prefix/LICENSE.txt" "$kit/movian/assets/licenses/libsmb2.txt"
definitions+=" MOVIAN_HAVE_SMB2"
vars+=(APP_INCLUDE_PATHS="ffmpeg-ps5/include smb2-ps5/include"
       APP_STATIC_ARCHIVES="$archives smb2-ps5/lib/libsmb2.a ")
vars+=(APP_DEFINITIONS="$definitions")
if [[ -n $deploy_host ]]; then
    make -C "$kit" "${vars[@]}" deploy PS5_HOST="$deploy_host"
else
    make -C "$kit" "${vars[@]}"
fi

rm -rf "$repo/build/app"
mkdir -p "$repo/build/app"
cp -r "$kit/dist/$title" "$kit/dist/$title.zip" "$repo/build/app/"
echo
echo "App folder: $repo/build/app/$title"
echo "ZIP:        $repo/build/app/$title.zip"
echo "Install:    copy the folder to /data/homebrew/$title on the console"
