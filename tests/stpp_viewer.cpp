// SPDX-License-Identifier: GPL-3.0-or-later
// Persistent host fixture for the installed Movian STPP viewer. This is a
// manual interoperability harness, not part of the automated self-check.
#include "stpp.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <thread>
using namespace movian::stpp;
namespace {
std::atomic<bool> running{true};
void stop(int) { running = false; }
struct Fixture {
    Graph graph;
    std::map<std::string, uint32_t> ids;
    Fixture() { Node root; root.type=Type::Directory; graph.nodes.push_back(root); ids[""]=0; }
    uint32_t path(const std::string &key) {
        auto found=ids.find(key); if(found!=ids.end()) return found->second;
        const auto slash=key.rfind('/'); const auto parent=slash==std::string::npos ? "" : key.substr(0,slash);
        const auto name=slash==std::string::npos ? key : key.substr(slash+1);
        const uint32_t pid=path(parent), id=uint32_t(graph.nodes.size());
        Node n; n.id=id; n.name=name; n.type=Type::Directory;
        graph.nodes.push_back(n); graph.nodes[pid].children.push_back(id); ids[key]=id; return id;
    }
    Node &node(const std::string &key) { return graph.nodes[path(key)]; }
    void text(const std::string &key,const std::string &value) { const auto id=path(key); graph.nodes[id].type=Type::String; graph.nodes[id].text=value; }
    void integer(const std::string &key,int value) { const auto id=path(key); graph.nodes[id].type=Type::Integer; graph.nodes[id].integer=value; }
    void alias(const std::string &key,const std::string &target) { const auto id=path(key), dest=path(target); graph.nodes[id].link=dest; }
    void initialize() {
        text("app/systemname","PS5 STPP host fixture");
        path("navigators/current"); path("navigators/current/pages");
        node("navigators/current/eventSink").binding="navigator";
        // The nested viewer clones pages and dereferences their model.type.
        text("fixture/home/url","page:home"); text("fixture/home/model/type","home");
        text("fixture/home/model/metadata/title","Home");
        text("fixture/settings/url","settings:general");
        text("fixture/settings/model/type","settings");
        text("fixture/settings/model/contents","settings");
        text("fixture/settings/model/metadata/title","General settings");
        integer("fixture/settings/model/loading",0);
        integer("fixture/settings/model/nodes/extensions/enabled",1);
        text("fixture/settings/model/nodes/extensions/type","bool");
        text("fixture/settings/model/nodes/extensions/metadata/title","Show filename extensions");
        integer("fixture/settings/model/nodes/extensions/value",1);
        node("fixture/settings/model/nodes/extensions/value").binding="setting:show_extensions";
        node("fixture/settings/model/nodes").selected=path("fixture/settings/model/nodes/extensions");
        text("services/stable/settings/title","Settings");
        text("services/stable/settings/type","setting");
        text("services/stable/settings/url","settings:general");
        integer("services/stable/settings/enabled",1);
        node("services/stable").selected=path("services/stable/settings");
        path("popups"); path("notifications/nodes"); path("news");
        path("media/current"); integer("playqueue/active",0); integer("audio/mastervolume",0);
        integer("runcontrol/canExit",0); integer("runcontrol/canStandby",0); integer("runcontrol/sleepTimer",0);
        integer("androidTV/isAndroidTV",0); integer("clock/timeFormat",0);
        path("glw/views/standard");
        alias("navigator","navigators/current");
        show(false);
    }
    void show(bool settings) {
        auto home=path("fixture/home"), setting=path("fixture/settings");
        node("navigators/current/pages").children=settings ? std::vector<uint32_t>{home,setting} : std::vector<uint32_t>{home};
        node("navigators/current/pages").selected=settings ? setting : home;
        alias("navigators/current/currentpage",settings ? "fixture/settings" : "fixture/home");
        integer("navigators/current/canGoBack",settings);
    }
};
}
int main(int argc,char **argv) {
    const uint16_t port=argc>1 ? uint16_t(std::strtoul(argv[1],nullptr,10)) : 42010;
    const std::string statefile=argc>2 ? argv[2] : "";
    std::signal(SIGINT,stop); std::signal(SIGTERM,stop);
    Fixture fixture; fixture.initialize(); Server server;
    if(!server.publish(fixture.graph) || !server.start(port)) { std::cerr << server.error() << '\n'; return 1; }
    std::cout << "{\"listening\":" << server.port() << "}" << std::endl;
    bool settings=false; unsigned callbacks=0;
    auto save=[&] {
        if(statefile.empty()) return;
        std::ofstream out(statefile);
        out << "{\"settings\":" << (settings?"true":"false") << ",\"show_extensions\":"
            << fixture.node("fixture/settings/model/nodes/extensions/value").integer
            << ",\"callbacks\":" << callbacks << "}\n";
    };
    save();
    while(running) {
        Command command;
        while(server.poll(command)) {
            ++callbacks;
            std::cout << "{\"callback\":" << callbacks << ",\"binding\":" << std::quoted(command.binding)
                      << ",\"kind\":" << int(command.kind) << ",\"text\":" << std::quoted(command.text)
                      << ",\"integer\":" << command.integer << "}" << std::endl;
            if(command.binding=="navigator") {
                if(command.kind==Command::Kind::OpenURL && command.text.rfind("settings:",0)==0) settings=true;
                if(command.kind==Command::Kind::Action && (command.text=="Back" || command.text=="Home")) settings=false;
                fixture.show(settings);
            }
            if(command.binding=="setting:show_extensions" && command.kind==Command::Kind::SetInteger)
                fixture.integer("fixture/settings/model/nodes/extensions/value",command.integer!=0);
            if(command.binding=="setting:show_extensions" && command.kind==Command::Kind::ToggleInteger)
                fixture.integer("fixture/settings/model/nodes/extensions/value",
                    !fixture.node("fixture/settings/model/nodes/extensions/value").integer);
            if(!server.publish(fixture.graph)) { std::cerr << "publish failed\n"; return 2; }
            save();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
