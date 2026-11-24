// SPDX-License-Identifier: GPL-3.0-or-later
// Compile with c++ -std=c++20 -Iapp/src scripts/planned-settings.cpp.
#include "settings_menu.hpp"
#include <cstdio>
#include <string>
using namespace movian::ui::settings_menu;
static unsigned next_id;
static void rows(std::span<const Row> entries, const std::string &prefix) {
    constexpr const char *names[]={"toggle","number","text","select","action","section","page"};
    for(const auto &row:entries) {
        if(row.kind==Kind::section) continue;
        const auto path=prefix+" → "+row.title;
        if(row.kind==Kind::page) rows({row.children,row.child_count},path);
        else if(row.action<0) std::printf("| S%03u | %s | %s: observe, exercise consumer, reset/reopen |\n",
                                        ++next_id,path.c_str(),names[static_cast<unsigned>(row.kind)]);
    }
}
int main() {
    std::puts("| ID | Pending setting | One acceptance test |\n| --- | --- | --- |");
    for(const auto &category:categories) rows(category.rows,category.title);
}
