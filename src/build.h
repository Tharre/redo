#ifndef __RBUILD_H__
#define __RBUILD_H__

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

extern char *get_do_file(const char *target);
extern int build_target(const char *target);
extern bool fexists(const char *target);
extern char *take_extension(const char *target);
extern char *remove_ext(const char *str);
extern char **parsecmd(char *cmd, size_t *i, size_t keep_free);
extern off_t fsize(const char *fn);

#endif
