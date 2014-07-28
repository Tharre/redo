#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libgen.h> /* dirname(), basename() */

#include "build.h"
#include "util.h"
#include "filepath.h"
#define _FILENAME "build.c"
#include "dbg.h"

/* TODO: more useful return codes? */

const char do_file_ext[] = ".do";
const char default_name[] = "default";
const char temp_ext[] = ".redoing.tmp";

int build_target(const char *target) {
    assert(target);

    int retval = 0;
    printf("redo  %s\n", target);

    /* get the do-file which we are going to execute */
    char *do_file = get_do_file(target);
    if (!do_file) {
        if (fexists(target)) {
            /* if our target file has no do file associated but exists,
               then we treat it as a source */
            /* TODO: write dependencies */
            goto exit;
        }
        log_err("%s couldn't be built because no "
                "suitable do-file exists\n", target);
        retval = 1;
        goto exit;
    }

    debug("Using do-file %s\n", do_file);

    char *temp_output = concat(2, target, temp_ext);

    pid_t pid = fork();
    if (pid == -1) {
        /* failure */
        fatal(ERRM_FORK);
    } else if (pid == 0) {
        /* child */

        /* change directory to our target */
        char *dirc = safe_strdup(target);
        char *dtarget = dirname(dirc);
        if (chdir(dtarget) == -1)
            fatal(ERRM_CHDIR, dtarget);

        free(dirc);

        /* target is now in the cwd so change path accordingly */
        char *btarget = xbasename(target);
        char *bdo_file = xbasename(do_file);
        char *btemp_output = xbasename(temp_output);

        char **argv = parse_shebang(btarget, bdo_file, btemp_output);

        /* excelp() has nearly everything we want: automatic parsing of the
           shebang line through execve() and fallback to /bin/sh if no valid
           shebang could be found. However, it fails if the target doesn't have
           the executeable bit set, which is something we don't want. For this
           reason we parse the shebang line ourselves. */
        execv(argv[0], argv);

        /* execv should never return */
        fatal(ERRM_EXEC, argv[0]);
    }

    /* parent */
    int status;
    waitpid(pid, &status, 0);
    bool remove_temp = true;

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status)) { // TODO: check that
            log_err("redo: invoked do-file %s failed: %d\n",
                    do_file, WEXITSTATUS(status));
        } else {
            /* successful */

            /* if the file is 0 bytes long we delete it */
            if (fsize(temp_output) > 0)
                remove_temp = false;
        }
    } else {
        /* something very wrong happened with the child */
        log_err("redo: invoked do-file did not terminate correctly\n");
    }

    if (remove_temp) {
        if (remove(temp_output))
            if (errno != ENOENT)
                fatal(ERRM_REMOVE, temp_output);
    } else {
        if (rename(temp_output, target))
            fatal(ERRM_RENAME, temp_output, target);
    }

    free(temp_output);
 exit:
    free(do_file);

    return retval;
}

/* Read and parse shebang and return an argv-like pointer array containing the
   arguments. If no valid shebang could be found, assume "/bin/sh -e" instead */
char **parse_shebang(char *target, char *dofile, char *temp_output) {
    FILE *fp = fopen(dofile, "rb+");
    if (!fp)
        fatal(ERRM_FOPEN, dofile)

    const size_t bufsize = 1024;
    char buf[bufsize];

    buf[ fread(buf, 1, sizeof(buf)-1, fp) ] = '\0';
    if (ferror(fp))
        fatal(ERRM_FREAD, dofile);

    fclose(fp);

    char **argv;
    size_t i = 0;
    if (buf[0] == '#' && buf[1] == '!') {
        argv = parsecmd(&buf[2], &i, 5);
    } else {
        argv = safe_malloc(7 * sizeof(char*));
        argv[i++] = "/bin/sh";
        argv[i++] = "-e";
    }

    argv[i++] = dofile;
    argv[i++] = (char*) target;
    char *basename = remove_ext(target);
    argv[i++] = basename;
    argv[i++] = temp_output;
    argv[i] = NULL;

    return argv;
}

/* Returns the right do-file for target */
char *get_do_file(const char *target) {
    assert(target);
    /* target + ".do" */
    char *temp = concat(2, target, do_file_ext);
    if (fexists(temp))
        return temp;
    free(temp);

    /* default + get_extension(target) + ".do" */
    temp = concat(3, default_name, take_extension(target), do_file_ext);
    if (fexists(temp))
        return temp;
    free(temp);

    /* Redofile */
    temp = safe_strdup("Redofile");
    if (fexists(temp))
        return temp;
    free(temp);

    return NULL;
}

/* Breaks cmd at spaces and stores a pointer to each argument in the returned
   array. The index i is incremented to point to the next free pointer. The
   returned array is guaranteed to have at least keep_free entries left */
char **parsecmd(char *cmd, size_t *i, size_t keep_free) {
    assert(cmd);
    size_t argv_len = 16;
    char **argv = safe_malloc(argv_len * sizeof(char*));
    size_t j = 0;
    bool prev_space = true;
    for (;; ++j) {
        switch (cmd[j]) {
        case ' ':
            cmd[j] = '\0';
            prev_space = true;
            break;
        case '\n':
        case '\r':
            cmd[j] = '\0';
        case '\0':
            return argv;
        default:
            if (!prev_space)
                break;
            /* check if we have enough space */
            while (*i+keep_free >= argv_len) {
                argv_len *= 2;
                debug("Reallocating memory (now %zu elements)\n", argv_len);
                argv = safe_realloc(argv, argv_len * sizeof(char*));
            }

            prev_space = false;
            argv[*i] = &cmd[j];
            ++*i;
        }
    }
}
