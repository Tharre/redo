#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "build.h"
#include "util.h"
#include "dbg.h"

/* TODO: for some reason this header is not included */
int setenv(const char *name, const char *value, int overwrite);

int main(int argc, char *argv[]) {
    /* create .redo directory */
    if (mkdir(".redo", 0744))
        fatal(ERRM_MKDIR, ".redo");

    /* set REDO_ROOT */
    char *cwd = getcwd(NULL, 0);
    if (!cwd)
        fatal("redo: failed to obtain cwd");

    if (setenv("REDO_ROOT", cwd, 0))
        fatal("redo: failed to setenv %s to %s", "REDO_ROOT", cwd);

    free(cwd);

    if (argc < 2) {
        build_target("all");
    } else {
        int i;
        for (i = 1; i < argc; ++i) {
            build_target(argv[i]);
        }
    }
}
