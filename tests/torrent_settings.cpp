#include "settings.hpp"

#include <cassert>
#include <fstream>
#include <sys/stat.h>

int main()
{
    using namespace movian::settings;
    mkdir("download0", 0700);
    std::ofstream("download0/settings.txt") << "torrent_cache_path=2\ntorrent_progressive=1\n";
    load();
    auto &d = get();
    assert(d.torrent_cache_path == 2 && d.torrent_cache_mib == 0 && d.torrent_read_ahead == 100);
    assert(d.torrent_preload == -1 && d.torrent_connections == 4 && d.torrent_requests == 16);
    assert(d.torrent_block_timeout == 6 && d.torrent_retrackers == 1);
    d.torrent_cache_mib = 96;
    d.torrent_read_ahead = 75;
    d.torrent_preload = 0;
    d.torrent_connections = 7;
    d.torrent_requests = 24;
    d.torrent_block_timeout = 12;
    d.torrent_retrackers = 2;
    save();
    load();
    assert(d.torrent_cache_path == 2 && d.torrent_cache_mib == 96 && d.torrent_read_ahead == 75);
    assert(d.torrent_preload == 0 && d.torrent_connections == 7 && d.torrent_requests == 24);
    assert(d.torrent_block_timeout == 12 && d.torrent_retrackers == 2);
}
