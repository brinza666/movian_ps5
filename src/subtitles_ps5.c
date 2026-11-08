#include "paw_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Forward declaration */
static double ps5_subtitle_parse_time(const char *time_str);

/* Subtitle entry */
typedef struct {
    double start_time;
    double end_time;
    char *text;
    struct subtitle_entry *next;
} subtitle_entry_t;

/* Subtitle track */
typedef struct {
    char *filename;
    char *language;
    int enabled;
    subtitle_entry_t *entries;
    int entry_count;
} subtitle_track_t;

/* Active subtitle tracks */
static subtitle_track_t g_subtitles[8];
static int g_num_subtitle_tracks = 0;
static int g_active_subtitle = -1;

/* Parse SRT subtitle file */
static int ps5_subtitle_parse_srt(const char *path, subtitle_track_t *track) {
    void *buf;
    size_t size;
    if (ps5_fs_read(path, &buf, &size) != 0) {
        fprintf(stderr, "[PS5 Subtitle] Cannot read %s\n", path);
        return -1;
    }

    char *content = malloc(size + 1);
    if (!content) {
        free(buf);
        return -1;
    }
    memcpy(content, buf, size);
    content[size] = '\0';
    free(buf);

    char *line = strtok(content, "\n");
    int entry_num = 0;
    subtitle_entry_t *last_entry = NULL;

    while (line) {
        while (*line && isspace(*line)) line++;
        if (*line == '\0') {
            line = strtok(NULL, "\n");
            continue;
        }

        if (isdigit(*line)) {
            entry_num = atoi(line);
            line = strtok(NULL, "\n");
            continue;
        }

        char *arrow = strstr(line, "-->");
        if (arrow) {
            subtitle_entry_t *entry = calloc(1, sizeof(subtitle_entry_t));
            if (!entry) break;

            char start_str[32] = {0};
            strncpy(start_str, line, arrow - line);
            start_str[arrow - line] = '\0';
            entry->start_time = ps5_subtitle_parse_time(start_str);

            char *end_start = arrow + 3;
            while (*end_start && isspace(*end_start)) end_start++;
            char *end_end = end_start;
            while (*end_end && *end_end != '\r' && *end_end != '\n') end_end++;
            char end_str[32] = {0};
            strncpy(end_str, end_start, end_end - end_start);
            entry->end_time = ps5_subtitle_parse_time(end_str);

            line = strtok(NULL, "\n");

            char text_buf[1024] = {0};
            int text_len = 0;
            while (line) {
                while (*line && isspace(*line)) line++;
                if (*line == '\0') break;
                if (isdigit(*line) && strstr(line, "-->") == NULL) break;

                int len = strlen(line);
                if (text_len + len + 1 < (int)sizeof(text_buf)) {
                    if (text_len > 0) {
                        text_buf[text_len++] = '\\';
                        text_buf[text_len++] = 'n';
                    }
                    memcpy(text_buf + text_len, line, len);
                    text_len += len;
                }
                line = strtok(NULL, "\n");
            }
            text_buf[text_len] = '\0';

            entry->text = strdup(text_buf);

            if (!track->entries) {
                track->entries = entry;
            } else {
                last_entry->next = entry;
            }
            last_entry = entry;
            track->entry_count++;
        }

        line = strtok(NULL, "\n");
    }

    free(content);
    fprintf(stderr, "[PS5 Subtitle] Parsed %d entries from %s\n",
            track->entry_count, path);
    return 0;
}

double ps5_subtitle_parse_time(const char *time_str) {
    if (!time_str) return 0;

    int h, m, s, ms;
    if (sscanf(time_str, "%d:%d:%d,%d", &h, &m, &s, &ms) == 4) {
        return h * 3600 + m * 60 + s + ms / 1000.0;
    }
    if (sscanf(time_str, "%d:%d:%d.%d", &h, &m, &s, &ms) == 4) {
        return h * 3600 + m * 60 + s + ms / 1000.0;
    }
    return 0;
}

int ps5_subtitle_load(const char *path, const char *language) {
    if (!path) return -1;

    int slot = -1;
    for (int i = 0; i < 8; i++) {
        if (!g_subtitles[i].filename) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        fprintf(stderr, "[PS5 Subtitle] No free track slots\n");
        return -1;
    }

    memset(&g_subtitles[slot], 0, sizeof(subtitle_track_t));
    g_subtitles[slot].filename = strdup(path);
    g_subtitles[slot].language = language ? strdup(language) : strdup("und");
    g_subtitles[slot].enabled = 0;

    const char *ext = strrchr(path, '.');
    if (ext && strcmp(ext, ".srt") == 0) {
        ps5_subtitle_parse_srt(path, &g_subtitles[slot]);
    } else if (ext && strcmp(ext, ".ass") == 0) {
        fprintf(stderr, "[PS5 Subtitle] ASS format not yet implemented\n");
    } else if (ext && (strcmp(ext, ".sub") == 0 || strcmp(ext, ".idx") == 0)) {
        fprintf(stderr, "[PS5 Subtitle] VOBsub format not yet implemented\n");
    }

    g_num_subtitle_tracks++;
    fprintf(stderr, "[PS5 Subtitle] Loaded track %d: %s (%s)\n",
            slot, path, g_subtitles[slot].language);

    if (g_active_subtitle < 0 && g_subtitles[slot].entry_count > 0) {
        g_subtitles[slot].enabled = 1;
        g_active_subtitle = slot;
    }

    return slot;
}

int ps5_subtitle_enable(int slot, int enabled) {
    if (slot < 0 || slot >= 8 || !g_subtitles[slot].filename) return -1;

    g_subtitles[slot].enabled = enabled;

    if (enabled) {
        for (int i = 0; i < 8; i++) {
            if (i != slot && g_subtitles[i].filename) {
                g_subtitles[i].enabled = 0;
            }
        }
        g_active_subtitle = slot;
    } else if (g_active_subtitle == slot) {
        g_active_subtitle = -1;
    }

    return 0;
}

const char* ps5_subtitle_get_text(double current_time) {
    if (g_active_subtitle < 0) return NULL;

    subtitle_track_t *track = &g_subtitles[g_active_subtitle];
    if (!track->enabled || !track->entries) return NULL;

    subtitle_entry_t *entry = track->entries;
    while (entry) {
        if (current_time >= entry->start_time && current_time <= entry->end_time) {
            return entry->text;
        }
        entry = entry->next;
    }

    return NULL;
}

int ps5_subtitle_count(void) {
    return g_num_subtitle_tracks;
}

const char* ps5_subtitle_get_language(int slot) {
    if (slot < 0 || slot >= 8) return NULL;
    return g_subtitles[slot].language;
}

int ps5_subtitle_is_enabled(int slot) {
    if (slot < 0 || slot >= 8) return 0;
    return g_subtitles[slot].enabled;
}

int ps5_subtitle_get_active(void) {
    return g_active_subtitle;
}

int ps5_subtitle_download_opensubtitles(const char *movie_hash, const char *language) {
    fprintf(stderr, "[PS5 Subtitle] OpenSubtitles download not yet implemented\n");
    return -1;
}

void ps5_subtitle_shutdown(void) {
    for (int i = 0; i < 8; i++) {
        if (g_subtitles[i].filename) {
            free(g_subtitles[i].filename);
            g_subtitles[i].filename = NULL;
        }
        if (g_subtitles[i].language) {
            free(g_subtitles[i].language);
            g_subtitles[i].language = NULL;
        }
        subtitle_entry_t *entry = g_subtitles[i].entries;
        while (entry) {
            subtitle_entry_t *next = entry->next;
            free(entry->text);
            free(entry);
            entry = next;
        }
        g_subtitles[i].entries = NULL;
        g_subtitles[i].entry_count = 0;
    }
    g_num_subtitle_tracks = 0;
    g_active_subtitle = -1;
    fprintf(stderr, "[PS5 Subtitle] Shutdown\n");
}
