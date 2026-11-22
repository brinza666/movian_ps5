#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pthread -DMOVIAN_HOST_PREVIEW -Iapp/src \
    app/src/diagnostics.cpp tests/diagnostics.cpp -o "$scratch/diagnostics"
(mkdir -p "$scratch/diagnostics-work/download0" && cd "$scratch/diagnostics-work" && ../diagnostics)

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -DMOVIAN_HOST_PREVIEW -Iapp/src \
    app/src/settings.cpp tests/torrent_settings.cpp -o "$scratch/torrent-settings"
(mkdir -p "$scratch/settings-work" && cd "$scratch/settings-work" && ../torrent-settings)

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pthread -DMOVIAN_HOST_PREVIEW -Iapp/src \
    app/src/torrent_stream.cpp tests/torrent_piece_cache.cpp -o "$scratch/piece-cache"
(cd "$scratch" && ./piece-cache)
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pthread -DMOVIAN_HOST_PREVIEW \
    -DMOVIAN_TORRENT_TEST -DMOVIAN_TORRENT_TEST_TRACKER -DMOVIAN_TORRENT_DHT -Iapp/src app/src/torrent.cpp \
    app/src/torrent_peer.cpp app/src/torrent_stream.cpp app/src/diagnostics.cpp tests/torrent_multi_peer.cpp \
    -l:libcrypto.so.3 -o "$scratch/multi-peer"
python3 tests/torrent_multi_peer.py "$scratch/multi-peer"
if [[ -n ${MOVIAN_HOST_PLUGIN_HEADERS:-} ]]; then
    "${CXX:-c++}" -std=c++20 -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -DMOVIAN_HAVE_DUKTAPE -DMOVIAN_HOST_PREVIEW -Iapp/src -I"$MOVIAN_HOST_PLUGIN_HEADERS" \
        app/src/plugins.cpp app/src/diagnostics.cpp tests/kinozal_login.cpp -o "$scratch/kinozal-login"
    (cd "$scratch" && ./kinozal-login)
fi
if [[ -n ${MOVIAN_HOST_FFMPEG_DIR:-} && -n ${MOVIAN_MATROSKA_FIXTURE:-} ]]; then
    ffmpeg=$MOVIAN_HOST_FFMPEG_DIR
    "${CXX:-c++}" -std=c++20 -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -DMOVIAN_HAVE_FFMPEG -Iapp/src -I.deps/ffmpeg-ps5/include tests/torrent_matroska.cpp \
        app/src/torrent_stream.cpp app/src/diagnostics.cpp "$ffmpeg/libavformat/libavformat.a" \
        "$ffmpeg/libavcodec/libavcodec.a" "$ffmpeg/libavutil/libavutil.a" -lm -o "$scratch/matroska"
    timeout 25s "$scratch/matroska" "$MOVIAN_MATROSKA_FIXTURE"
fi
if command -v node >/dev/null; then
    node --check app/assets/plugins/host.js
else
    node.exe --check app/assets/plugins/host.js
fi
bash -n app/build.sh
