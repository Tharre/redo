/* dbg.h
 *
 * Copyright (c) 2014 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __DBG_H__
#define __DBG_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#ifndef _FILENAME
#define _FILENAME __FILE__
#endif

/* helper functions which help in replacing the GNU extension ##__VA_ARGS__ */
#define STRINGIFY(x) #x
#define LOG_HELPER(f,l,...) fprintf(stderr, "("f":"STRINGIFY(l)"): "__VA_ARGS__)
#define FATAL_HELPER(M, ...) log_err(M ": %s\n", __VA_ARGS__)
#define FATAL_HELPER_(f,l,M,...) \
    fprintf(stderr, "(%s:%u): " M ": %s\n", f, l, __VA_ARGS__)

#ifdef NDEBUG
#define debug(...)
#else
#define debug(...) log_err(__VA_ARGS__)
#endif

#define log_err(...) LOG_HELPER(_FILENAME, __LINE__, __VA_ARGS__)
#define fatal(...) \
    {FATAL_HELPER(__VA_ARGS__, strerror(errno)); exit(EXIT_FAILURE);}
#define fatal_(f,l,...) \
    {FATAL_HELPER_(f, l, __VA_ARGS__, strerror(errno)); exit(EXIT_FAILURE);}

#define assert_str_equal(a,b) ({ \
    if (strcmp(a, b)) { \
        log_err("Assertion error: '%s' == '%s'\n", a, b); \
        abort(); \
    } \
})

#define assert_int_equal(a,b) ({ \
    if (a != b) { \
        log_err("Assertion error: '%d' == '%d'\n", a, b); \
        abort(); \
    } \
})

/* A neat macro that silences unused parameter warnings compiler independant */
#define UNUSED(x) (void)(x)

#endif
