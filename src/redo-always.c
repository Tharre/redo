/* redo-always.c
 *
 * Copyright (c) 2014 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "build.h"

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        build_target(argv[i]);
        add_dep(argv[i], 'c');
    }
}
