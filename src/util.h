/* util.h
 *
 * Copyright (c) 2014 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __RUTIL_H__
#define __RUTIL_H__

#include <stddef.h>

extern void __attribute__((noreturn)) die_(const char *err, ...);
extern void *xmalloc(size_t size);
extern void *xrealloc(void *ptr, size_t size);
extern char *xstrdup(const char *str);
extern char *concat(size_t count, ...);

#endif
