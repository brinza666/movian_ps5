// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <span>

namespace movian::ui::settings_menu
{
struct Row
{
    const char *title;
    int action = -1;
};

struct Category
{
    const char *title;
    std::span<const Row> rows;
};

// Order follows the Settings navigation observed in the installed WSL build.
inline constexpr Row general[] = {
    {"System name"}, {"Upgrade channel"}, {"Automatic upgrades"}, {"Launch plugin on startup"},
    {"Plugin repository", 12}, {"Automatically install plugins"}, {"Plugin settings", 11},
    {"Clear cache"}, {"Enable FTP server"}, {"FTP server port"}, {"FTP username"}, {"FTP password"},
    {"Debug log", 7},
};
inline constexpr Row video[] = {
    {"Hardware decode", 8}, {"VDPAU"}, {"Video buffer size"}, {"Pre-buffer video"},
    {"HLS live mode"}, {"HLS resolution limit"}, {"Deinterlace"}, {"Upscale"}, {"Adaptive format"},
    {"Resume playback", 0}, {"Play next automatically", 1}, {"Seek step", 4}, {"Match refresh rate"},
    {"Playback clock"}, {"Up and Down during playback"}, {"Playback speed"}, {"Repeat", 2}, {"Shuffle", 3},
};
inline constexpr Row audio[] = {
    {"Audio output"}, {"Volume", 5}, {"AC-3 passthrough"}, {"E-AC-3 passthrough"}, {"DTS passthrough"},
    {"TrueHD passthrough"}, {"Transcode audio"}, {"Stereo upmix"}, {"Downmix profile"},
    {"Audio delay"}, {"Gain"}, {"IEC61937 / S/PDIF"}, {"Speaker test"},
};
inline constexpr Row subtitles[] = {
    {"Preferred language"}, {"Secondary language"}, {"Source priority"}, {"Central subtitle folder"},
    {"Subtitle size"}, {"Text color"}, {"Outline"}, {"Shadow"}, {"Screen position"},
    {"Strip SDH cues"}, {"Bounding box opacity"}, {"Force subtitles on"},
};
inline constexpr Row language[] = {
    {"Interface language"}, {"Keyboard"}, {"24-hour clock", 6}, {"Clock offset"},
    {"Character encoding"}, {"Ignore “The” when sorting"},
};
inline constexpr Row appearance[] = {
    {"Skin"}, {"Interface size"}, {"Background image"}, {"Colors"}, {"Show Search"},
    {"Show USB"}, {"Show Settings"}, {"Show Plugins"}, {"Show Local network"}, {"Show M7 Data"},
    {"Screensaver delay"}, {"Screensaver source"}, {"Loading indicator"}, {"Animation speed"},
    {"Back button behavior"}, {"Show all files", 9}, {"Newest files first", 10},
};
inline constexpr Row bookmarks[] = {{"Manage bookmarks", 19}};
inline constexpr Row metadata[] = {{"TMDB language"}, {"TVDB language"}, {"Use original titles"}};
inline constexpr Row network[] = {
    {"HTTP proxy"}, {"SOCKS proxy"}, {"SMB large buffer"}, {"SMB extended attributes"},
    {"DLNA discovery"}, {"IPv6"},
};
inline constexpr Row bittorrent[] = {
    {"BitTorrent tracker discovery", 13}, {"Play while downloading", 14}, {"Cache location", 15},
    {"Cache and peer settings", 17}, {"Global speed limits"}, {"Maximum active torrents"},
    {"Free-space threshold"}, {"Maximum cache per torrent"}, {"Active memory limit"},
    {"Peer listen port"}, {"DHT options"}, {"Peer exchange"}, {"Local peer discovery"},
};
inline constexpr Row search[] = {{"Search sources"}, {"Search history"}};
inline constexpr Row developer[] = {
    {"Debug log", 7}, {"Remote diagnostics", 16}, {"Settings logging"}, {"Input events"},
    {"HTTP logging"}, {"Plugin logging"}, {"FTP logging"}, {"Torrent tracker logging"},
    {"Torrent peer connection logging"}, {"Torrent download logging"}, {"Torrent upload logging"},
    {"Torrent disk I/O logging"},
};
inline constexpr Row about[] = {{"About Movian for PS5", 18}};
inline constexpr Row upgrade[] = {{"Check for updates"}, {"Upgrade channel"}};

inline constexpr std::array categories = {
    Category{"General", general}, Category{"Video", video}, Category{"Audio", audio},
    Category{"Subtitles", subtitles}, Category{"Languages", language},
    Category{"Look and feel", appearance}, Category{"Bookmarks", bookmarks},
    Category{"Metadata", metadata}, Category{"Network", network}, Category{"BitTorrent", bittorrent},
    Category{"Search", search}, Category{"Developer / Advanced", developer}, Category{"About", about},
    Category{"Upgrade", upgrade},
};
} // namespace movian::ui::settings_menu
