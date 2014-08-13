#ifndef __RBUILD_H__
#define __RBUILD_H__

#include <stdbool.h>

extern void add_dep(const char *target, int indent);
extern bool has_changed(const char *target, int ident, bool is_sub_dependency);
extern int build_target(const char *target);

#endif
