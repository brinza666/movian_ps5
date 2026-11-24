// SPDX-License-Identifier: GPL-3.0-or-later
#include "profile_reset.hpp"
#include "network.hpp"
#include "title_access_io.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstdarg>
namespace movian::diagnostics { void record(const char *,const char *,...) noexcept {} }
namespace movian::network {
bool is_path(const char*) noexcept { return false; }
int list(const char*,movian::fs::Entry*,int,int*,int*,bool,movian::fs::SortOrder,bool) noexcept { return -1; }
}
namespace fs=std::filesystem;
int main() {
    fs::create_directories("app0"); fs::create_directories("download0/plugins-v23/old.settings");
    std::ofstream("download0/plugins-v23/old.settings/key")<<"stale";
    for(int i=0;i<100;++i) std::ofstream("download0/plugins-v23/item"+std::to_string(i))<<"stale";
    fs::create_directories("outside-dir"); std::ofstream("outside-dir/keep")<<"preserved";
    fs::create_directory_symlink("../../outside-dir","download0/plugins-v23/dirlink");
    std::ofstream("download0/settings.txt")<<"old";
    std::ofstream("download0/bookmarks.txt")<<"preserved";
    std::ofstream("outside")<<"preserved";
    int error=0; assert(movian_title_access_list_directory("download0/plugins-v23",&error)==0 && error==0);
    assert(movian_title_access_list_directory("missing-directory",&error)<0 && error!=0);
    fs::create_symlink("../../outside","download0/plugins-v23/link");
    std::ofstream("app0/stpp-fresh-profile.request")<<"wrong";
    assert(!movian::profile::reset_requested()); assert(fs::exists("download0/settings.txt"));
    std::ofstream("app0/stpp-fresh-profile.request")<<"PPSA99456 v61 fresh\n";
    assert(movian::profile::reset_requested()); assert(!fs::exists("download0/plugins-v23"));
    assert(!fs::exists("download0/settings.txt")); assert(fs::exists("outside")); assert(fs::exists("outside-dir/keep"));
    assert(fs::exists("download0/bookmarks.txt"));
    std::ofstream("download0/settings.txt")<<"new";
    assert(!movian::profile::reset_requested()); assert(fs::exists("download0/settings.txt"));
}
