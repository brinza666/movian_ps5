# Releases

GitHub Releases for this repository. **Old tags stay published.** Never
delete a previous Movian PS5 release to make room for a new one.

https://github.com/brinza666/movian_ps5/releases

| Tag | Title | What it is |
| --- | --- | --- |
| `pre-alpha` | SPLICER pre-alpha | Archived elfldr payload — not the home-screen title |
| `fw403-20261030` | 4.03 linear test ELFs | Archived loader tests |
| `shadowmount` | ShadowMount title | Early folder-title experiment |
| `01.000.000` | Movian PS5 v01.000.000 | Pre-reset binary (stubs / old claims) |
| `v0.3.0` | MPEG-1 player | Native title: Videos + software MPEG-1 |
| `v0.4.0` | Options / resume / bookmarks | Native title tested after MPEG-1 |
| `v0.5.0` | Playlist / auto-next | L1/R1 prev/next, newest-first, file info |

`v0.4.0` is the last **user-tested** native title. `v0.5.0` is the next
console build (playlist). Install the matching
`MovianPS5-v0.X.Y.zip` folder as `/data/homebrew/PPSA99455/`.

To publish the next version (do not `--clobber` old assets, do not
`gh release delete`):

```bash
gh release create v0.X.Y build/app/MovianPS5-v0.X.Y.zip \
  --title "Movian PS5 v0.X.Y" \
  --notes-file docs/release-notes/v0.X.Y.md
```
