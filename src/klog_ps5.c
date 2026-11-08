/*
 * Movian PS5 - Kernel Log (klog) via sceSysUtilSendKlog syscall
 * Writes directly to /dev/klog which is served by klogsrv.elf on port 3232
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Syscall numbers for PS5 (FreeBSD-based) */
#define SYS_sceSysUtilSendKlog  604  /* Approximate, may need adjustment */

/* Syscall wrapper */
static inline long ps5_syscall(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/* Send log to kernel log buffer (/dev/klog) */
void ps5_klog_send(const char *buf, size_t len) {
    ps5_syscall(SYS_sceSysUtilSendKlog, (long)buf, (long)len, 0, 0, 0, 0);
}

/* Formatted klog */
void ps5_klog(const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        ps5_klog_send(buf, (size_t)len < sizeof(buf) ? (size_t)len : sizeof(buf) - 1);
    }
}

/* Also send to stderr for good measure */
void ps5_klog_stderr(const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        ps5_klog_send(buf, (size_t)len < sizeof(buf) ? (size_t)len : sizeof(buf) - 1);
    }
}