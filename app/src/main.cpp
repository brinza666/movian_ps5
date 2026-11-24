/*
 * Movian for PS5 — native ShadowMount title entry.
 *
 * Build model follows VLC for PS5 (ZiZc3/VLC) and Hello PS5
 * (MiladRTX/ps5-homebrew-starter): a signed eboot.bin in
 * /data/homebrew/PPSA99456, not a Go ELF and not a port-9021 payload.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "audio.hpp"
#include "diagnostics.hpp"
#include "font.hpp"
#include "pad.hpp"
#include "paths.hpp"
#include "profile_reset.hpp"
#include "screen.hpp"
#include "title_access.hpp"
#include "ui.hpp"

#include <cstdio>

namespace {
void checkpoint(const char *message)
{
    movian::diagnostics::record("startup", "%s", message);
    std::fprintf(stderr, "[movian-hevc-test] %s\n", message);
    std::fflush(stderr);
    if (auto *file = std::fopen(MOVIAN_DOWNLOAD0 "/hevc-test-startup.log", "a")) {
        std::fprintf(file, "%s\n", message);
        std::fclose(file);
    }
}
}

int main()
{
    checkpoint("v62 entered main");
    checkpoint("v62 fresh reset begin");
    const bool reset=movian::profile::reset_requested();
    checkpoint(reset?"v62 fresh reset complete":"v62 fresh reset not completed or not requested");
    checkpoint("v62 title access begin");
    const auto access = movian::title_access::request_for_test();
    movian::diagnostics::record("startup", "title access enabled=%d data_verified=%d status=%u path=%s root_restored=%d",
        access.enabled, access.data_verified, access.status, access.path, access.root_restored);
    if (!movian::font::load(MOVIAN_APP0 "/assets/fonts/"))
        movian::screen::notify("Movian: fonts missing from title folder");
    checkpoint("v62 fonts attempted");
    (void)movian::pad::init();
    checkpoint("v62 pad initialized");
    movian::ui::load();
    checkpoint("v62 UI loaded; waiting for Cross");
    movian::screen::run(movian::ui::update, movian::ui::draw);
}
