#!/usr/bin/env bash
# Read-only check that this workstation can build and deploy Movian for PS5.
set -euo pipefail

ok=0
warn=0
fail() { echo "FAIL  $*"; ok=1; }
note() { echo "OK    $*"; }
soft() { echo "WARN  $*"; warn=1; }

echo "== Movian PS5 environment doctor =="

if command -v clang-18 >/dev/null; then
    note "clang-18 $(clang-18 --version | head -1)"
else
    fail "clang-18 missing (WSL Ubuntu 24.04: sudo apt install clang-18 lld-18)"
fi
command -v make >/dev/null && note "make $(make --version | head -1)" || fail "make missing"
command -v python3 >/dev/null && note "python3 $(python3 --version)" || fail "python3 missing"

sdk=${PS5_PAYLOAD_SDK:-$HOME/.ps5-starter/ps5-payload-sdk}
if [[ -x $sdk/bin/prospero-lld ]]; then
    note "PS5_PAYLOAD_SDK=$sdk"
else
    fail "payload SDK not at $sdk"
fi

starter=${PS5_STARTER:-/mnt/c/Users/brinza/Documents/deep/ps5-homebrew-starter}
[[ -d $starter/kit ]] && note "starter kit $starter" || fail "ps5-homebrew-starter missing"

host=${PS5_HOST:-192.168.10.238}
echo
echo "== Console $host =="
if timeout 2 bash -c "echo >/dev/tcp/$host/2121" 2>/dev/null; then
    note "FTP 2121 open"
else
    soft "FTP 2121 closed (ftpsrv / etaHEN not running?)"
fi
if timeout 2 bash -c "echo >/dev/tcp/$host/3232" 2>/dev/null; then
    note "klog 3232 open"
else
    soft "klog 3232 closed"
fi
if timeout 2 bash -c "echo >/dev/tcp/$host/10101" 2>/dev/null; then
    note "ShadowMount API 10101 open"
else
    soft "ShadowMount 10101 closed"
fi
if timeout 2 bash -c "echo >/dev/tcp/$host/9021" 2>/dev/null; then
    note "elfldr 9021 open (optional; titles do not use this)"
else
    soft "elfldr 9021 closed (expected for ShadowMount titles)"
fi

echo
if [[ $ok -eq 0 ]]; then
    echo "Doctor finished with $warn warning(s). Ready to build: bash app/build.sh"
    exit 0
fi
echo "Doctor found failures. Fix them before building."
exit 1
