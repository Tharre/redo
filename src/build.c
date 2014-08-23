/* build.c
 *
 * Copyright (c) 2014 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

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

#define HASHSIZE 20

static struct do_attr *get_dofiles(const char *target);
static void free_do_attr(struct do_attr *thing);
static char **parse_shebang(char *target, char *dofile, char *temp_output);
static char **parsecmd(char *cmd, size_t *i, size_t keep_free);
static char *get_relpath(const char *target);
static char *get_dep_path(const char *target);
static void write_dep_hash(const char *target);
static bool dependencies_changed(char buf[], size_t read);

struct do_attr {
    char *specific;
    char *general;
    char *redofile;
    char *chosen;
};


/* Build given target, using it's do-file. */
int build_target(const char *target) {
    /* get the do-file which we are going to execute */
    struct do_attr *dofiles = get_dofiles(target);
    if (!dofiles->chosen) {
        if (fexists(target)) {
            /* if our target file has no do file associated but exists,
               then we treat it as a source */
            write_dep_hash(target);
            free_do_attr(dofiles);
            return 0;
        }
        log_err("%s couldn't be built because no "
                "suitable do-file exists\n", target);
        free_do_attr(dofiles);
        return 1;
    }

    char *reltarget = get_relpath(target);
    printf("redo  %s\n", reltarget);
    free(reltarget);

    /* remove old dependency file */
    char *dep_file = get_dep_path(target);
    if (remove(dep_file))
        if (errno != ENOENT)
            fatal(ERRM_REMOVE, dep_file);
    free(dep_file);

    char *temp_output = concat(2, target, ".redoing.tmp");

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
        char *bdo_file = xbasename(dofiles->chosen);
        char *btemp_output = xbasename(temp_output);

        char **argv = parse_shebang(btarget, bdo_file, btemp_output);

        /* set "REDO_PARENT_TARGET" */
        if (setenv("REDO_PARENT_TARGET", target, 1))
            fatal(ERRM_SETENV, "REDO_PARENT_TARGET", target);

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
    if (waitpid(pid, &status, 0) == -1)
        fatal("waitpid() failed: ");
    bool remove_temp = true;

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status)) {
            log_err("redo: invoked do-file %s failed: %d\n",
                    dofiles->chosen, WEXITSTATUS(status));
            exit(EXIT_FAILURE);
        } else {
            /* successful */

            /* if the file is 0 bytes long we delete it */
            if (fsize(temp_output) > 0)
                remove_temp = false;
        }
    } else {
        /* something very wrong happened with the child */
        log_err("redo: invoked do-file did not terminate correctly\n");
        exit(EXIT_FAILURE);
    }

    /* depend on the do-file */
    add_dep(dofiles->chosen, target, 'c');
    write_dep_hash(dofiles->chosen);

    /* redo-ifcreate on specific if general was chosen */
    if (dofiles->general == dofiles->chosen)
        add_dep(dofiles->specific, target, 'e');

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
    free_do_attr(dofiles);

    return 0;
}

/* Read and parse shebang and return an argv-like pointer array containing the
   arguments. If no valid shebang could be found, assume "/bin/sh -e" instead. */
static char **parse_shebang(char *target, char *dofile, char *temp_output) {
    FILE *fp = fopen(dofile, "rb");
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

/* Return a struct with all the possible do-files, and the chosen one. */
static struct do_attr *get_dofiles(const char *target) {
    struct do_attr *dofiles = safe_malloc(sizeof(struct do_attr));

    dofiles->specific = concat(2, target, ".do");
    dofiles->general = concat(3, "default", take_extension(target), ".do");
    dofiles->redofile = safe_strdup("Redofile");

    if (fexists(dofiles->specific))
        dofiles->chosen = dofiles->specific;
    else if (fexists(dofiles->general))
        dofiles->chosen = dofiles->general;
    else if (fexists(dofiles->redofile))
        dofiles->chosen = dofiles->redofile;
    else
        dofiles->chosen = NULL;

    return dofiles;
}

/* Free the do_attr struct. */
static void free_do_attr(struct do_attr *thing) {
    assert(thing);
    free(thing->specific);
    free(thing->general);
    free(thing->redofile);
    free(thing);
}

/* Breaks cmd at spaces and stores a pointer to each argument in the returned
   array. The index i is incremented to point to the next free pointer. The
   returned array is guaranteed to have at least keep_free entries left. */
static char **parsecmd(char *cmd, size_t *i, size_t keep_free) {
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
   doesn't exist and allocates memory for the result itself. */
static char *xrealpath(const char *path) {
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

/* Return the relative path against "REDO_ROOT" of target. */
static char *get_relpath(const char *target) {
    assert(getenv("REDO_ROOT"));

    char *root = getenv("REDO_ROOT");
    char *abstarget = xrealpath(target);

    if (!abstarget)
        fatal(ERRM_REALPATH, target);

    char *relpath = safe_strdup(make_relative(root, abstarget));
    free(abstarget);
    return relpath;
}

/* Return the dependency file path of target. */
static char *get_dep_path(const char *target) {
    assert(getenv("REDO_ROOT"));

    char *root = getenv("REDO_ROOT");
    char *reltarget = get_relpath(target);
    char *safe_target = transform_path(reltarget);
    char *dep_path = concat(3, root, "/.redo/deps/", safe_target);

    free(reltarget);
    free(safe_target);
    return dep_path;
}

/* Add the dependency target, with the identifier ident. If parent is NULL, the
 * value of the environment variable REDO_PARENT will be taken instead. */
void add_dep(const char *target, const char *parent, int ident) {
    if (!parent) {
        assert(getenv("REDO_PARENT_TARGET"));
        parent = getenv("REDO_PARENT_TARGET");
    }

    char *dep_path = get_dep_path(parent);

    FILE *fp = fopen(dep_path, "rb+");
    if (!fp) {
        if (errno == ENOENT) {
            fp = fopen(dep_path, "w");
            if (!fp)
                fatal(ERRM_FOPEN, dep_path);
            /* skip the first n bytes that are reserved for the hash + magic number */
            fseek(fp, HASHSIZE + sizeof(unsigned int), SEEK_SET);
        } else {
            fatal(ERRM_FOPEN, dep_path);
        }
    } else {
        fseek(fp, 0L, SEEK_END);
    }

    char *reltarget = get_relpath(target);

    putc(ident, fp);
    fputs(reltarget, fp);
    putc('\0', fp);

    if (ferror(fp))
        fatal(ERRM_WRITE, dep_path);

    if (fclose(fp))
        fatal(ERRM_FCLOSE, dep_path);
    free(dep_path);
    free(reltarget);
}

/* Hash target, storing the result in hash. */
static void hash_file(const char *target, unsigned char (*hash)[HASHSIZE]) {
    FILE *in = fopen(target, "rb");
    if (!in)
        fatal(ERRM_FOPEN, target);

    SHA_CTX context;
    unsigned char data[8192];
    size_t read;

    SHA1_Init(&context);
    while ((read = fread(data, 1, sizeof data, in)))
        SHA1_Update(&context, data, read);

    if (ferror(in))
        fatal(ERRM_FREAD, target);
    SHA1_Final(*hash, &context);
    fclose(in);
}

/* Calculate and store the hash of target in the right dependency file. */
static void write_dep_hash(const char *target) {
    assert(getenv("REDO_MAGIC"));

    unsigned char hash[SHA_DIGEST_LENGTH];
    unsigned magic = atoi(getenv("REDO_MAGIC"));

    hash_file(target, &hash);

    char *dep_path = get_dep_path(target);
    int out = open(dep_path, O_WRONLY | O_CREAT, 0644);
    if (out < 0)
        fatal(ERRM_FOPEN, dep_path);

    if (write(out, &magic, sizeof(unsigned)) < (ssize_t) sizeof(unsigned))
        fatal("redo: failed to write magic number to '%s'", dep_path);

    if (write(out, hash, sizeof hash) < (ssize_t) sizeof hash)
        fatal("redo: failed to write hash to '%s'", dep_path);

    if (close(out))
        fatal(ERRM_FCLOSE, dep_path);
    free(dep_path);
}

/* Parse the dependency information from the dependency record and check if
   those are up-to-date. */
static bool dependencies_changed(char buf[], size_t read) {
    char *root = getenv("REDO_ROOT");
    char *ptr = buf;

    for (size_t i = 0; i < read; ++i) {
        if (!buf[i]) {
            if (is_absolute(&ptr[1])) {
                if (has_changed(&ptr[1], ptr[0], true))
                    return true;
            } else {
                char *abs = concat(3, root, "/", &ptr[1]);
                if (has_changed(abs, ptr[0], true)) {
                    free(abs);
                    return true;
                }
                free(abs);
            }
            ptr = &buf[i+1];
        }
    }
    return false;
}

/* Checks if a target should be rebuild, given it's identifier. */
bool has_changed(const char *target, int ident, bool is_sub_dependency) {
    switch(ident) {
    case 'a': return true;
    case 'e': return fexists(target);
    case 'c':
#define HEADERSIZE HASHSIZE + sizeof(unsigned)
        if (!fexists(target))
            return true;

        char *dep_path = get_dep_path(target);

        FILE *fp = fopen(dep_path, "rb");
        if (!fp) {
            if (errno == ENOENT) {
                /* dependency file does not exist */
                return true;
            } else {
                fatal(ERRM_FOPEN, dep_path);
            }
        }

        char buf[8096];
        if (fread(buf, 1, HEADERSIZE, fp) < HEADERSIZE)
            fatal("redo: failed to read %zu bytes from %s", HEADERSIZE, dep_path);

        free(dep_path);

        if (*(unsigned *) buf == (unsigned) atoi(getenv("REDO_MAGIC")))
            return is_sub_dependency;

        unsigned char hash[HASHSIZE];
        hash_file(target, &hash);
        if (memcmp(hash, buf+sizeof(unsigned), HASHSIZE)) {
            /*debug("Hash doesn't match for %s\n", target);*/
            return true;
        }

        while (!feof(fp)) {
            size_t read = fread(buf, 1, sizeof buf, fp);

            if (ferror(fp))
                fatal("redo: failed to read %zu bytes from file descriptor", sizeof buf);

            if (dependencies_changed(buf, read)) {
                fclose(fp);
                return true;
            }
        }

        fclose(fp);
        return false;

    default:
        log_err("Unknown identifier '%c'\n", ident);
        exit(EXIT_FAILURE);
    }
}
