#!/bin/sh
# Copy the PS5 platform layer into a checkout of https://github.com/czz/movian-go
# Usage: ./apply.sh /path/to/movian-go
set -eu

if [ $# -ne 1 ] || [ ! -f "$1/go.mod" ]; then
	echo "usage: $0 /path/to/movian-go" >&2
	exit 1
fi

DEST=$(CDPATH= cd -- "$1" && pwd)
HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

if ! grep -q 'github.com/czz/movian-go' "$DEST/go.mod"; then
	echo "$DEST does not look like movian-go" >&2
	exit 1
fi

copy() {
	rel=$1
	mkdir -p "$DEST/$(dirname "$rel")"
	cp "$HERE/overlay/$rel" "$DEST/$rel"
	echo "copied $rel"
}

copy cmd/movian-go/init_ps5.go
copy cmd/movian-go/render_ps5.go
copy cmd/movian-go/ui_glw_ps5.go
copy internal/arch/avtime_ps5.go
copy internal/arch/ps5/pad.go
copy internal/arch/ps5/display.go
copy internal/arch/ps5/ps5_display.c
copy internal/arch/ps5/ps5_display.h
cp "$HERE/ps5.mk" "$DEST/ps5.mk"
mkdir -p "$DEST/scripts"
cp "$HERE/scripts/build_sdl_ps5.sh" "$DEST/scripts/build_sdl_ps5.sh"
cp "$HERE/scripts/build_ffmpeg_ps5.sh" "$DEST/scripts/build_ffmpeg_ps5.sh"
chmod +x "$DEST/scripts/build_sdl_ps5.sh" "$DEST/scripts/build_ffmpeg_ps5.sh"

patch_line() {
	file=$1
	old=$2
	new=$3
	if grep -qF "$new" "$file"; then
		echo "already patched ${file#$DEST/}"
		return
	fi
	if ! grep -qF "$old" "$file"; then
		echo "pattern missing in ${file#$DEST/}" >&2
		echo "  wanted: $old" >&2
		exit 1
	fi
	# shell escape for sed: use a temp file instead
	tmp=$(mktemp)
	awk -v old="$old" -v new="$new" '
		!done && index($0, old) == 1 { print new; done=1; next }
		{ print }
	' "$file" > "$tmp"
	mv "$tmp" "$file"
	echo "patched ${file#$DEST/}"
}

patch_line "$DEST/internal/arch/linux_main.go" \
	"//go:build linux || darwin || windows" \
	"//go:build linux || darwin || windows || ps5"

patch_line "$DEST/cmd/movian-go/render.go" \
	"//go:build !(linux && rpi) && !(linux && sunxi)" \
	"//go:build !(linux && rpi) && !(linux && sunxi) && !ps5"

patch_line "$DEST/cmd/movian-go/ui_glw_stub.go" \
	"//go:build !((linux || darwin || windows) && glfw) && !android && !(linux && rpi) && !(linux && sunxi)" \
	"//go:build !((linux || darwin || windows) && glfw) && !android && !(linux && rpi) && !(linux && sunxi) && !ps5"

insert_cgo() {
	file=$1
	line=$2
	if grep -qF "$line" "$file"; then
		return
	fi
	tmp=$(mktemp)
	awk -v line="$line" '
		!done && $0 == "*/" { print line; done=1 }
		{ print }
	' "$file" > "$tmp"
	mv "$tmp" "$file"
	echo "cgo + ${file#$DEST/}"
}

# freebsd,amd64 lines are ignored by every other movian target.
insert_cgo "$DEST/internal/libav/cgo_directives.go" \
	'#cgo freebsd,amd64 CFLAGS: -I${SRCDIR}/../../ffmpeg/ps5/include'
insert_cgo "$DEST/internal/libav/cgo_directives.go" \
	'#cgo freebsd,amd64 LDFLAGS: -L${SRCDIR}/../../ffmpeg/ps5/lib -Wl,--start-group -lavcodec -lavformat -lavutil -lswscale -lswresample -Wl,--end-group -lm -lz -lpthread'

insert_cgo "$DEST/internal/media/libav/cgo_directives.go" \
	'#cgo freebsd,amd64 CFLAGS: -I${SRCDIR}/../../../ffmpeg/ps5/include'
insert_cgo "$DEST/internal/media/libav/cgo_directives.go" \
	'#cgo freebsd,amd64 LDFLAGS: -L${SRCDIR}/../../../ffmpeg/ps5/lib -Wl,--start-group -lavcodec -lavformat -lavutil -lswresample -lswscale -Wl,--end-group -lm -lz -lpthread'

insert_cgo "$DEST/internal/image/cgo_directives.go" \
	'#cgo freebsd,amd64 CFLAGS: -I${SRCDIR}/../../ffmpeg/ps5/include'
insert_cgo "$DEST/internal/image/cgo_directives.go" \
	'#cgo freebsd,amd64 LDFLAGS: -L${SRCDIR}/../../ffmpeg/ps5/lib -Wl,--start-group -lavcodec -lavutil -lswscale -lswresample -Wl,--end-group -lm -lz -lpthread'

insert_cgo "$DEST/internal/api/screenshot/screenshot_cgo.go" \
	'#cgo freebsd,amd64 CFLAGS: -I${SRCDIR}/../../../ffmpeg/ps5/include'
insert_cgo "$DEST/internal/api/screenshot/screenshot_cgo.go" \
	'#cgo freebsd,amd64 LDFLAGS: -L${SRCDIR}/../../../ffmpeg/ps5/lib -Wl,--start-group -lavcodec -lavutil -lswscale -Wl,--end-group -lm -lz -lpthread'

insert_cgo "$DEST/internal/ui/glw/z_cgo_gl.go" \
	"#cgo freebsd,ps5 LDFLAGS: -lOSMesa"

if ! grep -q '^-include ps5.mk' "$DEST/Makefile"; then
	printf '\n-include ps5.mk\n' >> "$DEST/Makefile"
	echo "Makefile includes ps5.mk"
fi

echo
echo "Applied. Next, on a machine with the payload SDK:"
echo "  export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk"
echo "  make ps5-sdl && make ps5-ffmpeg && make ps5"
echo "That ELF still needs the Go runtime patch in the port's runtime/ directory."
