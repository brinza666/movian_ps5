// SPDX-License-Identifier: GPL-3.0-or-later
#include "profile_reset.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstdarg>
namespace movian::diagnostics { void record(const char *,const char *,...) noexcept {} }
namespace fs=std::filesystem;
int main() {
    fs::create_directories("app0"); fs::create_directories("download0/plugins-v23/old.settings");
    std::ofstream("download0/plugins-v23/old.settings/key")<<"stale";
    std::ofstream("download0/settings.txt")<<"old";
    std::ofstream("download0/bookmarks.txt")<<"preserved";
    std::ofstream("outside")<<"preserved";
    fs::create_symlink("../../outside","download0/plugins-v23/link");
    std::ofstream("app0/stpp-fresh-profile.request")<<"wrong";
    assert(!movian::profile::reset_requested()); assert(fs::exists("download0/settings.txt"));
    std::ofstream("app0/stpp-fresh-profile.request")<<"PPSA99456 v60 fresh\n";
    assert(movian::profile::reset_requested()); assert(!fs::exists("download0/plugins-v23"));
    assert(!fs::exists("download0/settings.txt")); assert(fs::exists("outside"));
    assert(fs::exists("download0/bookmarks.txt"));
    std::ofstream("download0/settings.txt")<<"new";
    assert(!movian::profile::reset_requested()); assert(fs::exists("download0/settings.txt"));
}
