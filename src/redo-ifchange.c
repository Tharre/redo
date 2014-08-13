#include <stdio.h>
#include <stdbool.h>

#include "build.h"
#include "dbg.h"

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        /*debug("Testing if %s is up-to-date ...\n", argv[i]);*/
        if (has_changed(argv[i], 'c', false)) {
            /*printf("=> no\n");*/
            build_target(argv[i]);
        } else {
            /*printf("=> yes\n");*/
        }
        add_dep(argv[i], 'c');
    }
}
