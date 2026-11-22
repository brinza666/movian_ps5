#include "torrent.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

int main()
{
    using Stream = movian::torrent::VerifiedFile;
    constexpr unsigned piece_size = 8 * 1024 * 1024;
    constexpr std::uint64_t logical_size = 19100857385ULL;
    Stream cache;
    assert(cache.open_cache(logical_size, piece_size));
    assert(cache.size() == logical_size);
    std::vector<unsigned char> piece(piece_size, 0x69);
    std::uint32_t index = 0, second = 0;
    assert(cache.claim_piece(nullptr, 0, index) && index == 0);
    assert(cache.claim_piece(nullptr, 0, second) && second == 1);
    auto *reserved = cache.piece_buffer(second);
    assert(reserved && !cache.piece_buffer(2));
    std::memcpy(reserved, piece.data(), piece.size());
    assert(cache.publish_piece(second, reserved, piece.size()));
    assert(!cache.can_start_mkv());
    assert(!cache.piece_buffer(second));
    unsigned char bytes[8]{};
    std::size_t count;
    assert(cache.read(0, bytes, sizeof bytes, count) == Stream::ReadResult::waiting);
    constexpr unsigned char ebml[] = {0x1a, 0x45, 0xdf, 0xa3};
    std::memcpy(piece.data(), ebml, sizeof ebml);
    assert(cache.publish_piece(index, piece.data(), piece.size()));
    assert(cache.can_start_mkv());
    assert(cache.read(piece_size - 4, bytes, sizeof bytes, count) == Stream::ReadResult::data);
    assert(count == sizeof bytes && bytes[0] == 0x69 && bytes[7] == 0x69);
    constexpr unsigned target = 2048;
    constexpr auto offset = static_cast<std::uint64_t>(target) * piece_size;
    assert(cache.read(offset, bytes, sizeof bytes, count) == Stream::ReadResult::waiting);
    unsigned char bitfield[300]{};
    bitfield[target / 8] = 0x80;
    assert(cache.claim_piece(bitfield, sizeof bitfield, index) && index == target);
    assert(!cache.claim_piece(bitfield, sizeof bitfield, second));
    cache.release_piece(index);
    assert(cache.claim_piece(bitfield, sizeof bitfield, index) && index == target);
    piece[0] = 0xab;
    assert(cache.publish_piece(index, piece.data(), piece.size()));
    assert(cache.read(offset, bytes, sizeof bytes, count) == Stream::ReadResult::data);
    assert(count == sizeof bytes && bytes[0] == 0xab);
    assert(cache.read(0, bytes, sizeof bytes, count) == Stream::ReadResult::waiting);
    assert(cache.read(logical_size, bytes, sizeof bytes, count) == Stream::ReadResult::eof);
    cache.finish(false);
    assert(cache.size() == logical_size);
    assert(cache.read(offset, bytes, sizeof bytes, count) == Stream::ReadResult::data);
    assert(count == sizeof bytes && bytes[0] == 0xab);
    assert(cache.read(0, bytes, sizeof bytes, count) == Stream::ReadResult::failed);
    cache.reset();
    assert(!cache.size() && !cache.uses_cache());
    assert(cache.open(8));
    assert(cache.append_verified("test", 4));
    assert(cache.read(4, bytes, sizeof bytes, count) == Stream::ReadResult::waiting);
    assert(!cache.append_verified("overflow", 8));
    assert(cache.append_verified("data", 4));
    cache.finish(true);
    assert(cache.complete());
    assert(cache.read(0, bytes, sizeof bytes, count) == Stream::ReadResult::data);
    assert(count == 8 && bytes[0] == 't' && bytes[7] == 'a');
    assert(cache.read(8, bytes, sizeof bytes, count) == Stream::ReadResult::eof);
    cache.reset();
    std::puts("RAM cache: 19.1 GB logical seek and eviction, contiguous completion and reset passed");

    using movian::torrent::CacheStorage;
    assert(!cache.open_cache(logical_size, piece_size, CacheStorage::usb));
    std::filesystem::create_directory("download0");
    std::filesystem::create_directory("usb1");
    for (const auto storage : {CacheStorage::internal, CacheStorage::usb})
    {
        assert(cache.open_cache(logical_size, piece_size, storage));
        assert(cache.read(offset, bytes, sizeof bytes, count) == Stream::ReadResult::waiting);
        assert(cache.claim_piece(bitfield, sizeof bitfield, index) && index == target);
        assert(!cache.piece_buffer(index));
        assert(cache.publish_piece(index, piece.data(), piece.size()));
        assert(cache.read(offset, bytes, sizeof bytes, count) == Stream::ReadResult::data);
        assert(count == sizeof bytes && bytes[0] == 0xab);
        cache.reset();
        assert(cache.open(8, storage));
        assert(cache.append_verified("test", 4) && cache.append_verified("data", 4));
        assert(cache.read(0, bytes, sizeof bytes, count) == Stream::ReadResult::data);
        assert(count == sizeof bytes && std::memcmp(bytes, "testdata", sizeof bytes) == 0);
        cache.reset();
    }
    assert(std::filesystem::is_empty("download0") && std::filesystem::is_empty("usb1"));
    std::puts("Storage cache: explicit internal and USB, unavailable USB and cleanup passed");

    std::memcpy(piece.data(), ebml, sizeof ebml);
    assert(!cache.open_cache(logical_size, 2 * piece_size, CacheStorage::ram, {8, 100, 0}));
    assert(cache.open_cache(logical_size, piece_size, CacheStorage::ram, {32, 50, 75}));
    for (unsigned first = 0; first < 3; ++first)
    {
        assert(cache.claim_piece(nullptr, 0, index) && index == first);
        assert(cache.publish_piece(index, piece.data(), piece.size()));
        assert(cache.can_start_mkv() == (first == 2));
    }
    assert(!cache.claim_piece(nullptr, 0, index));
    assert(cache.read(2ULL * piece_size, bytes, sizeof bytes, count) == Stream::ReadResult::data);
    assert(cache.claim_piece(nullptr, 0, index) && index == 3);
    assert(cache.publish_piece(index, piece.data(), piece.size()));
    assert(!cache.claim_piece(nullptr, 0, index));
    assert(cache.read(3ULL * piece_size, bytes, sizeof bytes, count) == Stream::ReadResult::data);
    assert(cache.claim_piece(nullptr, 0, index) && index == 4);
    assert(cache.publish_piece(index, piece.data(), piece.size()));
    assert(cache.read(piece_size, bytes, sizeof bytes, count) == Stream::ReadResult::data);
    cache.reset();
    assert(cache.open_cache(logical_size, piece_size, CacheStorage::ram, {8, 5, 0}));
    assert(cache.claim_piece(nullptr, 0, index) && index == 0);
    assert(cache.publish_piece(index, piece.data(), piece.size()));
    assert(cache.can_start_mkv() && !cache.claim_piece(nullptr, 0, index));
    std::puts("Custom cache: configured budget, startup preload, read-ahead range and retained history passed");
}
