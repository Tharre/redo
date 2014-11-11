/* redo-ifchange.c
 *
 * Copyright (c) 2014 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdbool.h>

#include "build.h"
#include "dbg.h"

int main(int argc, char *argv[]) {
    if (!environment_sane()) {
        fprintf(stderr, "redo: environment variables are missing, \
                         please use %s only in do scripts.\n", argv[0]);
        exit(1);
    }

    for (int i = 1; i < argc; ++i) {
        /*debug("Testing if %s is up-to-date ...\n", argv[i]);*/
        if (has_changed(argv[i], 'c', false)) {
            /*printf("=> no\n");*/
            build_target(argv[i]);
        } else {
            /*printf("=> yes\n");*/
        }
        add_dep(argv[i], NULL, 'c');
    }

    return 0;
}
