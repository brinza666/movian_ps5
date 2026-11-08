/*
 * Movian PS5 - UI / GLW Interface
 * User interface with on-screen keyboard, settings, navigation
 * Based on Movian GLW skin system
 */

#include "paw_platform.h"
#include "duktape.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* UI state */
typedef struct {
    int width;
    int height;
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

/* UI colors (Movian branding) */
#define UI_COLOR_BG       0x1a1a2eFF   /* Dark blue background */
#define UI_COLOR_PANEL    0x2a2a3eFF   /* Panel background */
#define UI_COLOR_TEXT     0xFFFFFFFF   /* White text */
#define UI_COLOR_HIGHLIGHT 0x4a90e2FF  /* Movian blue highlight */
#define UI_COLOR_BUTTON   0x3a3a4eFF   /* Button background */
#define UI_COLOR_BUTTON_HOVER 0x4a4a5eFF

/* Initialize UI */
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

/* Create UI element */
ui_element_t* ps5_ui_create_element(int x, int y, int w, int h) {
    ui_element_t *elem = calloc(1, sizeof(ui_element_t));
    if (!elem) return NULL;

    elem->x = x;
    elem->y = y;
    elem->w = w;
    elem->h = h;
    elem->visible = 1;

    return elem;
}

/* Destroy UI element */
void ps5_ui_destroy_element(ui_element_t *elem) {
    if (!elem) return;
    free(elem->text);
    free(elem);
}

/* Set element text */
void ps5_ui_set_text(ui_element_t *elem, const char *text) {
    if (!elem) return;
    free(elem->text);
    elem->text = text ? strdup(text) : NULL;
}

/* Set element callback */
void ps5_ui_set_on_render(ui_element_t *elem, void (*on_render)(ui_element_t*)) {
    if (!elem) return;
    elem->on_render = on_render;
}

/* Set element key callback */
void ps5_ui_set_on_key(ui_element_t *elem, void (*on_key)(ui_element_t*, int, int)) {
    if (!elem) return;
    elem->on_key = on_key;
}

/* Focus element */
void ps5_ui_focus(ui_element_t *elem) {
    if (g_ui_focused) {
        g_ui_focused->focused = 0;
    }
    g_ui_focused = elem;
    if (elem) {
        elem->focused = 1;
    }
}

/* Handle key input */
void ps5_ui_key(int key, int pressed) {
    if (g_ui_focused && g_ui_focused->on_key) {
        g_ui_focused->on_key(g_ui_focused, key, pressed);
    }
}

/* Render all UI elements */
void ps5_ui_render(ps5_video_t *video) {
    if (!video || !video->framebuffer) return;

    /* Clear background */
    ps5_video_clear(video, 0x1a, 0x1a, 0x2e, 0xFF);

    /* Render root and children */
    ps5_ui_render_element(&g_ui_root, video);

    /* Flip buffers */
    ps5_video_flip(video);
}

/* Render a single UI element */
void ps5_ui_render_element(ui_element_t *elem, ps5_video_t *video) {
    if (!elem || !elem->visible) return;

    /* Draw element background */
    if (elem->w > 0 && elem->h > 0) {
        uint32_t color = elem->focused ? UI_COLOR_HIGHLIGHT : UI_COLOR_PANEL;
        uint8_t a = (color >> 24) & 0xFF;
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;

        ps5_video_draw_rect(video, elem->x, elem->y, elem->w, elem->h,
                           r, g, b, a);
    }

    /* Draw text (placeholder - would use FreeType) */
    if (elem->text) {
        /* Text rendering would go here using FreeType FT_* functions */
    }

    /* Call custom render callback */
    if (elem->on_render) {
        elem->on_render(elem);
    }
}

/* On-screen keyboard */
typedef struct {
    char text[256];
    int cursor_pos;
    int visible;
    int shift;
    int caps_lock;
} os_keyboard_t;

static os_keyboard_t g_keyboard = {0};

/* Show on-screen keyboard */
void ps5_ui_keyboard_show(void) {
    memset(&g_keyboard, 0, sizeof(g_keyboard));
    g_keyboard.visible = 1;
    g_keyboard.shift = 0;
    g_keyboard.caps_lock = 0;
    fprintf(stderr, "[PS5 UI] On-screen keyboard shown\n");
}

/* Hide on-screen keyboard */
void ps5_ui_keyboard_hide(void) {
    g_keyboard.visible = 0;
    fprintf(stderr, "[PS5 UI] On-screen keyboard hidden\n");
}

/* Handle keyboard input */
void ps5_ui_keyboard_key(int key, int pressed) {
    if (!g_keyboard.visible || !pressed) return;

    /* Handle shift */
    if (key == 0x10) { /* Shift */
        g_keyboard.shift = !g_keyboard.shift;
        return;
    }

    /* Handle backspace */
    if (key == 0x08) { /* Backspace */
        if (g_keyboard.cursor_pos > 0) {
            memmove(&g_keyboard.text[g_keyboard.cursor_pos - 1],
                    &g_keyboard.text[g_keyboard.cursor_pos],
                    strlen(&g_keyboard.text[g_keyboard.cursor_pos]) + 1);
            g_keyboard.cursor_pos--;
        }
        return;
    }

    /* Handle enter */
    if (key == 0x0D) { /* Enter */
        g_keyboard.visible = 0;
        fprintf(stderr, "[PS5 UI] Keyboard submitted: %s\n", g_keyboard.text);
        return;
    }

    /* Add character */
    int len = strlen(g_keyboard.text);
    if (len < sizeof(g_keyboard.text) - 1) {
        char c = key; /* Simplified - would map key codes to characters */
        if (g_keyboard.shift || g_keyboard.caps_lock) {
            c = toupper(c);
        }
        memmove(&g_keyboard.text[g_keyboard.cursor_pos + 1],
                &g_keyboard.text[g_keyboard.cursor_pos],
                len - g_keyboard.cursor_pos + 1);
        g_keyboard.text[g_keyboard.cursor_pos] = c;
        g_keyboard.cursor_pos++;
    }
}

/* Get keyboard text */
const char* ps5_ui_keyboard_get_text(void) {
    return g_keyboard.text;
}

/* Settings system */
typedef struct {
    char *id;
    char *name;
    int type;            /* 0=bool, 1=string, 2=integer, 3=multi-choice, 4=action, 5=divider, 6=info */
    union {
        int bool_val;
        char *str_val;
        int int_val;
        char **choice_vals;
        int choice_count;
        int choice_index;
    } value;
    void (*on_change)(const char *id, void *value);
} setting_t;

static setting_t g_settings[128];
static int g_num_settings = 0;

/* Register a setting */
int ps5_ui_register_setting(const char *id, const char *name, int type, void *default_val) {
    if (!id || !name || g_num_settings >= 128) return -1;

    memset(&g_settings[g_num_settings], 0, sizeof(setting_t));
    g_settings[g_num_settings].id = strdup(id);
    g_settings[g_num_settings].name = strdup(name);
    g_settings[g_num_settings].type = type;

    switch (type) {
        case 0: /* bool */
            g_settings[g_num_settings].value.bool_val = default_val ? *(int*)default_val : 0;
            break;
        case 1: /* string */
            g_settings[g_num_settings].value.str_val = default_val ? strdup((char*)default_val) : NULL;
            break;
        case 2: /* integer */
            g_settings[g_num_settings].value.int_val = default_val ? *(int*)default_val : 0;
            break;
        case 3: /* multi-choice */
            g_settings[g_num_settings].value.choice_index = 0;
            break;
    }

    g_num_settings++;
    fprintf(stderr, "[PS5 UI] Registered setting: %s (%s)\n", id, name);
    return g_num_settings - 1;
}

/* Get setting value */
int ps5_ui_get_setting_bool(const char *id) {
    for (int i = 0; i < g_num_settings; i++) {
        if (g_settings[i].id && strcmp(g_settings[i].id, id) == 0) {
            return g_settings[i].value.bool_val;
        }
    }
    return 0;
}

const char* ps5_ui_get_setting_string(const char *id) {
    for (int i = 0; i < g_num_settings; i++) {
        if (g_settings[i].id && strcmp(g_settings[i].id, id) == 0) {
            return g_settings[i].value.str_val;
        }
    }
    return NULL;
}

int ps5_ui_get_setting_int(const char *id) {
    for (int i = 0; i < g_num_settings; i++) {
        if (g_settings[i].id && strcmp(g_settings[i].id, id) == 0) {
            return g_settings[i].value.int_val;
        }
    }
    return 0;
}

/* Set setting value */
int ps5_ui_set_setting_bool(const char *id, int val) {
    for (int i = 0; i < g_num_settings; i++) {
        if (g_settings[i].id && strcmp(g_settings[i].id, id) == 0) {
            g_settings[i].value.bool_val = val;
            if (g_settings[i].on_change) {
                g_settings[i].on_change(id, &g_settings[i].value.bool_val);
            }
            return 0;
        }
    }
    return -1;
}

int ps5_ui_set_setting_string(const char *id, const char *val) {
    for (int i = 0; i < g_num_settings; i++) {
        if (g_settings[i].id && strcmp(g_settings[i].id, id) == 0) {
            free(g_settings[i].value.str_val);
            g_settings[i].value.str_val = val ? strdup(val) : NULL;
            if (g_settings[i].on_change) {
                g_settings[i].on_change(id, g_settings[i].value.str_val);
            }
            return 0;
        }
    }
    return -1;
}

/* Shutdown UI */
void ps5_ui_shutdown(void) {
    ps5_ui_keyboard_hide();

    for (int i = 0; i < g_num_settings; i++) {
        free(g_settings[i].id);
        free(g_settings[i].name);
        if (g_settings[i].type == 1) free(g_settings[i].value.str_val);
        if (g_settings[i].type == 3) {
            for (int j = 0; j < g_settings[i].value.choice_count; j++) {
                free(g_settings[i].value.choice_vals[j]);
            }
            free(g_settings[i].value.choice_vals);
        }
    }
    g_num_settings = 0;

    fprintf(stderr, "[PS5 UI] Shutdown\n");
}
