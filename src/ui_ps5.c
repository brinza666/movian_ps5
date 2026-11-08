#include "paw_platform.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* UI element type */
typedef struct ui_element {
    int x;
    int y;
    int w;
    int h;
    char *text;
    int focused;
    int visible;
    void (*on_render)(struct ui_element *);
    void (*on_key)(struct ui_element *, int key, int pressed);
    void *user_data;
} ui_element_t;

static int g_ui_width = 1920;
static int g_ui_height = 1080;
static ui_element_t *g_ui_focused = NULL;
static ui_element_t g_ui_root = {0};

/* Forward declarations */
void ps5_ui_render_element(ui_element_t *elem, ps5_video_t *video);

int ps5_ui_init(int width, int height) {
    g_ui_width = width;
    g_ui_height = height;
    memset(&g_ui_root, 0, sizeof(g_ui_root));
    g_ui_root.x = 0;
    g_ui_root.y = 0;
    g_ui_root.w = width;
    g_ui_root.h = height;
    g_ui_root.visible = 1;
    fprintf(stderr, "[PS5 UI] Initialized: %dx%d\n", width, height);
    return 0;
}

void* ps5_ui_create_element(int x, int y, int w, int h) {
    ui_element_t *elem = calloc(1, sizeof(ui_element_t));
    if (!elem) return NULL;
    elem->x = x;
    elem->y = y;
    elem->w = w;
    elem->h = h;
    elem->visible = 1;
    return elem;
}

void ps5_ui_destroy_element(void *elem) {
    if (!elem) return;
    ui_element_t *e = (ui_element_t *)elem;
    free(e->text);
    free(e);
}

void ps5_ui_set_text(void *elem, const char *text) {
    if (!elem) return;
    ui_element_t *e = (ui_element_t *)elem;
    free(e->text);
    e->text = text ? strdup(text) : NULL;
}

void ps5_ui_set_on_render(void *elem, void (*on_render)(void*)) {
    if (!elem) return;
    ui_element_t *e = (ui_element_t *)elem;
    e->on_render = (void (*)(struct ui_element *))on_render;
}

void ps5_ui_set_on_key(void *elem, void (*on_key)(void*, int, int)) {
    if (!elem) return;
    ui_element_t *e = (ui_element_t *)elem;
    e->on_key = (void (*)(struct ui_element *, int, int))on_key;
}

void ps5_ui_focus(void *elem) {
    if (g_ui_focused) {
        g_ui_focused->focused = 0;
    }
    g_ui_focused = (ui_element_t *)elem;
    if (g_ui_focused) {
        g_ui_focused->focused = 1;
    }
}

void ps5_ui_key(int key, int pressed) {
    if (g_ui_focused && g_ui_focused->on_key) {
        g_ui_focused->on_key(g_ui_focused, key, pressed);
    }
}

void ps5_ui_render(void *video) {
    if (!video) return;
    ps5_video_clear((ps5_video_t *)video, 0x1a, 0x1a, 0x2e, 0xFF);
    ps5_ui_render_element(&g_ui_root, (ps5_video_t *)video);
    ps5_video_flip((ps5_video_t *)video);
}

void ps5_ui_render_element(ui_element_t *elem, ps5_video_t *video) {
    if (!elem || !elem->visible) return;
    if (elem->w > 0 && elem->h > 0) {
        uint32_t color = elem->focused ? 0x4a90e2FF : 0x2a2a3eFF;
        uint8_t a = (color >> 24) & 0xFF;
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        ps5_video_draw_rect(video, elem->x, elem->y, elem->w, elem->h,
                           r, g, b, a);
    }
    if (elem->on_render) {
        elem->on_render(elem);
    }
}

void ps5_ui_keyboard_show(void) {
    fprintf(stderr, "[PS5 UI] On-screen keyboard shown\n");
}

void ps5_ui_keyboard_hide(void) {
    fprintf(stderr, "[PS5 UI] On-screen keyboard hidden\n");
}

void ps5_ui_keyboard_key(int key, int pressed) {
    if (!g_ui_focused || !pressed) return;
    fprintf(stderr, "[PS5 UI] Keyboard key: %d\n", key);
}

const char* ps5_ui_keyboard_get_text(void) {
    return "";
}

int ps5_ui_register_setting(const char *id, const char *name, int type, void *default_val) {
    (void)id; (void)name; (void)type; (void)default_val;
    fprintf(stderr, "[PS5 UI] Registered setting: %s\n", name);
    return 0;
}

int ps5_ui_get_setting_bool(const char *id) {
    (void)id;
    return 0;
}

const char* ps5_ui_get_setting_string(const char *id) {
    (void)id;
    return NULL;
}

int ps5_ui_get_setting_int(const char *id) {
    (void)id;
    return 0;
}

int ps5_ui_set_setting_bool(const char *id, int val) {
    (void)id; (void)val;
    return 0;
}

int ps5_ui_set_setting_string(const char *id, const char *val) {
    (void)id; (void)val;
    return 0;
}

void ps5_ui_shutdown(void) {
    fprintf(stderr, "[PS5 UI] Shutdown\n");
}
