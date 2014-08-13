#include "build.h"

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        build_target(argv[i]);
        add_dep(argv[i], 'c');
    }
}
