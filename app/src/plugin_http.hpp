// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>

namespace movian::plugins
{
struct Header { const char *name; const char *value; };
struct HttpResponse
{
    int status = 0;
    std::size_t length = 0;
    char headers[8192]{};
};

int http_request(const char *url, bool post, const Header *headers, int count,
                 const char *payload, std::size_t length, char *body,
                 std::size_t capacity, HttpResponse &response) noexcept;
}
