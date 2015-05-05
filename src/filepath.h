/* filepath.h
 *
 * Copyright (c) 2014 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __FILEPATH_H__
#define __FILEPATH_H__

#include <stdbool.h>

extern bool is_absolute(const char* path);
extern char *remove_ext(const char *str);
extern char *take_extension(const char *target);
extern char *relpath(char *path, char *start);
extern char *xbasename(const char *path);
extern bool fexists(const char *target);
extern off_t fsize(const char *fn);
extern void mkpath(char *path, mode_t mode);

#endif
