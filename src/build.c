#define _XOPEN_SOURCE 600
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libgen.h> /* dirname(), basename() */
#include <openssl/sha.h>

#include "build.h"
#include "util.h"
#include "filepath.h"
#define _FILENAME "build.c"
#include "dbg.h"


const char do_file_ext[] = ".do";
const char default_name[] = "default";
const char temp_ext[] = ".redoing.tmp";
const char deps_dir[] = "/.redo/deps/";
const char redo_root[] = "REDO_ROOT";
const char redo_parent_target[] = "REDO_PARENT_TARGET";

/* TODO: more useful return codes? */
int build_target(const char *target) {
    assert(target);

    int retval = 0;

    /* get the do-file which we are going to execute */
    char *do_file = get_do_file(target);
    if (!do_file) {
        if (fexists(target)) {
            /* if our target file has no do file associated but exists,
               then we treat it as a source */
            write_dep_hash(target);
            goto exit;
        }
        log_err("%s couldn't be built because no "
                "suitable do-file exists\n", target);
        retval = 1;
        goto exit;
    }

    printf("redo  %s\n", target);
    /*debug("Using do-file %s\n", do_file);*/

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

        /* set REDO_PARENT_TARGET */
        if (setenv(redo_parent_target, target, 1))
            fatal(ERRM_SETENV, redo_parent_target, target);

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

        write_dep_hash(target);
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

    char buf[1024];

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

/* Custom version of realpath that doesn't fail if the last part of path
   doesn't exit and allocates memory for the result itself */
char *xrealpath(const char *path) {
    char *dirc = safe_strdup(path);
    char *dname = dirname(dirc);
    char *absdir = realpath(dname, NULL);
    if (!absdir)
        return NULL;
    char *abstarget = concat(3, absdir, "/", xbasename(path));

    free(dirc);
    free(absdir);
    return abstarget;
}

/* Return the relative path against REDO_ROOT of target */
char *get_relpath(const char *target) {
    assert(getenv(redo_root));
    assert(target);

    char *root = getenv(redo_root);
    char *abstarget = xrealpath(target);

    if (!abstarget)
        fatal(ERRM_REALPATH, target);

    char *relpath = safe_strdup(make_relative(root, abstarget));
    free(abstarget);
    return relpath;
}

/* Return the dependency file path of target */
char *get_dep_path(const char *target) {
    assert(target);
    assert(getenv(redo_root));

    char *root = getenv(redo_root);
    char *reltarget = get_relpath(target);
    char *safe_target = transform_path(reltarget);
    char *dep_path = concat(3, root, deps_dir, safe_target);

    free(reltarget);
    free(safe_target);
    return dep_path;
}

/* Add the dependency target, with the identifier indent */
void add_dep(const char *target, int indent) {
    assert(target);
    assert(getenv(redo_parent_target));

    char *parent_target = getenv(redo_parent_target);
    char *dep_path = get_dep_path(parent_target);

    FILE *fp = fopen(dep_path, "rb+");
    if (!fp) {
        if (errno == ENOENT) {
            fp = fopen(dep_path, "w");
            if (!fp)
                fatal(ERRM_FOPEN, dep_path);
            /* skip the first n bytes that are reserved for the hash + magic number */
            fseek(fp, 20 + sizeof(unsigned int), SEEK_SET);
        } else {
            fatal(ERRM_FOPEN, dep_path);
        }
    } else {
        fseek(fp, 0L, SEEK_END);
    }

    char *reltarget = get_relpath(target);

    putc(indent, fp);
    fputs(reltarget, fp);
    putc('\0', fp);

    if (ferror(fp))
        fatal(ERRM_WRITE, dep_path);

    if (fclose(fp))
        fatal(ERRM_FCLOSE, dep_path);
    free(dep_path);
    free(reltarget);
}

/* Hash target, storing the result in hash */
void hash_file(const char *target, unsigned char (*hash)[20]) {
    FILE *in = fopen(target, "rb");
    if (!in)
        fatal(ERRM_FOPEN, target);

    SHA_CTX context;
    unsigned char data[8192];
    size_t read;

    SHA1_Init(&context);
    while ((read = fread(data, 1, sizeof data, in)))
        SHA1_Update(&context, data, read);
    SHA1_Final(*hash, &context);
    if (fclose(in))
        fatal(ERRM_FCLOSE, target);
}

/* Calculate and store the hash of target in the right dependency file */
void write_dep_hash(const char *target) {
    assert(getenv("REDO_MAGIC"));
    unsigned char hash[SHA_DIGEST_LENGTH];
    int magic = atoi(getenv("REDO_MAGIC"));

    hash_file(target, &hash);

    char *dep_path = get_dep_path(target);
    int out = open(dep_path, O_WRONLY | O_CREAT, 0644);
    if (out < 0)
        fatal("redo: failed to open() '%s'", dep_path);

    if (write(out, hash, sizeof hash) < (ssize_t) sizeof hash)
        fatal("redo: failed to write hash to '%s'", dep_path);

    if (write(out, &magic, sizeof(unsigned int)) < (ssize_t) sizeof(unsigned int))
        fatal("redo: failed to write magic number to '%s'", dep_path);

    if (close(out))
        fatal("redo: failed to close file descriptor.");
    free(dep_path);
}
