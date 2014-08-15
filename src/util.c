/* util.c
 *
 * Copyright (c) 2014 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdarg.h>
#include <stdint.h>
#include <assert.h>

#include "util.h"
#define _FILENAME "util.c"
#include "dbg.h"


void *safe_malloc_(size_t size, const char *file, unsigned line) {
    assert(size > 0);
    void *ptr = malloc(size);
    if (!ptr)
        fatal_(file, line, ERRM_MALLOC, size);

    return ptr;
}

void *safe_realloc_(void *ptr, size_t size, const char *file, unsigned line) {
    assert(size > 0 && ptr);
    void *ptr2 = realloc(ptr, size);
    if (!ptr2)
        fatal_(file, line, ERRM_REALLOC, size);

    return ptr2;
}

char *safe_strdup_(const char *str, const char *file, unsigned line) {
    assert(str);
    size_t len = strlen(str) + 1;
    char *ptr = malloc(len);
    if (!ptr)
        fatal_(file, line, ERRM_MALLOC, len);

    return memcpy(ptr, str, len);
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
    char *result = safe_malloc(size);
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
