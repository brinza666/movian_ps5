/*
 * Movian for PS5 — Home, Videos browse, MPEG-1 player, BitTorrent, Plugins.
 * Controller-first layout matching Movian / movian-go's four primary places.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "screen.hpp"

namespace movian::ui
{
void load() noexcept;
[[nodiscard]] bool update() noexcept;
void draw(screen::Canvas &canvas) noexcept;
} // namespace movian::ui
