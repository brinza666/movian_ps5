// SPDX-License-Identifier: GPL-3.0-or-later
#include "profile_reset.hpp"
#include "paths.hpp"
#include "diagnostics.hpp"
#include "fs.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace movian::profile {
namespace {
FILE *reset_log=nullptr;
void report(const char *stage,int error=0) noexcept {
    if(reset_log) { std::fprintf(reset_log,"%s errno=%d\n",stage,error); std::fflush(reset_log); }
    diagnostics::record("profile","%s errno=%d",stage,error);
}
bool remove_plugins(const std::string &path,unsigned depth,unsigned &budget) noexcept {
    if(depth>16 || !budget) { report("plugin reset budget exceeded",EOVERFLOW); return false; }
    --budget;
    // Opening the final component without following links avoids lstat and
    // never traverses a package's symlink into another profile/media folder.
    const int fd=open(path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0) {
        if(errno==ENOENT) return true;
        if(errno==ENOTDIR || errno==ELOOP || errno==EMLINK) return unlink(path.c_str())==0;
        report("plugin directory open failed",errno); return false;
    }
    bool okay=true;
    for(;;) {
        fs::Entry entries[32]; int count=0,error=0;
        if(fs::list_dir(path.c_str(),entries,32,&count,&error,true)!=0) {
            report("plugin directory read failed",error); okay=false; break;
        }
        if(!count) break;
        for(int i=0;i<count;++i)
            if(!remove_plugins(entries[i].path,depth+1,budget)) { okay=false; break; }
        if(!okay) break;
    }
    close(fd);
    if(okay && rmdir(path.c_str())!=0) { report("plugin directory remove failed",errno); okay=false; }
    return okay;
}
}
bool reset_requested() noexcept {
    constexpr const char request[]=MOVIAN_APP0 "/stpp-fresh-profile.request";
    constexpr const char done[]=MOVIAN_DOWNLOAD0 "/stpp-fresh-profile-v61.done";
    constexpr char expected[]="PPSA99456 v61 fresh\n";
    struct stat info{}; if(stat(done,&info)==0) return false;
    auto *file=std::fopen(request,"rb"); if(!file) return false;
    char text[sizeof expected]{}; const auto size=std::fread(text,1,sizeof text,file); std::fclose(file);
    if(size!=sizeof expected-1 || std::memcmp(text,expected,size)!=0) return false;
    reset_log=std::fopen(MOVIAN_DOWNLOAD0 "/profile-reset.log","a");
    report("v61 fresh request accepted");
    unsigned budget=16384;
    bool okay=remove_plugins(MOVIAN_DOWNLOAD0 "/plugins-v23",0,budget);
    constexpr const char *files[]={"settings.txt","settings.txt.tmp","browser-views.txt","browser-views.txt.tmp",
        "network-private.bin","network-private.bin.tmp","mprepo-cache.json"};
    for(const auto *name:files) {
        if(!okay) break;
        const auto path=std::string(MOVIAN_DOWNLOAD0 "/")+name;
        if(unlink(path.c_str())!=0 && errno!=ENOENT) { report("config remove failed",errno); okay=false; }
    }
    if(okay) {
        file=std::fopen(done,"wb");
        if(file) { const bool written=std::fwrite(expected,1,sizeof expected-1,file)==sizeof expected-1;
            const bool closed=std::fclose(file)==0; okay=written && closed; }
        else okay=false;
    }
    report(okay?"v61 fresh reset complete":"v61 fresh reset failed");
    if(reset_log) std::fclose(reset_log);
    reset_log=nullptr;
    return okay;
}
}
