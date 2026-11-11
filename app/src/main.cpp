/*
 * Movian for PS5 — native ShadowMount title entry.
 *
 * Build model follows VLC for PS5 (ZiZc3/VLC) and Hello PS5
 * (MiladRTX/ps5-homebrew-starter): a signed eboot.bin in
 * /data/homebrew/PPSA99455, not a Go ELF and not a port-9021 payload.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "audio.hpp"
#include "font.hpp"
#include "pad.hpp"
#include "paths.hpp"
#include "screen.hpp"
#include "ui.hpp"

int main()
{
    if (!movian::font::load(MOVIAN_APP0 "/assets/fonts/"))
        movian::screen::notify("Movian: fonts missing from /app0/assets/fonts");
    (void)movian::pad::init();
    (void)movian::audio::init();
    movian::ui::load();
    movian::screen::run(movian::ui::update, movian::ui::draw);
}
