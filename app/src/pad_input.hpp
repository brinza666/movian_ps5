/*
 * Held buttons become presses with the same timing on console and host.
 * Copied from ps5-homebrew-starter (MiladRTX), namespaced to movian::.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace movian::pad::input
{
// One controller sample: the buttons held in it (pad::Button bits).
void sample(unsigned held) noexcept;

// A sample while the system has the controller (its menu, the on-screen
// keyboard): nothing is pressed, and a button still down when the app gets
// the controller back isn't a new press. So the Cross that closed the
// keyboard doesn't also press Cross in the app.
void suspend() noexcept;

// The presses since the last call: each new press once, plus repeats of a
// held direction, L1/R1 or L2/R2 (after 350 ms, every 110 ms, then every
// 40 ms once held for 1.5 s).
[[nodiscard]] unsigned take_presses() noexcept;

// A stick as a D-pad, along its stronger axis: x and y from -128 to 127.
[[nodiscard]] unsigned stick_as_dpad(int x, int y) noexcept;
} // namespace movian::pad::input
