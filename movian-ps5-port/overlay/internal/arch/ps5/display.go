//go:build ps5

// SPDX-License-Identifier: GPL-3.0-or-later

package ps5

/*
#cgo pkg-config: sdl2
#include "ps5_display.h"
#include <stdlib.h>
*/
import "C"
import (
	"unsafe"
)

// Open brings up the SDL software framebuffer (sceVideoOut underneath).
// width/height 0 picks 1920x1080. Call from the OS thread that will Present.
func Open(width, height int) error {
	if C.movian_ps5_display_open(C.int(width), C.int(height)) != 0 {
		return errDisplay
	}
	return nil
}

// Close shuts SDL down. Same thread as Open.
func Close() { C.movian_ps5_display_close() }

// SetStatus replaces the text page. Newlines break lines.
func SetStatus(text string) {
	cs := C.CString(text)
	defer C.free(unsafe.Pointer(cs))
	C.movian_ps5_status(cs)
}

// Present flips the status page.
func Present() { C.movian_ps5_present() }

// Poll reads one press. ok is false when the queue is empty.
func Poll() (code int, ok bool) {
	var c C.int
	if C.movian_ps5_poll(&c) == 0 {
		return 0, false
	}
	return int(c), true
}

type displayError string

func (e displayError) Error() string { return string(e) }

const errDisplay displayError = "ps5: SDL display failed to open (is the PS5 SDL2 port installed in the payload SDK sysroot?)"
