/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * C ABI wrappers around the PS5 payload SDK libc.
 *
 * Go's freebsd/amd64 runtime (src/runtime/sys_freebsd_amd64.s) and the
 * freebsd assembly in syscall/ and golang.org/x/sys/unix do not call
 * these. They execute the SYSCALL instruction with FreeBSD numbers
 * from syscalls.master. Orbis does not use that table. A stock
 * GOOS=freebsd binary, even one linked by prospero-clang, dies in
 * runtime startup (mmap / thr_new).
 *
 * The working language port on this console (neokuze/ps5-rust) calls
 * this same libc and lets prospero-clang link it. A Go port has to do
 * the same: replace each `SYSCALL` stub with a call through the C ABI
 * to the function below, using the pattern already in
 * runtime·callCgoMmap (save SP, align to 16, CALL, restore SP).
 */

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

void *movian_ps5_mmap(void *addr, size_t len, int prot, int flags, int fd, int64_t off) {
	void *p = mmap(addr, len, prot, flags, fd, (off_t)off);
	return p;
}

int movian_ps5_munmap(void *addr, size_t len) { return munmap(addr, len); }

int movian_ps5_madvise(void *addr, size_t len, int advice) { return madvise(addr, len, advice); }

int movian_ps5_open(const char *path, int flags, int mode) { return open(path, flags, mode); }

intptr_t movian_ps5_read(int fd, void *buf, size_t n) { return read(fd, buf, n); }

intptr_t movian_ps5_write(int fd, const void *buf, size_t n) { return write(fd, buf, n); }

int movian_ps5_close(int fd) { return close(fd); }

int movian_ps5_pipe2(int fds[2], int flags) { return pipe2(fds, flags); }

int movian_ps5_fcntl(int fd, int cmd, intptr_t arg) { return fcntl(fd, cmd, arg); }

int movian_ps5_clock_gettime(int clock_id, struct timespec *ts) {
	return clock_gettime((clockid_t)clock_id, ts);
}

int movian_ps5_nanosleep(const struct timespec *req, struct timespec *rem) {
	return nanosleep(req, rem);
}

int movian_ps5_getpid(void) { return getpid(); }

int movian_ps5_kill(int pid, int sig) { return kill(pid, sig); }

int movian_ps5_sched_yield(void) { return sched_yield(); }

/* errno is thread-local in the SDK libc. Go's runtime expects the
 * negative-errno convention of its own assembly stubs. */
int movian_ps5_errno(void) { return errno; }
