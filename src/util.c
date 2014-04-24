#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>

#include "util.h"
#define _FILENAME "util.c"
#include "dbg.h"

#include "../config.h"

/* A safe malloc wrapper. */
void *safe_malloc_(size_t size, const char *file, unsigned int line) {
    void *ptr = malloc(size);

    if (!ptr)
        fatal_(file, line, "redo: cannot allocate %zu bytes", size);

    return ptr;
}

void *safe_realloc_(void *ptr, size_t size, const char *file, unsigned int line) {
    void *ptr2 = realloc(ptr, size);
    if (!ptr2)
        fatal_(file, line, "redo: cannot reallocate %zu bytes", size);

    return ptr2;
}

FILE *safe_fopen_(const char *path, const char *mode, const char *file,
                  unsigned int line) {
    FILE *temp = fopen(path, mode);
    if (!temp)
        fatal_(file, line, "redo: failed to open %s", path);

    return temp;
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
    size++;
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

char *xbasename(const char *path) {
    assert(path);
    char *ptr = strrchr(path, '/');
    return ptr? ptr+1 : (char*) path;
}

/* TODO: REIMPLEMENT */
char *ec_strdup(const char* str) {
    assert(str);
    size_t len = strlen(str) + 1;
    char *ptr = malloc(len);
    if (!ptr)
        fatal("redo: failed to duplicate string");

    return memcpy(ptr, str, len);;
}
