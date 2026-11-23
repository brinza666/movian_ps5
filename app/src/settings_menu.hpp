// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <span>

namespace movian::ui::settings_menu
{
enum class Kind { toggle, number, text, select, action, section, page };
enum Action {
    resume, auto_next, repeat, shuffle, seek, volume, time_format, debug, hardware,
    show_all, newest, plugins, repository, trackers, progressive, cache, diagnostics,
    torrent_settings, about_info, manage_bookmarks, extensions, playback_clock, custom_repository,
    peer_connections, peer_requests, retrackers,
    system_name, smb_large, smb_attributes, ftp_enabled, ftp_port, ftp_user, ftp_password, network_servers,
    tracker_protocols, remote_control,
};
struct Row
{
    const char *title;
    int action = -1;
    Kind kind = Kind::action;
    const Row *children = nullptr;
    std::size_t child_count = 0;
};

struct Category
{
    const char *title;
    std::span<const Row> rows;
};

constexpr Row heading(const char *title) { return {title, -1, Kind::section}; }
constexpr Row toggle(const char *title, int action = -1) { return {title, action, Kind::toggle}; }
constexpr Row number(const char *title, int action = -1) { return {title, action, Kind::number}; }
constexpr Row text(const char *title, int action = -1) { return {title, action, Kind::text}; }
constexpr Row select(const char *title, int action = -1) { return {title, action, Kind::select}; }
constexpr Row page(const char *title, std::span<const Row> rows)
{
    return {title, -1, Kind::page, rows.data(), rows.size()};
}

// Settings rows transcribed from the running WSL build. PS5-specific controls
// have their own group where the reference places them elsewhere or omits them.
inline constexpr Row general[] = {
    heading("File browsing"), toggle("Enable file deletion from item menu"),
    toggle("Store per-file settings in filesystem"), toggle("Show filename extensions", extensions),
    toggle("Browse archives as folders"),
    heading("Starting and stopping"), {"Quit"},
    heading("Plugins"), text("Launch plugin on M7 start"), text("Alternate plugin Repository URL", custom_repository),
    heading("Reset"), {"Clear cached files"}, {"Clear all metadata"}, {"Forget remembered passwords"},
    heading("PS5"), select("Plugin repository", repository), {"Plugin settings", plugins},
};
inline constexpr Row video[] = {
    heading("Accelerated Decoding"), select("OpenGL Driver"), select("OpenGL Adapter"), select("UI Refresh"),
    toggle("Hardware decode", hardware),
    heading("Info Banner"), number("Duration"), toggle("Title"), toggle("Description"), toggle("Audio & Subtitles"),
    heading("Visuals & Actions"), toggle("Show clock during playback", playback_clock),
    toggle("Automatically play next video in list", auto_next), select("Resume video playback", resume),
    number("Count video as played when reaching"), number("Step when seeking backward"),
    number("Step when seeking forward"), select("Up / Down during video playback controls"),
    heading("Zoom & Scale"), number("Video zoom"), number("Vertical scale"), number("Vertical pan"),
    number("Horizontal scale"), number("Horizontal pan"), toggle("Stretch video to widescreen"),
    toggle("Stretch video to fullscreen"), toggle("Video frame interpolation"),
    heading("Buffer & Adaptive Playback"), number("Buffer Size"), number("Pre-buffer"), select("Adaptive Format"),
    heading("PS5"), select("Seek step", seek), select("Repeat", repeat), toggle("Shuffle", shuffle),
};
inline constexpr Row audio[] = {
    heading("Video playback"), number("Audio gain"), number("Audio delay"),
    heading("Audio test"), toggle("Play test signal"), select("Test signal type"),
    toggle("Front Left"), toggle("Center"), toggle("Front Right"), toggle("LFE"),
    toggle("Surround Left"), toggle("Surround Right"), toggle("Rear Left"), toggle("Rear Right"),
    heading("PS5"), select("Volume", volume),
};
inline constexpr Row subtitle_source[] = {
    toggle("Enabled"), toggle("Automatically select from this source"), number("Source priority"),
};
inline constexpr Row subtitles[] = {
    heading("Subtitle size and positioning"), number("Subtitle size"),
    toggle("Force subtitles to reside on video frame"), select("Subtitle position"),
    number("Subtitle vertical displacement"), number("Subtitle horizontal displacement"),
    text("Color"), text("Shadow color"), number("Shadow offset"), text("Outline color"), number("Outline size"),
    number("Bounding box opacity"), toggle("Ignore embedded styling"), toggle("Remove SDH commentary"),
    heading("Central subtitle folder"), text("Path to central folder"),
    heading("Providers for Subtitles"),
    page("Subtitles embedded in video file", subtitle_source),
    page("Subtitles with matching filename in same folder as video", subtitle_source),
    page("Subtitles with matching filename in central folder", subtitle_source),
    page("Any subtitle in same folder as video", subtitle_source),
    page("Any subtitle in central folder", subtitle_source),
};
inline constexpr Row language[] = {
    select("Language"), select("Time format", time_format),
    text("Primary audio language code"), text("Secondary audio language code"), text("Tertiary audio language code"),
    text("Primary subtitle language code"), text("Secondary subtitle language code"),
    text("Tertiary subtitle language code"), select("Default character set"),
    toggle("Ignore 'The' at beginning of words when sorting"),
};
inline constexpr Row appearance[] = {
    {"Fonts"}, heading("Look"), number("Font and icon size"),
    number("Interface horizontal shrink"), number("Interface vertical shrink"),
    toggle("Wrap when reaching beginning/end of lists"), toggle("Emulate Up/Down buttons with mouse wheel"),
    select("Loading Indicator"), heading("Background"), text("Custom background image"),
    heading("Screensaver"), number("Screensaver delay"), toggle("Use Bing images of the day"),
    text("Folder for screensaver images"), number("Seconds per image"),
    heading("Preferred views from plugins"), select("Background"), select("Loading screen"),
    select("Screen saver"), select("Home page"), select("On Screen Keyboards"),
    heading("Browsing"), select("Audio tracks"), select("Album"), select("List of albums"), select("Artist"),
    select("TV channels"), select("Images"), select("Movies"),
    heading("PS5"), toggle("Show all files", show_all), toggle("Newest files first", newest),
};
inline constexpr Row bookmarks[] = {{"Add new entry"}, {"Manage bookmarks", manage_bookmarks}};
inline constexpr Row tvdb[] = {toggle("Enabled"), text("Language (ISO 639-1 code)")};
inline constexpr Row tmdb[] = {
    toggle("Enabled"), text("Language (ISO 639-1 code)"), toggle("Use original title"),
};
inline constexpr Row lastfm[] = {toggle("Enabled")};
inline constexpr Row metadata[] = {
    heading("Providers for Video"), page("thetvdb.com", tvdb), page("themoviedb.org", tmdb),
    heading("Providers for Music"), page("last.fm", lastfm),
};
inline constexpr Row network[] = {
    text("System name", system_name), toggle("SMB Large Buffer", smb_large), select("SMB Extended Attributes", smb_attributes),
    heading("FTP server"), toggle("Enable FTP server", ftp_enabled), text("Server TCP port", ftp_port), text("Username", ftp_user), text("Password", ftp_password),
    heading("Remote control"), toggle("Allow remote control", remote_control),
    heading("PS5"), {"Saved SMB / FTP servers", network_servers},
};
inline constexpr const char *retracker_labels[]{
    "Original trackers", "Add retrackers", "Replace with retrackers", "Remove trackers",
};
inline constexpr Row bittorrent[] = {
    number("Max upload speed"), number("Max usage of free space for caching torrents"),
    number("Max number of connections", peer_connections), number("Max concurrent requests per peer", peer_requests),
    number("Max memory for active transfers"), number("Max cache size per torrent"),
    select("Inject additional trackers", retrackers), select("Tracker protocols", tracker_protocols),
    heading("Status"), select("Torrent cache path", cache), {"Clear cache"},
    heading("PS5"), toggle("BitTorrent tracker discovery", trackers),
    toggle("Play while downloading", progressive), {"Cache and peer settings", torrent_settings},
};
inline constexpr Row search[] = {{"Search sources"}, {"Search history"}};
inline constexpr Row developer[] = {
    toggle("Disable HTTP connection reuse"), select("SW Create video thumbnails"), select("HLS SD width limit"),
    toggle("Enable indexer option"), heading("Debug log filtering"),
    toggle("Debug all HTTP requests"), toggle("Debug various ecmascript engine things"), toggle("Log AV-diff stats"),
    toggle("Debug HLS"), toggle("Debug FTP Client"), toggle("Debug FTP Server"), toggle("Debug directory listings"),
    toggle("Debug library indexer"), toggle("Debug SMB/CIFS (Windows File Sharing)"),
    toggle("Debug read/writes to URL key/value store"), toggle("Debug icecast streaming"),
    toggle("Debug image loading and decoding"), toggle("Debug metadata lookups"),
    toggle("Debug settings store/load from disk"), toggle("Debug threads"), toggle("Debug UPNP"),
    toggle("Debug Bittorrent general events"), toggle("Debug Bittorrent tracker communication"),
    toggle("Debug Bittorrent peer connections"), toggle("Debug Bittorrent peer downloading"),
    toggle("Debug Bittorrent peer uploading"), toggle("Debug Bittorrent disk I/O"), toggle("Debug input events"),
    heading("PS5"), toggle("Debug log", debug), toggle("Remote diagnostics", diagnostics),
};
inline constexpr Row about[] = {{"About Movian for PS5", about_info}};
inline constexpr Row upgrade[] = {{"Check for updates"}, select("Upgrade channel"), toggle("Automatic upgrades")};

inline constexpr std::array categories = {
    Category{"General", general}, Category{"Video", video}, Category{"Audio", audio},
    Category{"Subtitles", subtitles}, Category{"Languages", language},
    Category{"Look and feel", appearance}, Category{"Bookmarks", bookmarks}, Category{"Metadata", metadata},
    Category{"Network", network}, Category{"BitTorrent", bittorrent}, Category{"About", about},
    Category{"Developer", developer}, Category{"Search", search}, Category{"Upgrade", upgrade},
};
} // namespace movian::ui::settings_menu
