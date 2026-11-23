// SPDX-License-Identifier: GPL-3.0-or-later
// Shared production UI callbacks, exercised over a real loopback STPP socket.
#include "ui.cpp"
#include <cassert>
#include <chrono>
#include <csignal>
#include <deque>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

// Input and playback are inert: this test makes no audio/video claims.
static unsigned host_pressed;
unsigned movian::pad::poll() noexcept { return host_pressed; }
movian::pad::Touch movian::pad::touch() noexcept { return {}; }
bool movian::keyboard::open(const char *, const char *, const char *, int, bool) noexcept { return false; }
movian::keyboard::State movian::keyboard::poll() noexcept { return State::idle; }
const char *movian::keyboard::text() noexcept { return ""; }
void movian::keyboard::clear() noexcept {}
namespace movian::player {
Kind classify(const char *) noexcept { return Kind::none; }
bool open(const char *, char *, int, bool, const torrent::VerifiedFile *) noexcept { return false; }
void close() noexcept {}
bool active() noexcept { return false; }
bool playing() noexcept { return false; }
void toggle_pause() noexcept {}
void seek_step_by(int) noexcept {}
const char *path() noexcept { return ""; }
const char *title() noexcept { return ""; }
double time() noexcept { return 0; }
double duration() noexcept { return 0; }
bool ended() noexcept { return false; }
int width() noexcept { return 0; }
int height() noexcept { return 0; }
bool handle(unsigned, bool (*)(int, bool)) noexcept { return false; }
bool tick() noexcept { return false; }
void draw(screen::Canvas &) noexcept {}
}
extern "C" int hwdec_ready() { return 0; }
extern "C" void hwdec_force_software(int) {}
extern "C" const char *hwdec_status() { return "Inert test decoder"; }
extern "C" int hwdec_open(hwdec_t **, int, int, int, int) { return -1; }
extern "C" void hwdec_headers(hwdec_t *, const uint8_t *, int) {}
extern "C" void hwdec_bind(hwdec_t *, uint8_t *, int, int) {}
extern "C" void hwdec_close(hwdec_t *) {}
extern "C" void hwdec_reset(hwdec_t *) {}
extern "C" int hwdec_flush(hwdec_t *) { return 0; }
extern "C" int hwdec_shown_width(const hwdec_t *) { return 0; }
extern "C" int hwdec_shown_height(const hwdec_t *) { return 0; }
extern "C" unsigned hwdec_mean_conversion_us(const hwdec_t *) { return 0; }
extern "C" int64_t hwdec_shown_pts(const hwdec_t *) { return -1; }
extern "C" int hwdec_visible_from_headers(int, const uint8_t *, int, int *, int *) { return -1; }
extern "C" int hwdec_feed(hwdec_t *, const uint8_t *, int, int64_t) { return -1; }
extern "C" int hwdec_extradata_annexb(int, const uint8_t *, int, uint8_t *, int, int *) { return -1; }
extern "C" int hwdec_packet_annexb(const uint8_t *, int, int, uint8_t *, int) { return -1; }

using Bytes = std::vector<uint8_t>;
static void put32(Bytes &b, uint32_t v) { for (int i=0;i<4;++i) b.push_back(v>>(8*i)); }
static uint32_t get32(const Bytes &b, size_t at) {
    assert(at+4<=b.size()); return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;
}
static void add_path(Bytes &b, std::initializer_list<const char *> parts, bool end) {
    for(auto p:parts) { assert(strlen(p)<255); b.push_back(strlen(p)); b.insert(b.end(),p,p+strlen(p)); }
    if(end) b.push_back(0);
}
static void pump(unsigned ms=10) {
    const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(ms);
    do { (void)movian::ui::update(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
    while(std::chrono::steady_clock::now()<until);
}
struct Wire {
    int fd=-1; uint32_t next_sid=1; std::deque<Bytes> pending;
    explicit Wire(uint16_t port) {
        fd=socket(AF_INET,SOCK_STREAM,0); assert(fd>=0);
        sockaddr_in address{}; address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(port);
        assert(connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof address)==0);
        const std::string request="GET /api/stpp HTTP/1.1\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Key: 1\r\n\r\n";
        send_all(Bytes(request.begin(),request.end()));std::string response;
        while(response.find("\r\n\r\n")==std::string::npos) { response.push_back(read(1)[0]);assert(response.size()<4096); }
        assert(response.starts_with("HTTP/1.1 101 "));
        send({0,3,0});auto hello=receive();assert(hello.size()==19&&hello[0]==0&&hello[1]==3);
    }
    ~Wire() { if(fd>=0) ::close(fd); }
    void send_all(const Bytes &b) {
        size_t at=0;while(at<b.size()) { auto n=::send(fd,b.data()+at,b.size()-at,MSG_NOSIGNAL);assert(n>0);at+=n; }
    }
    void send(const Bytes &b) {
        Bytes frame{130};if(b.size()<126) frame.push_back(b.size());
        else {frame.push_back(126);frame.push_back(b.size()>>8);frame.push_back(b.size());}
        frame.insert(frame.end(),b.begin(),b.end());send_all(frame);
    }
    Bytes read(size_t count) {
        Bytes b(count);size_t at=0;auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(at<count) {
            assert(std::chrono::steady_clock::now()<deadline);(void)movian::ui::update();pollfd p{fd,POLLIN,0};
            if(::poll(&p,1,5)>0) {auto n=recv(fd,b.data()+at,count-at,0);assert(n>0);at+=n;}
        }
        return b;
    }
    Bytes receive() {
        auto h=read(2);assert(h[0]==130&&!(h[1]&128));size_t n=h[1];
        if(n==126) {auto size=read(2);n=size_t(size[0])<<8|size[1];}
        assert(n<=65536);return read(n);
    }
    Bytes event(uint32_t sid,uint8_t kind) {
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        for(;;) {
            for(auto it=pending.begin();it!=pending.end();++it)
                if((*it)[1]==kind&&get32(*it,2)==sid) {auto result=*it;pending.erase(it);return result;}
            assert(std::chrono::steady_clock::now()<deadline);auto b=receive();assert(b.size()>=6&&b[0]==4);pending.push_back(std::move(b));
        }
    }
    uint32_t subscribe(uint32_t base,std::initializer_list<const char *> path) {
        const auto sid=next_sid++;Bytes b{1};put32(b,sid);put32(b,base);b.push_back(0);b.push_back(0);add_path(b,path,false);send(b);return sid;
    }
    std::string text(uint32_t sid) {auto b=event(sid,3);assert(b.size()>=7);return {b.begin()+7,b.end()};}
    std::vector<uint32_t> children(uint32_t sid) {
        event(sid,5);auto b=event(sid,6);std::vector<uint32_t> refs;
        for(size_t i=6;i<b.size();i+=4) refs.push_back(get32(b,i));return refs;
    }
    void open(const char *url) {
        Bytes b{5};put32(b,0);add_path(b,{"navigators","current","eventSink"},true);b.push_back(14);b.push_back(1);
        b.push_back(strlen(url));b.insert(b.end(),url,url+strlen(url));send(b);
    }
    void write_value(uint32_t base,const Bytes &payload) {
        Bytes b{3};put32(b,base);add_path(b,{"value"},true);b.insert(b.end(),payload.begin(),payload.end());send(b);
    }
    void integer(uint32_t base,int value) {
        Bytes payload{1};put32(payload,static_cast<uint32_t>(value));write_value(base,payload);
    }
};
static volatile std::sig_atomic_t serving=1;
static void stop_serving(int) { serving=0; }
int main(int argc,char **argv) {
    using namespace movian;using namespace movian::ui;
    assert(argc>=2&&font::load(argv[1]));assert(mkdir("download0",0700)==0||errno==EEXIST);
    settings::get()=settings::Data{};settings::save();
    g_place=Place::home;g_overlay=Overlay::none;
    const bool serve=argc>2&&std::string(argv[2])=="--serve";
    assert(g_property_server.start(serve?42011:0));publish_properties();
    if(serve) {
        std::signal(SIGINT,stop_serving);std::signal(SIGTERM,stop_serving);
        printf("Shared UI STPP listening on ws://127.0.0.1:%u/api/stpp\n",g_property_server.port());fflush(stdout);
        while(serving) pump();g_property_server.stop();return 0;
    }
    Wire wire(g_property_server.port());
    auto url=wire.subscribe(0,{"navigators","current","currentpage","url"});assert(wire.text(url)=="page:home");
    auto pages=wire.subscribe(0,{"navigators","current","pages"});auto home=wire.children(pages);assert(home.size()==1);
    assert(get32(wire.event(pages,10),6)==home[0]);
    auto page_url=wire.subscribe(home[0],{"url"});assert(wire.text(page_url)=="page:home");
    auto type=wire.subscribe(0,{"navigators","current","currentpage","model","type"});assert(wire.text(type)=="home");
    auto services=wire.subscribe(0,{"services","stable"});auto roots=wire.children(services);assert(roots.size()==std::size(home_tiles));
    bool settings_root=false,plugins_root=false;
    for(auto ref:roots) {auto s=wire.subscribe(ref,{"url"});auto value=wire.text(s);settings_root|=value=="settings:";plugins_root|=value=="movian:plugins";}
    assert(settings_root&&plugins_root);puts("Home services, current-page alias and selected-page reference passed");
    wire.open("settings:");assert(wire.text(url)=="settings:");assert(g_overlay==Overlay::settings&&!g_settings_in_category);
    const auto settings_ref=get32(wire.event(pages,10),6);assert(settings_ref!=home[0]);
    wire.open("settings:general");assert(wire.text(url)=="settings:general");assert(g_settings_in_category&&g_settings_category==0);
    const auto general_ref=get32(wire.event(pages,10),6);assert(general_ref!=settings_ref);
    assert(wire.text(wire.subscribe(general_ref,{"url"}))=="settings:general");
    auto rows_sid=wire.subscribe(0,{"navigators","current","currentpage","model","nodes"});auto rows=wire.children(rows_sid);
    uint32_t extension=0;
    for(auto ref:rows) {auto s=wire.subscribe(ref,{"metadata","title"});if(wire.text(s)=="Show filename extensions") extension=ref;}
    assert(extension);auto value=wire.subscribe(extension,{"value"});assert(get32(wire.event(value,1),6)==1);
    wire.integer(extension,0);assert(get32(wire.event(value,1),6)==0);assert(!settings::get().show_extensions);
    settings::get().show_extensions=true;settings::load();assert(!settings::get().show_extensions);
    puts("Remote OpenURL and real boolean setting write persisted and pushed");
    // An independent local handler must update the same subscribed remote value.
    setting_nudge(settings_menu::extensions,1);assert(settings::get().show_extensions);
    assert(get32(wire.event(value,1),6)==1);settings::get().show_extensions=false;settings::load();assert(settings::get().show_extensions);
    // Out-of-range values and wrong types cannot change the shared persisted setting.
    wire.integer(extension,-1);wire.integer(extension,2);wire.integer(extension,INT32_MAX);
    wire.write_value(extension,{2,0,0,192,63}); // A valid float is still the wrong setting type.
    wire.write_value(extension,{3,0,'x'});wire.write_value(extension,{0});pump(250);
    assert(settings::get().show_extensions);settings::load();assert(settings::get().show_extensions);
    wire.integer(extension,0);assert(get32(wire.event(value,1),6)==0);assert(!settings::get().show_extensions);
    wire.integer(extension,1);assert(get32(wire.event(value,1),6)==1);assert(settings::get().show_extensions);
    puts("Independent local changes, bounded writes, restore and bidirectional pushes passed");
    // Native Movian sends tag 13 rather than an explicit replacement boolean.
    wire.write_value(extension,{13});assert(get32(wire.event(value,1),6)==0);assert(!settings::get().show_extensions);
    settings::get().show_extensions=true;settings::load();assert(!settings::get().show_extensions);
    wire.write_value(extension,{13});assert(get32(wire.event(value,1),6)==1);assert(settings::get().show_extensions);
    puts("Native boolean toggle updates the shared persisted setting and pushes both directions");
    wire.open("page:home");assert(wire.text(url)=="page:home");assert(g_overlay==Overlay::none&&g_place==Place::home);
    g_property_server.stop();puts("Shared UI STPP host acceptance passed (inert playback; no PS5/audio claims)");
}
