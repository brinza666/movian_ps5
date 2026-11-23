// SPDX-License-Identifier: GPL-3.0-or-later
#include "profile_reset.hpp"
#include "paths.hpp"
#include "diagnostics.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
namespace movian::profile {
namespace {
bool remove_plugins(const std::string &path,unsigned depth,unsigned &budget) noexcept {
    if(depth>16 || !budget--) return false;
    struct stat info{};
    if(lstat(path.c_str(),&info)!=0) return errno==ENOENT;
    // Never follow links from old plugin packages outside this private tree.
    if(!S_ISDIR(info.st_mode)) return unlink(path.c_str())==0;
    auto *dir=opendir(path.c_str()); if(!dir) return false;
    bool okay=true;
    while(auto *entry=readdir(dir)) {
        if(!std::strcmp(entry->d_name,".") || !std::strcmp(entry->d_name,"..")) continue;
        if(!remove_plugins(path+"/"+entry->d_name,depth+1,budget)) { okay=false; break; }
    }
    closedir(dir); return okay && rmdir(path.c_str())==0;
}
}
bool reset_requested() noexcept {
    constexpr const char request[]=MOVIAN_APP0 "/stpp-fresh-profile.request";
    constexpr const char done[]=MOVIAN_DOWNLOAD0 "/stpp-fresh-profile-v60.done";
    constexpr char expected[]="PPSA99456 v60 fresh\n";
    struct stat info{}; if(stat(done,&info)==0) return false;
    auto *file=std::fopen(request,"rb"); if(!file) return false;
    char text[sizeof expected]{}; const auto size=std::fread(text,1,sizeof text,file); std::fclose(file);
    if(size!=sizeof expected-1 || std::memcmp(text,expected,size)!=0) return false;
    unsigned budget=16384;
    bool okay=remove_plugins(MOVIAN_DOWNLOAD0 "/plugins-v23",0,budget);
    constexpr const char *files[]={"settings.txt","settings.txt.tmp","browser-views.txt","browser-views.txt.tmp",
        "network-private.bin","network-private.bin.tmp","mprepo-cache.json"};
    for(const auto *name:files) {
        const auto path=std::string(MOVIAN_DOWNLOAD0 "/")+name;
        if(unlink(path.c_str())!=0 && errno!=ENOENT) okay=false;
    }
    if(okay) {
        file=std::fopen(done,"wb");
        if(file) { const bool written=std::fwrite(expected,1,sizeof expected-1,file)==sizeof expected-1;
            const bool closed=std::fclose(file)==0; okay=written && closed; }
        else okay=false;
    }
    diagnostics::record("profile","v60 explicit fresh reset complete=%d",okay);
    return okay;
}
}
