#!/usr/bin/env bash
# Pinned libsmb2 client. No kernel or sandbox patches.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
sdk=${PS5_PAYLOAD_SDK:-$HOME/.ps5-starter/ps5-payload-sdk}
rev=ef32cff6cdda24279eaac802f468d9290a2c8b5d
sha=866fc771053e74e8be56fef91b7c4d3abc8c32e5cb89ee0ec68a126721ef1f57
work=${MOVIAN_SMB2_BUILD_ROOT:-$HOME/.movian-smb2}
src=$work/src-$rev
build=$work/ps5-$rev
mkdir -p "$work" "$root/.deps/smb2-ps5"
archive=$work/libsmb2-$rev.tar.gz
[[ -f $archive ]] || curl -fsSL "https://codeload.github.com/sahlberg/libsmb2/tar.gz/$rev" -o "$archive"
echo "$sha  $archive" | sha256sum -c -
if [[ ! -d $src ]]; then
    mkdir "$src"
    tar -xf "$archive" -C "$src" --strip-components=1
fi
# PS5 title sockets require SO_NBIO, as in our torrent/diagnostic transports.
python3 - "$src/lib/socket.c" <<'PY'
import sys
from pathlib import Path
p=Path(sys.argv[1]); s=p.read_text()
if '#define getaddrinfo movian_net_getaddrinfo' not in s:
    s='#ifdef __PROSPERO__\n#define getaddrinfo movian_net_getaddrinfo\n#define freeaddrinfo movian_net_freeaddrinfo\n#define gai_strerror movian_net_gai_strerror\n#endif\n'+s
if 'setsockopt(fd, SOL_SOCKET, 0x1200' not in s:
    s=s.replace('v = fcntl(fd, F_GETFL, 0);', '''#ifdef __PROSPERO__
        int one = 1;
        setsockopt(fd, SOL_SOCKET, 0x1200, &one, sizeof(one));
#else
        v = fcntl(fd, F_GETFL, 0);''').replace('fcntl(fd, F_SETFL, v | O_NONBLOCK);','fcntl(fd, F_SETFL, v | O_NONBLOCK);\n#endif')
p.write_text(s)
PY
python3 "$root/scripts/patch-smb2-random.py" "$src"
cmake -S "$src" -B "$build" \
    -DCMAKE_SYSTEM_NAME=FreeBSD -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DCMAKE_C_COMPILER="$sdk/bin/prospero-clang" -DCMAKE_AR="$sdk/bin/prospero-ar" \
    -DCMAKE_RANLIB="$sdk/bin/prospero-ranlib" -DCMAKE_C_FLAGS="-O2 -fPIC -D__PROSPERO__ -DSOL_TCP=6" \
    -DCMAKE_INSTALL_PREFIX="$root/.deps/smb2-ps5" -DCMAKE_INSTALL_LIBDIR=lib \
    -DBUILD_SHARED_LIBS=OFF -DENABLE_EXAMPLES=OFF -DENABLE_UTILS=OFF \
    -DENABLE_LIBKRB5=OFF -DENABLE_GSSAPI=OFF -DENABLE_LIBDCERPC=OFF \
    -DENABLE_LIBQUIC=OFF -DENABLE_RDMA=OFF
cmake --build "$build" -j"$(nproc)"
cmake --install "$build"
cp "$src/LICENCE-LGPL-2.1.txt" "$root/.deps/smb2-ps5/LICENSE.txt"
