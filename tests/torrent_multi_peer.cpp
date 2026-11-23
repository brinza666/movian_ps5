#include "torrent.hpp"
#include "plugin_http.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace { unsigned ports[2]; unsigned tracker_calls; unsigned stalled_ports[2]; }

int movian::plugins::http_request(const char *url, bool post, const Header *, int, const char *,
                                  std::size_t, char *body, std::size_t capacity,
                                  HttpResponse &response) noexcept
{
    assert(!post && std::strstr(url, "&compact=1&numwant=24") && capacity >= 31);
    ++tracker_calls;
    if (!std::strncmp(url, "http://tracker.invalid/", sizeof("http://tracker.invalid/") - 1))
    {
        if (stalled_ports[0])
        {
            unsigned char reply[] = {'d','5',':','p','e','e','r','s','1','2',':',
                                     127,0,0,1,0,0,127,0,0,1,0,0,'e'};
            for (int i = 0; i < 2; ++i)
            {
                reply[15 + i * 6] = static_cast<unsigned char>(stalled_ports[i] >> 8);
                reply[16 + i * 6] = static_cast<unsigned char>(stalled_ports[i]);
            }
            std::memcpy(body, reply, sizeof reply);
            response.length = sizeof reply;
        }
        else
        {
            constexpr char reply[] = "d5:peers0:e";
            std::memcpy(body, reply, sizeof reply - 1);
            response.length = sizeof reply - 1;
        }
        response.status = 200;
        return 0;
    }
    assert(std::strstr(url, "retracker.invalid"));
    unsigned char reply[] = {'d','5',':','p','e','e','r','s','1','8',':',
                             127,0,0,1,0,0,127,0,0,1,0,0,127,0,0,1,0,0,'e'};
    for (int i = 0; i < 2; ++i)
    {
        reply[15 + i * 6] = static_cast<unsigned char>(ports[i] >> 8);
        reply[16 + i * 6] = static_cast<unsigned char>(ports[i]);
    }
    std::memcpy(reply + 23, reply + 11, 6);
    movian::torrent::Endpoints parsed;
    assert(movian::torrent::tracker_peers(reinterpret_cast<const char *>(reply), sizeof reply, parsed));
    assert(parsed.count == 2);
    std::memcpy(body, reply, sizeof reply);
    response.length = sizeof reply;
    response.status = 200;
    return 0;
}

int main(int argc, char **argv)
{
    using namespace std::chrono_literals;
    using Result = movian::torrent::VerifiedFile::ReadResult;
    assert(argc == 3 || argc == 4 || argc == 5);
    ports[0] = std::atoi(argv[1]);
    ports[1] = std::atoi(argv[2]);
    movian::torrent::VerifiedFile stream;
    char path[384]{}, error[128]{};
    std::atomic<bool> cancel{false}, done{false};
    std::atomic<unsigned> progress{0};
    bool ok = false;
    const bool small = argc == 4 && !std::strcmp(argv[3], "small");
    const bool udp = argc == 5;
    const bool dht = argc == 4 && !std::strcmp(argv[3], "dht");
    const bool private_dht = argc == 4 && !std::strcmp(argv[3], "private-dht");
    if (udp)
    {
        stalled_ports[0] = std::atoi(argv[3]);
        stalled_ports[1] = std::atoi(argv[4]);
    }
    char endpoint[24]{};
    if (small)
        std::snprintf(endpoint, sizeof endpoint, "127.0.0.1:%u", ports[0]);
    std::thread transfer([&] {
        movian::torrent::TransferOptions options;
        options.connections = 2;
        ok = movian::torrent::download("fixture.torrent", endpoint, path, cancel,
                                        progress, error, sizeof error, stream,
                                        movian::torrent::CacheStorage::ram, options);
        done = true;
    });
    if (private_dht)
    {
        transfer.join();
        assert(!ok && tracker_calls == 1);
        return 0;
    }
    if (small)
    {
        transfer.join();
        if (!ok) std::fprintf(stderr, "%s\n", error);
        assert(ok && stream.complete() && progress == 100);
        assert(!std::strcmp(path, "fixture.mkv"));
        unsigned char sample[4]{};
        std::size_t count = 0;
        assert(stream.read(7 * 256 * 1024, sample, sizeof sample, count) == Result::data);
        assert(count == sizeof sample && sample[0] == 7);
        std::puts("Small torrent completed and remained readable in RAM");
        return 0;
    }
    unsigned char sample[4]{};
    auto read = [&](std::uint64_t offset) {
        for (unsigned attempt = 0; attempt < 800 && !done; ++attempt)
        {
            std::size_t count = 0;
            if (stream.read(offset, sample, sizeof sample, count) == Result::data &&
                count == sizeof sample)
                return true;
            std::this_thread::sleep_for(10ms);
        }
        return false;
    };
    const bool header = read(0) && sample[0] == 0x1a;
    if (header) std::this_thread::sleep_for(100ms);
    constexpr std::uint64_t span = 256 * 1024;
    const bool even = header && read(398 * span) && sample[0] == (398 & 255);
    const bool odd = even && read(399 * span) && sample[0] == (399 & 255);
    cancel = true;
    transfer.join();
    if (!ok) std::fprintf(stderr, "%s\n", error);
    assert(header && even && odd && ok);
    if (dht)
    {
        assert(tracker_calls == 0);
        std::puts("DHT-only peers supplied verified pieces for a distant seek");
        return 0;
    }
    assert(tracker_calls == (udp ? 1u : 2u));
    using movian::torrent::RetrackersMode;
    movian::torrent::PeerInfo configured{};
    constexpr char tracker_file[] = "data/movian/trackers.txt";
    std::strcpy(configured.trackers[0], "http://original.invalid/announce");
    configured.tracker_count = 1;
    movian::torrent::configure_trackers(configured, tracker_file, RetrackersMode::original);
    assert(configured.tracker_count == 1);
    movian::torrent::configure_trackers(configured, tracker_file, RetrackersMode::replace);
    assert(configured.tracker_count == 2 && !std::strcmp(configured.trackers[0], "http://tracker.invalid/announce"));
    configured.private_torrent = true;
    movian::torrent::configure_trackers(configured, tracker_file, RetrackersMode::remove);
    assert(configured.tracker_count == 2);
    configured.private_torrent = false;
    movian::torrent::configure_trackers(configured, tracker_file, RetrackersMode::remove);
    assert(configured.tracker_count == 0);
    constexpr char missing_file[] = "data/movian/no-tracker-override.txt";
    movian::torrent::configure_trackers(configured, missing_file, RetrackersMode::add);
    assert(configured.tracker_count == 12);
    assert(!std::strcmp(configured.trackers[0], "http://retracker.local/announce"));
    assert(!std::strcmp(configured.trackers[11], "udp://explodie.org:6969/announce"));
    {
        movian::torrent::PeerInfo mixed{};
        mixed.private_torrent=true;
        mixed.tracker_count=3;
        std::strcpy(mixed.trackers[0],"http://http.invalid/announce");
        std::strcpy(mixed.trackers[1],"udp://udp.invalid:1337/announce");
        std::strcpy(mixed.trackers[2],"https://https.invalid/announce");
        auto tcp=mixed,udp=mixed,both=mixed;
        using movian::torrent::TrackerProtocols;
        movian::torrent::filter_trackers(tcp,TrackerProtocols::tcp);
        movian::torrent::filter_trackers(udp,TrackerProtocols::udp);
        movian::torrent::filter_trackers(both,TrackerProtocols::tcp_udp);
        assert(tcp.tracker_count==2 && !std::strcmp(tcp.trackers[1],mixed.trackers[2]));
        assert(udp.tracker_count==1 && !std::strcmp(udp.trackers[0],mixed.trackers[1]));
        assert(both.tracker_count==3 && both.private_torrent && tcp.private_torrent && udp.private_torrent);
        movian::torrent::filter_trackers(udp,TrackerProtocols::tcp);
        assert(udp.tracker_count==0);
        std::puts("Tracker TCP/UDP selection filters effective announces and preserves private status");
    }
    movian::torrent::configure_trackers(configured, missing_file, RetrackersMode::add);
    assert(configured.tracker_count == 12);

    movian::torrent::PeerInfo saturated{};
    saturated.tracker_count = movian::torrent::PeerInfo::embedded_tracker_limit;
    for (int i = 0; i < saturated.tracker_count; ++i)
        std::snprintf(saturated.trackers[i], sizeof saturated.trackers[i], "http://original%d.invalid/announce", i);
    movian::torrent::configure_trackers(saturated, missing_file, RetrackersMode::add);
    assert(saturated.tracker_count == movian::torrent::PeerInfo::tracker_capacity);
    assert(!std::strcmp(saturated.trackers[saturated.tracker_count - 1], "udp://explodie.org:6969/announce"));
    std::puts("Two peers supplied complementary pieces for a distant seek");
}
