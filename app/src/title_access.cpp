// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_access.hpp"
#include "diagnostics.hpp"

#if defined(MOVIAN_TITLE_ACCESS_LAPY) && !defined(MOVIAN_HOST_PREVIEW)
// The build copies the installed public starter client with its original
// relative include layout; no kernel implementation belongs in this title.
#include "starter_elevation/elevation.hpp"
#include "network_runtime.hpp"
#include "paths.hpp"
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" int __real_sceSysmoduleLoadModule(std::uint16_t id);
extern "C" int __real_sceSysmoduleUnloadModule(std::uint16_t id);

namespace {
struct KeptModule { std::uint16_t id; bool loaded; };
KeptModule kept_modules[] = {{0x0096, false}, {207, false}};

void probe(int descriptor, const char *stage, int result) noexcept {
    char message[160];
    const int size = std::snprintf(message, sizeof message, "%s result=0x%x errno=%d\n",
        stage, static_cast<unsigned>(result), result < 0 ? errno : 0);
    if (size <= 0 || static_cast<unsigned>(size) >= sizeof message) return;
    if (descriptor >= 0) (void)write(descriptor, message, static_cast<unsigned>(size));
    (void)write(STDERR_FILENO, message, static_cast<unsigned>(size));
}

bool preload_modules(int log_fd) noexcept {
    bool ready = true;
    for (auto &module : kept_modules) {
        const int result = __real_sceSysmoduleLoadModule(module.id);
        module.loaded = result >= 0;
        movian::diagnostics::record("title-access", "preload module=%u rc=0x%x kept=%d",
            module.id, static_cast<unsigned>(result), module.loaded);
        probe(log_fd, module.id == 207 ? "preload videodec2" : "preload IME", result);
        ready &= module.loaded;
    }
    return ready;
}

bool exact_request(const char *path) noexcept {
    constexpr char expected[] = "PPSA99456\n";
    const int file = open(path, O_RDONLY);
    if (file < 0) return false;
    char contents[sizeof(expected)]{};
    const auto count = read(file, contents, sizeof(contents));
    const int closed = close(file);
    return count == sizeof(expected) - 1 && closed == 0 &&
        std::memcmp(contents, expected, sizeof(expected) - 1) == 0;
}

using LoadModule = int (*)(const char *, std::uint64_t, const void *, std::uint32_t, void *, int *);
using ResolveModule = int (*)(int, const char *, void **);
using ChangeRoot = int (*)(const char *);
using ChangeDirectoryFd = int (*)(int);

bool same_root(int root_fd) noexcept {
    struct stat saved{}, current{};
    return fstat(root_fd, &saved) == 0 && stat("/", &current) == 0 &&
        saved.st_dev == current.st_dev && saved.st_ino == current.st_ino;
}

bool restore_own_root(int root_fd, int log_fd) noexcept {
    // These are ordinary public loader APIs in the already-imported libkernel.
    // Resolve the privileged libc functions only after access, avoiding any
    // libkernel_sys dependency during title startup.
    auto root = reinterpret_cast<ChangeRoot>(dlsym(RTLD_DEFAULT, "chroot"));
    auto directory = reinterpret_cast<ChangeDirectoryFd>(dlsym(RTLD_DEFAULT, "fchdir"));
    probe(log_fd, "resolve loaded chroot/fchdir", root && directory ? 0 : -1);
    if (!root || !directory) {
        auto load = reinterpret_cast<LoadModule>(dlsym(RTLD_DEFAULT, "sceKernelLoadStartModule"));
        auto resolve = reinterpret_cast<ResolveModule>(dlsym(RTLD_DEFAULT, "sceKernelDlsym"));
        if (!load || !resolve) {
            probe(log_fd, "resolve public loader APIs", -1);
            return false;
        }
        int module_status = 0;
        // Exact SONAME exported by the installed public SDK; no guessed NIDs,
        // syscall numbers, system paths or alternate kernel implementation.
        const int handle = load("libkernel_sys.sprx", 0, nullptr, 0, nullptr, &module_status);
        if (log_fd >= 0) {
            char message[128];
            const int size = std::snprintf(message, sizeof message,
                "root restore load handle=0x%x status=0x%x\n",
                static_cast<unsigned>(handle), static_cast<unsigned>(module_status));
            if (size > 0 && static_cast<unsigned>(size) < sizeof message)
                (void)write(log_fd, message, static_cast<unsigned>(size));
        }
        if (handle < 0) return false;
        void *root_symbol = nullptr, *directory_symbol = nullptr;
        int result = resolve(handle, "chroot", &root_symbol);
        probe(log_fd, "resolve libkernel_sys chroot", result);
        if (result < 0 || !root_symbol) return false;
        result = resolve(handle, "fchdir", &directory_symbol);
        probe(log_fd, "resolve libkernel_sys fchdir", result);
        if (result < 0 || !directory_symbol) return false;
        root = reinterpret_cast<ChangeRoot>(root_symbol);
        directory = reinterpret_cast<ChangeDirectoryFd>(directory_symbol);
    }
    // Native APIs take/release vnode references for this process. Credentials,
    // capabilities and the helper's privileged backend remain untouched.
    int result = directory(root_fd);
    probe(log_fd, "restore fchdir", result);
    if (result != 0) return false;
    result = root(".");
    probe(log_fd, "restore chroot", result);
    if (result != 0) return false;
    result = chdir("/");
    probe(log_fd, "restore chdir", result);
    if (result != 0) return false;
    const bool verified = same_root(root_fd) &&
        exact_request(MOVIAN_APP0 "/stpp-title-access-test.request") &&
        exact_request(MOVIAN_DOWNLOAD0 "/stpp-title-access-test.consumed");
    probe(log_fd, "restore original root and title paths proof", verified ? 0 : -1);
    return verified;
}

[[noreturn]] void stop_after_path_failure(int log_fd) noexcept {
    constexpr char message[] = "title access root restoration failed; requesting shell exit\n";
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    if (log_fd >= 0) {
        (void)write(log_fd, message, sizeof(message) - 1);
        (void)close(log_fd);
    }
    (void)movian::title_access::request_exit();
    for (;;) (void)usleep(100000);
}
}

extern "C" int __wrap_sceSysmoduleLoadModule(std::uint16_t id) {
    for (const auto &module : kept_modules)
        if (module.id == id && module.loaded) return 0;
    return __real_sceSysmoduleLoadModule(id);
}
extern "C" int __wrap_sceSysmoduleUnloadModule(std::uint16_t id) {
    for (const auto &module : kept_modules)
        if (module.id == id && module.loaded) return 0;
    return __real_sceSysmoduleUnloadModule(id);
}
#endif

#if !defined(MOVIAN_HOST_PREVIEW)
extern "C" int sceSystemServiceLoadExec(const char *path, const char **arguments);
#endif

namespace movian::title_access {
namespace { bool access_active = false; const char *access_mode = "standard / unverified"; }
bool active() noexcept { return access_active; }
const char *mode() noexcept { return access_mode; }

Result request_for_test() noexcept {
#if defined(MOVIAN_TITLE_ACCESS_LAPY) && !defined(MOVIAN_HOST_PREVIEW)
    // The app0 request is uploaded separately; it is never packaged. The
    // private consumed marker prevents the remaining app0 file from enabling
    // another launch. The operator removes that marker for an explicit retry.
    constexpr char source[] = MOVIAN_APP0 "/stpp-title-access-test.request";
    constexpr char marker[] = MOVIAN_DOWNLOAD0 "/stpp-title-access-test";
    constexpr char consumed[] = MOVIAN_DOWNLOAD0 "/stpp-title-access-test.consumed";
    constexpr char expected[] = "PPSA99456\n";
    if (!exact_request(source))
        return {false, false, 0, "disabled"};
    const int claim = open(consumed, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (claim < 0) return {false, false, 0, "disabled"};
    const bool claimed = write(claim, expected, sizeof(expected) - 1) == sizeof(expected) - 1;
    const int closed_claim = close(claim);
    if (!claimed || closed_claim != 0) {
        diagnostics::record("title-access", "test request consumption failed");
        return {false, false, 0, "disabled"};
    }
    const int pending = open(marker, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (pending < 0) {
        diagnostics::record("title-access", "could not seed private test marker error=%d", errno);
        return {false, false, 0, "disabled"};
    }
    const bool seeded = write(pending, expected, sizeof(expected) - 1) == sizeof(expected) - 1;
    const int closed_pending = close(pending);
    if (!seeded || closed_pending != 0 || !exact_request(marker) || unlink(marker) != 0) {
        diagnostics::record("title-access", "test marker invalid or could not be consumed");
        return {false, false, 0, "disabled"};
    }
    // An open private descriptor remains usable when absolute paths change.
    // Refuse the experiment when its evidence file cannot be retained.
    const int log_fd = open(MOVIAN_DOWNLOAD0 "/title-access-test.log", O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (log_fd < 0) {
        diagnostics::record("title-access", "could not open private probe log; skipping helper");
        return {true, false, static_cast<unsigned>(elevation::Status::prepare_failed), "none"};
    }
    probe(log_fd, "v60 explicit request consumed", 0);
    if (!preload_modules(log_fd)) {
        diagnostics::record("title-access", "required module preload failed; skipping helper");
        (void)close(log_fd);
        return {true, false, static_cast<unsigned>(elevation::Status::prepare_failed), "none"};
    }
    const int initialized = network::initialize_native();
    if (initialized != 0) {
        probe(log_fd, "native network initialization", initialized);
        (void)close(log_fd);
        diagnostics::record("title-access", "native network unavailable rc=0x%x",
                            static_cast<unsigned>(initialized));
        return {true, false, static_cast<unsigned>(elevation::Status::transport_error), "none"};
    }
    const int root_fd = open("/", O_RDONLY | O_DIRECTORY);
    if (root_fd < 0 || !same_root(root_fd)) {
        probe(log_fd, "retain original root", -1);
        (void)close(log_fd);
        if (root_fd >= 0) (void)close(root_fd);
        diagnostics::record("title-access", "could not retain original root; skipping helper");
        return {true, false, static_cast<unsigned>(elevation::Status::prepare_failed), "none"};
    }
    probe(log_fd, "before public elevation request", 0);
    const auto status = elevation::request(elevation::Capability::filesystem);
    probe(log_fd, "public elevation status", static_cast<int>(status));
    const bool verified = status == elevation::Status::ok;
    bool root_restored = same_root(root_fd);
    probe(log_fd, "original root still active", root_restored ? 0 : -1);
    if (verified && !root_restored)
        root_restored = restore_own_root(root_fd, log_fd);
    if (!root_restored) {
        (void)close(root_fd);
        stop_after_path_failure(log_fd);
    }
    if (log_fd >= 0) {
        char message[128];
        const int size = std::snprintf(message, sizeof message,
            "title access status=%u data_verified=%d root_restored=%d path=%s\n",
            static_cast<unsigned>(status), verified, root_restored, elevation::path());
        if (size > 0 && static_cast<unsigned>(size) < sizeof message)
            (void)write(log_fd, message, static_cast<unsigned>(size));
        (void)close(log_fd);
    }
    (void)close(root_fd);
    diagnostics::record("title-access", "data_verified=%d status=%u path=%s",
                        verified, static_cast<unsigned>(status), elevation::path());
    access_active = verified && root_restored && geteuid() == 0 &&
        std::strcmp(elevation::path(), "helper") == 0;
    if (access_active) access_mode = "helper credentials, title paths";
    else if (verified) access_mode = "existing data access; credentials unverified";
    return {true, verified, static_cast<unsigned>(status), elevation::path(), root_restored};
#else
    return {false, false, 0, "disabled"};
#endif
}

int request_exit() noexcept {
#if !defined(MOVIAN_HOST_PREVIEW)
    const int result = sceSystemServiceLoadExec("exit", nullptr);
    diagnostics::record("title-access", "shell exit rc=0x%x", static_cast<unsigned>(result));
    return result;
#else
    return -1;
#endif
}
}
