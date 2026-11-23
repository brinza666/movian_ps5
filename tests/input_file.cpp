// SPDX-License-Identifier: GPL-3.0-or-later
#include "input_file.hpp"
#include <cassert>
#include <string>
#include <unistd.h>

namespace {
std::string payload(150001, 'x');
std::int64_t advertised = payload.size();
int calls = 0, closes = 0, fail_after = -1;
}
namespace movian::network {
struct File { std::size_t offset = 0; };
bool is_path(const char *path) noexcept { return !std::strncmp(path, "net://", 6); }
File *open(const char *) noexcept { calls=0; return new File; }
std::int64_t seek(File *, std::int64_t, int whence) noexcept { assert(whence==0x10000); return advertised; }
int read(File *file, unsigned char *out, int size) noexcept {
    assert(size>0 && size<=65536);
    if (fail_after>=0 && calls++>=fail_after) return -1;
    const auto n=std::min<std::size_t>({static_cast<std::size_t>(size), 8191, payload.size()-file->offset});
    std::memcpy(out, payload.data()+file->offset, n);file->offset+=n;
    return static_cast<int>(n);
}
void close(File *file) noexcept { ++closes;delete file; }
}
int main() {
    using movian::input::read_bounded;
    std::size_t size=99;
    auto *data=read_bounded("net://fixture",payload.size(),size);
    assert(data && size==payload.size() && !std::memcmp(data,payload.data(),size));std::free(data);
    for(const auto length:{std::int64_t(-1),std::int64_t(0),std::int64_t(200000)}) {
        advertised=length;
        assert(!read_bounded("net://fixture",150001,size) && size==0);
    }
    advertised=payload.size()+1; // File truncated after size query.
    assert(!read_bounded("net://fixture",200000,size) && size==0);
    advertised=payload.size();fail_after=2;
    assert(!read_bounded("net://fixture",200000,size) && size==0);
    assert(closes==6);
    assert(!read_bounded(nullptr,100,size) && size==0);
    assert(!read_bounded("net://fixture",0,size) && size==0 && closes==6);
    char path[]="/tmp/movian-input-XXXXXX";
    const int fd=mkstemp(path);assert(fd>=0);
    assert(write(fd,"fixture",7)==7);close(fd);
    data=read_bounded(path,7,size);assert(data && size==7 && !std::memcmp(data,"fixture",7));std::free(data);
    assert(!read_bounded(path,6,size) && size==0);unlink(path);
    assert(!read_bounded(path,7,size) && size==0);
    std::puts("Bounded input: partial reads, truncated files, read errors, size limits and cleanup passed");
}
