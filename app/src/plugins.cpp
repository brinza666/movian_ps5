// SPDX-License-Identifier: GPL-3.0-or-later
#include "plugins.hpp"
#include "diagnostics.hpp"
#include "paths.hpp"
#include "fs.hpp"
#include "plugin_http.hpp"
#include "torrent.hpp"
#include "input_file.hpp"
#include "settings.hpp"
#include "plugin_storage.hpp"
#include "stb_image.h"
#include <strings.h>

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <time.h>

#ifdef MOVIAN_HAVE_DUKTAPE
#include "duktape.h"
#include "miniz.h"
#endif

static std::uint64_t monotonic_ms() noexcept
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::uint64_t>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}

struct PluginBudget
{
    std::uint64_t deadline;
    std::size_t used = 0;
    unsigned requests = 0;
    char plugin_id[64]{};
    void *archive = nullptr;
};

extern "C" int movian_plugin_timed_out(void *data)
{
    return data && monotonic_ms() >= static_cast<PluginBudget *>(data)->deadline;
}

namespace movian::plugins
{
#ifdef MOVIAN_HAVE_GUMBO
void install_html(duk_context *ctx);
#endif
namespace
{
constexpr char kinozal_id[] = "KinoZal.TV";
char g_kinozal_cookie[512]{};
char g_kinozal_origin[264]{};

void erase(char *data, std::size_t length)
{
    volatile char *out = data;
    while (length--) *out++ = 0;
}

bool find_cookie(const char *headers, const char *name, char *value, std::size_t capacity)
{
    const auto name_length = std::strlen(name);
    for (const char *line = headers; *line; )
    {
        const char *end = std::strchr(line, '\n');
        const auto size = end ? static_cast<std::size_t>(end - line) : std::strlen(line);
        if (size > 11 && !strncasecmp(line, "Set-Cookie:", 11))
        {
            const char *cookie = line + 11;
            while (*cookie == ' ' || *cookie == '\t') ++cookie;
            if (!std::strncmp(cookie, name, name_length) && cookie[name_length] == '=')
            {
                const auto length = std::strcspn(cookie, ";\r\n");
                if (length <= name_length + 1 || length >= capacity)
                    return false;
                std::memcpy(value, cookie, length);
                value[length] = '\0';
                return true;
            }
        }
        if (!end) break;
        line = end + 1;
    }
    return false;
}

bool form_value(char *&out, const char *end, const char *text)
{
    constexpr char hex[] = "0123456789ABCDEF";
    for (const unsigned char *ch = reinterpret_cast<const unsigned char *>(text); *ch; ++ch)
    {
        if ((*ch >= 'a' && *ch <= 'z') || (*ch >= 'A' && *ch <= 'Z') ||
            (*ch >= '0' && *ch <= '9') || std::strchr("-._~", *ch))
        {
            if (out == end) return false;
            *out++ = static_cast<char>(*ch);
        }
        else
        {
            if (end - out < 3) return false;
            *out++ = '%';
            *out++ = hex[*ch >> 4];
            *out++ = hex[*ch & 15];
        }
    }
    return true;
}

void copy_text(char *dest, int capacity, const char *value) noexcept
{
    if (dest && capacity > 0)
        std::snprintf(dest, static_cast<std::size_t>(capacity), "%s", value ? value : "");
}

#ifdef MOVIAN_HAVE_DUKTAPE
constexpr char root[] = MOVIAN_APP0 "/assets/plugins/local-samples/";
constexpr char bootstrap[] = MOVIAN_APP0 "/assets/plugins/host.js";
constexpr char manifest[] = MOVIAN_APP0 "/assets/plugins/local-samples/plugin.json";
#define MOVIAN_PLUGIN_DIR MOVIAN_DOWNLOAD0 "/plugins-v23"
constexpr char installed_dir[] = MOVIAN_PLUGIN_DIR;
constexpr char bundled_id[] = "movian-ps5-samples";
constexpr unsigned max_archive = 8 * 1024 * 1024;
constexpr unsigned max_script = 128 * 1024;
constexpr std::size_t max_heap = 8 * 1024 * 1024;

struct alignas(std::max_align_t) Allocation
{
    std::size_t size;
};

void free_heap(void *data, void *pointer)
{
    if (!pointer)
        return;
    auto *allocation = static_cast<Allocation *>(pointer) - 1;
    static_cast<PluginBudget *>(data)->used -= allocation->size;
    std::free(allocation);
}

void *resize_heap(void *data, void *pointer, duk_size_t size)
{
    if (size == 0)
    {
        free_heap(data, pointer);
        return nullptr;
    }
    auto &budget = *static_cast<PluginBudget *>(data);
    auto *allocation = pointer ? static_cast<Allocation *>(pointer) - 1 : nullptr;
    const std::size_t previous = allocation ? allocation->size : 0;
    if (size > max_heap - (budget.used - previous))
        return nullptr;
    auto *result = static_cast<Allocation *>(std::realloc(allocation, sizeof(Allocation) + size));
    if (!result)
        return nullptr;
    budget.used = budget.used - previous + size;
    result->size = size;
    return result + 1;
}

void *allocate_heap(void *data, duk_size_t size)
{
    return resize_heap(data, nullptr, size);
}

struct Engine
{
    PluginBudget budget{monotonic_ms() + 300};
    duk_context *ctx = duk_create_heap(allocate_heap, resize_heap, free_heap, &budget, nullptr);
    ~Engine()
    {
        if (ctx)
            duk_destroy_heap(ctx);
    }
};

struct Archive
{
    unsigned char *data = nullptr;
    std::size_t size = 0;
    mz_zip_archive zip{};
    bool ready = false;

    bool load(const char *path)
    {
        data = reinterpret_cast<unsigned char *>(input::read_bounded(path, max_archive, size));
        if (!data)
            return false;
        ready = mz_zip_reader_init_mem(&zip, data, size, 0) != 0;
        return ready && mz_zip_reader_get_num_files(&zip) <= 1024;
    }

    ~Archive()
    {
        if (ready)
            mz_zip_reader_end(&zip);
        std::free(data);
    }

    char *extract(const char *path, std::size_t limit, std::size_t &size)
    {
        const int index = mz_zip_reader_locate_file(&zip, path, nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE);
        mz_zip_archive_file_stat info{};
        if (index < 0 || !mz_zip_reader_file_stat(&zip, index, &info) ||
            info.m_uncomp_size == 0 || info.m_uncomp_size > limit)
            return nullptr;
        return static_cast<char *>(mz_zip_reader_extract_to_heap(&zip, index, &size, 0));
    }
};

bool valid_name(const char *value)
{
    if (!value || !*value || !std::strcmp(value, ".") || !std::strcmp(value, ".."))
        return false;
    for (; *value; ++value)
        if (!((*value >= 'a' && *value <= 'z') || (*value >= 'A' && *value <= 'Z') ||
              (*value >= '0' && *value <= '9') || *value == '_' || *value == '-' || *value == '.'))
            return false;
    return true;
}

bool public_get(const char *url, char *body, std::size_t capacity, HttpResponse &res,
                bool repo_feed = false)
{
    char current[1024];
    if (!url || std::strlen(url) >= sizeof current || std::strncmp(url, "https://", 8))
        return false;
    copy_text(current, sizeof current, url);
    bool retry_available = repo_feed;
    for (int hop = 0; hop < 4; ++hop)
    {
        int result;
        do
        {
            res = {};
            result = http_request(current, false, nullptr, 0, nullptr, 0, body, capacity, res);
            if (repo_feed)
                if (auto *log = std::fopen(MOVIAN_DOWNLOAD0 "/plugin-http.log", "a"))
                {
                    const char *host = current + 8;
                    const int host_length = static_cast<int>(std::strcspn(host, "/?#"));
                    const bool log_host = !std::memchr(host, '@', host_length) &&
                                          !std::strpbrk(current, "\r\n");
                    std::fprintf(log, "v52 repository hop=%d host=%.*s status=%d rc=%#x bytes=%zu\n",
                                 hop + 1, log_host ? host_length : 0, host, res.status,
                                 static_cast<unsigned>(result), res.length);
                    std::fclose(log);
                }
            if (result >= 0 || res.status != 0 || !retry_available)
                break;
            retry_available = false;
        } while (true);
        if (result < 0)
            return false;
        if (res.status >= 200 && res.status < 300)
            return true;
        if (res.status < 300 || res.status >= 400)
            return false;
        char next[1024]{};
        for (const char *line = res.headers; *line; )
        {
            const char *end = std::strchr(line, '\n');
            const auto length = end ? static_cast<std::size_t>(end - line) : std::strlen(line);
            if (length > 9 && !strncasecmp(line, "Location:", 9))
            {
                const char *value = line + 9;
                while (*value == ' ' || *value == '\t') ++value;
                const char *last = line + length;
                while (last > value && (last[-1] == '\r' || last[-1] == ' ')) --last;
                if (last <= value || last - value >= static_cast<int>(sizeof next))
                    return false;
                if (*value == '/' && value[1] != '/')
                {
                    const auto host = std::strcspn(current + 8, "/?#") + 8;
                    if (host + static_cast<std::size_t>(last - value) >= sizeof next)
                        return false;
                    std::memcpy(next, current, host);
                    std::memcpy(next + host, value, static_cast<std::size_t>(last - value));
                }
                else
                    std::memcpy(next, value, static_cast<std::size_t>(last - value));
                break;
            }
            if (!end) break;
            line = end + 1;
        }
        if (std::strncmp(next, "https://", 8))
            return false;
        copy_text(current, sizeof current, next);
    }
    return false;
}

bool option_path(char *path, std::size_t capacity, const char *id, const char *key)
{
    if (!valid_name(id) || !valid_name(key) || std::strlen(id) > 63 || std::strlen(key) > 63)
        return false;
    const int written = std::snprintf(path, capacity, "%s/%s.settings/%s", installed_dir, id, key);
    return written > 0 && static_cast<std::size_t>(written) < capacity;
}

bool read_option(const char *id, const char *key, char (&value)[256])
{
    char path[256];
    if (!option_path(path, sizeof path, id, key)) return false;
    auto *file = std::fopen(path, "rb");
    if (!file) return false;
    const auto length = std::fread(value, 1, sizeof value, file);
    const bool ok = length < sizeof value && !std::ferror(file) &&
                    !std::memchr(value, 0, length);
    std::fclose(file);
    value[ok ? length : 0] = 0;
    return ok;
}

bool store_path(char *path, std::size_t capacity, const char *id, const char *name)
{
    if (!valid_name(id) || !valid_name(name) || std::strlen(id) > 63 || std::strlen(name) > 63)
        return false;
    const int written = std::snprintf(path, capacity, "%s/%s.stores/%s.json", installed_dir, id, name);
    return written > 0 && static_cast<std::size_t>(written) < capacity;
}

bool write_atomic(const char *path, const char *data, std::size_t size)
{
    char folder[256];
    copy_text(folder, sizeof folder, path);
    auto *slash = std::strrchr(folder, '/');
    if (!slash)
        return false;
    *slash = '\0';
    mkdir(installed_dir, 0777);
    mkdir(folder, 0777);
    char tmp[264];
    std::snprintf(tmp, sizeof tmp, "%s.tmp", path);
    auto *file = std::fopen(tmp, "wb");
    if (!file)
        return false;
    const bool written = std::fwrite(data, 1, size, file) == size;
    const bool closed = std::fclose(file) == 0;
    if (written && closed && std::rename(tmp, path) == 0)
        return true;
    std::remove(tmp);
    return false;
}

bool torrent_cache_path(const char *plugin_id, char *path, std::size_t capacity)
{
    if (!valid_name(plugin_id) || std::strlen(plugin_id) > 63)
        return false;
    const auto length = std::snprintf(path, capacity, "%s/%s.torrent", installed_dir, plugin_id);
    return length > 0 && static_cast<std::size_t>(length) < capacity;
}

duk_ret_t import_torrent(duk_context *ctx)
{
    duk_size_t encoded_size = 0;
    duk_require_lstring(ctx, 0, &encoded_size);
    if (!encoded_size || encoded_size > 4 * ((1024 * 1024 + 2) / 3))
        return duk_error(ctx, DUK_ERR_RANGE_ERROR, "Plugin torrent descriptor exceeds size limit");
    duk_base64_decode(ctx, 0);
    duk_size_t size = 0;
    const auto *bytes = static_cast<const char *>(duk_get_buffer_data(ctx, 0, &size));
    torrent::Metadata metadata;
    duk_memory_functions memory{};
    duk_get_memory_functions(ctx, &memory);
    const auto &budget = *static_cast<PluginBudget *>(memory.udata);
    char path[256];
    if (!torrent_cache_path(budget.plugin_id, path, sizeof path) ||
        !torrent::inspect_bytes(bytes, size, metadata) || !write_atomic(path, bytes, size))
        return duk_error(ctx, DUK_ERR_ERROR, "Could not import plugin torrent descriptor");
    duk_push_string(ctx, metadata.title);
    return 1;
}

duk_ret_t load_zip_module(duk_context *ctx)
{
    duk_size_t length = 0;
    const char *name = duk_require_lstring(ctx, 0, &length);
    if (length < 4 || length > 200 || std::strlen(name) != length ||
        std::strcmp(name + length - 3, ".js"))
        return duk_error(ctx, DUK_ERR_ERROR, "Invalid ZIP module path");
    for (const char *part = name; *part;)
    {
        const std::size_t size = std::strcspn(part, "/");
        if (!size || (size == 1 && part[0] == '.') ||
            (size == 2 && part[0] == '.' && part[1] == '.') ||
            std::strcspn(part, "\\:") < size)
            return duk_error(ctx, DUK_ERR_ERROR, "Invalid ZIP module path");
        part += size;
        if (*part) ++part;
    }
    duk_memory_functions memory{};
    duk_get_memory_functions(ctx, &memory);
    auto *archive = static_cast<Archive *>(static_cast<PluginBudget *>(memory.udata)->archive);
    if (!archive)
        return 0;
    const int index = mz_zip_reader_locate_file(&archive->zip, name, nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE);
    mz_zip_archive_file_stat stat{};
    if (index < 0 || !mz_zip_reader_file_stat(&archive->zip, index, &stat) ||
        !stat.m_uncomp_size || stat.m_uncomp_size > max_script)
        return 0;
    void *buffer = duk_push_fixed_buffer(ctx, static_cast<duk_size_t>(stat.m_uncomp_size));
    if (!mz_zip_reader_extract_to_mem(&archive->zip, index, buffer,
                                     static_cast<std::size_t>(stat.m_uncomp_size), 0))
        return 0;
    duk_buffer_to_string(ctx, -1);
    return 1;
}
duk_ret_t read_store(duk_context *ctx)
{
    duk_memory_functions memory{};
    duk_get_memory_functions(ctx, &memory);
    const auto &budget = *static_cast<PluginBudget *>(memory.udata);
    char path[256];
    if (!store_path(path, sizeof path, budget.plugin_id, duk_require_string(ctx, 0)))
        return 0;
    constexpr std::size_t limit = 32 * 1024 + 1;
    auto *buf = static_cast<char *>(duk_push_fixed_buffer(ctx, limit));
    auto *file = std::fopen(path, "rb");
    if (!file)
        return 0;
    const auto size = std::fread(buf, 1, limit, file);
    const bool ok = size < limit && !std::ferror(file);
    std::fclose(file);
    if (!ok)
        return 0;
    duk_push_lstring(ctx, buf, size);
    return 1;
}

duk_ret_t save_store(duk_context *ctx)
{
    duk_memory_functions memory{};
    duk_get_memory_functions(ctx, &memory);
    const auto &budget = *static_cast<PluginBudget *>(memory.udata);
    char path[256];
    const auto *name = duk_require_string(ctx, 0);
    duk_size_t size = 0;
    const auto *json = duk_require_lstring(ctx, 1, &size);
    duk_push_boolean(ctx, size <= 32 * 1024 && store_path(path, sizeof path, budget.plugin_id, name) &&
                          write_atomic(path, json, size));
    return 1;
}

duk_ret_t load_option(duk_context *ctx)
{
    duk_memory_functions mem{};
    duk_get_memory_functions(ctx, &mem);
    const auto &budget = *static_cast<PluginBudget *>(mem.udata);
    const auto *key = duk_require_string(ctx, 0);
    char value[256];
    if (!read_option(budget.plugin_id, key, value)) return 0;
    duk_push_string(ctx, value);
    return 1;
}

duk_ret_t request_http(duk_context *ctx)
{
    const char *url = duk_require_string(ctx, 0);
    const bool post = duk_get_boolean(ctx, 1);
    duk_size_t length = 0;
    const char *payload = duk_get_lstring(ctx, 3, &length);
    if (std::strlen(url) >= 4096 || length > 64 * 1024)
        return duk_error(ctx, DUK_ERR_RANGE_ERROR, "HTTP input exceeds size limit");
    Header headers[25];
    int count = 0;
    if (duk_is_array(ctx, 2))
    {
        const auto len = duk_get_length(ctx, 2);
        duk_require_stack(ctx, 56);
        for (duk_uarridx_t i = 0; i + 1 < len && i < 48; i += 2)
        {
            duk_get_prop_index(ctx, 2, i);
            duk_get_prop_index(ctx, 2, i + 1);
            const char *name = duk_get_string(ctx, -2);
            const char *value = duk_get_string(ctx, -1);
            if (name && value && std::strlen(name) < 128 && std::strlen(value) < 4096)
                headers[count++] = {name, value};
            // Keep the strings rooted across the call.
        }
    }
    duk_memory_functions mem{};
    duk_get_memory_functions(ctx, &mem);
    auto &budget = *static_cast<PluginBudget *>(mem.udata);
    const auto origin_length = std::strlen(g_kinozal_origin);
    if (g_kinozal_cookie[0] && !std::strcmp(budget.plugin_id, kinozal_id) &&
        !strncasecmp(url, g_kinozal_origin, origin_length) &&
        (url[origin_length] == 0 || url[origin_length] == '/' || url[origin_length] == '?'))
    {
        int kept = 0;
        for (int i = 0; i < count; ++i)
            if (strcasecmp(headers[i].name, "Cookie"))
                headers[kept++] = headers[i];
        headers[kept++] = {"Cookie", g_kinozal_cookie};
        count = kept;
    }
    if (budget.requests++ >= 4 || monotonic_ms() >= budget.deadline)
        return duk_error(ctx, DUK_ERR_RANGE_ERROR, "Plugin HTTP request limit reached");
    constexpr std::size_t capacity = 1024 * 1024;
    auto *body = static_cast<char *>(duk_push_fixed_buffer(ctx, capacity));
    const duk_idx_t buffer = duk_get_top_index(ctx);
    HttpResponse response;
    const auto started = monotonic_ms();
    const int rc = http_request(url, post, headers, count, payload, length, body, capacity, response);
    const auto elapsed = monotonic_ms() - started;
    budget.deadline += elapsed;
    if (!std::strcmp(budget.plugin_id, kinozal_id))
        diagnostics::record("KinoZal.TV", "HTTP status=%d result=%#x bytes=%zu ms=%llu",
                            response.status, static_cast<unsigned>(rc), response.length,
                            static_cast<unsigned long long>(elapsed));
    if (auto *log = std::fopen(MOVIAN_DOWNLOAD0 "/plugin-http.log", "a"))
    {
        std::fprintf(log, "v52 %s HTTP status=%d rc=%#x bytes=%zu ms=%llu\n",
                     budget.plugin_id, response.status, static_cast<unsigned>(rc),
                     response.length, static_cast<unsigned long long>(elapsed));
        std::fclose(log);
    }
    if (rc < 0)
        return duk_error(ctx, DUK_ERR_ERROR, "HTTP request failed (%x)", static_cast<unsigned>(rc));
    duk_push_object(ctx);
    duk_push_int(ctx, response.status);
    duk_put_prop_string(ctx, -2, "status");
    duk_push_string(ctx, response.headers);
    duk_put_prop_string(ctx, -2, "headers");
    duk_push_lstring(ctx, body, response.length);
    duk_put_prop_string(ctx, -2, "body");
    duk_push_buffer_object(ctx, buffer, 0, response.length, DUK_BUFOBJ_UINT8ARRAY);
    duk_put_prop_string(ctx, -2, "bytes");
    return 1;
}

void install_bindings(duk_context *ctx)
{
    duk_push_c_function(ctx, load_option, 1);
    duk_put_global_string(ctx, "__movianLoadOption");
    duk_push_c_function(ctx, request_http, 4);
    duk_put_global_string(ctx, "__movianHttp");
    duk_push_c_function(ctx, read_store, 1);
    duk_put_global_string(ctx, "__movianReadStore");
    duk_push_c_function(ctx, save_store, 2);
    duk_put_global_string(ctx, "__movianSaveStore");
    duk_push_c_function(ctx, import_torrent, 1);
    duk_put_global_string(ctx, "__movianImportTorrent");
    duk_push_c_function(ctx, load_zip_module, 1);
    duk_put_global_string(ctx, "__movianLoadZipModule");
#ifdef MOVIAN_HAVE_GUMBO
    install_html(ctx);
#endif
}

bool read_source(const char *path, char *buffer, std::size_t capacity, std::size_t &size) noexcept
{
    std::FILE *file = std::fopen(path, "rb");
    if (!file)
        return false;
    size = std::fread(buffer, 1, capacity, file);
    const bool ok = size > 0 && size < capacity && !std::ferror(file);
    std::fclose(file);
    return ok;
}

duk_ret_t decode_manifest(duk_context *ctx, void *)
{
    duk_json_decode(ctx, 0);
    return 1;
}

void property(duk_context *ctx, duk_idx_t object, const char *key, char *dest, int size)
{
    duk_get_prop_string(ctx, object, key);
    copy_text(dest, size, duk_get_string(ctx, -1));
    duk_pop(ctx);
}

bool parse_manifest(duk_context *ctx, const char *source, std::size_t size,
                    Plugin &plugin, char (&entry)[96], bool expose)
{
    if (!source)
        return false;
    duk_push_lstring(ctx, source, size);
    if (duk_safe_call(ctx, decode_manifest, nullptr, 1, 1) != DUK_EXEC_SUCCESS ||
        !duk_is_object(ctx, -1))
        return false;
    duk_get_prop_string(ctx, -1, "apiversion");
    const bool version_ok = duk_get_int(ctx, -1) == 2;
    duk_pop(ctx);
    duk_get_prop_string(ctx, -1, "id");
    duk_size_t id_size = 0;
    const char *id = duk_get_lstring(ctx, -1, &id_size);
    const bool id_ok = valid_name(id) && id_size == std::strlen(id) && id_size < sizeof plugin.id;
    copy_text(plugin.id, sizeof plugin.id, id);
    duk_pop(ctx);
    duk_get_prop_string(ctx, -1, "file");
    duk_size_t file_size = 0;
    const char *file = duk_get_lstring(ctx, -1, &file_size);
    const bool entry_ok = valid_name(file) && file_size == std::strlen(file) && file_size < sizeof entry;
    copy_text(entry, sizeof entry, file);
    duk_pop(ctx);
    property(ctx, -1, "title", plugin.title, sizeof plugin.title);
    property(ctx, -1, "version", plugin.version, sizeof plugin.version);
    property(ctx, -1, "synopsis", plugin.synopsis, sizeof plugin.synopsis);
    property(ctx, -1, "author", plugin.author, sizeof plugin.author);
    char icon[sizeof plugin.icon]{};
    property(ctx, -1, "icon", icon, sizeof icon);
    constexpr char archive_scheme[] = "plugin:/";
    constexpr auto scheme_length = sizeof archive_scheme - 1;
    const auto icon_length = std::strlen(icon);
    if (!std::strncmp(icon, "https://", 8) || !std::strncmp(icon, "plugin:/", 8))
        copy_text(plugin.icon, sizeof plugin.icon, icon);
    else if (icon[0] && icon[0] != '/' && !std::strstr(icon, "..") &&
             !std::strpbrk(icon, ":\\\r\n") && icon_length + scheme_length < sizeof plugin.icon)
    {
        std::memcpy(plugin.icon, archive_scheme, scheme_length);
        std::memcpy(plugin.icon + scheme_length, icon, icon_length + 1);
    }
    duk_pop(ctx);
    const bool valid = version_ok && id_ok && entry_ok && plugin.title[0];
    if (valid && expose)
    {
        duk_push_object(ctx);
        duk_push_string(ctx, plugin.id);
        duk_put_prop_string(ctx, -2, "id");
        duk_push_lstring(ctx, source, size);
        duk_put_prop_string(ctx, -2, "manifest");
        duk_push_string(ctx, "plugin:/");
        duk_put_prop_string(ctx, -2, "path");
        duk_put_global_string(ctx, "Plugin");
    }
    return valid;
}

bool read_manifest(duk_context *ctx, Plugin &plugin, char (&entry)[96], Archive *archive,
                   bool expose = false)
{
    std::size_t size = 0;
    if (archive)
    {
        char *source = archive->extract("plugin.json", 8192, size);
        const bool ok = parse_manifest(ctx, source, size, plugin, entry, expose);
        mz_free(source);
        return ok;
    }
    char source[16384];
    return read_source(manifest, source, sizeof source, size) &&
           parse_manifest(ctx, source, size, plugin, entry, expose);
}

bool evaluate(duk_context *ctx, const char *path, char *error, int capacity)
{
    auto *source = static_cast<char *>(std::malloc(max_script + 1));
    std::size_t size;
    if (!source || !read_source(path, source, max_script + 1, size))
    {
        std::free(source);
        copy_text(error, capacity, "Could not read plugin script");
        return false;
    }
    const bool ok = duk_peval_lstring(ctx, source, size) == DUK_EXEC_SUCCESS;
    std::free(source);
    if (!ok)
        copy_text(error, capacity, duk_safe_to_string(ctx, -1));
    duk_pop(ctx);
    return ok;
}

duk_ret_t read_page(duk_context *ctx, void *userdata)
{
    auto &page = *static_cast<Page *>(userdata);
    property(ctx, 0, "title", page.title, sizeof page.title);
    property(ctx, 0, "notice", page.notice, sizeof page.notice);
    duk_get_prop_string(ctx, 0, "items");
    const auto length = duk_get_length(ctx, -1);
    for (duk_uarridx_t i = 0; i < length && i < max_items; ++i)
    {
        duk_get_prop_index(ctx, -1, i);
        if (duk_is_object(ctx, -1))
        {
            Item &item = page.items[page.count];
            property(ctx, -1, "title", item.title, sizeof item.title);
            property(ctx, -1, "url", item.url, sizeof item.url);
            property(ctx, -1, "type", item.type, sizeof item.type);
            property(ctx, -1, "detail", item.detail, sizeof item.detail);
            property(ctx, -1, "icon", item.icon, sizeof item.icon);
            if (item.title[0] && (item.url[0] || !std::strcmp(item.type, "notice")))
                ++page.count;
        }
        duk_pop(ctx);
    }
    return 0;
}
#endif
} // namespace

int catalog(Plugin (&entries)[max_installed]) noexcept
{
#ifdef MOVIAN_HAVE_DUKTAPE
    Engine engine;
    char entry[96];
    if (!engine.ctx)
        return 0;
    int count = 0;
    Plugin bundled{};
    if (read_manifest(engine.ctx, bundled, entry, nullptr))
        entries[count++] = bundled;
    fs::Entry files[max_installed * 4];
    int found = 0;
    fs::list_dir(installed_dir, files, max_installed * 4, &found, nullptr, true);
    for (int i = 0; i < found && count < max_installed; ++i)
    {
        const char *ext = std::strrchr(files[i].name, '.');
        if (!ext || std::strcmp(ext, ".zip") || files[i].kind == fs::Kind::directory)
            continue;
        Archive archive;
        Plugin plugin{};
        duk_set_top(engine.ctx, 0);
        if (archive.load(files[i].path) && read_manifest(engine.ctx, plugin, entry, &archive))
        {
            copy_text(plugin.archive, sizeof plugin.archive, files[i].path);
            entries[count++] = plugin;
        }
    }
    std::sort(entries, entries + count, [](const Plugin &a, const Plugin &b) {
        const int title = strcasecmp(a.title, b.title);
        return title ? title < 0 : std::strcmp(a.id, b.id) < 0;
    });
    return count;
#else
    (void)entries;
    return 0;
#endif
}

bool inspect_package(const char *path, Plugin &plugin) noexcept
{
    plugin = {};
#ifdef MOVIAN_HAVE_DUKTAPE
    Archive archive;
    if (!archive.load(path)) return false;
    Engine engine;
    char entry[96];
    if (!engine.ctx || !read_manifest(engine.ctx, plugin, entry, &archive))
        return false;
    copy_text(plugin.archive, sizeof plugin.archive, path);
    return true;
#else
    (void)path;
    return false;
#endif
}

bool install(const char *path, char *error, int capacity) noexcept
{
#ifdef MOVIAN_HAVE_DUKTAPE
    Archive archive;
    if (!archive.load(path)) {
        copy_text(error, capacity, "Could not read plugin ZIP (8 MiB maximum)");
        return false;
    }
    Engine engine;
    Plugin plugin{};
    char entry[96];
    if (!engine.ctx ||
        !read_manifest(engine.ctx, plugin, entry, &archive) || !std::strcmp(plugin.id, bundled_id))
    {
        copy_text(error, capacity, "Expected a Movian API v2 ZIP with plugin.json at the top level");
        return false;
    }
    std::size_t size = 0;
    char *script = archive.extract(entry, max_script, size);
    if (!script)
    {
        copy_text(error, capacity, "Plugin script missing, damaged, or too large");
        return false;
    }
    const int compiled = duk_pcompile_lstring(engine.ctx, DUK_COMPILE_EVAL, script, size);
    mz_free(script);
    if (compiled != DUK_EXEC_SUCCESS)
    {
        copy_text(error, capacity, duk_safe_to_string(engine.ctx, -1));
        return false;
    }
    mkdir(installed_dir, 0777);
    char dest[256], temp[256];
    std::snprintf(dest, sizeof dest, "%s/%s.zip", installed_dir, plugin.id);
    std::snprintf(temp, sizeof temp, "%s/.%s.tmp", installed_dir, plugin.id);
    struct stat existing{};
    if (stat(dest, &existing) != 0)
    {
        Plugin installed[max_installed];
        if (catalog(installed) == max_installed)
        {
            copy_text(error, capacity, "Installed plugins list is full");
            return false;
        }
    }
    auto *out = std::fopen(temp, "wb");
    if (out)
    {
        const bool written = std::fwrite(archive.data, 1, archive.size, out) == archive.size;
        const bool closed = std::fclose(out) == 0;
        if (written && closed && std::rename(temp, dest) == 0)
            return true;
        std::remove(temp);
    }
    copy_text(error, capacity, "Could not save installed plugin");
    return false;
#else
    (void)path;
    copy_text(error, capacity, "Plugin engine unavailable");
    return false;
#endif
}

bool uninstall(const Plugin &plugin, char *error, int capacity) noexcept
{
    if (!storage::remove_plugin(plugin.id, plugin.archive, error, capacity)) return false;
    if (!std::strcmp(plugin.id, kinozal_id)) {
        erase(g_kinozal_cookie, sizeof g_kinozal_cookie);
        erase(g_kinozal_origin, sizeof g_kinozal_origin);
    }
    return true;
}

bool open(const Plugin &plugin, const char *url, Page &page, char *error, int capacity) noexcept
{
    page = {};
#ifdef MOVIAN_HAVE_DUKTAPE
    Engine engine;
    Archive archive;
    Plugin checked{};
    char entry[96];
    auto *ctx = engine.ctx;
    if (!ctx || (plugin.archive[0] && !archive.load(plugin.archive)) ||
        !read_manifest(ctx, checked, entry, plugin.archive[0] ? &archive : nullptr, true))
    {
        copy_text(error, capacity, "Could not load plugin manifest");
        return false;
    }
    char path[sizeof root + sizeof entry];
    std::snprintf(path, sizeof path, "%s%s", root, entry);
    copy_text(engine.budget.plugin_id, sizeof engine.budget.plugin_id, checked.id);
    if (plugin.archive[0])
        engine.budget.archive = &archive;
    install_bindings(ctx);
    if (!evaluate(ctx, bootstrap, error, capacity))
        return false;
    engine.budget.deadline = monotonic_ms() + 300;
    if (plugin.archive[0])
    {
        std::size_t size = 0;
        char *script = archive.extract(entry, max_script, size);
        if (!script)
        {
            copy_text(error, capacity, "Could not read plugin script from ZIP");
            return false;
        }
        const bool ok = duk_peval_lstring(ctx, script, size) == DUK_EXEC_SUCCESS;
        mz_free(script);
        if (!ok)
        {
            copy_text(error, capacity, duk_safe_to_string(ctx, -1));
            return false;
        }
        duk_pop(ctx);
    }
    else if (!evaluate(ctx, path, error, capacity))
        return false;
    duk_get_global_string(ctx, "openPluginPage");
    duk_push_string(ctx, url ? url : "");
    if (duk_pcall(ctx, 1) != DUK_EXEC_SUCCESS ||
        duk_safe_call(ctx, read_page, &page, 1, 1) != DUK_EXEC_SUCCESS)
    {
        copy_text(error, capacity, duk_safe_to_string(ctx, -1));
        return false;
    }
    return true;
#else
    (void)plugin;
    (void)url;
    copy_text(error, capacity, "Plugin engine unavailable");
    return false;
#endif
}

bool torrent_path(const Plugin &plugin, char *path, std::size_t capacity) noexcept
{
#ifdef MOVIAN_HAVE_DUKTAPE
    return torrent_cache_path(plugin.id, path, capacity);
#else
    (void)plugin; (void)path; (void)capacity;
    return false;
#endif
}

bool set_option(const Plugin &plugin, const char *assignment) noexcept
{
#ifdef MOVIAN_HAVE_DUKTAPE
    const char *sep = std::strchr(assignment, '=');
    char key[64]{};
    if (!sep || sep == assignment || sep - assignment >= static_cast<int>(sizeof key) ||
        std::strlen(sep + 1) >= 256)
        return false;
    std::memcpy(key, assignment, static_cast<std::size_t>(sep - assignment));
    char path[256];
    if (!option_path(path, sizeof path, plugin.id, key))
        return false;
    return write_atomic(path, sep + 1, std::strlen(sep + 1));
#else
    (void)plugin;
    (void)assignment;
#endif
    return false;
}

#ifdef MOVIAN_HAVE_DUKTAPE
static Category repo_category(const char *name) noexcept
{
    if (!strcasecmp(name, "tv")) return Category::tv;
    if (!strcasecmp(name, "video")) return Category::video;
    if (!strcasecmp(name, "music")) return Category::music;
    if (!strcasecmp(name, "subtitles")) return Category::subtitles;
    if (!strcasecmp(name, "osk") || !strcasecmp(name, "keyboard")) return Category::keyboards;
    if (!strcasecmp(name, "tvru") || !strcasecmp(name, "tv-ru")) return Category::regional_tv;
    return Category::other;
}
#endif

bool repository(const char *url, unsigned offset, Page &page, char *error, int capacity,
                bool refresh, Category category) noexcept
{
    page = {};
#ifdef MOVIAN_HAVE_DUKTAPE
    Engine engine;
    constexpr std::size_t limit = 1024 * 1024;
    constexpr char cached[] = MOVIAN_DOWNLOAD0 "/mprepo-cache.json";
    constexpr char seeded[] = MOVIAN_DATA_DIR "/mprepo-cache.json";
    const bool preferred_feed = url && (!std::strcmp(url, settings::mprepo_short_url) ||
                                        !std::strcmp(url, settings::mprepo_direct_url));
    auto *body = static_cast<char *>(std::malloc(limit));
    HttpResponse response;
    const bool snapshot = engine.ctx && body && preferred_feed && !refresh &&
                          (read_source(cached, body, limit, response.length) ||
                           read_source(seeded, body, limit, response.length));
    const bool use_direct = preferred_feed && refresh;
    if (!engine.ctx || !body || (!snapshot && !public_get(use_direct ? settings::mprepo_direct_url : url,
                                                         body, limit, response, true)))
    {
        std::free(body);
        copy_text(error, capacity, "Could not load HTTPS plugin repository");
        return false;
    }
    auto *ctx = engine.ctx;
    duk_push_lstring(ctx, body, response.length);
    if (duk_safe_call(ctx, decode_manifest, nullptr, 1, 1) != DUK_EXEC_SUCCESS ||
        !duk_is_object(ctx, -1))
    {
        std::free(body);
        copy_text(error, capacity, "Repository did not return valid JSON");
        return false;
    }
    duk_get_prop_string(ctx, -1, "plugins");
    if (!duk_is_array(ctx, -1))
    {
        std::free(body);
        copy_text(error, capacity, "Repository has no plugin list");
        return false;
    }
    if (preferred_feed && !snapshot)
        write_atomic(cached, body, response.length);
    std::free(body);
    copy_text(page.title, sizeof page.title, category_names[static_cast<int>(category)]);
    if (snapshot)
        copy_text(page.notice, sizeof page.notice, "Showing the saved bit.ly/mprepo catalog");
    const auto count = duk_get_length(ctx, -1);
    duk_uarridx_t i = offset;
    for (; i < count && page.count < max_items - (preferred_feed ? 2 : 1); ++i)
    {
        duk_get_prop_index(ctx, -1, i);
        if (duk_is_object(ctx, -1))
        {
            auto &item = page.items[page.count];
            item = {};
            property(ctx, -1, "title", item.title, sizeof item.title);
            property(ctx, -1, "synopsis", item.detail, sizeof item.detail);
            property(ctx, -1, "downloadURL", item.url, sizeof item.url);
            property(ctx, -1, "icon", item.icon, sizeof item.icon);
            property(ctx, -1, "id", item.id, sizeof item.id);
            property(ctx, -1, "version", item.version, sizeof item.version);
            property(ctx, -1, "author", item.author, sizeof item.author);
            char name[64]{};
            property(ctx, -1, "category", name, sizeof name);
            if ((category == Category::all || repo_category(name) == category) &&
                item.title[0] && !std::strncmp(item.url, "https://", 8))
            {
                copy_text(item.type, sizeof item.type, "package");
                ++page.count;
            }
        }
        duk_pop(ctx);
    }
    if (i < count)
    {
        auto &next = page.items[page.count++];
        copy_text(next.title, sizeof next.title, "More plugins");
        copy_text(next.type, sizeof next.type, "repo");
        std::snprintf(next.url, sizeof next.url, "%u", i);
    }
    if (preferred_feed)
    {
        auto &reload = page.items[page.count++];
        copy_text(reload.title, sizeof reload.title, "Refresh over HTTPS");
        copy_text(reload.type, sizeof reload.type, "repo-refresh");
    }
    return true;
#else
    (void)url;
    (void)offset;
    (void)refresh;
    (void)category;
    copy_text(error, capacity, "Plugin engine unavailable");
    return false;
#endif
}

bool install_remote(const char *url, char *error, int capacity) noexcept
{
#ifdef MOVIAN_HAVE_DUKTAPE
    auto *body = static_cast<char *>(std::malloc(max_archive));
    HttpResponse response;
    if (!body || !public_get(url, body, max_archive, response))
    {
        std::free(body);
        copy_text(error, capacity, "Could not download plugin ZIP");
        return false;
    }
    mkdir(installed_dir, 0777);
    constexpr char temp[] = MOVIAN_PLUGIN_DIR "/.repository-download";
    auto *file = std::fopen(temp, "wb");
    bool saved = false;
    if (file)
    {
        const auto length = std::fwrite(body, 1, response.length, file);
        saved = std::fclose(file) == 0 && length == response.length;
    }
    std::free(body);
    if (!saved)
        copy_text(error, capacity, "Could not stage plugin ZIP");
    const bool installed = saved && install(temp, error, capacity);
    std::remove(temp);
    return installed;
#else
    (void)url;
    copy_text(error, capacity, "Plugin engine unavailable");
    return false;
#endif
}

bool artwork(const Plugin &plugin, const char *url, image::Image &out) noexcept
{
#ifdef MOVIAN_HAVE_DUKTAPE
    constexpr std::size_t limit = 768 * 1024;
    char *body = nullptr;
    std::size_t size = 0;
    bool archive_bytes = false;
    if (!std::strncmp(url, "plugin:/", 8))
    {
        const char *entry = url + 8;
        if (!*entry || *entry == '/' || std::strstr(entry, "..") || std::strchr(entry, '\\'))
            return false;
        Archive archive;
        if (plugin.archive[0] && archive.load(plugin.archive))
        {
            body = archive.extract(entry, limit, size);
            archive_bytes = true;
        }
        else if (!plugin.archive[0])
        {
            char path[sizeof root + 512];
            const int length = std::snprintf(path, sizeof path, "%s%s", root, entry);
            if (length > 0 && length < static_cast<int>(sizeof path))
            {
                body = static_cast<char *>(std::malloc(limit));
                if (body && !read_source(path, body, limit, size)) size = 0;
            }
        }
    }
    else if (!std::strncmp(url, "https://", 8))
    {
        body = static_cast<char *>(std::malloc(limit));
        HttpResponse response;
        if (body && public_get(url, body, limit, response))
            size = response.length;
    }
    int w = 0, h = 0, channels = 0;
    unsigned char *pixels = nullptr;
    if (body && size &&
        stbi_info_from_memory(reinterpret_cast<unsigned char *>(body), static_cast<int>(size),
                              &w, &h, &channels) && w > 0 && h > 0 && w <= 1600 && h <= 1600)
        pixels = stbi_load_from_memory(reinterpret_cast<unsigned char *>(body),
                                       static_cast<int>(size), &w, &h, &channels, 4);
    if (archive_bytes)
        mz_free(body);
    else
        std::free(body);
    if (pixels)
    {
        out = {w, h, reinterpret_cast<std::uint32_t *>(pixels)};
        return true;
    }
#else
    (void)plugin;
    (void)url;
    (void)out;
#endif
    return false;
}

bool kinozal_login_origin(char (&origin)[264]) noexcept
{
#ifdef MOVIAN_HAVE_DUKTAPE
    char domain[256]{}, protocol[256]{};
    if (read_option(kinozal_id, "protocol", protocol) && std::strcmp(protocol, "https://"))
        return false;
    if (!read_option(kinozal_id, "domain", domain))
        copy_text(domain, sizeof domain, "xdomain0");
    const bool first = !std::strcmp(domain, "xdomain0");
    if (first || !std::strcmp(domain, "xdomainx"))
    {
        if (!read_option(kinozal_id, first ? "domain0" : "domainx", domain) && first)
            copy_text(domain, sizeof domain, "kinozal.cloudns.nz");
    }
    const char *host = domain;
    if (!*host || std::strlen(host) > 253 || !std::strchr(host, '.')) return false;
    for (const char *label = host; *label;)
    {
        const auto length = std::strcspn(label, ".");
        if (!length || length > 63 || label[0] == '-' || label[length - 1] == '-' ||
            std::strspn(label, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-") != length)
            return false;
        if (!label[length]) break;
        label += length + 1;
        if (!*label) return false;
    }
    std::snprintf(origin, sizeof origin, "https://%s", host);
    return true;
#else
    (void)origin;
    return false;
#endif
}

bool kinozal_login(const char *origin, const char *username, const char *password,
                   char *error, int capacity) noexcept
{
    char selected[264]{};
    if (!kinozal_login_origin(selected) || std::strcmp(origin, selected))
    {
        copy_text(error, capacity, "Select the KinoZal.TV domain and HTTPS in plugin settings");
        return false;
    }
    char form[1600] = "username=";
    char *out = form + sizeof("username=") - 1;
    constexpr char field[] = "&password=";
    const char *end = form + sizeof form;
    if (!username[0] || !password[0] || !form_value(out, end, username) ||
        end - out < static_cast<int>(sizeof field - 1))
    {
        erase(form, sizeof form);
        copy_text(error, capacity, "Invalid KinoZal.TV credentials");
        return false;
    }
    std::memcpy(out, field, sizeof field - 1);
    out += sizeof field - 1;
    constexpr char return_to[] = "&returnto=";
    if (!form_value(out, end, password) || end - out < static_cast<int>(sizeof return_to - 1))
    {
        erase(form, sizeof form);
        copy_text(error, capacity, "KinoZal.TV credentials too long");
        return false;
    }
    std::memcpy(out, return_to, sizeof return_to - 1);
    out += sizeof return_to - 1;
    constexpr std::size_t limit = 256 * 1024;
    auto *body = static_cast<char *>(std::malloc(limit));
    HttpResponse response;
    char url[288], referer[266];
    std::snprintf(url, sizeof url, "%s/takelogin.php", selected);
    std::snprintf(referer, sizeof referer, "%s/", selected);
    const Header headers[] = {{"Content-Type", "application/x-www-form-urlencoded"},
                              {"Referer", referer}, {"Origin", selected}};
    const int rc = body ? http_request(url, true, headers, 3, form,
                                      static_cast<std::size_t>(out - form), body, limit, response) : -1;
    erase(form, sizeof form);
    std::free(body);
    char uid[256]{}, pass[256]{};
    const bool ok = rc >= 0 && response.status >= 200 && response.status < 400 &&
                    find_cookie(response.headers, "uid", uid, sizeof uid) &&
                    find_cookie(response.headers, "pass", pass, sizeof pass) &&
                    std::strlen(uid) + std::strlen(pass) + 3 <= sizeof g_kinozal_cookie;
    if (ok)
    {
        copy_text(g_kinozal_origin, sizeof g_kinozal_origin, selected);
        const auto size = std::strlen(uid);
        std::memcpy(g_kinozal_cookie, uid, size);
        std::memcpy(g_kinozal_cookie + size, "; ", 2);
        std::memcpy(g_kinozal_cookie + size + 2, pass, std::strlen(pass) + 1);
    }
    else if (rc < 0)
        copy_text(error, capacity, "KinoZal.TV login connection failed");
    else if (response.status == 429)
        copy_text(error, capacity, "KinoZal.TV rate limited the login (HTTP 429)");
    else if (response.status == 403)
        copy_text(error, capacity, "KinoZal.TV blocked the login request (HTTP 403)");
    else
        copy_text(error, capacity, "KinoZal.TV did not accept the login");
    diagnostics::record("KinoZal.TV", "login status=%d result=%#x authenticated=%d",
                        response.status, static_cast<unsigned>(rc), ok);
    if (auto *log = std::fopen(MOVIAN_DOWNLOAD0 "/plugin-http.log", "a"))
    {
        std::fprintf(log, "v52 KinoZal.TV login status=%d rc=%#x authenticated=%d\n",
                     response.status, static_cast<unsigned>(rc), ok);
        std::fclose(log);
    }
    erase(uid, sizeof uid);
    erase(pass, sizeof pass);
    erase(response.headers, sizeof response.headers);
    return ok;
}
} // namespace movian::plugins
