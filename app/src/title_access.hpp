// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace movian::title_access {
struct Result {
    bool enabled;
    bool data_verified;
    unsigned status;
    const char *path;
    bool root_restored = false;
};

// Explicit test-build opt-in only. Call once, before UI/worker startup and
// preloads and retains IME/videodec2 before access. Success verifies filesystem
// access; the caller must separately prove bind/listen succeeds.
// Requires /app0/stpp-title-access-test.request = "PPSA99456\n" and absence of
// /download0/stpp-title-access-test-v62.consumed. Consumes the request privately.
Result request_for_test() noexcept;
// True only after the pinned helper exchange, data proof, UID 0 and root proof.
// Existing/resident data access alone never establishes credential changes.
bool active() noexcept;
const char *mode() noexcept;

// Ask the shell to close this title. Main must still never return if the
// request fails or closure is asynchronous. Call from the main/UI thread
// after stopping workers and saving state, rather than from a socket callback.
// Host preview returns -1 and does not exit the host process.
int request_exit() noexcept;
}
