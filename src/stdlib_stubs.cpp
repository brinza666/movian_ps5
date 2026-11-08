/* C++ standard library stubs for chrono and thread - minimal implementations for PS5 */

#include <cstddef>
#include <cstdint>

/* Simple sleep implementation using busy-wait (stub for now) */
extern "C" void ps5_sleep_ms(int ms) {
    // Busy wait - in real implementation would use sceKernelUsleep
    volatile int count = ms * 10000;
    while (count-- > 0) {
        __asm__ volatile("" ::: "memory");
    }
}

/* Also provide C-compatible time function for C code */
extern "C" double ps5_now_seconds(void) { return 0.0; }
