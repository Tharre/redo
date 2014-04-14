#include <stdio.h>

#include "build.h"
#include "util.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        build_target("all");
    } else {
        int i;
        for (i = 1; i < argc; ++i) {
            build_target(argv[i]);
        }
    }
}
