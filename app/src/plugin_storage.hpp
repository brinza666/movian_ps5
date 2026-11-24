// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paths.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <initializer_list>
#include <sys/stat.h>
#include <unistd.h>

namespace movian::plugins::storage {
// Use descriptor-relative operations: plugin data may contain links and the
// title's FTP service can be editing files while an uninstall is in progress.
struct Fd {
    int value;
    explicit Fd(int fd):value(fd) {}
    ~Fd() { if(value>=0) ::close(value); }
    Fd(const Fd &)=delete;
};
inline bool valid_id(const char *id) {
    if(!id || !*id || std::strlen(id)>63 || !std::strcmp(id,".") || !std::strcmp(id,"..")) return false;
    for(auto *p=id;*p;++p)
        if(!((*p>='a'&&*p<='z') || (*p>='A'&&*p<='Z') || (*p>='0'&&*p<='9') ||
             *p=='_' || *p=='-' || *p=='.')) return false;
    return std::strcmp(id,"movian-ps5-samples")!=0;
}
inline bool dot(const char *name) {
    return !std::strcmp(name,".") || !std::strcmp(name,"..");
}
inline int first_child(int fd,char (&name)[256]) {
    if(lseek(fd,0,SEEK_SET)<0) return -1;
#ifdef MOVIAN_HOST_PREVIEW
    const int copy=dup(fd); if(copy<0) return -1;
    DIR *dir=fdopendir(copy); if(!dir) { ::close(copy); return -1; }
    int result=0;
    for(;;) {
        errno=0; auto *entry=readdir(dir);
        if(!entry) { if(errno) result=-1; break; }
        if(dot(entry->d_name)) continue;
        if(std::strlen(entry->d_name)>=sizeof name) { errno=ENAMETOOLONG;result=-1;break; }
        std::strcpy(name,entry->d_name);result=1;break;
    }
    const int error=errno;closedir(dir);errno=error;return result;
#else
    // Same native directory ABI as fs.cpp; libc readdir was unreliable on PS5.
    int capacity=64*1024;
    auto *buffer=static_cast<char *>(std::malloc(capacity));
    if(!buffer) { errno=ENOMEM;return -1; }
    int result=0;
    for(;;) {
        int bytes=getdents(fd,buffer,capacity);
        if(bytes<0 && errno==EINVAL && capacity<1024*1024) {
            auto *larger=static_cast<char *>(std::realloc(buffer,capacity*2));
            if(!larger) { errno=ENOMEM;result=-1;break; }
            buffer=larger;capacity*=2;continue;
        }
        if(bytes<0 && (errno==ENOSYS || errno==EPERM)) {
            long base=0;bytes=getdirentries(fd,buffer,capacity,&base);
        }
        if(bytes<=0) { result=bytes<0?-1:0;break; }
        int offset=0;
        while(offset<bytes) {
            if(bytes-offset<8) { errno=EIO;result=-1;break; }
            const auto *entry=reinterpret_cast<const dirent *>(buffer+offset);
            const int length=entry->d_reclen;
            if(length<8 || length>bytes-offset) { errno=EIO;result=-1;break; }
            offset+=length;
            if(!entry->d_fileno) continue;
            const auto size=strnlen(entry->d_name,length-8);
            if(!size || size>=sizeof name || size>=static_cast<unsigned>(length-8)) {
                errno=EIO;result=-1;break;
            }
            std::memcpy(name,entry->d_name,size+1);
            if(dot(name)) continue;
            result=1;break;
        }
        if(result) break;
    }
    const int error=errno;std::free(buffer);errno=error;return result;
#endif
}
inline bool remove_tree(int parent,const char *name,unsigned depth,unsigned &budget) {
    if(depth>16 || !budget) { errno=EOVERFLOW;return false; }
    --budget;
    Fd dir(openat(parent,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    if(dir.value<0) {
        if(errno==ENOENT) return true;
        if(errno==ENOTDIR || errno==ELOOP || errno==EMLINK)
            return unlinkat(parent,name,0)==0 || errno==ENOENT;
        return false;
    }
    struct stat opened{};if(fstat(dir.value,&opened)) return false;
    for(;;) {
        char child[256];const int count=first_child(dir.value,child);
        if(count<0) return false;
        if(!count) break;
        if(!remove_tree(dir.value,child,depth+1,budget)) return false;
    }
    struct stat current{};
    if(fstatat(parent,name,&current,AT_SYMLINK_NOFOLLOW)) return errno==ENOENT;
    if(current.st_dev!=opened.st_dev || current.st_ino!=opened.st_ino) { errno=ESTALE;return false; }
    return unlinkat(parent,name,AT_REMOVEDIR)==0 || errno==ENOENT;
}
inline bool remove_plugin(const char *id,const char *archive,char *error,int capacity) {
    auto fail=[&](const char *stage) {
        const int code=errno;
        if(error && capacity>0) std::snprintf(error,capacity,"Could not uninstall plugin: %s (errno %d)",stage,code);
        errno=code;return false;
    };
    if(error && capacity>0) error[0]=0;
    if(!valid_id(id)) { errno=EINVAL;return fail("invalid or bundled plugin"); }
    char expected[256],package[80];
    std::snprintf(expected,sizeof expected,MOVIAN_DOWNLOAD0 "/plugins-v23/%s.zip",id);
    if(!archive || std::strcmp(archive,expected)) { errno=EINVAL;return fail("package path mismatch"); }
    Fd profile(::open(MOVIAN_DOWNLOAD0,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    if(profile.value<0) return fail("profile directory");
    Fd root(openat(profile.value,"plugins-v23",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    if(root.value<0) return fail("plugin directory");
    std::snprintf(package,sizeof package,"%s.zip",id);
    Fd file(openat(root.value,package,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));
    if(file.value<0) return fail("installed package");
    struct stat opened{};
    if(fstat(file.value,&opened)) return fail("package status");
    if(!S_ISREG(opened.st_mode)) { errno=EINVAL;return fail("package is not a regular file"); }
    unsigned budget=16384;
    for(const char *suffix:{".settings",".stores"}) {
        char data[80];std::snprintf(data,sizeof data,"%s%s",id,suffix);
        if(!remove_tree(root.value,data,0,budget)) return fail("private data removal; package retained");
    }
    char descriptor[80];std::snprintf(descriptor,sizeof descriptor,"%s.torrent",id);
    if(unlinkat(root.value,descriptor,0) && errno!=ENOENT) return fail("torrent descriptor; package retained");
    char temporary[80];std::snprintf(temporary,sizeof temporary,".%s.tmp",id);
    if(unlinkat(root.value,temporary,0) && errno!=ENOENT) return fail("temporary package; package retained");
    struct stat current{};
    if(fstatat(root.value,package,&current,AT_SYMLINK_NOFOLLOW)) return fail("package recheck");
    if(current.st_dev!=opened.st_dev || current.st_ino!=opened.st_ino) { errno=ESTALE;return fail("package changed"); }
    if(unlinkat(root.value,package,0)) return fail("package removal");
    return true;
}
} // namespace movian::plugins::storage
