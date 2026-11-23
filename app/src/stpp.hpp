// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace movian::stpp {
enum class Type { Void, Integer, Float, String, Directory };
struct Node {
    uint32_t id = 0;
    std::string name;
    Type type = Type::Void;
    int32_t integer = 0;
    float real = 0;
    std::string text;
    uint8_t string_kind = 0;
    std::vector<uint32_t> children;
    uint32_t selected = 0;
    // Zero means no link. Alias nodes resolve to the linked node's value/children.
    uint32_t link = 0;
    // Opaque UI-owned callback key. Empty bindings are read-only.
    std::string binding;
};
struct Graph { std::vector<Node> nodes; };
struct Command {
    enum class Kind { SetVoid, SetInteger, SetFloat, SetString, OpenURL, Action, Dynamic, ToggleInteger };
    Kind kind = Kind::SetVoid;
    std::string binding, text;
    int32_t integer = 0;
    float real = 0;
    uint8_t string_kind = 0;
};
// Publish and poll run on the UI thread. The listener only sees immutable copies;
// queued commands own their strings and never hold pointers into a graph.
class Server {
public:
    Server();
    ~Server();
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;
    bool start(uint16_t port, const std::string &bind_address = "127.0.0.1");
    void stop();
    bool publish(Graph graph);
    bool poll(Command &command);
    uint16_t port() const;
    std::string error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
