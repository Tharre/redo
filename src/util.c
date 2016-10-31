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
#include "sha1.h"
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

/* Hash the target file, returning a pointer to the heap allocated hash. */
unsigned char *hash_file(FILE *fp) {
	unsigned char *hash = xmalloc(20);

	SHA_CTX context;
	unsigned char data[8192];
	size_t read;

	SHA1_Init(&context);
	while ((read = fread(data, 1, sizeof data, fp)))
		SHA1_Update(&context, data, read);

	if (ferror(fp))
		fatal("redo: failed to read data");
	SHA1_Final(hash, &context);

	return hash;
}

/* Requires a buffer of at least 20*2+1 = 41 bytes */
void sha1_to_hex(const unsigned char *sha1, char *buf) {
	static const char hex[] = "0123456789abcdef";

	for (int i = 0; i < 20; ++i) {
		char *pos = buf + i*2;
		*pos++ = hex[sha1[i] >> 4];
		*pos = hex[sha1[i] & 0xf];
	}

	buf[40] = '\0';
}

void hex_to_sha1(const char *s, unsigned char *sha1) {
	static const char hex[] = "0123456789abcdef";

	for (; *s; s += 2, ++sha1)
		*sha1 = ((strchr(hex, *s) - hex) << 4) + strchr(hex, *(s+1)) - hex;
}

