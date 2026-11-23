// SPDX-License-Identifier: GPL-3.0-or-later
#include "stpp.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#if defined(__PROSPERO__)
#include <sys/sysctl.h>
#endif
extern "C" {
#if defined(MOVIAN_STPP_TEST) || defined(MOVIAN_HOST_PREVIEW)
unsigned char *SHA1(const unsigned char *, std::size_t, unsigned char *);
#else
#include <libavutil/sha.h>
#include <libavutil/mem.h>
#endif
}

namespace movian::stpp {
namespace {
using Bytes = std::vector<uint8_t>;
constexpr size_t max_frame = 65536, max_output = 1024 * 1024;
constexpr size_t max_subscriptions = 2048, max_refs = 65536;
constexpr uint32_t missing = UINT32_MAX;
struct Lock {
    pthread_mutex_t &mutex;
    explicit Lock(pthread_mutex_t &value) : mutex(value) { pthread_mutex_lock(&mutex); }
    ~Lock() { pthread_mutex_unlock(&mutex); }
};
uint64_t millis() {
    timespec now{}; clock_gettime(CLOCK_MONOTONIC, &now);
    return uint64_t(now.tv_sec) * 1000 + uint64_t(now.tv_nsec) / 1000000;
}
// The starter's minimal C++ runtime supports vector/string but does not ship
// libc++'s map/hash-table runtime. Keep bounded integer tables in sorted vectors.
template <typename Value> struct Table {
    using Pair = std::pair<uint32_t, Value>;
    std::vector<Pair> entries;
    auto begin() { return entries.begin(); }
    auto end() { return entries.end(); }
    auto end() const { return entries.end(); }
    size_t size() const { return entries.size(); }
    auto position(uint32_t key) {
        return std::lower_bound(entries.begin(), entries.end(), key,
            [](const Pair &entry, uint32_t value) { return entry.first < value; });
    }
    auto position(uint32_t key) const {
        return std::lower_bound(entries.begin(), entries.end(), key,
            [](const Pair &entry, uint32_t value) { return entry.first < value; });
    }
    auto find(uint32_t key) { auto it = position(key); return it != end() && it->first == key ? it : end(); }
    auto find(uint32_t key) const { auto it = position(key); return it != end() && it->first == key ? it : end(); }
    bool count(uint32_t key) const { return find(key) != end(); }
    Value &operator[](uint32_t key) {
        auto it = position(key);
        if (it == end() || it->first != key) it = entries.insert(it, Pair{key, Value{}});
        return it->second;
    }
    void erase(uint32_t key) { auto it = find(key); if (it != end()) entries.erase(it); }
    auto erase(typename std::vector<Pair>::iterator it) { return entries.erase(it); }
};
void le32(Bytes &b, uint32_t n) { for (int i = 0; i < 4; ++i) b.push_back(uint8_t(n >> (i * 8))); }
struct Reader {
    const Bytes &b; size_t p = 0;
    bool byte(uint8_t &v) { if (p == b.size()) return false; v = b[p++]; return true; }
    bool word(uint32_t &v) {
        if (b.size() - p < 4) return false;
        v = uint32_t(b[p]) | uint32_t(b[p+1]) << 8 | uint32_t(b[p+2]) << 16 | uint32_t(b[p+3]) << 24;
        p += 4; return true;
    }
    bool str(std::string &v) {
        uint8_t n; uint32_t length;
        if (!byte(n)) return false;
        length = n; if (n == 255 && !word(length)) return false;
        if (length > 16384 || length > b.size() - p) return false;
        v.assign(reinterpret_cast<const char *>(b.data() + p), length); p += length; return true;
    }
    bool path(std::vector<std::string> &v, bool terminated) {
        while (p < b.size() && v.size() < 64) {
            uint8_t n = b[p++]; if (!n) return p == b.size() || terminated;
            if (n > b.size() - p) return false;
            v.emplace_back(reinterpret_cast<const char *>(b.data() + p), n); p += n;
        }
        return !terminated && p == b.size();
    }
};
bool nonblock(int fd) {
#if defined(__PROSPERO__)
    // Public SDK omits the console's SO_NBIO name, as in torrent_peer.cpp.
    constexpr int console_so_nbio = 0x1200;
    int one = 1; return setsockopt(fd, SOL_SOCKET, console_so_nbio, &one, sizeof(one)) == 0;
#else
    const int flags = fcntl(fd, F_GETFL, 0); return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}
bool entropy(uint8_t *out, size_t size) {
#if defined(__PROSPERO__)
    size_t n = size; return sysctlbyname("kern.arandom", out, &n, nullptr, 0) == 0 && n == size;
#else
    arc4random_buf(out, size); return true;
#endif
}
std::string accept_key(const std::string &key) {
    const auto input = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    uint8_t digest[20];
#if defined(MOVIAN_STPP_TEST) || defined(MOVIAN_HOST_PREVIEW)
    if (!SHA1(reinterpret_cast<const uint8_t *>(input.data()), input.size(), digest)) return {};
#else
    AVSHA *sha = av_sha_alloc(); if (!sha) return {};
    if (av_sha_init(sha, 160)) { av_free(sha); return {}; }
    av_sha_update(sha, reinterpret_cast<const uint8_t *>(input.data()), input.size());
    av_sha_final(sha, digest); av_free(sha);
#endif
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    for (size_t i = 0; i < sizeof(digest); i += 3) {
        uint32_t n = uint32_t(digest[i]) << 16;
        if (i + 1 < sizeof(digest)) n |= uint32_t(digest[i+1]) << 8;
        if (i + 2 < sizeof(digest)) n |= digest[i+2];
        result += alphabet[n >> 18]; result += alphabet[(n >> 12) & 63];
        result += i + 1 < sizeof(digest) ? alphabet[(n >> 6) & 63] : '=';
        result += i + 2 < sizeof(digest) ? alphabet[n & 63] : '=';
    }
    return result;
}
struct Snapshot {
    Graph graph;
    const Node *get(uint32_t id) const {
        auto it = std::lower_bound(graph.nodes.begin(), graph.nodes.end(), id,
            [](const Node &node, uint32_t value) { return node.id < value; });
        return it != graph.nodes.end() && it->id == id ? &*it : nullptr;
    }
    const Node *resolve(uint32_t id) const {
        const Node *n = get(id);
        for (unsigned depth = 0; n && n->link && depth < 64; ++depth) n = get(n->link);
        return n && !n->link ? n : nullptr;
    }
    const Node *path(uint32_t id, const std::vector<std::string> &parts) const {
        const Node *n = resolve(id);
        for (const auto &part : parts) {
            if (!n) return nullptr;
            const Node *next = nullptr;
            for (uint32_t child : n->children) { auto *c = get(child); if (c && c->name == part) { next = c; break; } }
            n = next ? resolve(next->id) : nullptr;
        }
        return n;
    }
};
struct Subscription {
    uint32_t base; std::vector<std::string> path; bool sent = false;
    Node previous; bool present = false;
};
struct Connection {
    int fd = -1; bool upgraded = false, initialized = false, dead = false;
    Bytes input, output; size_t output_at = 0;
    Table<Subscription> subscriptions;
    Table<uint32_t> refs, reverse;
    uint32_t next_ref = 1;
    uint64_t accepted = millis();
    void queue(const Bytes &payload, uint8_t op = 2) {
        if (output.size() - output_at + payload.size() + 10 > max_output) { dead = true; return; }
        if (output_at) { output.erase(output.begin(), output.begin() + output_at); output_at = 0; }
        output.push_back(0x80 | op);
        if (payload.size() < 126) output.push_back(uint8_t(payload.size()));
        else if (payload.size() <= 65535) { output.push_back(126); output.push_back(payload.size() >> 8); output.push_back(payload.size()); }
        else { output.push_back(127); for (int i = 7; i >= 0; --i) output.push_back(uint8_t(uint64_t(payload.size()) >> (8*i))); }
        output.insert(output.end(), payload.begin(), payload.end());
    }
    void notify(uint8_t type, uint32_t sid, const Bytes &payload = {}) {
        Bytes b{4, type}; le32(b, sid); b.insert(b.end(), payload.begin(), payload.end()); queue(b);
    }
    uint32_t export_ref(uint32_t id) {
        auto it = reverse.find(id); if (it != reverse.end()) return it->second;
        if (refs.size() >= max_refs || next_ref == UINT32_MAX) { dead = true; return 0; }
        const auto ref = next_ref++; refs[ref] = id; reverse[id] = ref; return ref;
    }
    uint32_t base(uint32_t ref) const {
        if (!ref) return 0;
        auto it = refs.find(ref); return it == refs.end() ? missing : it->second;
    }
};
std::string lower(std::string s) { for (char &c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; return s; }
bool http(Connection &c) {
    std::string request(c.input.begin(), c.input.end()); auto end = request.find("\r\n\r\n");
    if (end == std::string::npos) { if (c.input.size() > 8192) c.dead = true; return false; }
    if (end > 8192 || request.rfind("GET /api/stpp HTTP/1.1\r\n", 0)) { c.dead = true; return false; }
    std::string upgrade, connection, key_header, version;
    bool has_version = false;
    size_t p = request.find("\r\n") + 2;
    while (p < end) {
        size_t finish = request.find("\r\n", p), colon = request.find(':', p);
        if (colon >= finish) { c.dead = true; return false; }
        std::string value = request.substr(colon + 1, finish - colon - 1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
        auto name = lower(request.substr(p, colon - p));
        if (name == "upgrade") upgrade = std::move(value);
        else if (name == "connection") connection = std::move(value);
        else if (name == "sec-websocket-key") key_header = std::move(value);
        else if (name == "sec-websocket-version") { version = std::move(value); has_version = true; }
        p = finish + 2;
    }
    if (lower(upgrade) != "websocket" || lower(connection).find("upgrade") == std::string::npos ||
        key_header.empty() || key_header.size() > 128 || (has_version && version != "13")) { c.dead = true; return false; }
    auto key = accept_key(key_header); if (key.empty()) { c.dead = true; return false; }
    auto response = "HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Accept: " + key + "\r\n\r\n";
    c.output.insert(c.output.end(), response.begin(), response.end());
    c.input.erase(c.input.begin(), c.input.begin() + end + 4); c.upgraded = true; return true;
}
void update(Connection &c, const Snapshot &s) {
    for (auto &[sid, sub] : c.subscriptions) {
        const Node *n = sub.base == missing ? nullptr : s.path(sub.base, sub.path);
        const bool present = n != nullptr;
        if (!present) {
            if (!sub.sent || sub.present) c.notify(0, sid);
            sub.sent = true; sub.present = false; continue;
        }
        const bool initial = !sub.sent || !sub.present || sub.previous.id != n->id || sub.previous.type != n->type;
        const Node &old = sub.previous;
        if (initial || n->integer != old.integer || n->real != old.real || n->text != old.text || n->string_kind != old.string_kind) {
            Bytes value;
            switch (n->type) {
            case Type::Void: c.notify(0, sid); break;
            case Type::Integer: le32(value, uint32_t(n->integer)); c.notify(1, sid, value); break;
            case Type::Float: { uint32_t bits; std::memcpy(&bits, &n->real, 4); le32(value, bits); c.notify(2, sid, value); break; }
            case Type::String: value.push_back(n->string_kind); value.insert(value.end(), n->text.begin(), n->text.end()); c.notify(3, sid, value); break;
            case Type::Directory: if (initial) c.notify(5, sid); break;
            }
        }
        if (n->type == Type::Directory) {
            auto current = initial ? std::vector<uint32_t>{} : old.children;
            for (auto it = current.begin(); it != current.end();) {
                if (std::find(n->children.begin(), n->children.end(), *it) == n->children.end()) {
                    Bytes ref; le32(ref, c.export_ref(*it)); c.notify(8, sid, ref); it = current.erase(it);
                } else ++it;
            }
            // Initial vector is one native add-vector; subsequent insertions and
            // moves preserve exact ordering without clearing existing child models.
            if (initial && !n->children.empty()) {
                Bytes refs; for (auto id : n->children) le32(refs, c.export_ref(id)); c.notify(6, sid, refs);
            } else for (size_t i = 0; i < n->children.size(); ++i) {
                const auto id = n->children[i]; if (i < current.size() && current[i] == id) continue;
                auto found = std::find(current.begin(), current.end(), id);
                const bool move = found != current.end(); if (move) current.erase(found);
                const uint32_t before = i < current.size() ? c.export_ref(current[i]) : 0;
                Bytes refs;
                if (!move && before) le32(refs, before);
                le32(refs, c.export_ref(id)); if (move && before) le32(refs, before);
                c.notify(move ? 9 : before ? 7 : 6, sid, refs);
                current.insert(current.begin() + i, id);
            }
            if ((initial && n->selected) || (!initial && old.selected != n->selected)) {
                Bytes ref; le32(ref, n->selected ? c.export_ref(n->selected) : 0); c.notify(10, sid, ref);
            }
        }
        sub.previous = *n; sub.sent = true; sub.present = true;
    }
    // Send removals using the original exported reference, then revoke it.
    for (auto it = c.refs.begin(); it != c.refs.end();) {
        if (!s.get(it->second)) {
            for (auto &[sid, sub] : c.subscriptions) { (void)sid; if (sub.base == it->second) sub.base = missing; }
            c.reverse.erase(it->second); it = c.refs.erase(it);
        } else ++it;
    }
}
}

struct Server::Impl {
    mutable pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    Snapshot snapshot;
    uint64_t revision = 0;
    std::vector<Command> commands;
    std::string failure;
    std::atomic<bool> running{false};
    std::atomic<uint16_t> bound_port{0};
    pthread_t worker{};
    bool worker_created = false;
    int listener = -1;
    std::array<uint8_t, 16> instance{};
    void enqueue(Command command) {
        Lock lock(mutex);
        if (commands.size() < 256) commands.push_back(std::move(command));
    }
    void message(Connection &c, const Bytes &b, const Snapshot &s) {
        if (b.empty()) return;
        if (b[0] == 0) {
            if (b.size() != 3 || b[1] != 3) { c.dead = true; return; }
            Bytes reply{0, 3}; reply.insert(reply.end(), instance.begin(), instance.end()); reply.push_back(0); c.queue(reply); c.initialized = true; return;
        }
        if (!c.initialized) return;
        Reader r{b, 1}; uint32_t sid, ref; uint8_t tag;
        if (b[0] == 2) { if (r.word(sid) && r.p == b.size()) c.subscriptions.erase(sid); return; }
        if (b[0] == 1) {
            uint8_t a, d; Subscription sub{};
            if (!r.word(sid) || !r.word(ref) || !r.byte(a) || !r.byte(d) || !r.path(sub.path, false)) return;
            if (c.subscriptions.size() >= max_subscriptions && !c.subscriptions.count(sid)) { c.dead = true; return; }
            sub.base = c.base(ref); c.subscriptions[sid] = std::move(sub); update(c, s); return;
        }
        if (b[0] != 3 && b[0] != 5) return;
        std::vector<std::string> path;
        if (!r.word(ref) || !r.path(path, true) || !r.byte(tag)) return;
        auto id = c.base(ref); auto *n = id == missing ? nullptr : s.path(id, path);
        if (!n || n->binding.empty()) return;
        Command command; command.binding = n->binding;
        if (b[0] == 3) {
            uint32_t bits;
            if (tag == 13 && r.p == b.size()) {
                command.kind = Command::Kind::ToggleInteger;
                enqueue(std::move(command)); return;
            }
            switch (tag) {
            case 0: command.kind = Command::Kind::SetVoid; break;
            case 1: if (!r.word(bits)) return; command.kind = Command::Kind::SetInteger; std::memcpy(&command.integer, &bits, 4); break;
            case 2: if (!r.word(bits)) return; command.kind = Command::Kind::SetFloat; std::memcpy(&command.real, &bits, 4); if (!std::isfinite(command.real)) return; break;
            case 3: if (!r.byte(command.string_kind) || b.size() - r.p > 16384) return; command.kind = Command::Kind::SetString; command.text.assign(reinterpret_cast<const char *>(b.data() + r.p), b.size() - r.p); r.p = b.size(); break;
            default: return;
            }
            if (r.p == b.size()) enqueue(std::move(command));
            return;
        }
        if (tag == 1) {
            std::vector<Command> actions;
            while (r.p < b.size() && actions.size() < 32) {
                uint8_t count; if (!r.byte(count) || !count || count > b.size() - r.p) return;
                Command action = command; action.kind = Command::Kind::Action;
                action.text.assign(reinterpret_cast<const char *>(b.data() + r.p), count); r.p += count; actions.push_back(std::move(action));
            }
            if (r.p == b.size()) for (auto &action : actions) enqueue(std::move(action));
        } else if (tag == 14) {
            uint8_t mask; if (!r.byte(mask) || (mask & ~63)) return;
            std::string url, ignored; uint32_t ignored_ref; std::vector<std::string> ignored_path;
            if ((mask & 1) && !r.str(url)) return;
            if ((mask & 2) && !r.str(ignored)) return;
            for (int bit : {4, 8}) if (mask & bit) { ignored_path.clear(); if (!r.word(ignored_ref) || !r.path(ignored_path, true)) return; }
            if ((mask & 16) && !r.str(ignored)) return;
            if ((mask & 32) && !r.str(ignored)) return;
            if (r.p == b.size() && (mask & 1)) { command.kind = Command::Kind::OpenURL; command.text = std::move(url); enqueue(std::move(command)); }
        } else if (tag == 27 && b.size() - r.p <= 16384) {
            command.kind = Command::Kind::Dynamic; command.text.assign(reinterpret_cast<const char *>(b.data() + r.p), b.size() - r.p); enqueue(std::move(command));
        }
    }
    void frames(Connection &c, const Snapshot &s) {
        while (!c.dead && c.input.size() >= 2) {
            const uint8_t first = c.input[0], second = c.input[1], op = first & 15;
            if (!(first & 128) || (first & 112)) { c.dead = true; return; }
            size_t at = 2; uint64_t size = second & 127;
            if (size == 126) { if (c.input.size() < 4) return; size = uint64_t(c.input[2]) << 8 | c.input[3]; at = 4; }
            else if (size == 127) { if (c.input.size() < 10) return; size = 0; for (int i=2; i<10; ++i) size = (size << 8) | c.input[i]; at = 10; }
            if (size > max_frame || ((op & 8) && size > 125)) { c.dead = true; return; }
            const bool masked = second & 128; const size_t mask_at = at; if (masked) at += 4;
            if (c.input.size() < at + size) return;
            Bytes payload(c.input.begin() + at, c.input.begin() + at + size);
            if (masked) for (size_t i=0; i<payload.size(); ++i) payload[i] ^= c.input[mask_at + i % 4];
            c.input.erase(c.input.begin(), c.input.begin() + at + size);
            if (op == 2) message(c, payload, s);
            else if (op == 9) c.queue(payload, 10);
            else if (op == 8) c.dead = true;
            else if (op != 10 && op != 1) c.dead = true;
        }
    }
    void run() {
        std::vector<Connection> clients;
        Snapshot s;
        uint64_t seen_revision = UINT64_MAX;
        while (running) {
            { Lock lock(mutex); if (seen_revision != revision) { s = snapshot; seen_revision = revision; } }
            std::vector<pollfd> fds{{listener, POLLIN, 0}};
            for (auto &c : clients) {
                if (c.initialized) update(c, s);
                fds.push_back({c.fd, short(POLLIN | (c.output_at < c.output.size() ? POLLOUT : 0)), 0});
            }
            const int result = ::poll(fds.data(), fds.size(), 20);
            if (result < 0 && errno != EINTR) break;
            // Process the old client list before accepting, keeping poll indices valid.
            for (size_t i=0; i<clients.size(); ++i) {
                auto &c = clients[i]; auto events = fds[i+1].revents;
                if (events & (POLLERR | POLLHUP | POLLNVAL)) c.dead = true;
                if (!c.dead && (events & POLLIN)) {
                    uint8_t buffer[8192]; auto n = recv(c.fd, buffer, sizeof(buffer), 0);
                    if (n > 0) {
                        if (c.input.size() + size_t(n) > max_frame + 32) c.dead = true;
                        else { c.input.insert(c.input.end(), buffer, buffer + n); if ((c.upgraded || http(c)) && !c.dead) frames(c, s); }
                    } else if (!n || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) c.dead = true;
                }
                if (!c.dead && (events & POLLOUT) && c.output_at < c.output.size()) {
#ifdef MSG_NOSIGNAL
                    constexpr int send_flags = MSG_NOSIGNAL;
#else
                    constexpr int send_flags = 0;
#endif
                    auto n = send(c.fd, c.output.data() + c.output_at, c.output.size() - c.output_at, send_flags);
                    if (n > 0) { c.output_at += size_t(n); if (c.output_at == c.output.size()) { c.output.clear(); c.output_at = 0; } }
                    else if (!n || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) c.dead = true;
                }
                if (!c.initialized && millis() - c.accepted > 10000) c.dead = true;
            }
            for (auto it=clients.begin(); it!=clients.end();) { if (it->dead) { close(it->fd); it = clients.erase(it); } else ++it; }
            if (fds[0].revents & POLLIN) {
                int fd = accept(listener, nullptr, nullptr);
                if (fd >= 0) {
                    if (clients.size() >= 4 || !nonblock(fd)) close(fd);
                    else {
#ifdef SO_NOSIGPIPE
                        int one = 1; setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
                        Connection c; c.fd = fd; clients.push_back(std::move(c));
                    }
                }
            }
        }
        for (auto &c : clients) close(c.fd);
        running = false;
    }
    static void *run_entry(void *value) { static_cast<Impl *>(value)->run(); return nullptr; }
};
Server::Server() : impl_(std::make_unique<Impl>()) {}
Server::~Server() { stop(); }
bool Server::start(uint16_t port, const std::string &address) {
    stop(); auto &p = *impl_;
    sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_port = htons(port);
#if defined(__PROSPERO__)
    sa.sin_len = sizeof(sa);
#endif
    std::string failure;
    if (inet_pton(AF_INET, address.c_str(), &sa.sin_addr) != 1) failure = "STPP requires a numeric IPv4 bind address";
    else if (!entropy(p.instance.data(), p.instance.size())) failure = "STPP instance entropy unavailable";
    else {
        p.listener = socket(AF_INET, SOCK_STREAM, 0);
        if (p.listener < 0) failure = "STPP socket: " + std::to_string(errno);
        else {
            int one=1; setsockopt(p.listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
            if (!nonblock(p.listener) || bind(p.listener, reinterpret_cast<sockaddr *>(&sa), sizeof(sa)) || listen(p.listener, 4)) failure = "STPP listener: " + std::to_string(errno);
            else {
                socklen_t size = sizeof(sa);
                if (getsockname(p.listener, reinterpret_cast<sockaddr *>(&sa), &size)) failure = "STPP getsockname: " + std::to_string(errno);
                else p.bound_port = ntohs(sa.sin_port);
            }
        }
    }
    { Lock lock(p.mutex); p.failure = failure; }
    if (!failure.empty()) { if (p.listener >= 0) close(p.listener); p.listener = -1; return false; }
    p.running = true;
    const int result = pthread_create(&p.worker, nullptr, Impl::run_entry, &p);
    if (result) {
        p.running = false; close(p.listener); p.listener = -1; p.bound_port = 0;
        Lock lock(p.mutex); p.failure = "STPP worker: " + std::to_string(result); return false;
    }
    p.worker_created = true; return true;
}
void Server::stop() {
    auto &p = *impl_; p.running = false;
    if (p.worker_created) { pthread_join(p.worker, nullptr); p.worker_created = false; }
    if (p.listener >= 0) close(p.listener);
    p.listener = -1; p.bound_port = 0;
    Lock lock(p.mutex); p.commands.clear();
}
bool Server::publish(Graph graph) {
    if (graph.nodes.size() > 32768) return false;
    Snapshot next; next.graph = std::move(graph);
    std::sort(next.graph.nodes.begin(), next.graph.nodes.end(), [](const Node &a, const Node &b) { return a.id < b.id; });
    for (size_t i=0; i<next.graph.nodes.size(); ++i) {
        const auto &n = next.graph.nodes[i];
        if (n.id == missing || n.name.size() > 255 || n.text.size() > 16384 || n.binding.size() > 1024 || n.children.size() > 4096 || !std::isfinite(n.real) || (i && next.graph.nodes[i-1].id == n.id)) return false;
    }
    if (!next.get(0)) return false;
    for (auto &n : next.graph.nodes) {
        auto seen = n.children; std::sort(seen.begin(), seen.end());
        for (size_t i=0; i<seen.size(); ++i) if (!next.get(seen[i]) || (i && seen[i-1] == seen[i])) return false;
        if ((n.link && !next.resolve(n.id)) || (n.selected && !std::binary_search(seen.begin(), seen.end(), n.selected))) return false;
    }
    Lock lock(impl_->mutex); impl_->snapshot = std::move(next); ++impl_->revision; return true;
}
bool Server::poll(Command &command) {
    Lock lock(impl_->mutex);
    if (impl_->commands.empty()) return false;
    command = std::move(impl_->commands.front()); impl_->commands.erase(impl_->commands.begin()); return true;
}
uint16_t Server::port() const { return impl_->bound_port; }
std::string Server::error() const { Lock lock(impl_->mutex); return impl_->failure; }
}
