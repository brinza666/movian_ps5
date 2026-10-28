//go:build ps5

// SPDX-License-Identifier: GPL-3.0-or-later

package main

// Orbis (PS5 homebrew) early init. There is no upstream C arch for this
// machine. Modelled on the windows/android platform hooks: device id,
// concurrency, and managed file services for the directories a payload
// can actually see.

import (
	"crypto/sha256"
	"encoding/hex"
	"log"
	"os"
	"runtime"

	"github.com/czz/movian-go/internal/service"
)

func init() {
	if os.Getenv("HOME") == "" {
		os.Setenv("HOME", "/data/movian")
	}
	if os.Getenv("TMPDIR") == "" {
		os.Setenv("TMPDIR", "/data/movian/tmp")
	}
}

// platformState — no linux/connman singleton on Orbis.
type platformState struct{}

func platformEarlyStart(ctx *appContext) {
	for _, dir := range []string{
		"/data/movian",
		"/data/movian/tmp",
		"/data/movian/cache",
	} {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			log.Printf("[ps5] mkdir %s: %v", dir, err)
		}
	}
	ctx.gconf.OSInfo = "Orbis PS5 (homebrew payload)"
	ctx.gconf.DeviceID = ps5DeviceID()
	ctx.gconf.Concurrency = runtime.NumCPU()
	log.Printf("[ps5] home=%s cpu=%d id=%s", os.Getenv("HOME"), ctx.gconf.Concurrency, ctx.gconf.DeviceID)
}

func platformProcessMonitorStart(ctx *appContext) {}

func platformMainStart(ctx *appContext) {
	if ctx.serviceSystem == nil {
		return
	}
	// url is a filesystem path, same as the windows xdg counterpart.
	candidates := []struct{ id, title, path, typ string }{
		{"ps5-data", "Console data", "/data", "files"},
		{"ps5-usb0", "USB 0", "/mnt/usb0", "files"},
		{"ps5-usb1", "USB 1", "/mnt/usb1", "files"},
		{"ps5-user", "User", "/user", "files"},
	}
	for _, c := range candidates {
		st, err := os.Stat(c.path)
		if err != nil || !st.IsDir() {
			continue
		}
		ctx.serviceSystem.ServiceCreateManaged(c.id, c.title, c.path, c.typ,
			"", false, true, service.SvcOriginSystem)
	}
}

func ps5DeviceID() string {
	// Stable per install, not a hardware serial. A payload often cannot
	// read the console id, and we should not need to.
	b, err := os.ReadFile("/data/movian/device-id")
	if err == nil && len(b) >= 16 {
		return string(b[:32])
	}
	sum := sha256.Sum256([]byte("movian-go-ps5-" + os.Getenv("HOME")))
	id := hex.EncodeToString(sum[:16])
	_ = os.WriteFile("/data/movian/device-id", []byte(id), 0o644)
	return id
}
