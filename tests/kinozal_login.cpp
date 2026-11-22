#include "plugins.hpp"
#include "plugin_http.hpp"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace
{
constexpr char settings[] = "download0/plugins-v23/KinoZal.TV.settings";
unsigned requests = 0;

void set(const char *name, const char *value)
{
    std::ofstream(std::filesystem::path(settings) / name) << value;
}
}

int movian::plugins::http_request(const char *url, bool post, const Header *headers, int count,
                                  const char *payload, std::size_t length, char *, std::size_t,
                                  HttpResponse &response) noexcept
{
    ++requests;
    assert(post && !std::strcmp(url, "https://mirror.example/takelogin.php"));
    assert(count == 3 && !std::strcmp(headers[1].value, "https://mirror.example/") &&
           !std::strcmp(headers[2].value, "https://mirror.example"));
    constexpr char form[] = "username=test&password=a%26b&returnto=";
    assert(length == sizeof form - 1 && !std::memcmp(payload, form, length));
    response.status = 302;
    std::strcpy(response.headers, "Set-Cookie: uid=1; Path=/\r\nSet-Cookie: pass=fixture; Path=/\r\n");
    return 0;
}

int main()
{
    using namespace movian::plugins;
    char origin[264]{}, error[160]{};
    std::filesystem::create_directories(settings);
    set("domain", "xdomainx");
    set("domainx", "mirror.example");
    set("protocol", "https://");
    assert(kinozal_login_origin(origin));
    assert(!std::strcmp(origin, "https://mirror.example"));
    assert(kinozal_login(origin, "test", "a&b", error, sizeof error) && requests == 1);
    set("domainx", "other.example");
    assert(!kinozal_login(origin, "test", "a&b", error, sizeof error) && requests == 1);
    set("domainx", "mirror.example@other.example");
    assert(!kinozal_login_origin(origin));
    set("protocol", "http://");
    set("domainx", "mirror.example");
    assert(!kinozal_login_origin(origin) && requests == 1);
}
