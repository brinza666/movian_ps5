// Small title-local FTP server, authenticated, passive mode, sandbox-visible roots.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "network.hpp"
#include "diagnostics.hpp"
#include "paths.hpp"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace movian::network {
namespace {
std::atomic<bool> serving{false};
std::atomic<bool> server_done{true};
int listener=-1;
pthread_t server_thread;
bool started=false;
Config active;
struct Root { const char *name; const char *path; };
constexpr Root roots[]={{"Title data",MOVIAN_DOWNLOAD0},{"Movian library",MOVIAN_DATA_DIR},
#ifdef MOVIAN_HOST_PREVIEW
    {"USB 0","./usb"}
#else
    {"USB 0","/mnt/usb0"},{"USB 1","/mnt/usb1"}
#endif
};
bool send_all(int fd,const char *data,std::size_t len) {
    while(len && serving.load()) {
        const auto n=::send(fd,data,len,
#ifdef MSG_NOSIGNAL
            MSG_NOSIGNAL
#else
            0
#endif
        );
        if(n<=0) return false;
        data+=n; len-=n;
    }
    return len==0;
}
bool reply(int fd,const char *s) { return send_all(fd,s,std::strlen(s)); }
bool line(int fd,char *out,unsigned cap) {
    unsigned n=0;
    while(n+1<cap && serving.load()) {
        char ch;
        const int r=::recv(fd,&ch,1,0);
        if(r<=0) return false;
        if(ch=='\n') { out[n]=0; return true; }
        if(ch!='\r') out[n++]=ch;
    }
    return false;
}
void timeout(int fd) {
#ifdef SO_NOSIGPIPE
    int one=1; setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof one);
#endif
    timeval t{1,0}; setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&t,sizeof t);
    t.tv_sec=5; setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&t,sizeof t);
}
// Resolve only named roots. realpath prevents symlink escapes and private config access.
bool resolve(const char *cwd,const char *input,char *virt,unsigned vcap,char *physical,unsigned pcap) {
    char joined[768];
    if(std::snprintf(joined,sizeof joined,"%s%s%s",input[0]=='/'?"":cwd,
                     input[0]=='/' || !std::strcmp(cwd,"/")?"":"/",input)>=static_cast<int>(sizeof joined)) return false;
    char normalized[768]="/";
    char *save=nullptr;
    for(char *p=strtok_r(joined,"/",&save);p;p=strtok_r(nullptr,"/",&save)) {
        if(!std::strcmp(p,".")) continue;
        if(!std::strcmp(p,"..")) {
            char *slash=std::strrchr(normalized,'/');
            if(slash && slash!=normalized) *slash=0; else normalized[1]=0;
            continue;
        }
        if(p[0]=='.' || !std::strncmp(p,"network-private.bin",19)) return false;
        const auto len=std::strlen(normalized);
        if(std::snprintf(normalized+len,sizeof normalized-len,"%s%s",len>1?"/":"",p)>=static_cast<int>(sizeof normalized-len)) return false;
    }
    if(std::strlen(normalized)>=vcap) return false;
    std::snprintf(virt,vcap,"%s",normalized);
    if(!std::strcmp(virt,"/")) { physical[0]=0; return true; }
    for(const auto &root:roots) {
        const auto n=std::strlen(root.name);
        if(std::strncmp(virt+1,root.name,n) || (virt[n+1] && virt[n+1]!='/')) continue;
        char base[1024],candidate[1024];
        if(!realpath(root.path,base)) return false;
        if(std::snprintf(candidate,sizeof candidate,"%s%s",base,virt+n+1)>=static_cast<int>(sizeof candidate)) return false;
        char actual[1024];
        if(!realpath(candidate,actual)) {
            // For a new upload, validate its existing parent before creating it.
            char *end=std::strrchr(candidate,'/');
            if(!end || !end[1]) return false;
            char name[256]; std::snprintf(name,sizeof name,"%s",end+1); *end=0;
            if(!realpath(candidate,actual)) return false;
            const auto used=std::strlen(actual);
            if(std::snprintf(actual+used,sizeof actual-used,"/%s",name)>=static_cast<int>(sizeof actual-used)) return false;
        }
        const char *basename=std::strrchr(actual,'/');
        if(basename && !std::strncmp(basename+1,"network-private.bin",19)) return false;
        const auto b=std::strlen(base);
        if(std::strncmp(actual,base,b) || (actual[b] && actual[b]!='/') || std::strlen(actual)>=pcap) return false;
        std::snprintf(physical,pcap,"%s",actual); return true;
    }
    return false;
}
void session(int client) {
    timeout(client);
    char welcome[160];
    std::snprintf(welcome,sizeof welcome,"220 %s FTP ready\r\n",active.system_name);
    reply(client,welcome);
    bool user=false,auth=false;
    char cwd[768]="/",cmd[1024];
    int passive=-1;
    std::int64_t rest=0;
    unsigned idle=0;
    while(serving.load()) {
        pollfd p{client,POLLIN,0};
        const int r=poll(&p,1,500);
        if(r==0) { if(++idle>120) break; continue; }
        if(r<0 || !(p.revents&POLLIN) || !line(client,cmd,sizeof cmd)) break;
        idle=0;
        char *arg=std::strchr(cmd,' '); if(arg) *arg++=0; else arg=cmd+std::strlen(cmd);
        for(char *s=cmd;*s;++s) if(*s>='a' && *s<='z') *s-=32;
        if(!std::strcmp(cmd,"QUIT")) { reply(client,"221 Goodbye\r\n"); break; }
        if(!std::strcmp(cmd,"USER")) { auth=false; user=!std::strcmp(arg,active.ftp_user); reply(client,"331 Password required\r\n"); continue; }
        if(!std::strcmp(cmd,"PASS")) { auth=user && !std::strcmp(arg,active.ftp_password); reply(client,auth?"230 Logged in\r\n":"530 Login incorrect\r\n"); continue; }
        if(!std::strcmp(cmd,"SYST")) { reply(client,"215 UNIX Type: L8\r\n"); continue; }
        if(!std::strcmp(cmd,"FEAT")) { reply(client,"211-Features\r\n EPSV\r\n PASV\r\n REST STREAM\r\n SIZE\r\n UTF8\r\n211 End\r\n"); continue; }
        if(!auth) { reply(client,"530 Login first\r\n"); continue; }
        if(!std::strcmp(cmd,"PWD")) { char out[800]; std::snprintf(out,sizeof out,"257 \"%s\"\r\n",cwd); reply(client,out); continue; }
        if(!std::strcmp(cmd,"TYPE") || !std::strcmp(cmd,"NOOP") || !std::strcmp(cmd,"OPTS")) { reply(client,"200 OK\r\n"); continue; }
        if(!std::strcmp(cmd,"PASV") || !std::strcmp(cmd,"EPSV")) {
            if(passive>=0) ::close(passive);
            passive=::socket(AF_INET,SOCK_STREAM,0);
            sockaddr_in address{}; socklen_t length=sizeof address;
            getsockname(client,reinterpret_cast<sockaddr *>(&address),&length);
            address.sin_port=0;
            if(passive<0 || bind(passive,reinterpret_cast<sockaddr *>(&address),sizeof address)<0 || listen(passive,1)<0) {
                if(passive>=0) ::close(passive); passive=-1; reply(client,"425 Passive listener unavailable\r\n"); continue;
            }
            getsockname(passive,reinterpret_cast<sockaddr *>(&address),&length);
            char out[128]; const auto ip=ntohl(address.sin_addr.s_addr); const auto port=ntohs(address.sin_port);
            if(!std::strcmp(cmd,"EPSV")) std::snprintf(out,sizeof out,"229 Entering Extended Passive Mode (|||%u|)\r\n",port);
            else std::snprintf(out,sizeof out,"227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)\r\n",ip>>24,(ip>>16)&255,(ip>>8)&255,ip&255,port/256,port%256);
            reply(client,out); continue;
        }
        if(!std::strcmp(cmd,"REST")) {
            errno=0; char *end=nullptr; const auto n=std::strtoll(arg,&end,10);
            if(!arg[0] || *end || n<0 || errno==ERANGE) { reply(client,"501 Invalid offset\r\n"); continue; }
            rest=n; reply(client,"350 Restart position accepted\r\n"); continue;
        }
        char virt[768],physical[1024];
        if(!resolve(cwd,!std::strcmp(cmd,"CDUP")?"..":arg,virt,sizeof virt,physical,sizeof physical)) { reply(client,"550 Path unavailable\r\n"); continue; }
        struct stat st{};
        const bool exists=physical[0] && stat(physical,&st)==0;
        if(!std::strcmp(cmd,"CWD") || !std::strcmp(cmd,"CDUP")) {
            if(!physical[0] || (exists && S_ISDIR(st.st_mode))) { std::snprintf(cwd,sizeof cwd,"%s",virt); reply(client,"250 Directory changed\r\n"); }
            else reply(client,"550 Directory unavailable\r\n");
            continue;
        }
        if(!std::strcmp(cmd,"SIZE")) { char out[96]; std::snprintf(out,sizeof out,"213 %lld\r\n",static_cast<long long>(st.st_size)); reply(client,exists && S_ISREG(st.st_mode)?out:"550 File unavailable\r\n"); continue; }
        const bool listing=!std::strcmp(cmd,"LIST") || !std::strcmp(cmd,"NLST");
        const bool retr=!std::strcmp(cmd,"RETR"),stor=!std::strcmp(cmd,"STOR");
        if(!listing && !retr && !stor) { reply(client,"502 Command not implemented\r\n"); continue; }
        if(passive<0) { reply(client,"425 Use EPSV or PASV\r\n"); continue; }
        if((retr && (!exists || !S_ISREG(st.st_mode))) || (stor && (!physical[0] || rest))) { reply(client,"550 File unavailable\r\n"); rest=0; continue; }
        reply(client,"150 Opening data connection\r\n");
        pollfd wait{passive,POLLIN,0};
        const int waited=poll(&wait,1,3000);
        sockaddr_in peer{},control{}; socklen_t plen=sizeof peer,clen=sizeof control;
        int data=waited>0?accept(passive,reinterpret_cast<sockaddr *>(&peer),&plen):-1;
        ::close(passive); passive=-1;
        getpeername(client,reinterpret_cast<sockaddr *>(&control),&clen);
        if(data<0 || peer.sin_addr.s_addr!=control.sin_addr.s_addr) {
            if(data>=0) ::close(data); reply(client,"425 Data connection failed\r\n"); continue;
        }
        timeout(data); bool ok=true;
        if(listing) {
            fs::Entry entries[256]; int count=0,error=0;
            if(!physical[0]) {
                for(const auto &root:roots) {
                    auto &e=entries[count++]; e={}; std::snprintf(e.name,sizeof e.name,"%s",root.name); e.kind=fs::Kind::directory;
                }
            } else ok=fs::list_dir(physical,entries,256,&count,&error,true)==0;
            for(int i=0;ok && i<count;++i) {
                const auto &e=entries[i];
                if(!std::strncmp(e.name,"network-private.bin",19) || std::strpbrk(e.name,"\r\n")) continue;
                char text[256];
                if(!std::strcmp(cmd,"NLST")) std::snprintf(text,sizeof text,"%s\r\n",e.name);
                else std::snprintf(text,sizeof text,"%crw-r--r-- 1 movian movian %lld Jan 01 2026 %s\r\n",e.kind==fs::Kind::directory?'d':'-',e.size,e.name);
                ok=reply(data,text);
            }
        } else {
            // O_NOFOLLOW also closes a symlink replacement race at the final component.
            const int fd=::open(physical,(retr?O_RDONLY:O_WRONLY|O_CREAT|O_TRUNC)|O_NOFOLLOW,0666);
            ok=fd>=0;
            if(ok && retr) ok=lseek(fd,rest,SEEK_SET)>=0;
            char buffer[64*1024];
            while(ok && serving.load()) {
                const auto n=retr ? ::read(fd,buffer,sizeof buffer) : ::recv(data,buffer,sizeof buffer,0);
                if(!n) break;
                if(n<0) { ok=false; break; }
                if(retr) ok=send_all(data,buffer,n);
                else { std::size_t used=0; while(used<static_cast<unsigned>(n)) {
                    const auto wrote=::write(fd,buffer+used,n-used); if(wrote<=0) {ok=false;break;} used+=wrote;
                }}
            }
            if(fd>=0) ::close(fd);
        }
        rest=0; ::close(data); reply(client,ok?"226 Transfer complete\r\n":"451 Transfer failed\r\n");
    }
    if(passive>=0) ::close(passive);
    ::close(client);
}
void *loop(void *) {
    while(serving.load()) {
        pollfd p{listener,POLLIN,0};
        if(poll(&p,1,500)<=0) continue;
        int client=accept(listener,nullptr,nullptr);
        if(client>=0) session(client);
    }
    server_done.store(true,std::memory_order_release);
    return nullptr;
}
}
void server_begin_shutdown() noexcept { serving=false; }
bool server_is_quiescent() noexcept {
    if(started && !server_done.load(std::memory_order_acquire)) return false;
    if(started) { pthread_join(server_thread,nullptr); started=false; }
    if(listener>=0) { ::close(listener); listener=-1; }
    return true;
}
bool server_running() noexcept { return serving.load(); }
bool restart_server() noexcept {
    serving=false;
    if(started) { pthread_join(server_thread,nullptr); started=false; }
    if(listener>=0) { ::close(listener); listener=-1; }
    if(!config().ftp_server) return true;
    if(!config().ftp_user[0] || !config().ftp_password[0]) return false;
    active=config();
    listener=socket(AF_INET,SOCK_STREAM,0);
    sockaddr_in address{};
#if defined(__FreeBSD__)
    address.sin_len=sizeof address;
#endif
    address.sin_family=AF_INET; address.sin_port=htons(active.ftp_port); address.sin_addr.s_addr=htonl(INADDR_ANY);
    int one=1; setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
    if(listener<0 || bind(listener,reinterpret_cast<sockaddr *>(&address),sizeof address)<0 || listen(listener,4)<0) {
        diagnostics::record("network","FTP server bind failed errno=%d",errno);
        if(listener>=0) ::close(listener); listener=-1; return false;
    }
    serving=true;
    server_done.store(false,std::memory_order_relaxed);
    started=pthread_create(&server_thread,nullptr,loop,nullptr)==0;
    if(!started) { serving=false; server_done=true; ::close(listener); listener=-1; }
    return started;
}
} // namespace movian::network
