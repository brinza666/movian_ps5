// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "diagnostics.hpp"
#include <pthread.h>
#include <cstdint>
#if !defined(MOVIAN_HOST_PREVIEW) || defined(MOVIAN_NATIVE_NETWORK_TEST)
extern "C" int sceNetInit();
#endif

namespace movian::network {
// libSceNet is process-wide. Do not terminate it when one HTTP/DNS job ends.
inline int initialize_native() noexcept {
#if !defined(MOVIAN_HOST_PREVIEW) || defined(MOVIAN_NATIVE_NETWORK_TEST)
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static bool ready = false;
    pthread_mutex_lock(&lock);
    int result = 0;
    if (!ready) {
        result = sceNetInit();
        // libSceNet may already have been initialized by the title runtime.
        ready = result == 0 || static_cast<std::uint32_t>(result) == 0x80410110u;
        diagnostics::record("network", "native init rc=0x%x ready=%d", static_cast<unsigned>(result), ready);
    }
    const int status = ready ? 0 : result < 0 ? result : -1;
    pthread_mutex_unlock(&lock);
    return status;
#else
    return 0;
#endif
}
} // namespace movian::network
