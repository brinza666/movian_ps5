#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT

python3 tests/stpp_protocol.py
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -DMOVIAN_HOST_PREVIEW -Iapp/src \
    tests/plugin_uninstall.cpp -o "$scratch/plugin-uninstall"
(mkdir -p "$scratch/plugin-work" && cd "$scratch/plugin-work" && ../plugin-uninstall)
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -DMOVIAN_HOST_PREVIEW -Iapp/src \
    app/src/profile_reset.cpp app/src/title_access_io.cpp app/src/fs.cpp tests/profile_reset.cpp -o "$scratch/profile-reset"
(mkdir -p "$scratch/profile-work" && cd "$scratch/profile-work" && ../profile-reset)
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pthread -DMOVIAN_STPP_TEST -Iapp/src \
    tests/stpp_server.cpp app/src/stpp.cpp -l:libcrypto.so.3 -o "$scratch/stpp-server"
"$scratch/stpp-server"
starter=${PS5_STARTER:-/mnt/c/Users/brinza/Documents/deep/ps5-homebrew-starter}
if [[ -f $starter/kit/tests/test_elevation.cpp ]]; then
    python3 tests/title_access_client.py "$starter" "$scratch" "${CXX:-c++}"
fi

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -DMOVIAN_HAVE_SMB2 -Iapp/src \
    tests/input_file.cpp -o "$scratch/input-file"
"$scratch/input-file"

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pthread -Iapp/src \
    app/src/plugin_http.cpp tests/plugin_http_native.cpp -o "$scratch/plugin-http-native"
"$scratch/plugin-http-native"

"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200112L -Wall -Wextra -Werror -DMOVIAN_NETWORK_POSIX_TEST \
    app/src/network_posix.c tests/network_posix.c -o "$scratch/network-posix"
"$scratch/network-posix"

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pthread -DMOVIAN_HOST_PREVIEW -Iapp/src \
    app/src/diagnostics.cpp tests/diagnostics.cpp -o "$scratch/diagnostics"
(mkdir -p "$scratch/diagnostics-work/download0" && cd "$scratch/diagnostics-work" && ../diagnostics)

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -DMOVIAN_HOST_PREVIEW -Iapp/src \
    app/src/settings.cpp tests/settings_clock.cpp -o "$scratch/settings-clock"
"$scratch/settings-clock"

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

# Optional real SMB2/FTP service fixtures, with disposable accounts and data.
if [[ -n ${MOVIAN_HOST_NETWORK_FFMPEG_DIR:-} && -n ${MOVIAN_HOST_SMB2_DIR:-} ]]; then
    package_flags=() package_objects=()
    if [[ -n ${MOVIAN_HOST_PLUGIN_HEADERS:-} ]]; then
        package_flags=(-DMOVIAN_HAVE_DUKTAPE -I"$MOVIAN_HOST_PLUGIN_HEADERS")
        for source in duktape miniz miniz_zip miniz_tinfl miniz_tdef; do
            "${CC:-cc}" -ffunction-sections -fdata-sections -I"$MOVIAN_HOST_PLUGIN_HEADERS" \
                -c "$MOVIAN_HOST_PLUGIN_HEADERS/$source.c" -o "$scratch/$source.o"
            package_objects+=("$scratch/$source.o")
        done
    fi
    "${CXX:-c++}" -std=c++20 -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -DMOVIAN_HOST_PREVIEW -DMOVIAN_HAVE_FFMPEG -DMOVIAN_HAVE_SMB2 "${package_flags[@]}" \
        -Iapp/src -I.deps/ffmpeg-ps5/include -I.deps/smb2-ps5/include tests/network.cpp \
        app/src/network.cpp app/src/network_ftp_server.cpp app/src/fs.cpp app/src/diagnostics.cpp \
        app/src/torrent.cpp app/src/plugins.cpp "${package_objects[@]}" \
        "$MOVIAN_HOST_NETWORK_FFMPEG_DIR/libavformat/libavformat.a" \
        "$MOVIAN_HOST_NETWORK_FFMPEG_DIR/libavcodec/libavcodec.a" \
        "$MOVIAN_HOST_NETWORK_FFMPEG_DIR/libavutil/libavutil.a" \
        "$MOVIAN_HOST_SMB2_DIR/lib/libsmb2.a" -lm -o "$scratch/network"
    "${MOVIAN_NETWORK_TEST_PYTHON:-python3}" tests/network_services.py "$scratch/network"
    if [[ -n ${MOVIAN_HOST_PLUGIN_HEADERS:-} ]]; then
        bash scripts/test-ui-stpp.sh
    fi
fi
