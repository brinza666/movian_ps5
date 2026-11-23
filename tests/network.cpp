// Controlled SMB2/FTP fixture client and FTP server driver.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "network.hpp"
#include "input_file.hpp"
#include "torrent.hpp"
#include "plugins.hpp"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
using namespace movian;
int main(int argc,char **argv) {
    assert(argc==2);
    mkdir("download0",0700);
    auto &cfg=network::config();
    if(!std::strcmp(argv[1],"server")) {
        cfg.ftp_server=true; cfg.ftp_port=std::atoi(std::getenv("MOVIAN_TEST_FTP_PORT"));
        std::strcpy(cfg.ftp_user,"fixture"); std::strcpy(cfg.ftp_password,"fixture-pass");
        assert(network::save() && network::restart_server());
        std::puts("READY"); std::fflush(stdout); std::getchar();
        cfg.ftp_server=false; assert(network::restart_server()); return 0;
    }
    if(!std::strcmp(argv[1],"list-server")) {
        auto &c=cfg.connections[0];
        std::snprintf(c.url,sizeof c.url,"%s",std::getenv("MOVIAN_TEST_FTP_URL"));
        std::strcpy(c.user,"fixture"); std::strcpy(c.password,"fixture-pass");
        fs::Entry entries[8];int count=0,error=0;bool listed=false;
        for(int n=0;n<1800;++n) {
            if(network::list("net://0/",entries,8,&count,&error,false,fs::SortOrder::filename,true)==0) {listed=true;break;}
            assert(error==EAGAIN);usleep(10000);(void)network::poll();
        }
        assert(listed && count==3);
        for(int i=0;i<count;++i) assert(entries[i].kind==fs::Kind::directory);
        auto *file=network::open("net://0/Title data/clip.bin");assert(file);
        assert(network::seek(file,200001,SEEK_SET)==200001);
        unsigned char data[32];assert(network::read(file,data,32)==32 && data[0]==(200001%256));
        network::close(file);
        std::puts("FTP LIST fallback and REST read against Movian server passed");
        return 0;
    }
    assert(!network::valid_url("ftp://user:password@host/"));
    assert(!network::valid_url("smb2://server"));
    assert(!network::valid_url("ftp://server/\r\nUSER other"));
    for(int index=0;index<2;++index) {
        auto &c=cfg.connections[index];
        std::snprintf(c.url,sizeof c.url,"%s",std::getenv(index==0?"MOVIAN_TEST_SMB_URL":"MOVIAN_TEST_FTP_URL"));
        std::strcpy(c.user,"fixture"); std::strcpy(c.password,"fixture-pass");
    }
    assert(network::save()); network::load();
    assert(!std::strcmp(cfg.connections[0].password,"fixture-pass"));
    struct stat permissions{}; assert(stat("download0/network-private.bin",&permissions)==0);
    assert((permissions.st_mode&0777)==0600);
    for(int index=0;index<2;++index) {
        char path[96]; network::root_path(index,path,sizeof path);
        fs::Entry entries[8]; int count=0,error=0; bool listed=false;
        for(int n=0;n<1800;++n) {
            if(network::list(path,entries,8,&count,&error,false,fs::SortOrder::filename,true)==0) { listed=true; break; }
            assert(error==EAGAIN); usleep(10000); (void)network::poll();
        }
        assert(listed && count==3 && entries[0].kind==fs::Kind::directory);
        assert(std::strlen(entries[2].name)==fs::max_name && std::strlen(entries[2].path)>150);
        auto *long_file=network::open(entries[2].path); assert(long_file); network::close(long_file);
        int small_count=0,omitted=0;fs::Entry small[1];
        assert(network::list(path,small,1,&small_count,&omitted,false,fs::SortOrder::filename,true)==0);
        assert(small_count==1 && omitted==2);
        assert(!std::strcmp(entries[1].name,"a sample.mkv"));
        std::snprintf(path,sizeof path,"net://%d/a sample.mkv",index);
        auto *file=network::open(path); assert(file);
        unsigned char a[4096],b[4096];
        assert(network::read(file,a,sizeof a)==sizeof a);
        assert(network::seek(file,0,SEEK_SET)==0);
        assert(network::read(file,b,sizeof b)==sizeof b && !std::memcmp(a,b,sizeof a));
        assert(network::seek(file,-32,SEEK_END)>0);
        assert(network::read(file,b,32)==32);
        assert(network::seek(file,-1,SEEK_SET)<0);
        network::close(file);
        // Descriptors and packages share the authenticated seekable input.
        std::snprintf(path,sizeof path,"net://%d/Nested/sample.torrent",index);
        torrent::Metadata metadata{};
        assert(torrent::inspect(path,metadata) && metadata.count==1 && metadata.direct_peer);
        assert(metadata.files[0].size==256000 && !std::strcmp(metadata.files[0].path,"a sample.mkv"));
        std::size_t bytes=99;
        auto *descriptor=input::read_bounded(path,1024*1024,bytes);
        torrent::PeerInfo peer{};
        assert(descriptor && torrent::peer_info(descriptor,bytes,peer) && peer.hash_bytes==80);
        std::free(descriptor);
        assert(!input::read_bounded(path,16,bytes) && bytes==0);
#ifdef MOVIAN_HAVE_DUKTAPE
        char install_error[160]{};plugins::Plugin plugin{};
        std::snprintf(path,sizeof path,"net://%d/Nested/package.zip",index);
        assert(plugins::inspect_package(path,plugin) && !std::strcmp(plugin.id,"network-fixture"));
        assert(plugins::install(path,install_error,sizeof install_error));
        assert(plugins::inspect_package("download0/plugins-v23/network-fixture.zip",plugin));
        std::snprintf(path,sizeof path,"net://%d/Nested/invalid.zip",index);
        assert(!plugins::install(path,install_error,sizeof install_error));
        std::snprintf(path,sizeof path,"net://%d/Nested/oversized.zip",index);
        assert(!plugins::inspect_package(path,plugin));
        // Failed installs must leave the valid installed copy readable.
        assert(plugins::inspect_package("download0/plugins-v23/network-fixture.zip",plugin));
#endif
        int packages=0,package_error=0;
        std::snprintf(path,sizeof path,"net://%d/Nested",index);
        for(int n=0;n<1800;++n) {
            if(fs::list_directory(path,entries,8,&packages,&package_error,true,fs::SortOrder::filename,true,".zip")==0) break;
            assert(package_error==EAGAIN);usleep(10000);(void)network::poll();
        }
        assert(packages==3);
        // The ZIP picker omits an error output; this must also be safe remotely.
        assert(fs::list_directory(path,entries,8,&packages,nullptr,true,fs::SortOrder::filename,true,".zip")==0);
        for(int i=0;i<packages;++i) assert(std::strstr(entries[i].name,".zip"));
        std::snprintf(path,sizeof path,"net://%d/a sample.mkv",index);
        auto *whole=input::read_bounded(path,256000,bytes);
        assert(whole && bytes==256000);
        for(std::size_t n=0;n<bytes;++n) assert(static_cast<unsigned char>(whole[n])==n%256);
        std::free(whole);
        cfg.connections[index].password[0]=0;
        assert(!network::open(path));
        std::printf("%s login, listing, seek, torrent descriptor and ZIP install passed\n",index==0?"SMB2":"FTP");
    }
    auto &c=cfg.connections[0]; c.remember=false;
    std::strcpy(c.password,"session-only");
    assert(network::save()); assert(!std::strcmp(c.password,"session-only"));
    network::load(); assert(!cfg.connections[0].password[0] && !cfg.connections[0].user[0]);
    assert(!network::open("net://0/../private"));
    assert(!network::open("net://9/a.mkv"));
}
