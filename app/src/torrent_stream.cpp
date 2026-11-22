// SPDX-License-Identifier: GPL-3.0-or-later
#include "torrent.hpp"
#include "diagnostics.hpp"
#include "paths.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#if defined(__PROSPERO__)
extern "C" {
int sceKernelMapFlexibleMemory(void **, std::size_t, int, int);
int sceKernelMunmap(void *, std::size_t);
}
#endif

namespace movian::torrent
{
VerifiedFile::~VerifiedFile() { reset(); pthread_mutex_destroy(&lock_); }

void VerifiedFile::reset() noexcept
{
#if defined(__PROSPERO__)
    if (buffer_) sceKernelMunmap(buffer_, mapped_);
#else
    std::free(buffer_);
#endif
    buffer_ = nullptr;
    mapped_ = 0;
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    if (file_[0]) ::unlink(file_);
    file_[0] = 0;
    total_ = 0;
    verified_ = 0;
    piece_size_ = 0;
    slots_active_ = 0;
    read_ahead_slots_ = 0;
    preload_slots_ = 0;
    requested_ = 0;
    reading_ = false;
    for (auto &slot : slots_) slot = {};
    state_ = State::idle;
}

bool VerifiedFile::allocate(std::size_t capacity, CacheStorage storage) noexcept
{
    if (storage == CacheStorage::ram)
    {
#if defined(__PROSPERO__)
        constexpr std::size_t page_mask = 16 * 1024 - 1;
        const auto bytes = (capacity + page_mask) & ~page_mask;
        void *mapping = nullptr;
        const int rc = sceKernelMapFlexibleMemory(&mapping, bytes, 0x03, 0);
        diagnostics::record("cache", "ram bytes=%zu result=%#x", bytes, static_cast<unsigned>(rc));
        if (auto *log = std::fopen(MOVIAN_LOG_PATH, "a"))
        {
            std::fprintf(log, "v52 torrent RAM allocate bytes=%zu rc=%#x\n", bytes,
                         static_cast<unsigned>(rc));
            std::fclose(log);
        }
        if (rc != 0 || !mapping) return false;
        mapped_ = bytes;
        buffer_ = static_cast<unsigned char *>(mapping);
#else
        buffer_ = static_cast<unsigned char *>(std::malloc(capacity));
#endif
        return buffer_ != nullptr;
    }
    timespec now{};
    clock_gettime(CLOCK_REALTIME, &now);
    const auto serial = static_cast<unsigned long long>(now.tv_sec) * 1000000000 + now.tv_nsec;
    auto create_file = [&](const char *directory) {
        for (unsigned attempt = 0; attempt < 4; ++attempt)
        {
            std::snprintf(file_, sizeof file_, "%s/.movian-cache-%llx", directory, serial + attempt);
            fd_ = ::open(file_, O_RDWR | O_CREAT | O_EXCL, 0600);
            if (fd_ >= 0 || errno != EEXIST) break;
        }
    };
    if (storage == CacheStorage::internal)
    {
        create_file(MOVIAN_DOWNLOAD0);
    }
    else if (storage == CacheStorage::usb)
    {
        for (int i = 0; i < 4 && fd_ < 0; ++i)
        {
            char directory[32];
#ifdef MOVIAN_HOST_PREVIEW
            std::snprintf(directory, sizeof directory, "usb%d", i);
#else
            std::snprintf(directory, sizeof directory, "/mnt/usb%d", i);
#endif
            create_file(directory);
        }
    }
    const int error = fd_ < 0 ? errno : 0;
#ifdef __PROSPERO__
    diagnostics::record("cache", "storage mode=%d errno=%d", static_cast<int>(storage), error);
    if (auto *log = std::fopen(MOVIAN_LOG_PATH, "a"))
    {
        std::fprintf(log, "v52 torrent storage open mode=%d errno=%d\n", static_cast<int>(storage), error);
        std::fclose(log);
    }
#else
    (void)error;
#endif
    if (fd_ < 0)
    {
        file_[0] = 0;
        return false;
    }
    if (storage == CacheStorage::internal && ::unlink(file_) == 0) file_[0] = 0;
    return true;
}

bool VerifiedFile::write_bytes(std::size_t offset, const void *data, std::size_t size) noexcept
{
    if (buffer_)
    {
        if (buffer_ + offset != data) std::memcpy(buffer_ + offset, data, size);
        return true;
    }
    if (::lseek(fd_, static_cast<off_t>(offset), SEEK_SET) < 0) return false;
    while (size)
    {
        const auto got = ::write(fd_, data, size);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) return false;
        data = static_cast<const char *>(data) + got;
        size -= got;
    }
    return true;
}

bool VerifiedFile::read_bytes(std::size_t offset, void *data, std::size_t size) const noexcept
{
    if (buffer_)
    {
        std::memcpy(data, buffer_ + offset, size);
        return true;
    }
    if (::lseek(fd_, static_cast<off_t>(offset), SEEK_SET) < 0) return false;
    while (size)
    {
        const auto got = ::read(fd_, data, size);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) return false;
        data = static_cast<char *>(data) + got;
        size -= got;
    }
    return true;
}

bool VerifiedFile::open_cache(std::uint64_t total, std::uint32_t piece_size, CacheStorage storage,
                              CacheOptions options) noexcept
{
    if (state_ != State::idle || !total || !piece_size || piece_size > 16 * 1024 * 1024 ||
        (total - 1) / piece_size >= UINT32_MAX)
        return false;
    constexpr unsigned mib = 1024 * 1024;
    const auto budget = std::min(options.size_mib, cache_limit_mib(storage)) * mib;
    const auto slots = budget ? std::min(budget / piece_size, slot_count) :
        storage == CacheStorage::ram ? std::clamp<unsigned>((16 * mib) / piece_size, 2, 8) : 8;
    if (!slots) return false;
    const auto capacity = std::min<std::uint64_t>(total, std::uint64_t(slots) * piece_size);
    if (!allocate(static_cast<std::size_t>(capacity), storage)) return false;
    slots_active_ = static_cast<unsigned>((capacity + piece_size - 1) / piece_size);
    const auto read_percent = std::clamp(options.read_ahead_percent, 5u, 100u);
    read_ahead_slots_ = (slots_active_ * read_percent + 99) / 100;
    preload_slots_ = options.preload_percent < 0 ? std::min(slots_active_, 2u) :
        std::max(1u, (slots_active_ * static_cast<unsigned>(std::min(options.preload_percent, 100)) + 99) / 100);
#if defined(__PROSPERO__)
    diagnostics::record("cache", "capacity=%llu piece=%u slots=%u ahead=%u preload=%u",
                        static_cast<unsigned long long>(capacity), piece_size, slots_active_,
                        read_ahead_slots_, preload_slots_);
#endif
    total_ = total;
    piece_size_ = piece_size;
    state_.store(State::downloading, std::memory_order_release);
    return true;
}

bool VerifiedFile::uses_cache() const noexcept
{
    return state_.load(std::memory_order_acquire) != State::idle && piece_size_ != 0;
}

bool VerifiedFile::claim_piece(const unsigned char *bitfield, std::size_t size,
                               std::uint32_t &piece) noexcept
{
    if (!uses_cache() || state_ != State::downloading) return false;
    pthread_mutex_lock(&lock_);
    bool claimed = false;
    const std::uint64_t end = (total_ - 1) / piece_size_ + 1;
    const auto wanted = reading_ ? read_ahead_slots_ : std::max(read_ahead_slots_, preload_slots_);
    for (unsigned i = 0; i < wanted; ++i)
    {
        const std::uint64_t next = static_cast<std::uint64_t>(requested_) + i;
        if (next >= end) break;
        if (bitfield && (next / 8 >= size || !(bitfield[next / 8] & (0x80 >> (next % 8)))))
            continue;
        auto &slot = slots_[next % slots_active_];
        if (slot.state == SlotState::requested ||
            (slot.state == SlotState::verified && slot.piece == next))
            continue;
        piece = static_cast<std::uint32_t>(next);
        slot = {piece, SlotState::requested};
        claimed = true;
        break;
    }
    pthread_mutex_unlock(&lock_);
    return claimed;
}

unsigned char *VerifiedFile::piece_buffer(std::uint32_t piece) noexcept
{
    if (!buffer_ || !uses_cache()) return nullptr;
    pthread_mutex_lock(&lock_);
    const auto &slot = slots_[piece % slots_active_];
    auto *result = slot.piece == piece && slot.state == SlotState::requested ?
        buffer_ + static_cast<std::size_t>(piece % slots_active_) * piece_size_ : nullptr;
    pthread_mutex_unlock(&lock_);
    return result;
}

bool VerifiedFile::publish_piece(std::uint32_t piece, const void *data, std::size_t size) noexcept
{
    if (!uses_cache() || !data) return false;
    const auto offset = static_cast<std::uint64_t>(piece) * piece_size_;
    if (offset >= total_ || size != std::min<std::uint64_t>(piece_size_, total_ - offset))
        return false;
    pthread_mutex_lock(&lock_);
    auto &slot = slots_[piece % slots_active_];
    bool ok = slot.piece == piece && slot.state == SlotState::requested;
    if (ok)
    {
        const auto slot_offset = static_cast<std::size_t>(piece % slots_active_) * piece_size_;
        ok = write_bytes(slot_offset, data, size);
        slot.state = ok ? SlotState::verified : SlotState::empty;
    }
    pthread_mutex_unlock(&lock_);
    return ok;
}

void VerifiedFile::release_piece(std::uint32_t piece) noexcept
{
    pthread_mutex_lock(&lock_);
    auto &slot = slots_[piece % slots_active_];
    if (slot.piece == piece && slot.state == SlotState::requested)
        slot.state = SlotState::empty;
    pthread_mutex_unlock(&lock_);
}

VerifiedFile::ReadResult VerifiedFile::read_cache(std::uint64_t offset, void *buffer,
                                                   std::size_t capacity, std::size_t &count) const noexcept
{
    if (offset == total_) return ReadResult::eof;
    pthread_mutex_lock(&lock_);
    reading_ = true;
    requested_ = static_cast<std::uint32_t>(offset / piece_size_);
    const auto limit = std::min<std::uint64_t>(capacity, total_ - offset);
    auto result = ReadResult::waiting;
    while (count < limit)
    {
        const auto pos = offset + count;
        const auto piece = static_cast<std::uint32_t>(pos / piece_size_);
        const auto within = pos % piece_size_;
        const auto &slot = slots_[piece % slots_active_];
        if (slot.piece != piece || slot.state != SlotState::verified) break;
        const auto want = std::min<std::uint64_t>(limit - count, piece_size_ - within);
        const auto slot_offset = static_cast<std::size_t>(piece % slots_active_) * piece_size_ + within;
        if (!read_bytes(slot_offset, static_cast<char *>(buffer) + count, static_cast<std::size_t>(want)))
        {
            result = ReadResult::failed;
            break;
        }
        count += static_cast<std::size_t>(want);
        result = ReadResult::data;
    }
    pthread_mutex_unlock(&lock_);
    return result;
}

bool VerifiedFile::can_start_mkv() const noexcept
{
    if (!uses_cache()) return false;
    unsigned char header[4]{};
    constexpr unsigned char ebml[] = {0x1a, 0x45, 0xdf, 0xa3};
    pthread_mutex_lock(&lock_);
    bool ready = total_ >= sizeof header;
    for (unsigned i = 0; ready && i < preload_slots_; ++i)
        ready = slots_[i].piece == i && slots_[i].state == SlotState::verified;
    ready = ready && read_bytes(0, header, sizeof header) && std::memcmp(header, ebml, sizeof ebml) == 0;
    pthread_mutex_unlock(&lock_);
    return ready;
}

bool VerifiedFile::open(std::uint64_t total, CacheStorage storage) noexcept
{
    if (state_ != State::idle || !total || total > 96 * 1024 * 1024)
        return false;
    if (!allocate(static_cast<std::size_t>(total), storage))
        return false;
    total_ = total;
    state_.store(State::downloading, std::memory_order_release);
    return true;
}

bool VerifiedFile::append_verified(const void *data, std::size_t size) noexcept
{
    const auto offset = verified_.load(std::memory_order_relaxed);
    if (state_ != State::downloading || piece_size_ || !data || size > total_ - offset)
        return false;
    if (fd_ >= 0) pthread_mutex_lock(&lock_);
    const bool written = write_bytes(static_cast<std::size_t>(offset), data, size);
    if (fd_ >= 0) pthread_mutex_unlock(&lock_);
    if (!written) return false;
    verified_.store(offset + size, std::memory_order_release);
    return true;
}

void VerifiedFile::finish(bool success) noexcept
{
    state_.store(success && verified_ == total_ ? State::complete : State::failed,
                 std::memory_order_release);
}

std::uint64_t VerifiedFile::size() const noexcept
{
    const auto state = state_.load(std::memory_order_acquire);
    return state == State::idle || (state == State::failed && !piece_size_) ? 0 : total_;
}

bool VerifiedFile::complete() const noexcept
{
    return state_.load(std::memory_order_acquire) == State::complete;
}

bool VerifiedFile::can_start_mp4() const noexcept
{
    const auto total = size();
    const auto prefix = verified_.load(std::memory_order_acquire);
    if (prefix < 512 * 1024 || prefix >= total)
        return false;
    bool index = false;
    for (std::uint64_t offset = 0; offset + 8 <= prefix;)
    {
        unsigned char box[8];
        std::size_t count;
        if (read(offset, box, sizeof box, count) != ReadResult::data || count != sizeof box)
            return false;
        std::uint32_t length = 0, type = 0;
        for (int i = 0; i < 4; ++i)
        {
            length = (length << 8) | box[i];
            type = (type << 8) | box[i + 4];
        }
        if (offset == 0 && type != 0x66747970) // ftyp
            return false;
        if (type == 0x6d646174) // mdat
            return index && length >= 8 && length <= total - offset;
        if (length < 8 || length > prefix - offset)
            return false;
        index |= type == 0x6d6f6f76; // moov
        offset += length;
    }
    return false;
}

VerifiedFile::ReadResult VerifiedFile::read(std::uint64_t offset, void *buffer,
                                           std::size_t capacity, std::size_t &count) const noexcept
{
    count = 0;
    const auto state = state_.load(std::memory_order_acquire);
    if (!buffer || !capacity)
        return ReadResult::failed;
    if (state == State::idle)
        return ReadResult::waiting;
    if (offset > total_)
        return ReadResult::failed;
    if (piece_size_)
    {
        const auto result = read_cache(offset, buffer, capacity, count);
        return state == State::failed && result == ReadResult::waiting ? ReadResult::failed : result;
    }
    if (state == State::failed)
        return ReadResult::failed;
    const auto verified = verified_.load(std::memory_order_acquire);
    if (offset >= verified)
        return state == State::complete ? ReadResult::eof : ReadResult::waiting;
    const auto available = verified - offset;
    const auto requested = capacity < available ? capacity : static_cast<std::size_t>(available);
    if (fd_ >= 0) pthread_mutex_lock(&lock_);
    const bool read = read_bytes(static_cast<std::size_t>(offset), buffer, requested);
    if (fd_ >= 0) pthread_mutex_unlock(&lock_);
    if (!read) return ReadResult::failed;
    count = requested;
    return ReadResult::data;
}
}
