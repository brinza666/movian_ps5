//go:build ps5

// SPDX-License-Identifier: GPL-3.0-or-later

package main

import (
	navcore "github.com/czz/movian-go/internal/navigator"
	settingscore "github.com/czz/movian-go/internal/settings"
)

// The GLW skin needs an OpenGL context. The PS5 SDL2 port in
// ps5-payload-dev/SDL is a software sceVideoOut framebuffer, not GL.
// These stay empty until an OSMesa or Piglet context exists; the
// render loop in render_ps5.go drives the framebuffer directly.

func glwFrontendEarlyStart(ctx *appContext, settingsManager *settingscore.SettingsManager) {
}

func glwFrontendSettingsStart(ctx *appContext) {}

func glwFrontendLateStart(ctx *appContext, defaultNav *navcore.Navigator, settingsManager *settingscore.SettingsManager) {
}
