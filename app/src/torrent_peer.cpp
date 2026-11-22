// SPDX-License-Identifier: GPL-3.0-or-later
#include "torrent.hpp"
#include "diagnostics.hpp"
#include "paths.hpp"
#include "plugin_http.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <iterator>

#if defined(MOVIAN_HAVE_FFMPEG) || defined(MOVIAN_TORRENT_TEST)
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#if defined(__PROSPERO__)
#include <sys/sysctl.h>
#endif

extern "C" {
#if defined(__PROSPERO__)
int sceNetPoolCreate(const char *, int, int);
int sceNetPoolDestroy(int);
int sceNetResolverCreate(const char *, int, int);
int sceNetResolverStartNtoa(int, const char *, in_addr *, int, int, int);
int sceNetResolverDestroy(int);
#endif
#if defined(MOVIAN_TORRENT_TEST)
unsigned char *SHA1(const unsigned char *, std::size_t, unsigned char *);
#else
#include <libavutil/sha.h>
#include <libavutil/mem.h>
#endif
}
#endif

namespace movian::torrent
{
namespace
{
bool fail(char *error, std::size_t size, const char *message)
{
    if (error && size)
        std::snprintf(error, size, "%s", message);
    return false;
}

#if defined(MOVIAN_HAVE_FFMPEG) || defined(MOVIAN_TORRENT_TEST)
constexpr unsigned block_size = 16 * 1024;
constexpr unsigned io_timeout_ms = 6000;
constexpr char download_dir[] = MOVIAN_DOWNLOAD0 "/torrents";

[[maybe_unused]] bool random_bytes(void *bytes, std::size_t length)
{
#if defined(__PROSPERO__)
    auto actual = length;
    return sysctlbyname("kern.arandom", bytes, &actual, nullptr, 0) == 0 && actual == length;
#else
    arc4random_buf(bytes, length);
    return true;
#endif
}

bool sha1(const void *data, std::size_t size, unsigned char (&hash)[20])
{
#if defined(MOVIAN_TORRENT_TEST)
    return SHA1(static_cast<const unsigned char *>(data), size, hash) != nullptr;
#else
    AVSHA *sha = av_sha_alloc();
    if (!sha)
        return false;
    const bool ok = av_sha_init(sha, 160) == 0;
    if (ok)
    {
        av_sha_update(sha, static_cast<const unsigned char *>(data), size);
        av_sha_final(sha, hash);
    }
    av_free(sha);
    return ok;
#endif
}

char *read_peer_info(const char *descriptor, PeerInfo &info, unsigned char (&hash)[20])
{
    struct stat st{};
    if (stat(descriptor, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > 1024 * 1024)
        return nullptr;
    const auto size = static_cast<std::size_t>(st.st_size);
    auto *data = static_cast<char *>(std::malloc(size));
    auto *file = data ? std::fopen(descriptor, "rb") : nullptr;
    const bool ok = file && std::fread(data, 1, size, file) == size &&
                    peer_info(data, size, info) && sha1(info.info, info.info_size, hash);
    if (file) std::fclose(file);
    if (!ok)
    {
        std::free(data);
        return nullptr;
    }
    return data;
}

bool download_path(const unsigned char (&digest)[20], const File &file,
                   char (&path)[384], char (&hash)[41])
{
    for (int i = 0; i < 20; ++i)
        std::snprintf(hash + 2 * i, 3, "%02x", unsigned(digest[i]));
    const int size = std::snprintf(path, sizeof path, "%s/%s-%s", download_dir, hash, file.path);
    return size > 0 && size < static_cast<int>(sizeof path);
}

std::uint64_t now_ms()
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::uint64_t>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}

struct Peer
{
    int fd = -1;
    std::atomic<bool> &cancel;
    unsigned char bitfield[8192]{};
    std::size_t bitfield_size = 0;
    ~Peer() { if (fd >= 0) close(fd); }
    void reset() { if (fd >= 0) close(fd); fd = -1; bitfield_size = 0; }

    bool wait(short events, std::uint64_t deadline)
    {
        while (!cancel.load(std::memory_order_relaxed) && now_ms() < deadline)
        {
            pollfd p{fd, events, 0};
            const int rc = poll(&p, 1, 200);
            if (rc > 0)
                return (p.revents & events) != 0;
            if (rc < 0 && errno != EINTR)
                return false;
        }
        return false;
    }

    bool transfer(void *data, std::size_t size, bool writing, std::uint64_t deadline)
    {
        auto *next = static_cast<char *>(data);
        while (size && wait(writing ? POLLOUT : POLLIN, deadline))
        {
#ifdef MSG_NOSIGNAL
            constexpr int send_flags = MSG_NOSIGNAL;
#else
            constexpr int send_flags = 0;
#endif
            const auto rc = writing ? send(fd, next, size, send_flags) : recv(fd, next, size, 0);
            if (rc < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
                continue;
            if (rc <= 0)
                return false;
            size -= static_cast<std::size_t>(rc);
            next += rc;
        }
        return size == 0;
    }
};

void put32(unsigned char *p, std::uint32_t value)
{
    for (int i = 0; i < 4; ++i)
        p[i] = static_cast<unsigned char>(value >> (24 - 8 * i));
}

std::uint32_t get32(const unsigned char *p)
{
    std::uint32_t result = 0;
    for (int i = 0; i < 4; ++i)
        result = (result << 8) | p[i];
    return result;
}

bool peer_endpoint(const char *address, sockaddr_in &target)
{
#if defined(__FreeBSD__)
    target.sin_len = sizeof target;
#endif
    target.sin_family = AF_INET;
    char canonical[32]{};
    std::size_t used = 0;
    const auto *input = reinterpret_cast<const unsigned char *>(address);
    while (*input == ' ' || *input == '\t')
        ++input;
    while (*input && *input != ' ' && *input != '\t' && *input != '\r' && *input != '\n')
    {
        if (used == sizeof canonical - 1)
            return false;
        // The PS5 IME can emit full-width digits or punctuation.
        if (input[0] == 0xef && input[1] == 0xbc &&
            ((input[2] >= 0x90 && input[2] <= 0x9a) || input[2] == 0x8e))
        {
            canonical[used++] = static_cast<char>(input[2] - 0x60);
            input += 3;
        }
        else
            canonical[used++] = static_cast<char>(*input++);
    }
    while (*input == ' ' || *input == '\t' || *input == '\r' || *input == '\n')
        ++input;
    if (*input)
        return false;
    std::uint32_t ip = 0;
    const char *next = canonical;
    for (int i = 0; i < 4; ++i)
    {
        unsigned segment = 0, digits = 0;
        while (*next >= '0' && *next <= '9' && digits < 3)
        {
            segment = segment * 10 + (*next++ - '0');
            ++digits;
        }
        if (!digits || segment > 255 || *next++ != (i == 3 ? ':' : '.'))
            return false;
        ip = (ip << 8) | segment;
    }
    unsigned port = 0, digits = 0;
    while (*next >= '0' && *next <= '9' && digits < 5)
    {
        port = port * 10 + (*next++ - '0');
        ++digits;
    }
    if (*next || !port || port > 65535)
        return false;
    target.sin_addr.s_addr = htonl(ip);
    target.sin_port = htons(static_cast<unsigned short>(port));
    return true;
}

bool open_peer(Peer &peer, const char *address, const unsigned char (&info_hash)[20], const char (&id)[21],
               char *error, std::size_t error_size)
{
    sockaddr_in target{};
    if (!peer_endpoint(address, target))
        return fail(error, error_size, "Enter the peer as IPv4:port");
    auto socket_error = [&](const char *stage, int code) {
        if (error && error_size)
            std::snprintf(error, error_size, "Peer %s failed (errno %d)", stage, code);
        return false;
    };
    peer.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (peer.fd < 0)
        return socket_error("socket", errno);
    int nonblocking = 1;
#if defined(__PROSPERO__)
    // PS5 libc can reject fcntl on a socket or accept it without changing
    // its mode. Use the platform socket option used by the starter kit.
    constexpr int console_so_nbio = 0x1200;
    if (setsockopt(peer.fd, SOL_SOCKET, console_so_nbio, &nonblocking, sizeof nonblocking) < 0)
        return socket_error("nonblocking socket option", errno);
#else
    if (fcntl(peer.fd, F_SETFL, O_NONBLOCK) < 0 &&
        ioctl(peer.fd, FIONBIO, &nonblocking) < 0)
        return socket_error("nonblocking setup", errno);
#endif
#ifdef SO_NOSIGPIPE
    int no_sigpipe = 1;
    setsockopt(peer.fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof no_sigpipe);
#endif
    const auto deadline = now_ms() + io_timeout_ms;
    if (connect(peer.fd, reinterpret_cast<sockaddr *>(&target), sizeof target) < 0)
    {
        if (errno != EINPROGRESS && errno != EWOULDBLOCK)
            return socket_error("connect", errno);
        const bool ready = peer.wait(POLLOUT, deadline);
        int error = 0;
        socklen_t size = sizeof error;
        if (getsockopt(peer.fd, SOL_SOCKET, SO_ERROR, &error, &size))
            return socket_error("connection status", errno);
        if (error || !ready)
            return socket_error("connect", error ? error : ETIMEDOUT);
    }
    unsigned char handshake[68]{};
    handshake[0] = 19;
    std::memcpy(handshake + 1, "BitTorrent protocol", 19);
    std::memcpy(handshake + 28, info_hash, 20);
    std::memcpy(handshake + 48, id, 20);
    if (!peer.transfer(handshake, sizeof handshake, true, deadline) ||
        !peer.transfer(handshake, sizeof handshake, false, deadline) ||
        handshake[0] != 19 || std::memcmp(handshake + 1, "BitTorrent protocol", 19) ||
        std::memcmp(handshake + 28, info_hash, 20))
        return fail(error, error_size, "Peer handshake failed or info hash did not match");
    unsigned char interested[] = {0, 0, 0, 1, 2};
    return peer.transfer(interested, sizeof interested, true, deadline) ||
           fail(error, error_size, "Could not send BitTorrent interest");
}

bool read_message(Peer &peer, unsigned char *data, std::uint32_t &length, std::uint64_t deadline)
{
    unsigned char prefix[4];
    if (!peer.transfer(prefix, sizeof prefix, false, deadline))
        return false;
    length = get32(prefix);
    if (length > block_size + 9 || !peer.transfer(data, length, false, deadline))
        return false;
    if (length > 1 && data[0] == 5 && length - 1 <= sizeof peer.bitfield)
    {
        peer.bitfield_size = length - 1;
        std::memcpy(peer.bitfield, data + 1, peer.bitfield_size);
    }
    if (length == 5 && data[0] == 4 && peer.bitfield_size)
    {
        const auto index = get32(data + 1);
        if (index / 8 < peer.bitfield_size)
            peer.bitfield[index / 8] |= 0x80 >> (index % 8);
    }
    return true;
}

bool unchoke(Peer &peer, unsigned char *message)
{
    const auto deadline = now_ms() + io_timeout_ms;
    std::uint32_t size;
    while (read_message(peer, message, size, deadline))
        if (size == 1 && message[0] == 1)
            return true;
    return false;
}

#if !defined(MOVIAN_TORRENT_TEST) || defined(MOVIAN_TORRENT_TEST_TRACKER)
void put64(unsigned char *out, std::uint64_t value)
{
    put32(out, static_cast<std::uint32_t>(value >> 32));
    put32(out + 4, static_cast<std::uint32_t>(value));
}

bool resolve_udp(const char *host, const char *port, sockaddr_in &address)
{
    address.sin_family = AF_INET;
#if defined(__FreeBSD__)
    address.sin_len = sizeof address;
#endif
    address.sin_port = htons(static_cast<unsigned short>(std::atoi(port)));
    if (inet_pton(AF_INET, host, &address.sin_addr) == 1) return true;
#if defined(__PROSPERO__)
    const int pool = sceNetPoolCreate("movian_udp_dns", 16 * 1024, 0);
    if (pool < 0) return false;
    const int resolver = sceNetResolverCreate("movian_udp_dns", pool, 0);
    const bool resolved = resolver >= 0 &&
        sceNetResolverStartNtoa(resolver, host, &address.sin_addr, 2500000, 1, 0) >= 0;
    if (resolver >= 0) sceNetResolverDestroy(resolver);
    sceNetPoolDestroy(pool);
    return resolved;
#else
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    addrinfo *resolved = nullptr;
    if (getaddrinfo(host, port, &hints, &resolved) != 0 || !resolved) return false;
    address = *reinterpret_cast<const sockaddr_in *>(resolved->ai_addr);
    freeaddrinfo(resolved);
    return true;
#endif
}

#if defined(MOVIAN_TORRENT_DHT)
constexpr std::uint64_t dht_lookup_ms = 40000;

bool dht_public_ipv4(const unsigned char *ip)
{
    if (!ip[0] || ip[0] >= 224) return false;
#if !defined(MOVIAN_TORRENT_TEST)
    if (ip[0] == 127 || ip[0] == 10 || (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) ||
        (ip[0] == 192 && ip[1] == 168) || (ip[0] == 169 && ip[1] == 254) ||
        (ip[0] == 100 && ip[1] >= 64 && ip[1] <= 127)) return false;
#endif
    return true;
}

// The DHT worker appends unique candidates and publishes a count. Published
// entries cannot change while the cache coordinator is consuming them.
struct DhtLookup
{
    const unsigned char (&hash)[20];
    Endpoints peers;
    std::atomic<unsigned> published{0};
    std::atomic<bool> stop{false}, done{false};
};

void publish_dht_peers(DhtLookup &job, const DhtReply &reply)
{
    const int previous = job.peers.count;
    for (std::size_t i = 0; i < reply.peer_bytes && job.peers.count < Endpoints::capacity; i += DhtReply::peer_size)
    {
        const auto *compact = reply.peers + i;
        Endpoints peer;
        if (!dht_public_ipv4(compact) || !compact_tracker_peers(compact, DhtReply::peer_size, peer))
            continue;
        auto *end = job.peers.values + job.peers.count;
        if (std::find_if(job.peers.values, end, [&](const auto &known) {
                return !std::strcmp(known, peer.values[0]);
            }) != end)
            continue;
        std::memcpy(*end, peer.values[0], sizeof peer.values[0]);
        ++job.peers.count;
    }
    if (job.peers.count != previous)
    {
        job.published.store(job.peers.count, std::memory_order_release);
        diagnostics::record("dht", "IPv4 candidates=%d", job.peers.count);
    }
}

struct DhtNode
{
    sockaddr_in address{};
    unsigned char distance[20]{};
    bool queried = false;
};

bool same_dht_node(const sockaddr_in &a, const sockaddr_in &b)
{
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

bool send_get_peers(int fd, const sockaddr_in &address, const unsigned char (&id)[20],
                    const unsigned char (&hash)[20], const unsigned char (&transaction)[4])
{
    // KRPC get_peers as used by anacrolix/dht AnnounceTraversal. All binary
    // fields have fixed lengths, so hashes and IDs may contain NUL bytes.
    unsigned char packet[100];
    std::size_t length = 0;
    auto append = [&](const void *data, std::size_t size) {
        std::memcpy(packet + length, data, size);
        length += size;
    };
    constexpr char prefix[] = "d1:ad2:id20:";
    constexpr char key[] = "9:info_hash20:";
    constexpr char query[] = "e1:q9:get_peers1:t4:";
    constexpr char suffix[] = "1:y1:qe";
    append(prefix, sizeof prefix - 1);
    append(id, sizeof id);
    append(key, sizeof key - 1);
    append(hash, sizeof hash);
    append(query, sizeof query - 1);
    append(transaction, sizeof transaction);
    append(suffix, sizeof suffix - 1);
    return sendto(fd, packet, length, 0, reinterpret_cast<const sockaddr *>(&address), sizeof address) ==
           static_cast<ssize_t>(length);
}

void lookup_dht(DhtLookup &job)
{
    Peer udp{socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP), job.stop};
    unsigned char node_id[20];
    if (udp.fd < 0 || !random_bytes(node_id, sizeof node_id))
    {
        diagnostics::record("dht", "UDP initialization failed errno=%d", errno);
        return;
    }

    // Port the bounded lookup path from TorrServer's pinned anacrolix/dht
    // AnnounceTraversal: bootstrap, query closest nodes, and stream peers.
    const auto deadline = now_ms() + dht_lookup_ms;
    constexpr unsigned max_queries = 64;
    DhtNode nodes[128];
    auto *end = nodes;
    auto add = [&](const sockaddr_in &address, const unsigned char *id) {
        const auto *ip = reinterpret_cast<const unsigned char *>(&address.sin_addr);
        if (!address.sin_port || !dht_public_ipv4(ip) ||
            std::find_if(nodes, end, [&](const DhtNode &node) {
                return same_dht_node(node.address, address);
            }) != end)
            return;
        if (end == nodes + std::size(nodes)) return;
        end->address = address;
        for (unsigned i = 0; i < sizeof end->distance; ++i)
            end->distance[i] = id ? id[i] ^ job.hash[i] : 0;
        ++end;
    };
    struct Seed { const char *host; const char *port; };
#if defined(MOVIAN_TORRENT_TEST)
    const Seed seeds[] = {{std::getenv("MOVIAN_TEST_DHT_HOST"), std::getenv("MOVIAN_TEST_DHT_PORT")}};
#else
    const Seed seeds[] = {{"dht.transmissionbt.com", "6881"}, {"router.bittorrent.com", "6881"},
                          {"dht.libtorrent.org", "25401"}, {"dht.anacrolix.link", "42069"}};
#endif
    for (const auto &seed : seeds)
    {
        if (job.stop) return;
        sockaddr_in address{};
        if (seed.host && seed.port && resolve_udp(seed.host, seed.port, address))
            add(address, nullptr);
    }
    diagnostics::record("dht", "bootstrap nodes=%d", static_cast<int>(end - nodes));

    struct Query
    {
        sockaddr_in address{};
        unsigned char transaction[4]{};
        std::uint64_t timeout = 0;
    } pending[3];
    unsigned tried = 0, responses = 0;
    while (!job.stop && now_ms() < deadline && job.peers.count < Endpoints::capacity)
    {
        unsigned active = 0;
        const auto now = now_ms();
        for (auto &slot : pending)
        {
            if (slot.timeout > now)
            {
                ++active;
                continue;
            }
            slot.timeout = 0;
            auto *next = end;
            for (auto *node = nodes; node != end && tried < max_queries; ++node)
                if (!node->queried && (next == end || std::memcmp(node->distance, next->distance, 20) < 0))
                    next = node;
            if (next == end) continue;
            next->queried = true;
            ++tried;
            slot.address = next->address;
            if (!random_bytes(slot.transaction, sizeof slot.transaction)) return;
            if (send_get_peers(udp.fd, slot.address, node_id, job.hash, slot.transaction))
            {
                slot.timeout = now + 2000;
                ++active;
            }
        }
        if (!active)
        {
            if (tried >= max_queries || std::all_of(nodes, end, [](const DhtNode &node) { return node.queried; }))
                break;
            continue;
        }
        if (!udp.wait(POLLIN, std::min(deadline, now + 200))) continue;

        char packet[2048];
        sockaddr_in source{};
        socklen_t length = sizeof source;
        const auto size = recvfrom(udp.fd, packet, sizeof packet, 0,
                                   reinterpret_cast<sockaddr *>(&source), &length);
        if (size <= 0 || size == static_cast<ssize_t>(sizeof packet) || length != sizeof source || source.sin_family != AF_INET)
            continue;
        DhtReply reply;
        if (!dht_reply(packet, static_cast<std::size_t>(size), reply)) continue;
        const auto received_at = now_ms();
        auto *slot = std::find_if(std::begin(pending), std::end(pending), [&](const Query &query) {
            return query.timeout > received_at && same_dht_node(query.address, source) &&
                   !std::memcmp(query.transaction, reply.transaction, sizeof query.transaction);
        });
        if (slot == std::end(pending)) continue;
        slot->timeout = 0;
        ++responses;
        publish_dht_peers(job, reply);
        for (std::size_t i = 0; i < reply.node_bytes; i += DhtReply::node_size)
        {
            const auto *node = reply.nodes + i;
            sockaddr_in address{};
            address.sin_family = AF_INET;
#if defined(__FreeBSD__)
            address.sin_len = sizeof address;
#endif
            std::memcpy(&address.sin_addr, node + 20, 4);
            std::memcpy(&address.sin_port, node + 24, 2);
            add(address, node);
        }
    }
    diagnostics::record("dht", "finished queries=%u replies=%u candidates=%d", tried, responses, job.peers.count);
}

void *dht_worker(void *context)
{
    auto &job = *static_cast<DhtLookup *>(context);
    lookup_dht(job);
    job.done.store(true, std::memory_order_release);
    return nullptr;
}
#endif

const char *announce_udp(const char *url, std::uint64_t left, const unsigned char (&hash)[20],
                         const char (&id)[21], std::atomic<bool> &cancel, Endpoints &endpoints)
{
    const char *authority = url + 6;
    const char *path = authority + std::strcspn(authority, "/?#");
    const char *colon = static_cast<const char *>(std::memchr(authority, ':', path - authority));
    char host[254]{}, port[6]{};
    if (!colon || colon == authority || colon - authority >= static_cast<int>(sizeof host) ||
        path - colon <= 1 || path - colon > static_cast<int>(sizeof port) ||
        std::memchr(authority, '@', path - authority) || (*path && std::strcmp(path, "/announce")))
        return "unsupported UDP tracker URL";
    std::memcpy(host, authority, colon - authority);
    std::memcpy(port, colon + 1, path - colon - 1);
    if (std::strspn(port, "0123456789") != std::strlen(port) || !std::atoi(port) || std::atoi(port) > 65535)
        return "invalid UDP tracker port";
    sockaddr_in address{};
    diagnostics::record("tracker", "UDP resolving destination");
    if (!resolve_udp(host, port, address)) return "UDP tracker DNS failed";
    diagnostics::record("tracker", "UDP destination resolved");
    Peer peer{-1, cancel};
    peer.fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    const bool connected = !cancel && peer.fd >= 0 &&
        connect(peer.fd, reinterpret_cast<const sockaddr *>(&address), sizeof address) == 0;
    if (!connected) return "UDP tracker socket failed";

    unsigned char answer[2048];
    auto exchange = [&](const unsigned char *request, std::size_t length, unsigned action,
                        std::uint32_t transaction, std::size_t minimum) -> std::size_t {
        const auto deadline = now_ms() + 3000;
        if (!peer.wait(POLLOUT, deadline) || send(peer.fd, request, length, 0) != static_cast<ssize_t>(length))
            return 0;
        while (peer.wait(POLLIN, deadline))
        {
            const auto size = recv(peer.fd, answer, sizeof answer, 0);
            if (size < 0) return 0;
            if (size < 8 || get32(answer + 4) != transaction) continue;
            if (static_cast<std::size_t>(size) < minimum || get32(answer) != action) return 0;
            return static_cast<std::size_t>(size);
        }
        return 0;
    };
    std::uint32_t nonce[3];
    if (!random_bytes(nonce, sizeof nonce))
        return "UDP tracker random source failed";
    diagnostics::record("tracker", "UDP sending connect request");
    unsigned char request[98]{};
    put64(request, 0x41727101980ULL);
    put32(request + 12, nonce[0]);
    if (!exchange(request, 16, 0, nonce[0], 16)) return "UDP tracker connect failed";
    std::memcpy(request, answer + 8, 8);
    put32(request + 8, 1);
    put32(request + 12, nonce[1]);
    std::memcpy(request + 16, hash, 20);
    std::memcpy(request + 36, id, 20);
    put64(request + 64, left);
    put32(request + 80, 2); // started
    put32(request + 88, nonce[2]);
    put32(request + 92, Endpoints::capacity);
    request[96] = 6881 >> 8;
    request[97] = 6881 & 255;
    const auto received = exchange(request, sizeof request, 1, nonce[1], 20);
    if (!received) return "UDP tracker announce failed";
    return compact_tracker_peers(answer + 20, received - 20, endpoints) ? nullptr :
                                                                       "UDP tracker returned no usable peers";
}

bool announce_tracker(const PeerInfo &info, int tracker_index,
                      const unsigned char (&info_hash)[20], const char (&id)[21],
                      char *body, Endpoints &endpoints, std::atomic<bool> &cancel)
{
    const char *tracker = info.trackers[tracker_index];
    if (!std::strncmp(tracker, "udp://", 6))
    {
        endpoints = {};
        const auto *failure = announce_udp(tracker, info.file.size, info_hash, id, cancel, endpoints);
        diagnostics::record("tracker", "announce=%d UDP candidates=%d result=%s",
                            tracker_index + 1, endpoints.count, failure ? failure : "ok");
        return !failure;
    }
    char hash[61]{}, peer_id[61]{};
    for (int i = 0; i < 20; ++i)
    {
        std::snprintf(hash + 3 * i, 4, "%%%02X", unsigned(info_hash[i]));
        std::snprintf(peer_id + 3 * i, 4, "%%%02X", unsigned(static_cast<unsigned char>(id[i])));
    }
    char url[1024];
    std::snprintf(url, sizeof url,
                  "%s%cinfo_hash=%s&peer_id=%s&port=6881&uploaded=0&downloaded=0&left=%llu&compact=1&numwant=%d&event=started",
                  tracker, std::strchr(tracker, '?') ? '&' : '?', hash, peer_id,
                  static_cast<unsigned long long>(info.file.size), Endpoints::capacity);
    plugins::HttpResponse response;
    const int rc = plugins::http_request(url, false, nullptr, 0, nullptr, 0, body, 64 * 1024, response);
    const bool found = rc >= 0 && response.status == 200 && tracker_peers(body, response.length, endpoints);
    diagnostics::record("tracker", "announce=%d status=%d result=%#x candidates=%d",
                        tracker_index + 1, response.status, static_cast<unsigned>(rc),
                        found ? endpoints.count : 0);
    if (auto *log = std::fopen(MOVIAN_LOG_PATH, "a"))
    {
        std::fprintf(log, "v52 tracker=%d status=%d rc=%#x peers=%d\n", tracker_index + 1, response.status,
                     static_cast<unsigned>(rc), found ? endpoints.count : 0);
        std::fclose(log);
    }
    return found;
}
#endif

bool connect_peer(Peer &peer, const char *address, const PeerInfo &info,
                  const unsigned char (&info_hash)[20], unsigned char *message,
                  char *error, std::size_t error_size)
{
    char id[21];
    std::snprintf(id, sizeof id, "-MV0029-%012llx",
                  static_cast<unsigned long long>(now_ms() & 0xffffffffffffull));
    if (address[0])
    {
        if (!open_peer(peer, address, info_hash, id, error, error_size))
            return false;
        return unchoke(peer, message) || fail(error, error_size, "Peer did not unchoke");
    }
#if !defined(MOVIAN_TORRENT_TEST) || defined(MOVIAN_TORRENT_TEST_TRACKER)
    auto *body = static_cast<char *>(std::malloc(64 * 1024));
    if (!body) return fail(error, error_size, "Could not allocate tracker response");
    bool connected = false, received_peers = false;
    int attempts = 0;
    for (int i = 0; i < info.tracker_count && i < 3 && !peer.cancel && !connected; ++i)
    {
        Endpoints endpoints;
        if (!announce_tracker(info, i, info_hash, id, body, endpoints, peer.cancel)) continue;
        received_peers = true;
        for (int j = 0; j < endpoints.count && attempts < 4 && !peer.cancel; ++j)
        {
            ++attempts;
            if (open_peer(peer, endpoints.values[j], info_hash, id, error, error_size) &&
                unchoke(peer, message))
            {
                connected = true;
                break;
            }
            peer.reset();
        }
        if (attempts == 4) break;
    }
    std::free(body);
    if (connected) return true;
    if (info.tracker_count == 0)
        return fail(error, error_size, "This torrent has no supported tracker");
    return fail(error, error_size, received_peers ? "Tracker peers did not connect or unchoke" :
                                                    "Trackers returned no usable peers");
#else
    (void)info;
    return fail(error, error_size, "Tracker discovery unavailable in this test build");
#endif
}

bool request_block(Peer &peer, unsigned index, unsigned offset, unsigned length,
                   std::uint64_t deadline)
{
    unsigned char request[17]{};
    put32(request, 13);
    request[4] = 6;
    put32(request + 5, index);
    put32(request + 9, offset);
    put32(request + 13, length);
    return peer.transfer(request, sizeof request, true, deadline);
}

bool receive_piece(Peer &peer, unsigned index, unsigned length, unsigned window,
                   unsigned char *message, unsigned char *dest, unsigned timeout_ms = io_timeout_ms)
{
    constexpr unsigned max_blocks = 16 * 1024 * 1024 / block_size;
    const unsigned blocks = (length + block_size - 1) / block_size;
    if (!blocks || blocks > max_blocks || !window) return false;

    bool received[max_blocks]{};
    unsigned next = 0, remaining = blocks;
    auto deadline = now_ms() + timeout_ms;
    while (remaining)
    {
        const unsigned in_flight = next - (blocks - remaining);
        const unsigned count = std::min(blocks - next, window - in_flight);
        for (unsigned end = next + count; next < end; ++next)
        {
            const unsigned offset = next * block_size;
            if (!request_block(peer, index, offset, std::min(block_size, length - offset), deadline))
                return false;
        }
        std::uint32_t size;
        if (!read_message(peer, message, size, deadline) || (size && message[0] == 0))
            return false;
        if (!size || message[0] != 7)
            continue;
        if (size < 9 || get32(message + 1) != index)
            return false;
        const auto offset = get32(message + 5);
        const unsigned block = offset / block_size;
        if (offset % block_size || block >= next || received[block] ||
            size - 9 != std::min(block_size, length - offset))
            return false;
        std::memcpy(dest + offset, message + 9, size - 9);
        received[block] = true;
        --remaining;
        deadline = now_ms() + timeout_ms;
    }
    return true;
}

struct CacheSession
{
    const PeerInfo &info;
    const unsigned char (&hash)[20];
    VerifiedFile &stream;
    unsigned requests;
    unsigned block_timeout_ms;
    char id[21]{};
    std::atomic<bool> stop{false};
    std::atomic<unsigned> received{0};
    std::atomic<std::uint64_t> received_bytes{0};
};

struct CachePeer
{
    CacheSession *session = nullptr;
    const char *address = nullptr;
    bool manual = false;
    std::atomic<bool> done{false};
    char failure[128]{};
};

const char *serve_cache(CacheSession &job, const char *address, unsigned request_window,
                        unsigned char *&scratch, unsigned &received, bool &connected,
                        char (&connection_error)[128])
{
    Peer peer{-1, job.stop};
    unsigned char message[block_size + 9];
    if (!open_peer(peer, address, job.hash, job.id, connection_error, sizeof connection_error))
        return connection_error;
    connected = true;
    if (!unchoke(peer, message)) return "waiting for unchoke";
    const auto span = job.info.piece_length;
    while (!job.stop)
    {
        std::uint32_t index = 0;
        if (!job.stream.claim_piece(peer.bitfield_size ? peer.bitfield : nullptr,
                                    peer.bitfield_size, index))
        {
            // Receive HAVE updates when waiting for the player's demand.
            pollfd p{peer.fd, POLLIN, 0};
            const int ready = poll(&p, 1, 200);
            if (ready < 0 && errno != EINTR) return "idle poll";
            if (ready > 0 && !(p.revents & POLLIN)) return "idle disconnect";
            if (ready > 0 && !read_message(peer, message, index, now_ms() + io_timeout_ms))
                return "idle receive";
            continue;
        }
        auto *piece = job.stream.piece_buffer(index);
        if (!piece)
        {
            if (!scratch) scratch = static_cast<unsigned char *>(std::malloc(static_cast<std::size_t>(span)));
            if (!scratch)
            {
                job.stream.release_piece(index);
                return "receive buffer allocation";
            }
            piece = scratch;
        }
        const auto available = job.info.file.size - std::uint64_t(index) * span;
        const auto size = static_cast<unsigned>(available < span ? available : span);
        unsigned char digest[20];
        const char *problem = nullptr;
        if (!receive_piece(peer, index, size, request_window, message, piece, job.block_timeout_ms))
            problem = "block transfer";
        else if (!sha1(piece, size, digest) || std::memcmp(digest, job.info.pieces + 20 * index, 20))
            problem = "piece hash";
        else if (!job.stream.publish_piece(index, piece, size))
            problem = "cache publish";
        if (problem)
        {
            job.stream.release_piece(index);
            return problem;
        }
        ++received;
        job.received_bytes.fetch_add(size, std::memory_order_relaxed);
        job.received.fetch_add(1);
    }
    return nullptr;
}

void *cache_peer(void *opaque)
{
    auto &entry = *static_cast<CachePeer *>(opaque);
    auto &job = *entry.session;
    unsigned char *scratch = nullptr;
    // Allow a transient failure to reconnect without losing verified slots.
    for (const unsigned window : {job.requests, std::min(job.requests, 4u)})
    {
        if (job.stop) break;
        unsigned received = 0;
        char connection_error[128]{};
        bool connected = false;
        const char *problem = serve_cache(job, entry.address, window, scratch, received, connected,
                                          connection_error);
        if (job.stop || !problem) break;
        std::snprintf(entry.failure, sizeof entry.failure, "%s", problem);
        diagnostics::record("peer", "window=%u verified=%u reason=%s", window, received, problem);
        if (auto *log = std::fopen(MOVIAN_LOG_PATH, "a"))
        {
            std::fprintf(log, "v52 cache peer ended window=%u verified=%u reason=%s\n",
                         window, received, problem);
            std::fclose(log);
        }
        // A smaller block window can recover a stalled transfer. It cannot help
        // a tracker endpoint that refused the connection or never handshook.
        if (!connected && !entry.manual) break;
    }
    std::free(scratch);
    entry.done.store(true, std::memory_order_release);
    return nullptr;
}

bool stream_cache(const PeerInfo &info, const unsigned char (&hash)[20], const char *address,
                  std::atomic<bool> &cancel, VerifiedFile &stream,
                  char *error, std::size_t error_size, CacheStorage storage, TransferOptions options)
{
    const auto connections = std::clamp(options.connections, 1u, max_cache_connections);
    CacheSession job{info, hash, stream, std::clamp(options.requests, 1u, 32u),
                     std::clamp(options.block_timeout_seconds, 3u, 30u) * 1000};
    std::snprintf(job.id, sizeof job.id, "-MV0052-%012llx",
                  static_cast<unsigned long long>(now_ms() & 0xffffffffffffull));
#if !defined(MOVIAN_TORRENT_TEST) || defined(MOVIAN_TORRENT_TEST_TRACKER)
    std::unique_ptr<char, decltype(&std::free)> body(nullptr, std::free);
    if (!address[0] && info.tracker_count)
    {
        body.reset(static_cast<char *>(std::malloc(64 * 1024)));
        if (!body) return fail(error, error_size, "Could not allocate tracker response");
    }
#endif
    if (options.cache.size_mib && info.piece_length > std::uint64_t(options.cache.size_mib) * 1024 * 1024)
        return fail(error, error_size, "Increase Torrent cache size to fit at least one piece");
    if (!stream.open_cache(info.file.size, static_cast<std::uint32_t>(info.piece_length), storage, options.cache))
        return fail(error, error_size, storage == CacheStorage::ram ? "Could not allocate torrent RAM. Try a smaller cache size." :
                    storage == CacheStorage::usb ? "Could not open a writable USB torrent cache" :
                    "Could not open the PS5 storage torrent cache");
    diagnostics::record("torrent", "connections=%u requests=%u block_timeout_ms=%u retrackers=%d trackers=%d",
                        connections, job.requests, job.block_timeout_ms,
                        static_cast<int>(options.retrackers), info.tracker_count);
    pthread_t threads[max_cache_connections]{};
    CachePeer peers[max_cache_connections]{};
    bool active[max_cache_connections]{};
    unsigned started = 0;
    char candidates[(PeerInfo::tracker_capacity + 1) * Endpoints::capacity][sizeof Endpoints::values[0]]{};
    unsigned next_candidate = 0;
    unsigned candidate_count = 0;
    [[maybe_unused]] auto queue = [&](const auto &endpoint) {
        auto *end = candidates + candidate_count;
        if (std::find_if(candidates, end, [&](const auto &known) {
                return !std::strcmp(known, endpoint);
            }) != end)
            return;
        std::memcpy(*end, endpoint, sizeof endpoint);
        ++candidate_count;
    };
#if defined(MOVIAN_TORRENT_DHT)
    DhtLookup lookup{hash, {}};
    pthread_t lookup_thread{};
    bool use_dht = !address[0] && !info.private_torrent;
#if defined(MOVIAN_TORRENT_TEST)
    use_dht = use_dht && std::getenv("MOVIAN_TEST_DHT_HOST");
#endif
    const bool dht_started = use_dht && pthread_create(&lookup_thread, nullptr, dht_worker, &lookup) == 0;
    if (use_dht && !dht_started) diagnostics::record("dht", "lookup thread failed");
    unsigned next_dht = 0;
#endif
    [[maybe_unused]] int next_tracker = 0;
    [[maybe_unused]] auto next_stalled_announce = now_ms() + 2000;
    bool manual_pending = address[0];
    constexpr unsigned preload_idle_ms = 90000;
    auto startup_limit = now_ms() + preload_idle_ms;
    unsigned preload_verified = 0;
    auto report_at = now_ms() + 2000;
    std::uint64_t observed_bytes = 0, reported_bytes = 0;
    bool ready = false;
    while (!cancel)
    {
        if (!ready)
        {
            const auto verified = job.received.load();
            if (verified != preload_verified)
            {
                preload_verified = verified;
                startup_limit = now_ms() + preload_idle_ms;
            }
            ready = stream.can_start_mkv();
            if (ready)
                diagnostics::record("torrent", "initial Matroska pieces verified");
            else if (now_ms() >= startup_limit)
                break;
        }
#if defined(MOVIAN_TORRENT_DHT)
        const bool dht_pending = dht_started && !lookup.done.load(std::memory_order_acquire);
        const auto published = lookup.published.load(std::memory_order_acquire);
        while (next_dht < published)
            queue(lookup.peers.values[next_dht++]);
#endif
        unsigned running = 0;
        for (unsigned i = 0; i < connections; ++i)
        {
            auto &peer = peers[i];
            if (active[i] && peer.done.load(std::memory_order_acquire))
            {
                pthread_join(threads[i], nullptr);
                active[i] = false;
            }
            if (!active[i])
            {
                const bool manual = manual_pending;
                const char *candidate = manual ? address : nullptr;
                manual_pending = false;
                if (!candidate && next_candidate < candidate_count)
                    candidate = candidates[next_candidate++];
                if (candidate)
                {
                    peer.session = &job;
                    peer.address = candidate;
                    peer.manual = manual;
                    peer.failure[0] = 0;
                    peer.done.store(false, std::memory_order_relaxed);
                    active[i] = pthread_create(&threads[i], nullptr, cache_peer, &peer) == 0;
                    started += active[i];
                }
            }
            running += active[i];
        }
        const auto now = now_ms();
        const auto bytes = job.received_bytes.load(std::memory_order_relaxed);
        if (bytes != observed_bytes)
        {
            observed_bytes = bytes;
            next_stalled_announce = now + 2000;
        }
        if (now >= report_at)
        {
            diagnostics::record("torrent", "workers=%u verified=%u bytes=%llu new_bytes=%llu window_ms=%llu",
                                running, job.received.load(), static_cast<unsigned long long>(bytes),
                                static_cast<unsigned long long>(bytes - reported_bytes),
                                static_cast<unsigned long long>(now - (report_at - 2000)));
            reported_bytes = bytes;
            report_at = now + 2000;
        }
#if !defined(MOVIAN_TORRENT_TEST) || defined(MOVIAN_TORRENT_TEST_TRACKER)
        const bool refill = running < std::min(connections, 2u) && next_candidate == candidate_count;
        const bool stalled = now >= next_stalled_announce;
        if (body && next_tracker < info.tracker_count && (refill || stalled))
        {
            Endpoints found;
            if (stalled && !refill)
                diagnostics::record("tracker", "announce while peers are stalled workers=%u", running);
            if (announce_tracker(info, next_tracker++, hash, job.id, body.get(), found, cancel))
                for (int i = 0; i < found.count; ++i)
                    queue(found.values[i]);
            next_stalled_announce = now_ms() + 2000;
            continue;
        }
#endif
        if (!running && next_candidate == candidate_count
#if defined(MOVIAN_TORRENT_DHT)
            && !dht_pending
#endif
        ) break;
        usleep(50000);
    }
    diagnostics::record("torrent", "discovery ended trackers=%d candidates=%u workers_started=%u verified=%u",
                        next_tracker, candidate_count, started, job.received.load());
    job.stop = true;
#if defined(MOVIAN_TORRENT_DHT)
    lookup.stop = true;
    if (dht_started) pthread_join(lookup_thread, nullptr);
#endif
    for (unsigned i = 0; i < connections; ++i)
        if (active[i]) pthread_join(threads[i], nullptr);
    if (auto *log = std::fopen(MOVIAN_LOG_PATH, "a"))
    {
        std::fprintf(log, "v52 piece cache workers=%u verified=%u\n", started, job.received.load());
        std::fclose(log);
    }
    if (cancel) return true;
    if (!ready && address[0] && peers[0].failure[0])
        return fail(error, error_size, peers[0].failure);
    if (!ready && !job.received)
        return fail(error, error_size, "No torrent peer supplied a verified piece");
    return fail(error, error_size, ready ? "Torrent peers stopped supplying pieces" :
                                          "Could not preload the first Matroska pieces");
}
#endif
}

bool cached_path(const char *descriptor, char (&result)[384]) noexcept
{
#if defined(MOVIAN_HAVE_FFMPEG) || defined(MOVIAN_TORRENT_TEST)
    // Older builds published completed downloads here. They remain playable.
    PeerInfo info;
    unsigned char digest[20];
    auto *data = read_peer_info(descriptor, info, digest);
    if (!data) return false;
    std::free(data);
    char hash[41]{};
    struct stat st{};
    return download_path(digest, info.file, result, hash) &&
           !stat(result, &st) && S_ISREG(st.st_mode) && st.st_size >= 0 &&
           static_cast<std::uint64_t>(st.st_size) == info.file.size;
#else
    (void)descriptor; (void)result;
    return false;
#endif
}

bool download(const char *descriptor, const char *address, char (&path)[384],
              std::atomic<bool> &cancel, std::atomic<unsigned> &progress,
              char *error, std::size_t error_size, VerifiedFile &stream, CacheStorage storage,
              TransferOptions options) noexcept
{
#if defined(MOVIAN_HAVE_FFMPEG) || defined(MOVIAN_TORRENT_TEST)
    struct FinishStream
    {
        VerifiedFile &stream;
        bool success = false;
        ~FinishStream() { stream.finish(success); }
    } finish{stream};
    PeerInfo info;
    unsigned char info_hash[20];
    auto *data = read_peer_info(descriptor, info, info_hash);
    if (!data)
        return fail(error, error_size, "Unsupported single-file v1 torrent");
    if (!address[0])
        configure_trackers(info, MOVIAN_DATA_DIR "/trackers.txt", options.retrackers);
    std::memcpy(path, info.file.path, sizeof path);
    if (info.stream_only)
    {
        const bool result = stream_cache(info, info_hash, address, cancel, stream,
                                          error, error_size, storage, options);
        std::free(data);
        return result;
    }
    Peer peer{-1, cancel};
    unsigned char message[block_size + 9];
    if (!connect_peer(peer, address, info, info_hash, message, error, error_size))
    {
        std::free(data);
        return cancel ? fail(error, error_size, "Peer download cancelled") : false;
    }
    auto *piece = static_cast<unsigned char *>(std::malloc(static_cast<std::size_t>(info.piece_length)));
    const char *problem = !piece ? "Could not allocate torrent receive buffer" :
        stream.open(info.file.size, storage) ? nullptr :
        storage == CacheStorage::ram ? "Could not allocate RAM for verified torrent data" :
        storage == CacheStorage::usb ? "Could not open a writable USB torrent cache" :
        "Could not open the PS5 storage torrent cache";
    std::uint64_t written = 0;
    for (unsigned i = 0; !problem && written < info.file.size; ++i)
    {
        const auto left = info.file.size - written;
        const unsigned length = static_cast<unsigned>(left < info.piece_length ? left : info.piece_length);
        if (!receive_piece(peer, i, length, 1, message, piece))
            problem = cancel ? "Peer download cancelled" : "Peer stopped sending torrent blocks";
        unsigned char actual[20];
        if (!problem && (!sha1(piece, length, actual) ||
                         std::memcmp(actual, info.pieces + 20 * i, 20)))
            problem = "Peer sent a piece with an invalid hash";
        if (!problem && !stream.append_verified(piece, length))
            problem = "Could not buffer verified torrent piece";
        if (!problem)
        {
            written += length;
            progress.store(static_cast<unsigned>(written * 100 / info.file.size),
                           std::memory_order_relaxed);
        }
    }
    std::free(piece);
    std::free(data);
    if (cancel && !problem)
        problem = "Peer download cancelled";
    if (problem)
        return fail(error, error_size, problem);
    finish.success = true;
    return true;
#else
    (void)descriptor; (void)address; (void)path; (void)cancel; (void)progress; (void)storage; (void)options;
    stream.finish(false);
    return fail(error, error_size, "Peer playback unavailable");
#endif
}
}
