#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>

#include "build.h"
#include "util.h"
#include "dbg.h"


static inline int digits(unsigned n) {
  return (int) log10(n) + 1;
}

int main(int argc, char *argv[]) {
    /* create .redo directory */
    if (mkdir(".redo/deps", 0744))
        if (errno != EEXIST) /* TODO: unsafe, dir could be a file or broken symlink */
            fatal(ERRM_MKDIR, ".redo");

    /* set REDO_ROOT */
    char *cwd = getcwd(NULL, 0);
    if (!cwd)
        fatal("redo: failed to obtain cwd");

    if (setenv("REDO_ROOT", cwd, 0))
        fatal("redo: failed to setenv %s to %s", "REDO_ROOT", cwd);

    free(cwd);

    srand(time(NULL)); /* TODO: error checking */
    unsigned magic = rand();

    char magic_str[digits(UINT_MAX) + 1];
    sprintf(magic_str, "%u", magic);

    debug("magic number: %s\n", magic_str);

    if (setenv("REDO_MAGIC", magic_str, 0))
        fatal("setenv()");

    if (argc < 2) {
        build_target("all");
    } else {
        int i;
        for (i = 1; i < argc; ++i) {
            build_target(argv[i]);
        }
    }
}
