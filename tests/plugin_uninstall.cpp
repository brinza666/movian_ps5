// SPDX-License-Identifier: GPL-3.0-or-later
#include "plugin_storage.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
namespace fs=std::filesystem;
using movian::plugins::storage::remove_plugin;
static void write(const std::string &path) {
    fs::create_directories(fs::path(path).parent_path());std::ofstream(path)<<"fixture";
}
int main() {
    constexpr const char *root="download0/plugins-v23/";
    const std::string base=root;
    const auto package=base+"target.zip";
    write(package);write(base+"target.settings/key");write(base+"target.stores/store.json");
    for(int i=0;i<100;++i) write(base+"target.stores/nested/"+std::to_string(i));
    write(base+"target.torrent");write(base+".target.tmp");
    write(base+"other.zip");write(base+"other.settings/key");write("media/clip.mkv");
    write("outside/keep");
    fs::create_directory_symlink(fs::absolute("outside"),base+"target.settings/link-dir");
    fs::create_symlink(fs::absolute("outside/keep"),base+"target.stores/link-file");
    char error[256];
    assert(!remove_plugin("../other",package.c_str(),error,sizeof error)&&error[0]);
    assert(!remove_plugin("target",(base+"other.zip").c_str(),error,sizeof error));
    assert(!remove_plugin("movian-ps5-samples",package.c_str(),error,sizeof error));
    assert(fs::exists(package)&&fs::exists(base+"target.settings/key"));
    assert(remove_plugin("target",package.c_str(),error,sizeof error)&&!error[0]);
    for(const auto *suffix:{".zip",".settings",".stores",".torrent"}) assert(!fs::exists(base+"target"+suffix));
    assert(!fs::exists(base+".target.tmp"));
    for(const auto *path:{"other.zip","other.settings/key"}) assert(fs::exists(base+path));
    assert(fs::exists("media/clip.mkv")&&fs::exists("outside/keep"));
    assert(!remove_plugin("target",package.c_str(),error,sizeof error)&&error[0]);
    // A descriptor that points outside this installation never authorizes
    // removal, even when the expected archive itself has been replaced by a link.
    fs::create_symlink(fs::absolute("outside/keep"),package);
    assert(!remove_plugin("target",package.c_str(),error,sizeof error));
    assert(fs::is_symlink(package)&&fs::exists("outside/keep"));fs::remove(package);
    fs::rename(base,"saved-root");fs::create_directory_symlink(fs::absolute("saved-root"),"download0/plugins-v23");
    assert(!remove_plugin("other",(base+"other.zip").c_str(),error,sizeof error));
    assert(fs::exists("saved-root/other.zip"));fs::remove("download0/plugins-v23");fs::rename("saved-root",base);
    // Refuse an over-deep private tree and keep the package for a visible retry.
    write(package);auto deep=base+"target.settings";
    for(int i=0;i<18;++i) deep+="/d";
    write(deep+"/key");
    assert(!remove_plugin("target",package.c_str(),error,sizeof error)&&error[0]);
    assert(fs::exists(package)&&fs::exists(deep+"/key"));
    fs::remove_all(base+"target.settings");assert(remove_plugin("target",package.c_str(),error,sizeof error));
    std::puts("Plugin removal: package/private data, large directories, isolation, links, depth failure and retry passed");
}
