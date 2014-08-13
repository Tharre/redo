#ifndef __RBUILD_H__
#define __RBUILD_H__

#include <stdbool.h>
#include <stddef.h>

char *realpath(const char *path, char *resolved_path);
int setenv(const char *name, const char *value, int overwrite);
extern char *get_do_file(const char *target);
extern int build_target(const char *target);
extern char **parse_shebang(char *target, char *dofile, char *temp_output);
extern char **parsecmd(char *cmd, size_t *i, size_t keep_free);
extern char *get_relpath(const char *target);
extern char *get_dep_path(const char *target);
extern void add_dep(const char *target, int indent);
extern void write_dep_hash(const char *target);
extern  bool has_changed(const char *target, int ident, bool is_sub_dependency);
extern bool dependencies_changed(char buf[], size_t read);


#endif
