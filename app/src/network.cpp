// SMB2/3 uses libsmb2; FTP uses libavformat's passive FTP client.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "network.hpp"
#include "network_runtime.hpp"
#include "diagnostics.hpp"
#include "paths.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <new>
#include <pthread.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__PROSPERO__)
#include <arpa/inet.h>
#include <sys/socket.h>
extern "C" {
int sceNetPoolCreate(const char *, int, int);
int sceNetPoolDestroy(int);
int sceNetResolverCreate(const char *, int, int);
int sceNetResolverStartNtoa(int, const char *, in_addr *, int, int, int);
int sceNetResolverDestroy(int);
}
#endif
#ifdef MOVIAN_HAVE_FFMPEG
extern "C" {
#include <libavformat/avio.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
}
#endif
#ifdef MOVIAN_HAVE_SMB2
extern "C" {
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
}
#endif

namespace movian::network {
namespace {
Config cfg;
bool shutting_down=false;
constexpr char config_path[] = MOVIAN_DOWNLOAD0 "/network-private.bin";
constexpr char magic[] = "MOVIAN-NET-2";
bool clean(const char *s, unsigned cap) {
    for(unsigned i=0;i<cap;++i) {
        const auto c=static_cast<unsigned char>(s[i]);
        if(!c) return true;
        if(c<32 || c==127) return false;
    }
    return false;
}
bool parse(const char *path, Connection &c, const char *&relative) {
    if (!is_path(path) || path[6] < '0' || path[6] >= '0'+max_connections || path[7] != '/') return false;
    c = cfg.connections[path[6]-'0'];
    relative = path+8;
    if (!valid_url(c.url) || !clean(relative, fs::max_path+1-8)) return false;
    // Names are literal UTF-8, never interpreted as URI escapes or path traversal.
    for (const char *p=relative; *p;) {
        const auto n=std::strcspn(p,"/");
        if ((n==2 && p[0]=='.' && p[1]=='.') || (n==1 && p[0]=='.')) return false;
        p+=n; if (*p) ++p;
    }
    return true;
}
bool remote_path(const Connection &c, const char *relative, char *out, unsigned cap) {
    const auto n=std::strlen(c.url);
    const int written=std::snprintf(out,cap,"%s%s%s",c.url,n && c.url[n-1]=='/'?"":"/",relative);
    return written>=0 && static_cast<unsigned>(written)<cap;
}
bool resolve_host(Connection &c) {
#if defined(__PROSPERO__)
    const int prefix=!std::strncmp(c.url,"smb2://",7)?7:6;
    const char *authority=c.url+prefix;
    if(authority[0]=='[') return true; // Numeric IPv6 is passed to the transport.
    const auto length=std::strcspn(authority,":/");
    char host[256]; std::memcpy(host,authority,length); host[length]=0;
    in_addr address{};
    if(inet_pton(AF_INET,host,&address)==1) return true;
    if(initialize_native()<0) return false;
    const int pool=sceNetPoolCreate("movian_network_dns",16*1024,0);
    if(pool<0) return false;
    const int resolver=sceNetResolverCreate("movian_network_dns",pool,0);
    const bool ok=resolver>=0 && sceNetResolverStartNtoa(resolver,host,&address,2500000,1,0)>=0;
    if(resolver>=0) sceNetResolverDestroy(resolver);
    sceNetPoolDestroy(pool);
    if(!ok) return false;
    char ip[INET_ADDRSTRLEN],url[sizeof c.url];
    if(!inet_ntop(AF_INET,&address,ip,sizeof ip)) return false;
    const int n=std::snprintf(url,sizeof url,"%.*s%s%s",prefix,c.url,ip,authority+length);
    if(n<0 || n>=static_cast<int>(sizeof url)) return false;
    std::memcpy(c.url,url,n+1);
#endif
    return true;
}
#ifdef MOVIAN_HAVE_FFMPEG
AVDictionary *ftp_options(const Connection &c) {
    AVDictionary *opts=nullptr;
    av_dict_set(&opts,"ftp-user",c.user[0]?c.user:"anonymous",0);
    av_dict_set(&opts,"ftp-password",c.password,0);
    av_dict_set(&opts,"timeout","8000000",0);
    av_dict_set(&opts,"rw_timeout","8000000",0);
    return opts;
}
#endif
#ifdef MOVIAN_HAVE_SMB2
smb2_context *connect(const Connection &c, char *path, unsigned cap) {
    char url[768];
    std::snprintf(url,sizeof url,"%s",c.url+7);
    char *share=std::strchr(url,'/');
    if (!share || !share[1]) return nullptr;
    *share++=0;
    char *folder=std::strchr(share,'/');
    if (folder) *folder++=0;
    std::snprintf(path,cap,"%s",folder?folder:"");
    diagnostics::record("network","SMB2 context initialization starting");
    auto *ctx=smb2_init_context();
    if (!ctx) { diagnostics::record("network","SMB2 context initialization failed errno=%d",errno); return nullptr; }
    diagnostics::record("network","SMB2 context initialized");
    smb2_set_timeout(ctx,8);
    smb2_set_version(ctx,SMB2_VERSION_ANY);
    smb2_set_user(ctx,c.user[0]?c.user:"guest");
    smb2_set_password(ctx,c.password);
    smb2_set_domain(ctx,c.domain);
    diagnostics::record("network","SMB2 share connection starting");
    const int result=smb2_connect_share(ctx,url,share,c.user[0]?c.user:"guest");
    diagnostics::record("network","SMB2 share connection result=%d",result);
    if (result<0) { smb2_destroy_context(ctx); return nullptr; }
    return ctx;
}
bool append_relative(char *path, unsigned cap, const char *relative) {
    const auto len=std::strlen(path);
    return std::snprintf(path+len,cap-len,"%s%s",len && relative[0]?"/":"",relative)<static_cast<int>(cap-len);
}
#endif
constexpr int listing_cap=256;
struct Listing {
    char path[fs::max_path+1]{};
    Connection connection;
    fs::Entry entries[listing_cap];
    int count=0, error=0, omitted=0;
    bool other=false, folders=true;
    fs::SortOrder sort=fs::SortOrder::filename;
} listing;
std::atomic<bool> done{false};
bool running=false, ready=false;
unsigned generation=0, listing_generation=0;
pthread_t worker;
bool before(const fs::Entry &a,const fs::Entry &b) {
    if (listing.folders && (a.kind==fs::Kind::directory)!=(b.kind==fs::Kind::directory))
        return a.kind==fs::Kind::directory;
    if (listing.sort!=fs::SortOrder::filename && a.mtime!=b.mtime)
        return listing.sort==fs::SortOrder::newest?a.mtime>b.mtime:a.mtime<b.mtime;
    const int names=strcasecmp(a.name,b.name);
    return names?names<0:std::strcmp(a.path,b.path)<0;
}
void entry(const char *name,bool directory,std::int64_t size,std::int64_t mtime) {
    if (!name || !name[0] || name[0]=='.' || std::strchr(name,'/') || std::strchr(name,'\\') ||
        !clean(name,fs::max_path+1)) return;
    const auto kind=fs::classify_name(name,directory);
    if (!listing.other && kind==fs::Kind::other) return;
    fs::Entry e{};
    std::snprintf(e.name,sizeof e.name,"%s",name);
    const auto len=std::strlen(listing.path);
    if (std::snprintf(e.path,sizeof e.path,"%s%s%s",listing.path,len && listing.path[len-1]=='/'?"":"/",name)>=static_cast<int>(sizeof e.path)) {
        ++listing.omitted; return;
    }
    e.kind=kind; e.size=size; e.mtime=mtime;
    int pos=0;
    while(pos<listing.count && !before(e,listing.entries[pos])) ++pos;
    if (listing.count==listing_cap) ++listing.omitted;
    else ++listing.count;
    if(pos<listing.count) {
        std::memmove(listing.entries+pos+1,listing.entries+pos,(listing.count-pos-1)*sizeof e);
        listing.entries[pos]=e;
    }
}
void *list_worker(void *) {
    auto c=listing.connection;
    const char *relative=listing.path+8;
    if(!resolve_host(c)) {
        listing.error=EHOSTUNREACH;
        done.store(true,std::memory_order_release);
        return nullptr;
    }
#ifdef MOVIAN_HAVE_SMB2
    if (!std::strncmp(c.url,"smb2://",7)) {
        char path[768];
        auto *ctx=connect(c,path,sizeof path);
        if (!ctx) listing.error=EACCES;
        else {
            auto *dir=append_relative(path,sizeof path,relative)?smb2_opendir(ctx,path):nullptr;
            if (!dir) listing.error=EIO;
            else {
                while(auto *e=smb2_readdir(ctx,dir))
                    entry(e->name,e->st.smb2_type==SMB2_TYPE_DIRECTORY,e->st.smb2_size,e->st.smb2_mtime);
                smb2_closedir(ctx,dir);
            }
            smb2_destroy_context(ctx);
        }
    } else
#endif
#ifdef MOVIAN_HAVE_FFMPEG
    if (!std::strncmp(c.url,"ftp://",6)) {
        char url[768];
        AVIODirContext *dir=nullptr;
        auto *opts=ftp_options(c);
        int ret=remote_path(c,relative,url,sizeof url)?avio_open_dir(&dir,url,&opts):-1;
        av_dict_free(&opts);
        if (ret<0) listing.error=EACCES;
        else {
            AVIODirEntry *e=nullptr;
            while ((ret=avio_read_dir(dir,&e))>=0 && e) {
                entry(e->name,e->type==AVIO_ENTRY_DIRECTORY,e->size,e->modification_timestamp/1000000);
                avio_free_directory_entry(&e);
            }
            if(ret<0 && ret!=AVERROR_EOF) listing.error=EIO;
            avio_close_dir(&dir);
        }
    } else
#endif
    { listing.error=ENOTSUP; }
    diagnostics::record("network","listing completed protocol=%s entries=%d error=%d",
                         !std::strncmp(c.url,"ftp:",4)?"FTP":"SMB2",listing.count,listing.error);
    done.store(true,std::memory_order_release);
    return nullptr;
}
} // namespace
Config &config() noexcept { return cfg; }
bool valid_url(const char *url) noexcept {
    if (!clean(url,256)) return false;
    const int prefix=!std::strncmp(url,"smb2://",7)?7:!std::strncmp(url,"ftp://",6)?6:0;
    if(!prefix) return false;
    const char *host=url+prefix;
    const auto n=std::strcspn(host,"/");
    if(!n || std::strpbrk(url,"@?#\\") || std::memchr(host,' ',n)) return false;
    return prefix!=7 || (host[n]=='/' && host[n+1]);
}
bool is_path(const char *path) noexcept { return path && std::strlen(path)>=8 && !std::strncmp(path,"net://",6); }
void root_path(int index,char *out,unsigned cap) noexcept { std::snprintf(out,cap,"net://%d/",index); }
void load() noexcept {
    invalidate();
    cfg=Config{};
    auto *f=std::fopen(config_path,"rb");
    if(!f) return;
    char header[sizeof magic];
    Config next{};
    const bool ok=std::fread(header,1,sizeof header,f)==sizeof header && !std::memcmp(header,magic,sizeof magic) &&
        std::fread(&next,1,sizeof next,f)==sizeof next && std::fgetc(f)==EOF;
    std::fclose(f);
    // Validate boolean object representations before using a loaded structure.
    const auto *bytes=reinterpret_cast<const unsigned char *>(&next);
    if(!ok || bytes[offsetof(Config,smb_large_buffer)]>1 || bytes[offsetof(Config,ftp_server)]>1 ||
        !clean(next.system_name,sizeof next.system_name) || !clean(next.ftp_user,sizeof next.ftp_user) ||
        !clean(next.ftp_password,sizeof next.ftp_password) || next.ftp_port<1024 || next.ftp_port>65535 ||
        next.smb_xattr<0 || next.smb_xattr>3) return;
    for(auto &c:next.connections)
        if(reinterpret_cast<const unsigned char *>(&c)[offsetof(Connection,remember)]>1 ||
           (c.url[0] && !valid_url(c.url)) || !clean(c.user,sizeof c.user) ||
           !clean(c.password,sizeof c.password) || !clean(c.domain,sizeof c.domain)) return;
    cfg=next;
}
bool save() noexcept {
    constexpr char tmp[] = MOVIAN_DOWNLOAD0 "/network-private.bin.tmp";
    const int fd=::open(tmp,O_WRONLY|O_CREAT|O_TRUNC,0600);
    if(fd<0) return false;
    fchmod(fd,0600);
    auto *f=fdopen(fd,"wb");
    if(!f) { ::close(fd); return false; }
    Config stored=cfg;
    for(auto &c:stored.connections) if(!c.remember) {
        std::memset(c.user,0,sizeof c.user); std::memset(c.password,0,sizeof c.password);
        std::memset(c.domain,0,sizeof c.domain);
    }
    bool ok=std::fwrite(magic,1,sizeof magic,f)==sizeof magic && std::fwrite(&stored,1,sizeof stored,f)==sizeof stored;
    ok=std::fflush(f)==0 && ok;
    ok=std::fclose(f)==0 && ok;
    if(ok) ok=std::rename(tmp,config_path)==0;
    if(!ok) std::remove(tmp);
    invalidate();
    return ok;
}
void invalidate() noexcept { ready=false; ++generation; }
bool poll() noexcept {
    if(!running || !done.load(std::memory_order_acquire)) return false;
    pthread_join(worker,nullptr); running=false; ready=listing_generation==generation; return true;
}
void begin_shutdown() noexcept {
    shutting_down=true;
    server_begin_shutdown();
}
bool is_quiescent() noexcept {
    (void)poll();
    const bool server_done=server_is_quiescent();
    return !running && server_done;
}
bool cancel_shutdown() noexcept {
    if(!is_quiescent()) return false;
    shutting_down=false;
    return restart_server();
}
int list(const char *path,fs::Entry *out,int cap,int *count,int *error,bool other,fs::SortOrder sort,bool folders) noexcept {
    *count=0; *error=0;
    if(shutting_down) { *error=ECANCELED; return -1; }
    if(!running && ready && !std::strcmp(path,listing.path) && other==listing.other && sort==listing.sort && folders==listing.folders) {
        if(listing.error) { *error=listing.error; return -1; }
        *count=std::min(cap,listing.count);
        std::memcpy(out,listing.entries,*count*sizeof *out);
        *error=listing.omitted+listing.count-*count;
        return 0;
    }
    if(running) { *error=EAGAIN; return -1; }
    Connection c; const char *relative;
    if(!parse(path,c,relative)) { *error=EINVAL; return -1; }
    listing=Listing{};
    std::snprintf(listing.path,sizeof listing.path,"%s",path);
    listing.connection=c; listing.other=other; listing.sort=sort; listing.folders=folders;
    ready=false; listing_generation=generation; done.store(false);
    running=pthread_create(&worker,nullptr,list_worker,nullptr)==0;
    *error=running?EAGAIN:EIO;
    return -1;
}
struct File {
#ifdef MOVIAN_HAVE_FFMPEG
    AVIOContext *ftp=nullptr;
#endif
#ifdef MOVIAN_HAVE_SMB2
    smb2_context *smb=nullptr;
    smb2fh *handle=nullptr;
#endif
    std::int64_t size=0,offset=0;
    unsigned chunk=65536;
};
File *open(const char *path) noexcept {
    Connection c; const char *relative;
    if(!parse(path,c,relative) || !resolve_host(c)) return nullptr;
    auto *f=new(std::nothrow) File;
    if(!f) return nullptr;
#ifdef MOVIAN_HAVE_SMB2
    if(!std::strncmp(c.url,"smb2://",7)) {
        char remote[768];
        f->smb=connect(c,remote,sizeof remote);
        if(f->smb && append_relative(remote,sizeof remote,relative)) {
            f->handle=smb2_open(f->smb,remote,O_RDONLY);
            smb2_stat_64 st{};
            if(f->handle && smb2_fstat(f->smb,f->handle,&st)==0) {
                f->size=st.smb2_size;
                f->chunk=std::min(smb2_get_max_read_size(f->smb),cfg.smb_large_buffer?1048576u:65536u);
                diagnostics::record("network","file opened protocol=SMB2 bytes=%lld",static_cast<long long>(f->size));
                return f;
            }
        }
    }
#endif
#ifdef MOVIAN_HAVE_FFMPEG
    if(!std::strncmp(c.url,"ftp://",6)) {
        char url[768]; auto *opts=ftp_options(c);
        int ret=remote_path(c,relative,url,sizeof url)?avio_open2(&f->ftp,url,AVIO_FLAG_READ,nullptr,&opts):-1;
        av_dict_free(&opts);
        if(ret>=0) {
            f->size=avio_size(f->ftp);
            diagnostics::record("network","file opened protocol=FTP bytes=%lld",static_cast<long long>(f->size));
            return f;
        }
    }
#endif
    close(f); return nullptr;
}
int read(File *f,unsigned char *out,int size) noexcept {
    if(!f || size<=0) return -EINVAL;
    int n=-EIO;
#ifdef MOVIAN_HAVE_SMB2
    if(f->smb) n=smb2_pread(f->smb,f->handle,out,std::min(static_cast<unsigned>(size),f->chunk),f->offset);
#endif
#ifdef MOVIAN_HAVE_FFMPEG
    if(f->ftp) { n=avio_read(f->ftp,out,size); if(n==AVERROR_EOF) n=0; }
#endif
    if(n>0) f->offset+=n;
    return n;
}
std::int64_t seek(File *f,std::int64_t offset,int whence) noexcept {
    if(!f) return -EINVAL;
    if(whence==0x10000) return f->size; // AVSEEK_SIZE
    whence &= ~0x20000; // AVSEEK_FORCE
    const auto base=whence==SEEK_SET?0:whence==SEEK_CUR?f->offset:whence==SEEK_END?f->size:-1;
    if(base<0 || (offset<0 && offset < -base) || (offset>=0 && offset>f->size-base)) return -EINVAL;
    const auto target=base+offset;
#ifdef MOVIAN_HAVE_FFMPEG
    if(f->ftp && avio_seek(f->ftp,target,SEEK_SET)<0) return -EIO;
#endif
    f->offset=target; return target;
}
void close(File *f) noexcept {
    if(!f) return;
#ifdef MOVIAN_HAVE_SMB2
    if(f->handle) smb2_close(f->smb,f->handle);
    if(f->smb) smb2_destroy_context(f->smb);
#endif
#ifdef MOVIAN_HAVE_FFMPEG
    if(f->ftp) avio_closep(&f->ftp);
#endif
    delete f;
}
} // namespace movian::network
