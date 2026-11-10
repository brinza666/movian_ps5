//go:build ps5

// SPDX-License-Identifier: GPL-3.0-or-later

package arch

import "time"

// GetAvtime — microseconds since process start, monotonic.
// The linux CLOCK_MONOTONIC path lives in avtime_linux.go and is not
// built for this target. Playback clock anchoring only needs a steady
// microsecond counter (C: arch_get_avtime).
var avtimeBase = time.Now()

func GetAvtime() int64 {
	return time.Since(avtimeBase).Microseconds()
}
