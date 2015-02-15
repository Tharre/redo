/* filepath.c
 *
 * Copyright (c) 2014 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "util.h"
#define _FILENAME "build.c"
#include "dbg.h"


/* Check if the given path is absolute. */
bool is_absolute(const char* path) {
	return path[0] == '/';
}

/* Returns a new copy of str with the extension removed, where the extension is
   everything behind the last dot, including the dot. */
char *remove_ext(const char *str) {
	assert(str);
	size_t len;
	char *ret, *dot = NULL;

	for (len = 0; str[len]; ++len)
		if (str[len] == '.')
			dot = (char*) &str[len];

	if (dot) /* recalculate length to only reach just before the last dot */
		len = dot - str;

	ret = xmalloc(len+1);
	memcpy(ret, str, len);
	ret[len] = '\0';

	return ret;
}

/* Returns the extension of the target or the empty string if none was found. */
char *take_extension(const char *target) {
	assert(target);
	char *temp = strrchr(target, '.');
	if (temp)
		return temp;
	else
		return "";
}

/* Returns a pointer to a relative path to `path` from `start`. This pointer may
   be pointing to either of it's arguments, or to the statically allocated
   string "." should both paths match. Both paths must be canonicalized.
   A few examples:
    relpath("/abc/de", "/abc/de") => "."
    relpath("some/path/a/b/c", "some/path") => "a/b/c"
    relpath("some/path", "some/path/a/b/c") => "some/path/a/b/c"
    relpath("/", "/") => "."
    relpath("/abc", "/") => "abc"
 */
char *relpath(char *path, char *start) {
	int i;
	for (i = 0; path[i] && start[i]; ++i) {
		if (path[i] != start[i])
			return path; /* paths share nothing */
	}

	if (!path[i] && start[i])
		return start; /* path is above start */

	if (!path[i] && !start[i])
		return "."; /* paths match completely */

	if (path[i] == '/')
		++i;

	return &path[i];
}

/* Transforms target into a safe filename, replacing all '/' with '!'. */
char *transform_path(const char *target) {
	char *ptr = (char*) target;
	size_t escape = 0, i = 0;
	while (*ptr++)
		if (*ptr == '!') escape++;

	ptr = xmalloc((ptr-target) + escape + 1);
	do {
		if (*target == '/')
			ptr[i++] = '!';
		else if (*target == '!') {
			ptr[i++] = '!';
			ptr[i++] = '!';
		} else
			ptr[i++] = *target;
	} while (*target++);

	ptr[i] = '\0';
	return ptr;
}

/* Sane and portable basename implementation. */
char *xbasename(const char *path) {
	assert(path);
	char *ptr = strrchr(path, '/');
	return ptr? ptr+1 : (char*) path;
}

/* Checks if target exists and prints a debug message if access() failed
   except if it failed with ENOENT. */
bool fexists(const char *target) {
	assert(target);
	if (!access(target, F_OK))
		return true;
	if (errno != ENOENT)
		debug("Failed to access %s: %s\n", target, strerror(errno));
	return false;
}

/* Returns the size of fn, or -1 if the file doesn't exist. */
off_t fsize(const char *fn) {
	struct stat st;
	if (stat(fn, &st)) {
		if (errno != ENOENT)
			fatal("redo: failed to aquire stat() information about %s", fn);
		return -1;
	}

	return st.st_size;
}

/* Create the directory dir, while removing other 'files' with the same name.
   Returns true if the directory had to be created or false if it existed */
// TODO: fix confusing name
bool mkdirp(const char *dir) {
	struct stat st;
	if (stat(dir, &st)) {
		/* dir doesn't exist or stat failed */
		if (errno != ENOENT)
			fatal("redo: failed to aquire stat() information about %s", dir);
		if (mkdir(dir, 0755))
			fatal("redo: failed to mkdir() '%s'", dir);
		return 1;
	} else {
		if (!S_ISDIR(st.st_mode)) {
			if (remove(dir))
				fatal("redo: failed to remove %s", dir);
			if (mkdir(dir, 0755))
				fatal("redo: failed to mkdir() '%s'", dir);
			return 1;
		}
		return 0;
	}
}
