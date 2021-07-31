/* filepath.c
 *
 * Copyright (c) 2014-2017 Tharre
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

#if defined (_WIN32)
	#define UNI_mkdir(path, mode) mkdir(path)
#else
	#define UNI_mkdir(path, mode) mkdir(path, mode)
#endif


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

/* Create the target directory recursively much like `mkdir -p` */
void mkpath(char *path, mode_t mode) {
	assert(path && *path);
	char *p;

	for (p=strchr(path+1, '/'); p; p=strchr(p+1, '/')) {
		*p = '\0';
		if (mkdir(path, mode) == -1 && errno != EEXIST)
			fatal("redo: failed to mkdir() '%s'", path);

		*p = '/';
	}
}

/* Make path absolute by prepending root, if path isn't already absolute. */
char *make_abs(char *root, char *path) {
	if (!is_absolute(path))
		return concat(root, "/", path);
	else
		return xstrdup(path);
}
