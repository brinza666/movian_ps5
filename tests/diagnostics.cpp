#include "diagnostics.hpp"

#include <cassert>
#include <cstring>
#include <fstream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
int connect_local()
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    timeval timeout{2, 0};
    assert(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) == 0);
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(movian::diagnostics::port);
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, reinterpret_cast<const sockaddr *>(&dest), sizeof dest) == 0) return fd;
    close(fd);
    return -1;
}

std::string request(const char *line)
{
    const int fd = connect_local();
    assert(fd >= 0);
    const std::string query = std::string(line) + "\r\nHost: localhost\r\n\r\n";
    assert(send(fd, query.data(), query.size(), 0) == static_cast<ssize_t>(query.size()));
    std::string result;
    char chunk[512];
    for (;;)
    {
        const auto got = recv(fd, chunk, sizeof chunk, 0);
        assert(got >= 0);
        if (!got) break;
        result.append(chunk, static_cast<std::size_t>(got));
    }
    close(fd);
    return result;
}
}

int main()
{
    using namespace movian::diagnostics;
    assert(connect_local() == -1);
    record("decoder", "evicted marker");
    for (int i = 0; i < 220; ++i) record("peer", "piece %d", i);
    record("decoder", "line\nbreak");
    assert(enable(true));
    assert(request("GET /api/logfile HTTP/1.1").find("/api/logfile/0") != std::string::npos);
    const auto log = request("GET /api/logfile/0?mode=download HTTP/1.1");
    assert(log.starts_with("HTTP/1.1 200 OK"));
    assert(log.find("evicted marker") == std::string::npos);
    assert(log.find("[peer] piece 219\n") != std::string::npos);
    assert(log.find("[decoder] line break\n") != std::string::npos);
    assert(request("POST /api/logfile/0 HTTP/1.1").starts_with("HTTP/1.1 405"));
    assert(request("GET /settings.txt HTTP/1.1").starts_with("HTTP/1.1 404"));
    const int pending = connect_local();
    assert(pending >= 0 && enable(false));
    close(pending);
    assert(connect_local() == -1);
    assert(enable(true) && enable(false));

    int occupied = socket(AF_INET, SOCK_STREAM, 0);
    assert(occupied >= 0);
    int reuse = 1;
    assert(setsockopt(occupied, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse) == 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    assert(bind(occupied, reinterpret_cast<sockaddr *>(&address), sizeof address) == 0);
    assert(listen(occupied, 1) == 0);
    assert(enable(true) && !lan_enabled());
    record("fallback", "record written after TCP bind failed");
    assert(enable(false));
    std::ifstream log_file("download0/remote-diagnostics.log");
    const std::string saved((std::istreambuf_iterator<char>(log_file)), {});
    assert(saved.find("[fallback] record written after TCP bind failed") != std::string::npos);
    close(occupied);
}
