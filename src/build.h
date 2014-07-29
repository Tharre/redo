#ifndef __RBUILD_H__
#define __RBUILD_H__

#include <stdbool.h>
#include <stddef.h>

extern char *get_do_file(const char *target);
extern int build_target(const char *target);
extern char **parse_shebang(char *target, char *dofile, char *temp_output);
extern char **parsecmd(char *cmd, size_t *i, size_t keep_free);

#endif
