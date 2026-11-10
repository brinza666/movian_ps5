/*
 * Movian PS5 - File System Implementation
 * PS5 file system access (app0, user data, USB, host0)
 */

#include "paw_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define PS5_APP0_PATH  "app0:/"
#define PS5_HOST0_PATH "host0:/"
#define PS5_USB_PATH   "usb0:/"

/* Check if a file or directory exists */
int ps5_fs_exists(const char *path) {
    if (!path) return 0;

    struct stat st;
    if (stat(path, &st) == 0) {
        return 1;
    }
    return 0;
}

/* Read a file into memory */
int ps5_fs_read(const char *path, void **buf, size_t *size) {
    if (!path || !buf || !size) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[PS5 FS] Cannot open %s for reading\n", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < 0) {
        fclose(f);
        return -1;
    }

    void *data = malloc(fsize);
    if (!data) {
        fclose(f);
        return -1;
    }

    size_t read = fread(data, 1, fsize, f);
    fclose(f);

    if (read != (size_t)fsize) {
        free(data);
        return -1;
    }

    *buf = data;
    *size = (size_t)fsize;
    return 0;
}

/* Write data to a file */
int ps5_fs_write(const char *path, const void *buf, size_t size) {
    if (!path || !buf || size == 0) return -1;

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[PS5 FS] Cannot open %s for writing\n", path);
        return -1;
    }

    size_t written = fwrite(buf, 1, size, f);
    fclose(f);

    if (written != size) {
        return -1;
    }

    return 0;
}

/* List directory contents */
int ps5_fs_listdir(const char *path, char ***entries, int *count) {
    if (!path || !entries || !count) return -1;

    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "[PS5 FS] Cannot open directory %s\n", path);
        return -1;
    }

    /* First pass: count entries */
    int n = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        n++;
    }
    rewinddir(dir);

    /* Allocate array */
    char **result = malloc((n + 1) * sizeof(char*));
    if (!result) {
        closedir(dir);
        return -1;
    }

    /* Second pass: copy names */
    int i = 0;
    while ((de = readdir(dir)) != NULL) {
        result[i] = strdup(de->d_name);
        if (!result[i]) {
            /* Cleanup on failure */
            for (int j = 0; j < i; j++) free(result[j]);
            free(result);
            closedir(dir);
            return -1;
        }
        i++;
    }
    result[i] = NULL;

    closedir(dir);

    *entries = result;
    *count = n;
    return 0;
}

/* Free directory listing */
void ps5_fs_listdir_free(char **entries, int count) {
    if (!entries) return;
    for (int i = 0; i < count; i++) {
        free(entries[i]);
    }
    free(entries);
}

/* Check if path is a directory */
int ps5_fs_is_directory(const char *path) {
    if (!path) return 0;

    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

/* Get file size */
long ps5_fs_get_size(const char *path) {
    if (!path) return -1;

    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return st.st_size;
}

/* Create directory */
int ps5_fs_mkdir(const char *path) {
    if (!path) return -1;
    return mkdir(path, 0755);
}

/* Remove file */
int ps5_fs_remove(const char *path) {
    if (!path) return -1;
    return remove(path);
}

/* Rename file */
int ps5_fs_rename(const char *old_path, const char *new_path) {
    if (!old_path || !new_path) return -1;
    return rename(old_path, new_path);
}
