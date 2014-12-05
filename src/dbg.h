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
#define PREFIX(...) PREFIX_HELPER(_FILENAME, __LINE__, __VA_ARGS__)
#define SUFFIX(S, M, ...) M S, __VA_ARGS__

#define log_err(...) fprintf(stderr, PREFIX(__VA_ARGS__))
#define die(...) die_(PREFIX(__VA_ARGS__))
#define fatal(...) die(SUFFIX(": %s\n", __VA_ARGS__, strerror(errno)))

#ifdef NDEBUG
#define PREFIX_HELPER(f,l,...) __VA_ARGS__
#else
#define PREFIX_HELPER(f,l,...) "(" f ":" STRINGIFY(l) "): " __VA_ARGS__
#endif

#ifdef NDEBUG
#define debug(...)
#else
#define debug(...) log_err(__VA_ARGS__)
#endif

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
