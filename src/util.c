/* util.c
 *
 * Copyright (c) 2014 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#define _XOPEN_SOURCE 600
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#define _FILENAME "util.c"
#include "dbg.h"


/* Print a given formated error message and die. */
extern void __attribute__((noreturn)) die_(const char *err, ...) {
	assert(err);
	va_list ap;
	va_start(ap, err);

	vfprintf(stderr, err, ap);

	va_end(ap);
	exit(EXIT_FAILURE);
}

void *xmalloc(size_t size) {
	assert(size > 0);
	void *ptr = malloc(size);
	if (!ptr)
		fatal("Cannot allocate %zu bytes", size);

	return ptr;
}

void *xrealloc(void *ptr, size_t size) {
	assert(size > 0 && ptr);
	if (!(ptr = realloc(ptr, size)))
		fatal("Cannot reallocate %zu bytes", size);

	return ptr;
}

char *xstrdup(const char *str) {
	assert(str);
	if (!(str = strdup(str)))
		fatal("Insufficient memory for string allocation");

	return (char*) str;
}

/* For concating multiple strings into a single larger one. */
char *concat(size_t count, ...) {
	assert(count > 0);
	va_list ap, ap2;
	va_start(ap, count);
	va_copy(ap2, ap);
	size_t i, size = 0, args_len[count];
	for (i = 0; i < count; ++i) {
		args_len[i] = strlen(va_arg(ap, char*));
		size += args_len[i];
	}
	++size;
	char *result = xmalloc(size);
	/* debug("Allocated %zu bytes at %p\n", size, result); */
	uintptr_t offset = 0;
	for (i = 0; i < count; ++i) {
		strcpy(&result[offset], va_arg(ap2, char*));
		offset += args_len[i];
	}
	va_end(ap);
	va_end(ap2);
	return result;
}
