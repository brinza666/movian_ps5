// SPDX-License-Identifier: GPL-3.0-or-later
// Real TCP/WebSocket tests; no private protocol internals or test hooks.
#include "stpp.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
using namespace movian::stpp;
using Bytes = std::vector<uint8_t>;
static void put32(Bytes &b, uint32_t v) { for (int i=0; i<4; ++i) b.push_back(v >> (i*8)); }
static uint32_t get32(const Bytes &b, size_t at) { assert(at+4 <= b.size()); return uint32_t(b[at]) | uint32_t(b[at+1])<<8 | uint32_t(b[at+2])<<16 | uint32_t(b[at+3])<<24; }
static void path(Bytes &b, std::initializer_list<const char *> parts, bool end = true) { for (auto p : parts) { b.push_back(std::strlen(p)); b.insert(b.end(), p, p+std::strlen(p)); } if (end) b.push_back(0); }
static Bytes subscribe(uint32_t sid, uint32_t base, std::initializer_list<const char *> parts) { Bytes b{1}; put32(b, sid); put32(b, base); b.push_back(0); b.push_back(0); path(b, parts, false); return b; }
static Bytes mutation(uint8_t op, uint32_t base, std::initializer_list<const char *> parts, Bytes payload) { Bytes b{op}; put32(b, base); path(b, parts); b.insert(b.end(), payload.begin(), payload.end()); return b; }
struct Client {
    int fd; bool masked; std::array<uint8_t,16> instance{};
    Client(uint16_t port, bool modern) : masked(modern) {
        fd = socket(AF_INET, SOCK_STREAM, 0); assert(fd >= 0);
        sockaddr_in sa{}; sa.sin_family=AF_INET; sa.sin_addr.s_addr=htonl(INADDR_LOOPBACK); sa.sin_port=htons(port);
        assert(connect(fd, reinterpret_cast<sockaddr *>(&sa), sizeof(sa)) == 0);
        std::string request = "GET /api/stpp HTTP/1.1\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Key: ";
        request += modern ? "dGhlIHNhbXBsZSBub25jZQ==\r\nHost: localhost\r\nSec-WebSocket-Version: 13\r\n\r\n" : "1\r\n\r\n";
        send_all(Bytes(request.begin(), request.end()));
        std::string response;
        while (response.find("\r\n\r\n") == std::string::npos) { auto b = read(1); response.push_back(b[0]); assert(response.size() < 4096); }
        assert(response.rfind("HTTP/1.1 101 ", 0) == 0);
        if (modern) assert(response.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);
        send({0,3,0}); auto hello=receive(); assert(hello.size()==19 && hello[0]==0 && hello[1]==3 && hello[18]==0);
        std::copy(hello.begin()+2, hello.begin()+18, instance.begin());
    }
    ~Client() { close(fd); }
    Bytes read(size_t n) {
        Bytes b(n); size_t at=0;
        while (at<n) { pollfd p{fd,POLLIN,0}; assert(::poll(&p,1,2000)>0); auto got=recv(fd,b.data()+at,n-at,0); assert(got>0); at+=got; }
        return b;
    }
    void send_all(const Bytes &b) { size_t at=0; while(at<b.size()) { auto n=::send(fd,b.data()+at,b.size()-at,MSG_NOSIGNAL); assert(n>0); at+=n; } }
    void send(const Bytes &b, uint8_t op=2) {
        Bytes frame{uint8_t(128|op)}; auto flag=masked?128:0;
        if(b.size()<126) frame.push_back(flag|b.size());
        else { frame.push_back(flag|126); frame.push_back(b.size()>>8); frame.push_back(b.size()); }
        if(masked) frame.insert(frame.end(),{11,22,33,44});
        for(size_t i=0;i<b.size();++i) frame.push_back(b[i] ^ (masked ? std::array<uint8_t,4>{11,22,33,44}[i%4] : 0));
        send_all(frame);
    }
    Bytes receive(uint8_t op=2) {
        auto head=read(2); assert(head[0]==uint8_t(128|op) && !(head[1]&128));
        size_t n=head[1]; if(n==126) { auto l=read(2); n=size_t(l[0])<<8|l[1]; }
        else if(n==127) { auto l=read(8); n=0; for(auto v:l) n=(n<<8)|v; }
        assert(n<=65536); return read(n);
    }
    Bytes notification(uint32_t sid, uint8_t type) { auto b=receive(); assert(b.size()>=6 && b[0]==4 && b[1]==type && get32(b,2)==sid); return b; }
    void quiet() { pollfd p{fd,POLLIN,0}; assert(::poll(&p,1,100)==0); }
};
static Node directory(uint32_t id, const char *name, std::vector<uint32_t> children) { Node n; n.id=id; n.name=name; n.type=Type::Directory; n.children=std::move(children); return n; }
static Node string(uint32_t id, const char *name, const char *value) { Node n; n.id=id; n.name=name; n.type=Type::String; n.text=value; return n; }
static Graph fixture() {
    Graph g; g.nodes={directory(0,"",{1,3,6,7}),directory(1,"app",{2}),string(2,"systemname","Host STPP"),directory(3,"items",{4,5}),directory(4,"",{8}),directory(5,"",{}),directory(6,"eventSink",{}),directory(7,"alias",{})};
    g.nodes[3].selected=4; g.nodes[6].binding="nav"; g.nodes[7].link=4;
    Node value; value.id=8; value.name="value"; value.type=Type::Integer; value.integer=1; value.binding="setting:disposable"; g.nodes.push_back(value); return g;
}
static Command command(Server &s) {
    Command c;
    for(int i=0;i<200;++i) { if(s.poll(c)) return c; std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
    assert(false && "callback command timed out"); return c;
}
static void no_commands(Server &s) { std::this_thread::sleep_for(std::chrono::milliseconds(80)); Command c; assert(!s.poll(c)); }
int main() {
    Server server; auto g=fixture(); assert(server.publish(g)); assert(server.start(0)); assert(server.port());
    Client c(server.port(),true), legacy(server.port(),false);
    assert(c.instance==legacy.instance);
    c.send({1,2,3},9); assert(c.receive(10)==Bytes({1,2,3}));
    c.send(subscribe(1,0,{"app","systemname"})); auto name=c.notification(1,3); assert(std::string(name.begin()+7,name.end())=="Host STPP");
    legacy.send(subscribe(1,0,{"alias","value"})); assert(get32(legacy.notification(1,1),6)==1);
    c.send(subscribe(2,0,{"items"})); c.notification(2,5); auto children=c.notification(2,6); assert(children.size()==14);
    const auto first=get32(children,6), second=get32(children,10); assert(first && second && first!=second);
    assert(get32(c.notification(2,10),6)==first);
    c.send(subscribe(3,first,{"value"})); assert(get32(c.notification(3,1),6)==1);
    c.send(subscribe(4,0,{"future"})); c.notification(4,0);
    // Set, float, string and void carry copied bindings and typed payloads.
    c.send(mutation(3,first,{"value"},{1,42,0,0,0})); auto cmd=command(server); assert(cmd.kind==Command::Kind::SetInteger && cmd.integer==42 && cmd.binding=="setting:disposable");
    c.quiet(); // Network writes must never mutate the UI-owned snapshot.
    c.send(mutation(3,first,{"value"},{2,0,0,192,63})); cmd=command(server); assert(cmd.kind==Command::Kind::SetFloat && cmd.real==1.5f);
    c.send(mutation(3,first,{"value"},{3,0,'t','e','x','t'})); cmd=command(server); assert(cmd.kind==Command::Kind::SetString && cmd.text=="text");
    c.send(mutation(3,first,{"value"},{0})); assert(command(server).kind==Command::Kind::SetVoid);
    // Native bool.view calls toggle(value). Preserve separate queued toggles:
    // resolving both against the worker's old snapshot would lose the second.
    c.send(mutation(3,first,{"value"},{13}));
    c.send(mutation(3,first,{"value"},{13}));
    auto toggle1=command(server), toggle2=command(server);
    assert(toggle1.kind==Command::Kind::ToggleInteger && toggle2.kind==Command::Kind::ToggleInteger);
    assert(toggle1.binding=="setting:disposable" && toggle2.binding==toggle1.binding);
    c.send(mutation(5,0,{"eventSink"},{14,1,9,'s','e','t','t','i','n','g','s',':'})); cmd=command(server); assert(cmd.kind==Command::Kind::OpenURL && cmd.text=="settings:" && cmd.binding=="nav");
    c.send(mutation(5,0,{"eventSink"},{1,4,'B','a','c','k',4,'H','o','m','e'})); assert(command(server).text=="Back"); assert(command(server).text=="Home");
    // Malformed, unsupported, read-only and unexported references do nothing.
    c.send(mutation(3,0,{"app","systemname"},{1,2,0,0,0}));
    c.send(mutation(3,999999,{}, {1,2,0,0,0}));
    c.send(mutation(3,first,{"value"},{1,2}));
    c.send(mutation(3,first,{"value"},{2,0,0,192,127})); // NaN
    c.send(mutation(5,0,{"eventSink"},{14,1,255,0,0,1,0}));
    c.send(mutation(5,0,{"eventSink"},{1,4,'B','a'})); c.send({9,0,0,0}); no_commands(server);
    g.nodes.back().integer=42; g.nodes.push_back(string(9,"future","arrived")); g.nodes[0].children.push_back(9); assert(server.publish(g));
    assert(get32(c.notification(3,1),6)==42); auto arrived=c.notification(4,3); assert(std::string(arrived.begin()+7,arrived.end())=="arrived");
    assert(get32(legacy.notification(1,1),6)==42);
    // Exact order changes use move; delete uses the old exported reference.
    g.nodes[3].children={5,4}; g.nodes[3].selected=5; assert(server.publish(g));
    auto move=c.notification(2,9); assert(get32(move,6)==second && get32(move,10)==first); assert(get32(c.notification(2,10),6)==second);
    g.nodes[3].children={5}; g.nodes[7].link=0;
    g.nodes.erase(std::remove_if(g.nodes.begin(),g.nodes.end(),[](const Node &n){return n.id==4 || n.id==8;}),g.nodes.end()); assert(server.publish(g));
    assert(get32(c.notification(2,8),6)==first); c.notification(3,0); legacy.notification(1,0);
    c.send(mutation(3,first,{"value"},{1,3,0,0,0})); no_commands(server);
    g.nodes.push_back(directory(10,"",{})); g.nodes[3].children={10,5}; assert(server.publish(g));
    auto inserted=c.notification(2,7); assert(get32(inserted,6)==second); const auto added=get32(inserted,10); assert(added!=first && added!=second);
    // Reinserting a deleted ID receives a new export; the old ref stays invalid.
    g.nodes.push_back(directory(4,"",{8})); Node restored; restored.id=8; restored.name="value"; restored.type=Type::Integer; restored.binding="setting:restored"; g.nodes.push_back(restored);
    g.nodes[3].children.push_back(4); assert(server.publish(g)); const auto restored_ref=get32(c.notification(2,6),6); assert(restored_ref!=first);
    c.send(mutation(3,first,{"value"},{1,3,0,0,0})); no_commands(server);
    c.send(mutation(3,restored_ref,{"value"},{1,3,0,0,0})); assert(command(server).binding=="setting:restored");
    c.send({2,4,0,0,0}); std::this_thread::sleep_for(std::chrono::milliseconds(60));
    for(auto &n:g.nodes) if(n.id==9) n.text="after unsubscribe";
    assert(server.publish(g)); c.quiet();
    // Exported refs are connection-local; a fresh client cannot use another's.
    { Client fresh(server.port(),true); fresh.send(mutation(5,second,{}, {1,4,'B','a','c','k'})); no_commands(server); }
    // Commands are bounded even if the UI thread stops draining.
    for(int i=0;i<300;++i) c.send(mutation(5,0,{"eventSink"},{1,4,'B','a','c','k'}));
    std::this_thread::sleep_for(std::chrono::milliseconds(180)); int count=0; while(server.poll(cmd)) ++count; assert(count==256);
    // Oversized and fragmented frames are rejected without blocking other clients.
    { Client bad(server.port(),true); bad.send_all({130,127,0,0,0,0,0,1,0,1}); pollfd p{bad.fd,POLLIN,0}; assert(::poll(&p,1,2000)>0); char ch; assert(recv(bad.fd,&ch,1,0)==0); }
    { Client bad(server.port(),true); bad.send_all({2,0}); pollfd p{bad.fd,POLLIN,0}; assert(::poll(&p,1,2000)>0); char ch; assert(recv(bad.fd,&ch,1,0)==0); }
    legacy.send({9,8},9); assert(legacy.receive(10)==Bytes({9,8}));
    auto invalid=fixture(); invalid.nodes.push_back(invalid.nodes[0]); assert(!server.publish(invalid));
    invalid=fixture(); invalid.nodes[7].link=7; assert(!server.publish(invalid));
    server.stop(); assert(!server.port()); assert(server.start(0)); Client restarted(server.port(),true); assert(restarted.instance!=c.instance); server.stop();
    std::puts("STPP server: modern/legacy WebSocket, typed callbacks, snapshots, aliases, deltas, unsubscribe, scoped refs and bounds passed");
}
