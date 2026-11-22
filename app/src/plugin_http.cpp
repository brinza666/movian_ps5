// SPDX-License-Identifier: GPL-3.0-or-later
#include "plugin_http.hpp"

#include <cstdint>
#include <cstring>
#include <strings.h>
#include <time.h>

#ifndef MOVIAN_HOST_PREVIEW
extern "C" {
int sceNetPoolCreate(const char *, int, int);
int sceNetPoolDestroy(int);
int sceSslInit(std::size_t);
int sceSslTerm(int);
int sceHttpInit(int, int, std::size_t);
int sceHttpTerm(int);
int sceHttpCreateTemplate(int, const char *, int, int);
int sceHttpDeleteTemplate(int);
int sceHttpCreateConnectionWithURL(int, const char *, int);
int sceHttpDeleteConnection(int);
int sceHttpCreateRequestWithURL(int, int, const char *, std::uint64_t);
int sceHttpDeleteRequest(int);
int sceHttpSetAutoRedirect(int, int);
int sceHttpSetResolveTimeOut(int, std::uint32_t);
int sceHttpSetConnectTimeOut(int, std::uint32_t);
int sceHttpSetSendTimeOut(int, std::uint32_t);
int sceHttpSetRecvTimeOut(int, std::uint32_t);
int sceHttpsEnableOption(int, std::uint32_t);
int sceHttpAddRequestHeader(int, const char *, const char *, int);
int sceHttpSendRequest(int, const void *, std::size_t);
int sceHttpGetStatusCode(int, int *);
int sceHttpGetAllResponseHeaders(int, char **, std::size_t *);
int sceHttpReadData(int, void *, std::size_t);
}
#endif

namespace movian::plugins
{
#ifndef MOVIAN_HOST_PREVIEW
namespace
{
struct Request
{
    int pool = -1, ssl = -1, http = -1, tmpl = -1, connection = -1, request = -1;
    ~Request()
    {
        if (request >= 0) sceHttpDeleteRequest(request);
        if (connection >= 0) sceHttpDeleteConnection(connection);
        if (tmpl >= 0) sceHttpDeleteTemplate(tmpl);
        if (http >= 0) sceHttpTerm(http);
        if (ssl >= 0) sceSslTerm(ssl);
        if (pool >= 0) sceNetPoolDestroy(pool);
    }
};

bool valid_header(const Header &header, bool tls)
{
    if (!header.name[0] || std::strpbrk(header.name, " :\t\r\n") ||
        std::strpbrk(header.value, "\r\n") ||
        !strcasecmp(header.name, "Host") || !strcasecmp(header.name, "Content-Length") ||
        !strcasecmp(header.name, "Transfer-Encoding"))
        return false;
    return tls || (strcasecmp(header.name, "Cookie") && strcasecmp(header.name, "Authorization"));
}

std::uint64_t now_ms()
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::uint64_t>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}
}

int http_request(const char *url, bool post, const Header *headers, int count,
                 const char *payload, std::size_t length, char *body,
                 std::size_t capacity, HttpResponse &response) noexcept
{
    const bool tls = std::strncmp(url, "https://", 8) == 0;
    if ((!tls && std::strncmp(url, "http://", 7)) || std::strpbrk(url, "\r\n\t ") ||
        (post && !tls))
        return -1;
    const char *authority = url + (tls ? 8 : 7);
    const std::size_t authority_length = std::strcspn(authority, "/?#");
    if (!authority_length || std::memchr(authority, '@', authority_length))
        return -1;
    const auto deadline = now_ms() + 8000;
    Request req;
    int result = req.pool = sceNetPoolCreate("movian_plugin", 1024 * 1024, 0);
    if (result >= 0) result = req.ssl = sceSslInit(304 * 1024);
    if (result >= 0) result = req.http = sceHttpInit(req.pool, req.ssl, 4 * 1024 * 1024);
    if (result >= 0) result = req.tmpl = sceHttpCreateTemplate(req.http, "Movian/PS5", 2, 0);
    constexpr unsigned timeout = 3000000;
    if (result >= 0) result = sceHttpSetAutoRedirect(req.tmpl, 0);
    if (result >= 0) result = sceHttpSetResolveTimeOut(req.tmpl, timeout);
    if (result >= 0) result = sceHttpSetConnectTimeOut(req.tmpl, timeout);
    if (result >= 0) result = sceHttpSetSendTimeOut(req.tmpl, timeout);
    if (result >= 0) result = sceHttpSetRecvTimeOut(req.tmpl, timeout);
    if (result >= 0 && tls) result = sceHttpsEnableOption(req.tmpl, 0xbd);
    if (result >= 0) result = req.connection = sceHttpCreateConnectionWithURL(req.tmpl, url, 0);
    if (result >= 0)
        result = req.request = sceHttpCreateRequestWithURL(req.connection, post ? 1 : 0, url, length);
    for (int i = 0; i < count && result >= 0; ++i)
        if (valid_header(headers[i], tls))
            result = sceHttpAddRequestHeader(req.request, headers[i].name, headers[i].value, 0);
    if (result >= 0) result = sceHttpSendRequest(req.request, payload, length);
    if (result >= 0) result = sceHttpGetStatusCode(req.request, &response.status);
    if (result < 0)
        return result;
    char *raw = nullptr;
    std::size_t raw_length = 0;
    if (sceHttpGetAllResponseHeaders(req.request, &raw, &raw_length) >= 0 && raw &&
        raw_length < sizeof response.headers)
        std::memcpy(response.headers, raw, raw_length);
    while (response.length < capacity)
    {
        if (now_ms() >= deadline)
            return -3;
        const int read = sceHttpReadData(req.request, body + response.length, capacity - response.length);
        if (read <= 0)
            return read;
        response.length += static_cast<std::size_t>(read);
    }
    char extra;
    const int read = sceHttpReadData(req.request, &extra, 1);
    return read > 0 ? -2 : read;
}
#else
int http_request(const char *, bool, const Header *, int, const char *, std::size_t,
                 char *, std::size_t, HttpResponse &) noexcept
{
    return -1;
}
#endif
}
