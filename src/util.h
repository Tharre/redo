#ifndef __RUTIL_H__
#define __RUTIL_H__

#include <stdbool.h>
#include <stddef.h>

/* standard error messages */
#define _PROGNAME "redo"

#define ERRM_MALLOC _PROGNAME": cannot allocate %zu bytes"
#define ERRM_REALLOC _PROGNAME": cannot reallocate %zu bytes"
#define ERRM_FOPEN _PROGNAME": failed to open %s"
#define ERRM_FREAD _PROGNAME": failed to read from %s"
#define ERRM_FCLOSE _PROGNAME": failed to close %s"
#define ERRM_WRITE _PROGNAME": failed to write to %s"
#define ERRM_CHDIR _PROGNAME": failed to change directory to %s"
#define ERRM_SETENV _PROGNAME": failed to setenv %s to %s"
#define ERRM_EXEC _PROGNAME": failed to replace child process with %s"
#define ERRM_REMOVE _PROGNAME": failed to remove %s"
#define ERRM_RENAME _PROGNAME": failed to rename %s to %s"
#define ERRM_FORK _PROGNAME": failed to fork() new process"
#define ERRM_REALPATH _PROGNAME": failed to get realpath() of %s"
#define ERRM_STAT _PROGNAME": failed to aquire stat() information about %s"
#define ERRM_MKDIR _PROGNAME": failed to mkdir() '%s'"

#define safe_malloc(size) safe_malloc_(size, _FILENAME, __LINE__)
#define safe_realloc(ptr, size) safe_realloc_(ptr, size, _FILENAME, __LINE__)
#define safe_strdup(str) safe_strdup_(str, _FILENAME, __LINE__)

extern void *safe_malloc_(size_t size, const char *file, unsigned line);
extern void *safe_realloc_(void *ptr, size_t size, const char *file, unsigned line);
extern char *safe_strdup_(const char *str, const char *file, unsigned line);
extern char *concat(size_t count, ...);

#endif
