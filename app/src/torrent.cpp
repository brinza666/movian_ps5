// SPDX-License-Identifier: GPL-3.0-or-later
#include "torrent.hpp"
#include "paths.hpp"
#include "input_file.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <limits>
#include <sys/stat.h>

namespace movian::torrent
{
namespace
{
struct Slice
{
    const char *data = nullptr;
    std::size_t size = 0;

    bool equals(const char *literal) const
    {
        return std::strlen(literal) == size && !std::memcmp(data, literal, size);
    }
};

struct Reader
{
    const char *at, *end;
    char peek() const { return at < end ? *at : '\0'; }
    bool take(char token)
    {
        if (peek() != token || at == end) return false;
        ++at;
        return true;
    }
    bool number(std::uint64_t &value)
    {
        value = 0;
        if (peek() < '0' || peek() > '9') return false;
        if (take('0')) return peek() < '0' || peek() > '9';
        while (peek() >= '0' && peek() <= '9')
        {
            const unsigned digit = static_cast<unsigned>(*at++ - '0');
            if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) return false;
            value = value * 10 + digit;
        }
        return true;
    }
    bool text(Slice &value)
    {
        std::uint64_t length;
        if (!number(length) || !take(':') || length > static_cast<std::size_t>(end - at))
            return false;
        value = {at, static_cast<std::size_t>(length)};
        at += value.size;
        return true;
    }
    bool integer(std::uint64_t &value) { return take('i') && number(value) && take('e'); }

    bool skip(int depth = 0)
    {
        if (depth >= 24) return false;
        if (take('i'))
        {
            const bool negative = take('-');
            std::uint64_t value;
            return number(value) && (!negative || value != 0) && take('e');
        }
        const char kind = peek();
        if (kind == 'l' || kind == 'd')
        {
            ++at;
            while (!take('e'))
            {
                Slice key;
                if ((kind == 'd' && !text(key)) || !skip(depth + 1)) return false;
            }
            return true;
        }
        Slice ignored;
        return text(ignored);
    }
};

bool append_component(char *path, std::size_t capacity, Slice piece)
{
    if (!piece.size || piece.equals(".") || piece.equals(".."))
        return false;
    for (std::size_t i = 0; i < piece.size; ++i)
        if (static_cast<unsigned char>(piece.data[i]) < 32 || piece.data[i] == '/' ||
            piece.data[i] == '\\' || piece.data[i] == ':')
            return false;
    auto length = std::strlen(path);
    if ((length ? 1 : 0) + length + piece.size >= capacity)
        return false;
    if (length) path[length++] = '/';
    std::memcpy(path + length, piece.data, piece.size);
    path[length + piece.size] = 0;
    return true;
}

bool read_file(Reader &reader, File &file)
{
    bool length = false;
    bool path = false;
    if (!reader.take('d')) return false;
    while (!reader.take('e'))
    {
        Slice key;
        if (!reader.text(key)) return false;
        if (key.equals("length"))
        {
            if (length || !reader.integer(file.size)) return false;
            length = true;
        }
        else if (key.equals("path"))
        {
            if (path || !reader.take('l')) return false;
            while (!reader.take('e'))
            {
                Slice part;
                if (!reader.text(part) || !append_component(file.path, sizeof file.path, part))
                    return false;
            }
            path = file.path[0] != 0;
        }
        else if (!reader.skip())
            return false;
    }
    return length && path;
}

bool read_info(Reader &reader, Metadata &out, PeerInfo *peer)
{
    bool have_title = false, single = false, multiple = false;
    std::uint64_t size = 0;
    std::uint64_t piece_length = 0;
    Slice pieces{};
    if (!reader.take('d')) return false;
    while (!reader.take('e'))
    {
        Slice key;
        if (!reader.text(key)) return false;
        if (key.equals("name"))
        {
            Slice title;
            if (have_title || !reader.text(title) ||
                !append_component(out.title, sizeof out.title, title))
                return false;
            have_title = true;
        }
        else if (key.equals("length"))
        {
            if (single || !reader.integer(size)) return false;
            single = true;
        }
        else if (key.equals("files"))
        {
            if (multiple || !reader.take('l')) return false;
            multiple = true;
            while (!reader.take('e'))
            {
                File file{};
                if (!read_file(reader, file)) return false;
                if (out.count < max_files) out.files[out.count++] = file;
            }
        }
        else if (key.equals("piece length"))
        {
            if (piece_length || !reader.integer(piece_length) || !piece_length)
                return false;
        }
        else if (key.equals("pieces"))
        {
            if (pieces.data || !reader.text(pieces)) return false;
        }
        else if (key.equals("private"))
        {
            std::uint64_t flag;
            if (!reader.integer(flag)) return false;
            if (peer) peer->private_torrent |= flag != 0;
        }
        else if (!reader.skip())
            return false;
    }
    if (!have_title || single == multiple) return false;
    out.stream_only = size > 96 * 1024 * 1024 || piece_length > 4 * 1024 * 1024;
    const auto *extension = std::strrchr(out.title, '.');
    const bool matroska = extension && (!strcasecmp(extension, ".mkv") ||
                                        !strcasecmp(extension, ".webm"));
    out.direct_peer = single && size && size <= 64ULL * 1024 * 1024 * 1024 && piece_length &&
                      piece_length <= 16 * 1024 * 1024 && (!out.stream_only || matroska) && pieces.data &&
                      ((size - 1) / piece_length + 1) * 20 == pieces.size;
    if (peer && !out.direct_peer)
        return false;
    if (peer)
    {
        peer->piece_length = piece_length;
        peer->pieces = pieces.data;
        peer->hash_bytes = pieces.size;
        peer->stream_only = out.stream_only;
    }
    if (single)
    {
        std::snprintf(out.files[0].path, sizeof out.files[0].path, "%s", out.title);
        out.files[0].size = size;
        out.count = 1;
        if (peer)
            peer->file = out.files[0];
    }
    else
        for (int i = 0; i < out.count; ++i)
        {
            char file[384];
            const int length = std::snprintf(file, sizeof file, "%s/%s", out.title, out.files[i].path);
            if (length < 0 || length >= static_cast<int>(sizeof file)) return false;
            std::memcpy(out.files[i].path, file, static_cast<std::size_t>(length) + 1);
        }
    return out.count > 0;
}

void add_tracker(Slice url, Metadata &metadata, PeerInfo *peer,
                 int limit = PeerInfo::embedded_tracker_limit)
{
    if (url.size < 8 || url.size >= sizeof(PeerInfo::trackers[0]) ||
        (std::memcmp(url.data, "http://", 7) && std::memcmp(url.data, "https://", 8) &&
         std::memcmp(url.data, "udp://", 6)))
        return;
    for (std::size_t i = 0; i < url.size; ++i)
        if (static_cast<unsigned char>(url.data[i]) <= ' ' || url.data[i] == '#')
            return;
    metadata.has_trackers = true;
    if (!peer || peer->tracker_count >= limit)
        return;
    for (int i = 0; i < peer->tracker_count; ++i)
        if (url.equals(peer->trackers[i]))
            return;
    auto *destination = peer->trackers[peer->tracker_count++];
    std::memcpy(destination, url.data, url.size);
    destination[url.size] = 0;
}

bool parse(const char *data, std::size_t size, Metadata &out, PeerInfo *peer = nullptr)
{
    Reader reader{data, data + size};
    bool found = false;
    if (!reader.take('d')) return false;
    while (!reader.take('e'))
    {
        Slice key;
        if (!reader.text(key)) return false;
        if (key.equals("info"))
        {
            const char *start = reader.at;
            if (found || !read_info(reader, out, peer)) return false;
            if (peer)
            {
                peer->info = start;
                peer->info_size = static_cast<std::size_t>(reader.at - start);
            }
            found = true;
        }
        else if (key.equals("announce"))
        {
            Slice tracker;
            if (!reader.text(tracker)) return false;
            add_tracker(tracker, out, peer);
        }
        else if (key.equals("announce-list") && reader.take('l'))
        {
            while (!reader.take('e'))
            {
                if (!reader.take('l'))
                {
                    if (!reader.skip()) return false;
                    continue;
                }
                while (!reader.take('e'))
                {
                    Slice tracker;
                    if (!reader.text(tracker)) return false;
                    add_tracker(tracker, out, peer);
                }
            }
        }
        else if (!reader.skip())
            return false;
    }
    return found && reader.at == reader.end;
}
}

bool peer_info(const char *data, std::size_t size, PeerInfo &result) noexcept
{
    result = {};
    Metadata metadata{};
    return data && size && parse(data, size, metadata, &result);
}

void configure_trackers(PeerInfo &info, const char *path, RetrackersMode mode) noexcept
{
    if (info.private_torrent || mode == RetrackersMode::original) return;
    if (mode == RetrackersMode::replace || mode == RetrackersMode::remove) info.tracker_count = 0;
    if (mode == RetrackersMode::remove) return;
    auto *file = std::fopen(path, "rb");
    if (!file) file = std::fopen(MOVIAN_APP0 "/assets/trackers.txt", "rb");
    if (!file) return;
    Metadata metadata{};
    char line[514];
    for (int lines = 0; lines < 64 && info.tracker_count < PeerInfo::tracker_capacity &&
         std::fgets(line, sizeof line, file); ++lines)
    {
        const auto length = std::strcspn(line, "\r\n");
        if (length == sizeof line - 1) break;
        add_tracker({line, length}, metadata, &info, PeerInfo::tracker_capacity);
    }
    std::fclose(file);
}

void filter_trackers(PeerInfo &info, TrackerProtocols protocols) noexcept
{
    int kept = 0;
    for (int i = 0; i < info.tracker_count; ++i) {
        const bool udp = !std::strncmp(info.trackers[i], "udp://", 6);
        const bool tcp = !std::strncmp(info.trackers[i], "http://", 7) ||
                         !std::strncmp(info.trackers[i], "https://", 8);
        if ((udp && protocols != TrackerProtocols::tcp) ||
            (tcp && protocols != TrackerProtocols::udp)) {
            if (kept != i) std::memcpy(info.trackers[kept], info.trackers[i], sizeof info.trackers[0]);
            ++kept;
        }
    }
    info.tracker_count = kept;
}

bool compact_tracker_peers(const unsigned char *data, std::size_t size, Endpoints &result) noexcept
{
    result = {};
    if (!data || size % 6) return false;
    for (std::size_t i = 0; i < size && result.count < Endpoints::capacity; i += 6)
    {
        const auto *ip = data + i;
        const unsigned port = (unsigned(ip[4]) << 8) | ip[5];
        if (!port || !ip[0] || ip[0] >= 224) continue;
        char endpoint[sizeof result.values[0]];
        std::snprintf(endpoint, sizeof endpoint, "%u.%u.%u.%u:%u", unsigned(ip[0]),
                      unsigned(ip[1]), unsigned(ip[2]), unsigned(ip[3]), port);
        bool duplicate = false;
        for (int j = 0; j < result.count && !duplicate; ++j)
            duplicate = !std::strcmp(endpoint, result.values[j]);
        if (!duplicate) std::strcpy(result.values[result.count++], endpoint);
    }
    return result.count > 0;
}

bool tracker_peers(const char *data, std::size_t size, Endpoints &result) noexcept
{
    result = {};
    if (!data || !size || size > 64 * 1024) return false;
    Reader reader{data, data + size};
    bool found = false;
    if (!reader.take('d')) return false;
    while (!reader.take('e'))
    {
        Slice key;
        if (!reader.text(key)) return false;
        if (key.equals("failure reason")) return false;
        if (!key.equals("peers") || reader.peek() == 'l')
        {
            if (!reader.skip()) return false;
            continue;
        }
        Slice peers;
        if (found || !reader.text(peers) || peers.size % 6) return false;
        found = true;
        compact_tracker_peers(reinterpret_cast<const unsigned char *>(peers.data), peers.size, result);
    }
    return found && reader.at == reader.end && result.count > 0;
}

#if defined(MOVIAN_TORRENT_DHT)
bool dht_reply(const char *data, std::size_t size, DhtReply &result) noexcept
{
    result = {};
    Reader packet{data, data + size};
    bool transaction = false, response = false, values = false;
    if (!packet.take('d')) return false;
    while (!packet.take('e'))
    {
        Slice key;
        if (!packet.text(key)) return false;
        if (key.equals("t"))
        {
            Slice id;
            if (transaction || !packet.text(id) || id.size != sizeof result.transaction) return false;
            std::memcpy(result.transaction, id.data, id.size);
            transaction = true;
        }
        else if (key.equals("y"))
        {
            Slice type;
            if (response || !packet.text(type) || !type.equals("r")) return false;
            response = true;
        }
        else if (key.equals("r"))
        {
            if (values || !packet.take('d')) return false;
            bool node_id = false;
            while (!packet.take('e'))
            {
                Slice field, text;
                if (!packet.text(field)) return false;
                if (field.equals("id"))
                {
                    if (node_id || !packet.text(text) || text.size != 20) return false;
                    node_id = true;
                }
                else if (field.equals("nodes"))
                {
                    if (!packet.text(text) || text.size % DhtReply::node_size) return false;
                    result.node_bytes = text.size < sizeof result.nodes ? text.size : sizeof result.nodes;
                    std::memcpy(result.nodes, text.data, result.node_bytes);
                }
                else if (field.equals("values"))
                {
                    if (!packet.take('l')) return false;
                    while (!packet.take('e'))
                    {
                        if (!packet.text(text)) return false;
                        if (text.size == DhtReply::peer_size && result.peer_bytes < sizeof result.peers)
                        {
                            std::memcpy(result.peers + result.peer_bytes, text.data, text.size);
                            result.peer_bytes += text.size;
                        }
                    }
                }
                else if (!packet.skip()) return false;
            }
            if (!node_id) return false;
            values = true;
        }
        else if (!packet.skip()) return false;
    }
    return packet.at == packet.end && transaction && response && values;
}
#endif

bool inspect(const char *path, Metadata &result) noexcept
{
    result = {};
    std::size_t size = 0;
    auto *data = input::read_bounded(path, 1024 * 1024, size);
    const bool valid = data && parse(data, size, result);
    std::free(data);
    if (!valid) result = {};
    return valid;
}

bool inspect_bytes(const char *data, std::size_t size, Metadata &result) noexcept
{
    result = {};
    return data && size && size <= 1024 * 1024 && parse(data, size, result);
}

bool local_path(const char *descriptor, const File &file, char (&result)[384]) noexcept
{
    const char *slash = std::strrchr(descriptor, '/');
    if (!slash || !file.path[0]) return false;
    const int length = std::snprintf(result, sizeof result, "%.*s/%s",
                                     static_cast<int>(slash - descriptor), descriptor, file.path);
    struct stat info{};
    return length > 0 && length < static_cast<int>(sizeof result) &&
           stat(result, &info) == 0 && S_ISREG(info.st_mode) &&
           info.st_size >= 0 && static_cast<std::uint64_t>(info.st_size) == file.size;
}

bool magnet_hash(const char *uri, char (&hash)[41]) noexcept
{
    constexpr char prefix[] = "xt=urn:btih:";
    if (std::strncmp(uri, "magnet:?", 8)) return false;
    for (const char *part = uri + 8; *part; )
    {
        const auto size = std::strcspn(part, "&");
        if (size == sizeof prefix - 1 + 40 && !strncasecmp(part, prefix, sizeof prefix - 1))
        {
            const char *value = part + sizeof prefix - 1;
            for (int i = 0; i < 40; ++i)
                if (!std::strchr("0123456789abcdefABCDEF", value[i]))
                    return false;
            std::memcpy(hash, value, 40);
            hash[40] = 0;
            return true;
        }
        part += size;
        if (*part == '&') ++part;
    }
    return false;
}
}
