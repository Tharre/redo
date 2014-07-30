#ifndef __FILEPATH_H__
#define __FILEPATH_H__

#include <stdbool.h>

extern bool is_absolute(const char* path);
extern char *remove_ext(const char *str);
extern char *take_extension(const char *target);
extern const char *make_relative(const char *target, const char *to);
extern char *transform_path(const char *target);
extern char *xbasename(const char *path);
extern bool fexists(const char *target);
extern off_t fsize(const char *fn);

#endif