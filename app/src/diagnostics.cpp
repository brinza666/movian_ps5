// SPDX-License-Identifier: GPL-3.0-or-later
#include "diagnostics.hpp"
#include "paths.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

namespace movian::diagnostics
{
namespace
{
constexpr unsigned line_count = 192;
constexpr unsigned line_size = 200;
Entry lines[line_count]{};
uint64_t last_sequence = 0;
unsigned next_line = 0;
unsigned used_lines = 0;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
constexpr char fallback_path[] = MOVIAN_DOWNLOAD0 "/remote-diagnostics.log";
constexpr char fallback_previous[] = MOVIAN_DOWNLOAD0 "/remote-diagnostics.previous.log";
FILE *fallback = nullptr;
unsigned fallback_lines = 0;

std::atomic<bool> serving{false};
int listener = -1;
pthread_t worker{};

void startup_failure(const char *stage, int error) noexcept
{
    record("diagnostics", "TCP startup failed at %s error=%d", stage, error);
    if (auto *log = std::fopen(MOVIAN_LOG_PATH, "a"))
    {
        std::fprintf(log, "v52 diagnostics TCP startup failed at %s error=%d\n", stage, error);
        std::fclose(log);
    }
}

bool start_fallback() noexcept
{
    pthread_mutex_lock(&log_lock);
    if (!fallback)
    {
        fallback = std::fopen(fallback_path, "w");
        fallback_lines = 0;
        if (fallback)
        {
            for (unsigned i = 0; i < used_lines; ++i)
                std::fprintf(fallback, "%s\n", lines[(next_line + line_count - used_lines + i) % line_count].text);
            fallback_lines = used_lines;
            std::fflush(fallback);
        }
    }
    const bool active = fallback != nullptr;
    pthread_mutex_unlock(&log_lock);
    return active;
}

unsigned long long millis() noexcept
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<unsigned long long>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}

bool nonblocking(int fd) noexcept
{
#ifdef __PROSPERO__
    constexpr int console_so_nbio = 0x1200;
    int enabled = 1;
    return setsockopt(fd, SOL_SOCKET, console_so_nbio, &enabled, sizeof enabled) == 0;
#else
    return fcntl(fd, F_SETFL, O_NONBLOCK) == 0;
#endif
}

bool lan_client(unsigned ip) noexcept
{
    return (ip >> 24) == 10 || (ip >> 24) == 127 || (ip >> 20) == 0xac1 ||
           (ip >> 16) == 0xc0a8;
}

bool wait_for(int fd, short flags, unsigned long long deadline) noexcept
{
    while (serving && millis() < deadline)
    {
        pollfd event{fd, flags, 0};
        const auto result = poll(&event, 1, 50);
        if (result > 0) return (event.revents & flags) != 0;
        if (result < 0 && errno != EINTR) return false;
    }
    return false;
}

bool send_all(int fd, const char *data, std::size_t size, unsigned long long deadline) noexcept
{
    while (size && wait_for(fd, POLLOUT, deadline))
    {
#ifdef MSG_NOSIGNAL
        const auto sent = send(fd, data, size, MSG_NOSIGNAL);
#else
        const auto sent = send(fd, data, size, 0);
#endif
        if (sent < 0 && (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)) continue;
        if (sent <= 0) return false;
        data += sent;
        size -= static_cast<std::size_t>(sent);
    }
    return size == 0;
}

void reply(int fd, const char *status, const char *body, std::size_t size) noexcept
{
    char header[192];
    const int length = std::snprintf(header, sizeof header,
        "HTTP/1.1 %s\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %zu\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n\r\n", status, size);
    const auto deadline = millis() + 2000;
    if (length > 0 && static_cast<std::size_t>(length) < sizeof header &&
        send_all(fd, header, length, deadline))
        send_all(fd, body, size, deadline);
}

void handle(int fd) noexcept
{
    char request[1024]{};
    std::size_t size = 0;
    const auto deadline = millis() + 1000;
    while (size < sizeof request - 1 && wait_for(fd, POLLIN, deadline))
    {
        const auto got = recv(fd, request + size, sizeof request - 1 - size, 0);
        if (got < 0 && (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)) continue;
        if (got <= 0) return;
        size += static_cast<std::size_t>(got);
        if (std::strstr(request, "\r\n\r\n")) break;
    }
    if (!std::strstr(request, "\r\n\r\n")) return;
    auto *end = std::strstr(request, "\r\n");
    *end = 0;
    if (std::strncmp(request, "GET ", 4))
    {
        reply(fd, "405 Method Not Allowed", "Read-only diagnostics\n", 22);
        return;
    }
    const char *target = request + 4;
    auto *version = std::strchr(request + 4, ' ');
    if (!version || (std::strcmp(version + 1, "HTTP/1.1") && std::strcmp(version + 1, "HTTP/1.0")))
        return;
    *version = 0;
    if (!std::strcmp(target, "/api/logfile"))
    {
        constexpr char index[] = "Current session: /api/logfile/0\n";
        reply(fd, "200 OK", index, sizeof index - 1);
        return;
    }
    if (std::strcmp(target, "/api/logfile/0") && std::strcmp(target, "/api/logfile/0?mode=download"))
    {
        reply(fd, "404 Not Found", "Not found\n", 10);
        return;
    }

    char snapshot[line_count * line_size];
    std::size_t length = 0;
    pthread_mutex_lock(&log_lock);
    for (unsigned i = 0; i < used_lines; ++i)
    {
        const char *line = lines[(next_line + line_count - used_lines + i) % line_count].text;
        const auto bytes = std::strlen(line);
        std::memcpy(snapshot + length, line, bytes);
        length += bytes;
        snapshot[length++] = '\n';
    }
    pthread_mutex_unlock(&log_lock);
    reply(fd, "200 OK", snapshot, length);
}

void *serve(void *) noexcept
{
    while (serving)
    {
        if (!wait_for(listener, POLLIN, millis() + 200)) continue;
        sockaddr_in remote{};
        socklen_t length = sizeof remote;
        const int client = accept(listener, reinterpret_cast<sockaddr *>(&remote), &length);
        if (client < 0) continue;
#ifdef SO_NOSIGPIPE
        int enabled = 1;
        setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof enabled);
#endif
        if (lan_client(ntohl(remote.sin_addr.s_addr)) && nonblocking(client)) handle(client);
        close(client);
    }
    return nullptr;
}
}

void record(const char *area, const char *format, ...) noexcept
{
    char message[line_size];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof message, format, args);
    va_end(args);
    for (char *c = message; *c; ++c)
        if (static_cast<unsigned char>(*c) < ' ') *c = ' ';

    pthread_mutex_lock(&log_lock);
    char line[line_size]{};
    const int prefix = std::snprintf(line, line_size, "%llu [%s] ", millis(), area);
    if (prefix >= 0 && static_cast<unsigned>(prefix) < line_size)
    {
        const auto size = std::min<std::size_t>(std::strlen(message), line_size - prefix - 1);
        std::memcpy(line + prefix, message, size);
        line[prefix + size] = 0;
        auto &entry = lines[next_line];
        entry.sequence = ++last_sequence;
        std::memcpy(entry.text, line, line_size);
        if (fallback)
        {
            if (fallback_lines == line_count)
            {
                std::fclose(fallback);
                std::rename(fallback_path, fallback_previous);
                fallback = std::fopen(fallback_path, "w");
                fallback_lines = 0;
            }
            if (fallback)
            {
                std::fprintf(fallback, "%s\n", line);
                std::fflush(fallback);
                ++fallback_lines;
            }
        }
        next_line = (next_line + 1) % line_count;
        used_lines = std::min(used_lines + 1, line_count);
    }
    pthread_mutex_unlock(&log_lock);
}

std::size_t copy_entries(Entry *entries, std::size_t capacity) noexcept
{
    if (!entries || !capacity) return 0;
    pthread_mutex_lock(&log_lock);
    const auto count = std::min<std::size_t>(capacity, used_lines);
    const auto first = (next_line + line_count - count) % line_count;
    for (std::size_t i = 0; i < count; ++i)
        entries[i] = lines[(first + i) % line_count];
    pthread_mutex_unlock(&log_lock);
    return count;
}

bool enable(bool enabled) noexcept
{
    if (!enabled)
    {
        if (serving.exchange(false))
        {
            pthread_join(worker, nullptr);
            close(listener);
            listener = -1;
        }
        pthread_mutex_lock(&log_lock);
        if (fallback) std::fclose(fallback);
        fallback = nullptr;
        pthread_mutex_unlock(&log_lock);
        return true;
    }
    if (serving) return true;

    sockaddr_in address{};
#if defined(__FreeBSD__)
    address.sin_len = sizeof address;
#endif
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0)
        startup_failure("socket", errno);
    else
    {
        int reuse = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);
        if (bind(listener, reinterpret_cast<const sockaddr *>(&address), sizeof address) != 0)
            startup_failure("bind", errno);
        else if (listen(listener, 2) != 0)
            startup_failure("listen", errno);
        else if (!nonblocking(listener))
            startup_failure("nonblocking", errno);
        else
        {
            serving = true;
            const int result = pthread_create(&worker, nullptr, serve, nullptr);
            if (result == 0) return true;
            serving = false;
            startup_failure("thread", result);
        }
        close(listener);
        listener = -1;
    }
    return start_fallback();
}

bool lan_enabled() noexcept { return serving; }
bool capture_file() noexcept { return start_fallback(); }
}
