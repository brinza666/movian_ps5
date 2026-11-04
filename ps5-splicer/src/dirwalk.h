#ifndef MOVIAN_DIRWALK_H
#define MOVIAN_DIRWALK_H

/* One directory level. fn returns non-zero to stop.
 * is_dir is 1 for a directory. *err is 0 when the directory opened,
 * otherwise errno. Returns 0 when it opened, -1 when it did not.
 */
int dir_walk(const char *path,
	     int (*fn)(const char *full, const char *name, int is_dir, void *ud),
	     void *ud, int *err);

#endif
