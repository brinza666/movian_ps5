//go:build ps5

// SPDX-License-Identifier: GPL-3.0-or-later

package ps5

// SDL2 gamecontroller button ids (SDL_gamecontroller.h). Stable since SDL 2.0.0.
const (
	ButtonA             = 0  // DualSense Cross
	ButtonB             = 1  // Circle
	ButtonX             = 2  // Square
	ButtonY             = 3  // Triangle
	ButtonBack          = 4  // Create
	ButtonGuide         = 5  // PS
	ButtonStart         = 6  // Options
	ButtonLeftStick     = 7
	ButtonRightStick    = 8
	ButtonLeftShoulder  = 9  // L1
	ButtonRightShoulder = 10 // R1
	ButtonDpadUp        = 11
	ButtonDpadDown      = 12
	ButtonDpadLeft      = 13
	ButtonDpadRight     = 14
)

// Keyboard codes are 1000 + SDL_Scancode (USB HID usage).
const (
	keyBase = 1000
	keyRet  = keyBase + 40
	keyEsc  = keyBase + 41
	keyBs   = keyBase + 42
	keySpc  = keyBase + 44
	keyRight = keyBase + 79
	keyLeft  = keyBase + 80
	keyDown  = keyBase + 81
	keyUp    = keyBase + 82
)

// QuitCode is returned by the display poller for SDL_QUIT.
const QuitCode = -1

// Action maps a DualSense / USB-keyboard edge to a Movian action name
// (internal/event actionNames). Cross confirms, Circle goes back.
// Swap Cross/Circle in this table if you want the Japanese layout.
func Action(code int) (string, bool) {
	switch code {
	case QuitCode:
		return "Quit", true
	case ButtonDpadUp, keyUp:
		return "Up", true
	case ButtonDpadDown, keyDown:
		return "Down", true
	case ButtonDpadLeft, keyLeft:
		return "Left", true
	case ButtonDpadRight, keyRight:
		return "Right", true
	case ButtonA, keyRet, keySpc:
		return "Activate", true
	case ButtonB, keyEsc, keyBs:
		return "Back", true
	case ButtonX:
		return "ItemMenu", true
	case ButtonY:
		return "Home", true
	case ButtonStart:
		return "Menu", true
	case ButtonBack:
		return "Playqueue", true
	case ButtonLeftShoulder:
		return "PageUp", true
	case ButtonRightShoulder:
		return "PageDown", true
	case ButtonLeftStick:
		return "PlayPause", true
	case ButtonRightStick:
		return "Stop", true
	default:
		return "", false
	}
}
