#ifndef PAW_PLATFORM_PS5_H
#define PAW_PLATFORM_PS5_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct duk_context duk_context;


/* Video */
typedef struct {
    void *handle;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    void *framebuffer;
    uint32_t format;
} ps5_video_t;

int ps5_video_open(ps5_video_t *video);
int ps5_video_register_buffer(ps5_video_t *video, void *buf, uint32_t size);
int ps5_video_flip(ps5_video_t *video);
int ps5_video_wait_vblank(ps5_video_t *video);
int ps5_video_close(ps5_video_t *video);
void ps5_video_clear(ps5_video_t *video, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ps5_video_draw_rect(ps5_video_t *video, int x, int y, int w, int h,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void* ps5_video_get_framebuffer(ps5_video_t *video);
ps5_video_t* ps5_video_get_global(void);
int ps5_video_global_init(void);
void ps5_video_global_shutdown(void);

/* Audio */
typedef struct {
    void *handle;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t format;
} ps5_audio_t;

int ps5_audio_open(ps5_audio_t *audio);
int ps5_audio_write(ps5_audio_t *audio, const void *data, uint32_t size);
int ps5_audio_close(ps5_audio_t *audio);
ps5_audio_t* ps5_audio_get_global(void);
int ps5_audio_global_init(void);
void ps5_audio_global_shutdown(void);

/* Input */
typedef struct {
    uint32_t buttons;
    int16_t left_stick_x;
    int16_t left_stick_y;
    int16_t right_stick_x;
    int16_t right_stick_y;
    uint8_t left_trigger;
    uint8_t right_trigger;
} ps5_input_t;

int ps5_input_poll(ps5_input_t *input);
int ps5_input_button_pressed(ps5_input_t *input, uint32_t button);
ps5_input_t* ps5_input_get_global(void);
int ps5_input_global_init(void);
void ps5_input_global_shutdown(void);

/* Media player state */
typedef struct {
    void *fmt_ctx;
    int video_stream_index;
    int audio_stream_index;
    double duration;
    double current_time;
    int playing;
    int paused;
    double playback_speed;
    int seek_requested;
    double seek_target;
} media_player_t;

/* Media functions */
int ps5_media_init(void);
int ps5_media_open(const char *path);
int ps5_media_play(void);
int ps5_media_pause(void);
int ps5_media_stop(void);
int ps5_media_seek(double seconds);
int ps5_media_set_speed(double speed);
double ps5_media_get_time(void);
double ps5_media_get_duration(void);
int ps5_media_is_playing(void);
int ps5_media_decode_video_frame(uint8_t **buf, int *w, int *h);
int ps5_media_decode_audio_frame(uint8_t **buf, int *size);
void ps5_media_close(void);
media_player_t* ps5_media_get_global(void);
int ps5_media_global_init(void);
void ps5_media_global_shutdown(void);

/* File System */
int ps5_fs_exists(const char *path);
int ps5_fs_read(const char *path, void **buf, size_t *size);
int ps5_fs_write(const char *path, const void *buf, size_t size);
int ps5_fs_listdir(const char *path, char ***entries, int *count);
void ps5_fs_listdir_free(char **entries, int count);
int ps5_fs_is_directory(const char *path);
long ps5_fs_get_size(const char *path);
int ps5_fs_mkdir(const char *path);
int ps5_fs_remove(const char *path);
int ps5_fs_rename(const char *old_path, const char *new_path);

/* Network (SMB2) */
int ps5_net_smb2_init(void);
int ps5_net_smb2_connect(const char *host, const char *share,
                          const char *user, const char *pass);
int ps5_net_smb2_listdir(const char *share, const char *path,
                          char ***entries, int *count);
int ps5_net_smb2_open(const char *path);
int ps5_net_smb2_read(int fd, void *buf, size_t size);
int ps5_net_smb2_close(int fd);
void ps5_net_smb2_disconnect(void);
int ps5_net_smb2_is_connected(void);
void ps5_net_smb2_shutdown(void);

/* Torrent */
typedef struct {
    char info_hash[41];
    char name[256];
    char save_path[512];
    int64_t total_size;
    int64_t downloaded;
    int64_t uploaded;
    int num_peers;
    int num_seeds;
    double download_rate;
    double upload_rate;
    int state;
    int priority;
} torrent_t;

int ps5_torrent_add_magnet(const char *magnet_uri, const char *save_path);
int ps5_torrent_add_file(const char *torrent_path, const char *save_path);
int ps5_torrent_start(int slot);
int ps5_torrent_stop(int slot);
int ps5_torrent_remove(int slot);
int ps5_torrent_update(void);
int ps5_torrent_get_info(int slot, torrent_t *out);
int ps5_torrent_count(void);
int ps5_torrent_set_priority(int slot, int piece_index, int priority);
int ps5_torrent_piece_available(int slot, int piece_index);
void ps5_torrent_shutdown(void);

/* Subtitles */
int ps5_subtitle_load(const char *path, const char *language);
int ps5_subtitle_enable(int slot, int enabled);
const char* ps5_subtitle_get_text(double current_time);
int ps5_subtitle_count(void);
const char* ps5_subtitle_get_language(int slot);
int ps5_subtitle_is_enabled(int slot);
int ps5_subtitle_get_active(void);
int ps5_subtitle_download_opensubtitles(const char *movie_hash, const char *language);
void ps5_subtitle_shutdown(void);

/* Plugins */
typedef struct {
    char *id;
    char *name;
    char *version;
    char *description;
    char *author;
    char *url;
    char *local_path;
    int installed;
    int enabled;
    int api_version;
    duk_context *ctx;
} plugin_t;

int ps5_plugin_init(void);
void ps5_plugin_load_installed(void);
int ps5_plugin_install_from_repo(const char *plugin_id);
int ps5_plugin_install_from_path(const char *path, const char *id);
int ps5_plugin_enable(const char *id, int enabled);
int ps5_plugin_count(void);
int ps5_plugin_get_info(int index, plugin_t *out);
int ps5_plugin_find(const char *id);
int ps5_plugin_execute(void *duk_ctx, const char *plugin_id,
                        const char *function_name, int arg_count);
void ps5_plugin_set_repo_url(const char *url);
const char* ps5_plugin_get_repo_url(void);
void ps5_plugin_set_beta_password(const char *password);
void ps5_plugin_set_auto_upgrade(int enabled);
void ps5_plugin_set_install_on_demand(int enabled);
int ps5_plugin_check_updates(void);
void ps5_plugin_shutdown(void);

/* UI */
int ps5_ui_init(int width, int height);
void* ps5_ui_create_element(int x, int y, int w, int h);
void ps5_ui_destroy_element(void *elem);
void ps5_ui_set_text(void *elem, const char *text);
void ps5_ui_set_on_render(void *elem, void (*on_render)(void*));
void ps5_ui_set_on_key(void *elem, void (*on_key)(void*, int, int));
void ps5_ui_focus(void *elem);
void ps5_ui_key(int key, int pressed);
void ps5_ui_render(void *video);
void ps5_ui_keyboard_show(void);
void ps5_ui_keyboard_hide(void);
void ps5_ui_keyboard_key(int key, int pressed);
const char* ps5_ui_keyboard_get_text(void);
int ps5_ui_register_setting(const char *id, const char *name, int type, void *default_val);
int ps5_ui_get_setting_bool(const char *id);
const char* ps5_ui_get_setting_string(const char *id);
int ps5_ui_get_setting_int(const char *id);
int ps5_ui_set_setting_bool(const char *id, int val);
int ps5_ui_set_setting_string(const char *id, const char *val);
void ps5_ui_shutdown(void);

/* System utilities */
double ps5_now_seconds(void);
void ps5_sleep_ms(int ms);

/* System utilities */
double ps5_now_seconds(void);
void ps5_sleep_ms(int ms);

#ifdef __cplusplus
}
#endif

#endif /* PAW_PLATFORM_PS5_H */
