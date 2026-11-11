/*
 * Movian for PS5 — POSIX time helpers FFmpeg 6.1 expects.
 * The payload SDK exports gmtime_s / localtime_s, not *_r.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <errno.h>
#include <string.h>
#include <time.h>

#ifndef MOVIAN_HOST_PREVIEW
int gmtime_s(struct tm *buf, const time_t *timer);
int localtime_s(struct tm *buf, const time_t *timer);
#endif

struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
    if (!timep || !result)
    {
        errno = EINVAL;
        return NULL;
    }
#ifndef MOVIAN_HOST_PREVIEW
    if (gmtime_s(result, timep) != 0)
        return NULL;
    return result;
#else
    struct tm *t = gmtime(timep);
    if (!t)
        return NULL;
    memcpy(result, t, sizeof *result);
    return result;
#endif
}

struct tm *localtime_r(const time_t *timep, struct tm *result)
{
    if (!timep || !result)
    {
        errno = EINVAL;
        return NULL;
    }
#ifndef MOVIAN_HOST_PREVIEW
    if (localtime_s(result, timep) != 0)
        return NULL;
    return result;
#else
    struct tm *t = localtime(timep);
    if (!t)
        return NULL;
    memcpy(result, t, sizeof *result);
    return result;
#endif
}
