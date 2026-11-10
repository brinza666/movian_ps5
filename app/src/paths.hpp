/*
 * Movian for PS5 — filesystem layout.
 *
 * On the console: /app0 is the title image, /data is console storage,
 * /download0 is this title's persistent sandbox (when downloadDataSize > 0).
 * Host preview/simulator builds remap those prefixes into a work folder.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifdef MOVIAN_HOST_PREVIEW
#define MOVIAN_APP0 "app0"
#define MOVIAN_DATA "data"
#define MOVIAN_DOWNLOAD0 "download0"
#else
#define MOVIAN_APP0 "/app0"
#define MOVIAN_DATA "/data"
#define MOVIAN_DOWNLOAD0 "/download0"
#endif

#define MOVIAN_DATA_DIR MOVIAN_DATA "/movian"
#define MOVIAN_LOG_PATH MOVIAN_DOWNLOAD0 "/movian-ps5.log"
#define MOVIAN_SETTINGS_PATH MOVIAN_DOWNLOAD0 "/settings.txt"
