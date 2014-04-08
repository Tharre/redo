#ifndef __RUTIL_H__
#define __RUTIL_H__

#include <stdlib.h>
#include "../config.h"

extern char *concat(size_t count, ...);
extern void *ec_malloc(size_t size);
extern char *ec_strdup(const char* str);
extern char *strdup(const char *str);
extern char *xbasename(const char *path);

#endif
