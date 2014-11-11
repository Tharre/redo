/* build.h
 *
 * Copyright (c) 2014 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __RBUILD_H__
#define __RBUILD_H__

#include <stdbool.h>

extern void add_dep(const char *target, const char *parent, int ident);
extern bool has_changed(const char *target, int ident, bool is_sub_dependency);
extern int build_target(const char *target);
extern bool environment_sane();

#endif
