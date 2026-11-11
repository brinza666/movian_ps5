#!/usr/bin/env bash
# Publish a Movian PS5 GitHub release WITHOUT deleting older ones.
#
#   scripts/release.sh v0.5.0 "Movian PS5 v0.5.0 — Playlist"
#
# Copies build/app/PPSA99455.zip to MovianPS5-<tag>.zip, creates an
# annotated tag if missing, and runs `gh release create` (no --clobber).
set -euo pipefail
tag=${1:-}
title=${2:-}
if [[ -z $tag || -z $title ]]; then
    echo "usage: scripts/release.sh v0.X.Y \"title\"" >&2
    exit 2
fi
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
zip_src=$repo/build/app/PPSA99455.zip
zip_dst=$repo/build/app/MovianPS5-$tag.zip
notes=$repo/docs/release-notes/$tag.md
[[ -f $zip_src ]] || { echo "missing $zip_src — build first" >&2; exit 1; }
[[ -f $notes ]] || { echo "missing $notes" >&2; exit 1; }
cp -f "$zip_src" "$zip_dst"
cd "$repo"
if ! git rev-parse "$tag" >/dev/null 2>&1; then
    git tag -a "$tag" -m "$title"
    git push origin "$tag"
fi
if gh release view "$tag" >/dev/null 2>&1; then
    echo "release $tag already exists — not deleting it; upload skipped" >&2
    gh release view "$tag"
    exit 0
fi
gh release create "$tag" --title "$title" --notes-file "$notes" \
    "$zip_dst#MovianPS5-$tag.zip"
echo "published $tag (older releases were not touched)"
