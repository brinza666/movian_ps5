// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <pthread.h>

namespace movian::torrent
{
constexpr int max_files = 64;
enum class CacheStorage { ram, internal, usb };
enum class RetrackersMode { original, add, replace, remove };
enum class TrackerProtocols { tcp_udp, tcp, udp }; // Observed WSL chooser order.
constexpr unsigned max_cache_connections = 8;
struct CacheOptions
{
    unsigned size_mib = 0; // Auto
    unsigned read_ahead_percent = 100;
    int preload_percent = -1; // Auto
};

struct TransferOptions
{
    CacheOptions cache;
    unsigned connections = 4;
    unsigned requests = 16;
    unsigned block_timeout_seconds = 6;
    RetrackersMode retrackers = RetrackersMode::add;
    TrackerProtocols tracker_protocols = TrackerProtocols::tcp_udp;
};

constexpr unsigned cache_limit_mib(CacheStorage storage) noexcept
{
    return storage == CacheStorage::ram ? 64 : 512;
}

struct File
{
    char path[384];
    std::uint64_t size;
};

struct Metadata
{
    char title[128];
    File files[max_files];
    int count;
    bool direct_peer;
    bool has_trackers;
    bool stream_only;
};

struct PeerInfo
{
    static constexpr int embedded_tracker_limit = 24;
    static constexpr int tracker_capacity = embedded_tracker_limit + 12; // Room for all supported defaults.
    const char *info = nullptr;
    std::size_t info_size = 0;
    const char *pieces = nullptr;
    std::size_t hash_bytes = 0;
    std::uint64_t piece_length = 0;
    File file{};
    char trackers[tracker_capacity][512]{};
    int tracker_count = 0;
    bool private_torrent = false;
    bool stream_only = false;
};

struct Endpoints
{
    static constexpr int capacity = 24;
    char values[capacity][24]{};
    int count = 0;
};

bool inspect(const char *path, Metadata &result) noexcept;
bool inspect_bytes(const char *data, std::size_t size, Metadata &result) noexcept;
bool local_path(const char *descriptor, const File &file, char (&result)[384]) noexcept;
bool cached_path(const char *descriptor, char (&result)[384]) noexcept;
bool magnet_hash(const char *uri, char (&hash)[41]) noexcept;
bool peer_info(const char *data, std::size_t size, PeerInfo &result) noexcept;
void configure_trackers(PeerInfo &info, const char *path, RetrackersMode mode) noexcept;
void filter_trackers(PeerInfo &info, TrackerProtocols protocols) noexcept;
bool tracker_peers(const char *data, std::size_t size, Endpoints &result) noexcept;
bool compact_tracker_peers(const unsigned char *data, std::size_t size, Endpoints &result) noexcept;
#if defined(MOVIAN_TORRENT_DHT)
struct DhtReply
{
    static constexpr std::size_t node_size = 26;
    static constexpr std::size_t peer_size = 6;
    unsigned char transaction[4]{};
    unsigned char nodes[32 * node_size]{};
    unsigned char peers[Endpoints::capacity * peer_size]{};
    std::size_t node_bytes = 0;
    std::size_t peer_bytes = 0;
};
bool dht_reply(const char *data, std::size_t size, DhtReply &result) noexcept;
#endif


// A verified contiguous buffer or rolling piece window. Reads never wait for a peer.
// reset() is called only after both sides have stopped using the transfer.
class VerifiedFile
{
public:
    enum class ReadResult { data, waiting, eof, failed };
    ~VerifiedFile();
    void reset() noexcept;
    bool open(std::uint64_t total, CacheStorage storage = CacheStorage::ram) noexcept;
    bool open_cache(std::uint64_t total, std::uint32_t piece_size,
                    CacheStorage storage = CacheStorage::ram, CacheOptions options = {}) noexcept;
    bool append_verified(const void *data, std::size_t size) noexcept;
    bool claim_piece(const unsigned char *bitfield, std::size_t size, std::uint32_t &piece) noexcept;
    unsigned char *piece_buffer(std::uint32_t piece) noexcept;
    bool publish_piece(std::uint32_t piece, const void *data, std::size_t size) noexcept;
    void release_piece(std::uint32_t piece) noexcept;
    void finish(bool success) noexcept;
    std::uint64_t size() const noexcept;
    bool complete() const noexcept;
    bool uses_cache() const noexcept;
    bool can_start_mp4() const noexcept;
    bool can_start_mkv() const noexcept;
    ReadResult read(std::uint64_t offset, void *buffer, std::size_t capacity,
                    std::size_t &count) const noexcept;
private:
    enum class State { idle, downloading, complete, failed };
    enum class SlotState { empty, requested, verified };
    struct Slot
    {
        std::uint32_t piece = 0;
        SlotState state = SlotState::empty;
    };
    static constexpr unsigned slot_count = 4096;
    bool allocate(std::size_t capacity, CacheStorage storage) noexcept;
    bool write_bytes(std::size_t offset, const void *data, std::size_t size) noexcept;
    bool read_bytes(std::size_t offset, void *data, std::size_t size) const noexcept;
    ReadResult read_cache(std::uint64_t offset, void *buffer, std::size_t capacity,
                          std::size_t &count) const noexcept;
    std::atomic<State> state_{State::idle};
    std::atomic<std::uint64_t> verified_{0};
    unsigned char *buffer_ = nullptr;
    std::size_t mapped_ = 0;
    int fd_ = -1;
    char file_[384]{};
    std::uint64_t total_ = 0;
    std::uint32_t piece_size_ = 0;
    unsigned slots_active_ = 0;
    unsigned read_ahead_slots_ = 0;
    unsigned preload_slots_ = 0;
    mutable pthread_mutex_t lock_ = PTHREAD_MUTEX_INITIALIZER;
    mutable std::uint32_t requested_ = 0;
    mutable bool reading_ = false;
    Slot slots_[slot_count]{};
};

bool download(const char *descriptor, const char *peer, char (&path)[384],
              std::atomic<bool> &cancel, std::atomic<unsigned> &progress,
              char *error, std::size_t error_size, VerifiedFile &stream,
              CacheStorage storage = CacheStorage::ram, TransferOptions options = {}) noexcept;
}
