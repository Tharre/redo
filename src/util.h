#ifndef __RUTIL_H__
#define __RUTIL_H__

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "../config.h"

#define safe_malloc(size) safe_malloc_(size, __FILENAME__, __LINE__)
#define safe_realloc(ptr, size) safe_realloc_(ptr, size, __FILENAME__, __LINE__)
#define safe_fopen(path, mode) safe_fopen_(path, mode, __FILENAME__, __LINE__)

extern void *safe_malloc_(size_t size, const char *file, unsigned int line);
extern void *safe_realloc_(void *ptr, size_t size, const char *file, unsigned int line);
extern FILE *safe_fopen_(const char *path, const char *mode,
                         const char *file, unsigned int line);
extern char *concat(size_t count, ...);
extern char *ec_strdup(const char* str);
extern char *strdup(const char *str);
extern char *xbasename(const char *path);

#endif
