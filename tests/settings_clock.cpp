// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.hpp"
#include <cassert>
#include <cstring>
#include <ctime>
#include <cstdio>
static std::tm sample{};
static bool valid=true;
extern "C" std::tm *localtime(const time_t *) noexcept { return valid ? &sample : nullptr; }
int main() {
    using namespace movian::settings;
    char text[24];
    get().clock_24h=true;sample.tm_hour=23;sample.tm_min=7;
    format_clock(text,sizeof text);assert(!strcmp(text,"23:07"));
    get().clock_24h=false;sample.tm_hour=0;sample.tm_min=1;
    format_clock(text,sizeof text);assert(!strcmp(text,"12:01 AM"));
    sample.tm_hour=12;sample.tm_min=3;
    format_clock(text,sizeof text);assert(!strcmp(text,"12:03 PM"));
    sample.tm_hour=23;sample.tm_min=59;
    format_clock(text,sizeof text);assert(!strcmp(text,"11:59 PM"));
    valid=false;format_clock(text,sizeof text);assert(!strcmp(text,"--:--"));
    char tiny[2]={'x','x'};format_clock(tiny,sizeof tiny);assert(tiny[0]=='-' && tiny[1]==0);
    tiny[0]='x';format_clock(tiny,0);assert(tiny[0]=='x');
    format_clock(tiny,1);assert(tiny[0]==0);
    format_clock(nullptr,0);
    valid=true;sample.tm_hour=-1;format_clock(text,sizeof text);assert(!strcmp(text,"--:--"));
    puts("Clock 12/24-hour, conversion failure and bounded output passed");
}
