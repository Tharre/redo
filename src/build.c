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

#include "sha1.h"
#include "build.h"
#include "util.h"
#include "filepath.h"
#define _FILENAME "build.c"
#include "dbg.h"

#define HASHSIZE 20
#define HEADERSIZE HASHSIZE + sizeof(unsigned int)

static struct do_attr *get_dofiles(const char *target);
static void free_do_attr(struct do_attr *thing);
static char **parse_shebang(char *target, char *dofile, char *temp_output);
static char **parsecmd(char *cmd, size_t *i, size_t keep_free);
static char *get_relpath(const char *target);
static char *get_dep_path(const char *target);
static int write_dep_hash(const char *target, unsigned char *old_hash);
static int handle_c(const char *target);

struct do_attr {
	char *specific;
	char *general;
	char *chosen;
};


/* Build given target, using it's do-file. */
int build_target(const char *target) {
	int retval = 1;
	/* get the do-file which we are going to execute */
	struct do_attr *dofiles = get_dofiles(target);
	if (!dofiles->chosen) {
		if (fexists(target)) {
			/* if our target file has no do file associated but exists,
			   then we treat it as a source */
			write_dep_hash(target, NULL);
			free_do_attr(dofiles);
			return retval;
		}

		die("%s couldn't be built as no suitable do-file exists\n", target);
	}

	char *reltarget = get_relpath(target);
	printf("redo  %s\n", reltarget);
	free(reltarget);

	/* get the old hash (if any) */
	unsigned char old_hash[HASHSIZE];
	char *dep_file = get_dep_path(target);
	FILE *fp = fopen(dep_file, "rb");
	if (!fp) {
		if (errno != ENOENT)
			fatal("redo: failed to open %s\n", dep_file);
		memset(old_hash, 0, HASHSIZE); // FIXME
	} else {
		if (fseek(fp, sizeof(unsigned), SEEK_SET))
			fatal("redo: fseek() failed");
		if (fread(old_hash, 1, HASHSIZE, fp) < HASHSIZE)
			fatal("redo: failed to read stuff");
		fclose(fp);
	}

	/* remove old dependency file */
	if (remove(dep_file))
		if (errno != ENOENT)
			fatal("redo: failed to remove %s", dep_file);
	free(dep_file);

	char *temp_output = concat(2, target, ".redoing.tmp");

	pid_t pid = fork();
	if (pid == -1) {
		/* failure */
		fatal("redo: failed to fork() new process");
	} else if (pid == 0) {
		/* child */

		/* change directory to our target */
		char *dirc = xstrdup(target);
		char *dtarget = dirname(dirc);
		if (chdir(dtarget) == -1)
			fatal("redo: failed to change directory to %s", dtarget);

		free(dirc);

		/* target is now in the cwd so change path accordingly */
		char *btarget = xbasename(target);
		char *bdo_file = xbasename(dofiles->chosen);
		char *btemp_output = xbasename(temp_output);

		char **argv = parse_shebang(btarget, bdo_file, btemp_output);

		/* set "REDO_PARENT_TARGET" */
		if (setenv("REDO_PARENT_TARGET", target, 1))
			fatal("redo: failed to setenv() REDO_PARENT_TARGET to %s", target);

		/* excelp() has nearly everything we want: automatic parsing of the
		   shebang line through execve() and fallback to /bin/sh if no valid
		   shebang could be found. However, it fails if the target doesn't have
		   the executeable bit set, which is something we don't want. For this
		   reason we parse the shebang line ourselves. */
		execv(argv[0], argv);

		/* execv should never return */
		fatal("redo: failed to replace child process with %s", argv[0]);
	}

	/* parent */
	int status;
	if (waitpid(pid, &status, 0) == -1)
		fatal("waitpid() failed: ");
	bool remove_temp = true;

	/* check how our child exited */
	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status))
			die("redo: invoked do-file %s failed: %d\n", dofiles->chosen,
			    WEXITSTATUS(status));
	} else {
		/* something very wrong happened with the child */
		die("redo: invoked do-file did not terminate correctly\n");
	}

	/* check if our output file is > 0 bytes long */
	if (fsize(temp_output) > 0) {
		if (rename(temp_output, target))
			fatal("redo: failed to rename %s to %s", temp_output, target);

		retval = write_dep_hash(target, old_hash);
	} else {
		if (remove(temp_output))
			if (errno != ENOENT)
				fatal("redo: failed to remove %s", temp_output);
	}

	/* depend on the do-file */
	char *temp = get_dep_path(dofiles->chosen);
	if (!fexists(temp))
		write_dep_hash(dofiles->chosen, NULL);

	free(temp);
	add_dep(dofiles->chosen, target, 'c');

	/* redo-ifcreate on specific if general was chosen */
	if (dofiles->general == dofiles->chosen)
		add_dep(dofiles->specific, target, 'e');

	free(temp_output);
	free_do_attr(dofiles);

	return retval;
}

/* Read and parse shebang and return an argv-like pointer array containing the
   arguments. If no valid shebang could be found assume "/bin/sh -e" instead. */
static char **parse_shebang(char *target, char *dofile, char *temp_output) {
	FILE *fp = fopen(dofile, "rb");
	if (!fp)
		fatal("redo: failed to open %s", dofile);

	char buf[1024];

	buf[ fread(buf, 1, sizeof(buf)-1, fp) ] = '\0';
	if (ferror(fp))
		fatal("redo: failed to read from %s", dofile);

	fclose(fp);

	char **argv;
	size_t i = 0;
	if (buf[0] == '#' && buf[1] == '!') {
		argv = parsecmd(&buf[2], &i, 5);
	} else {
		argv = xmalloc(7 * sizeof(char*));
		argv[i++] = "/bin/sh";
		argv[i++] = "-e";
	}

	argv[i++] = dofile;
	argv[i++] = target;
	char *basename = remove_ext(target);
	argv[i++] = basename;
	argv[i++] = temp_output;
	argv[i] = NULL;

	return argv;
}

/* Breaks cmd at spaces and stores a pointer to each argument in the returned
   array. The index i is incremented to point to the next free pointer. The
   returned array is guaranteed to have at least keep_free entries left. */
static char **parsecmd(char *cmd, size_t *i, size_t keep_free) {
	size_t argv_len = 16;
	char **argv = xmalloc(argv_len * sizeof(char*));
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
				argv = xrealloc(argv, argv_len * sizeof(char*));
			}

			prev_space = false;
			argv[*i] = &cmd[j];
			++*i;
		}
	}
}

/* Return a struct with all the possible do-files, and the chosen one. */
static struct do_attr *get_dofiles(const char *target) {
	struct do_attr *dofiles = xmalloc(sizeof(struct do_attr));

	dofiles->specific = concat(2, target, ".do");
	if (!is_absolute(target)) {
		dofiles->general = concat(3, "default", take_extension(target), ".do");
	} else {
		char *dirc = xstrdup(target);
		char *dt = dirname(dirc);

		dofiles->general = concat(4, dt, "/default", take_extension(target), ".do");
		free(dirc);
	}

	if (fexists(dofiles->specific))
		dofiles->chosen = dofiles->specific;
	else if (fexists(dofiles->general))
		dofiles->chosen = dofiles->general;
	else
		dofiles->chosen = NULL;

	return dofiles;
}
/* Free the do_attr struct. */
static void free_do_attr(struct do_attr *thing) {
	free(thing->specific);
	free(thing->general);
	free(thing);
}

/* Custom version of realpath that doesn't fail if the last part of path
   doesn't exist and allocates memory for the result itself. */
static char *xrealpath(const char *path) {
	char *dirc = xstrdup(path);
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
	char *root = getenv("REDO_ROOT");
	char *abstarget = xrealpath(target);

	if (!abstarget)
		fatal("redo: failed to get realpath() of %s", target);

	char *relpath = xstrdup(make_relative(root, abstarget));
	free(abstarget);
	return relpath;
}

/* Return the dependency file path of target. */
static char *get_dep_path(const char *target) {
	char *root = getenv("REDO_ROOT");
	char *reltarget = get_relpath(target);
	char *xtarget = transform_path(reltarget);
	char *dep_path = concat(3, root, "/.redo/deps/", xtarget);

	free(reltarget);
	free(xtarget);
	return dep_path;
}

/* Add the dependency target, with the identifier ident. If parent is NULL, the
 * value of the environment variable REDO_PARENT will be taken instead. */
void add_dep(const char *target, const char *parent, int ident) {
	if (!parent)
		parent = getenv("REDO_PARENT_TARGET");

	char *dep_path = get_dep_path(parent);

	FILE *fp = fopen(dep_path, "rb+");
	if (!fp) {
		if (errno == ENOENT) {
			fp = fopen(dep_path, "w");
			if (!fp)
				fatal("redo: failed to open %s", dep_path);
			/* skip the first n bytes that are reserved for the header */
			fseek(fp, HEADERSIZE, SEEK_SET);
		} else {
			fatal("redo: failed to open %s", dep_path);
		}
	} else {
		fseek(fp, 0L, SEEK_END);
	}

	char *reltarget = get_relpath(target);

	putc(ident, fp);
	fputs(reltarget, fp);
	putc('\0', fp);

	if (ferror(fp))
		fatal("redo: failed to write to %s", dep_path);

	if (fclose(fp))
		fatal("redo: failed to close %s", dep_path);
	free(dep_path);
	free(reltarget);
}

/* Hash target, storing the result in hash. */
static void hash_file(const char *target, unsigned char *hash) {
	FILE *in = fopen(target, "rb");
	if (!in)
		fatal("redo: failed to open %s", target);

	SHA_CTX context;
	unsigned char data[8192];
	size_t read;

	SHA1_Init(&context);
	while ((read = fread(data, 1, sizeof data, in)))
		SHA1_Update(&context, data, read);

	if (ferror(in))
		fatal("redo: failed to read from %s", target);
	SHA1_Final(hash, &context);
	fclose(in);
}

/* Calculate and store the hash of target in the right dependency file. Returns
   a nonzero value if the hash changed or zero otherwise. */
static int write_dep_hash(const char *target, unsigned char *old_hash) {
	unsigned char hash[HASHSIZE];
	unsigned magic = atoi(getenv("REDO_MAGIC"));

	hash_file(target, hash);

	char *dep_path = get_dep_path(target);

	int out = open(dep_path, O_WRONLY | O_CREAT, 0644);
	if (out < 0)
		fatal("redo: failed to open %s", dep_path);

	if (write(out, &magic, sizeof(unsigned)) < (ssize_t) sizeof(unsigned))
		fatal("redo: failed to write magic number to '%s'", dep_path);

	if (write(out, hash, sizeof hash) < (ssize_t) sizeof hash)
		fatal("redo: failed to write hash to '%s'", dep_path);

	if (close(out))
		fatal("redo: failed to close %s", dep_path);

	free(dep_path);

	if (old_hash)
		return memcmp(hash, old_hash, HASHSIZE);
	else
		return 1;

}

int update_target(const char *target, int ident) {
	switch(ident) {
	case 'a':
		return build_target(target);
	case 'e':
		if (fexists(target))
			return build_target(target);

		return 0;
	case 'c':
		return handle_c(target);
	default:
		die("redo: unknown identifier '%c'\n", ident);
	}
}

static int handle_c(const char *target) {
	if (!fexists(target))
		return build_target(target);

	char *dep_path = get_dep_path(target);

	FILE *fp = fopen(dep_path, "rb");
	if (!fp) {
		if (errno == ENOENT) {
			/* dependency file does not exist */
			free(dep_path);
			return build_target(target);
		} else {
			fatal("redo: failed to open %s", dep_path);
		}
	}

	char buf[8096 > FILENAME_MAX+3 ? 8096 : FILENAME_MAX*2];

	if (fread(buf, 1, HEADERSIZE, fp) < HEADERSIZE)
		fatal("redo: failed to read %zu bytes from %s", HEADERSIZE, dep_path);

	free(dep_path);

	if (*(unsigned *) buf == (unsigned) atoi(getenv("REDO_MAGIC")))
		/* magic number matches */
		return 1;

	char *root = getenv("REDO_ROOT");
	bool rebuild = false;
	unsigned char hash[HASHSIZE];
	hash_file(target, hash);
	if (memcmp(hash, buf+sizeof(unsigned), HASHSIZE))
		return build_target(target);

	char *ptr;

	while (!feof(fp)) {
		ptr = buf;

		size_t read = fread(buf, 1, sizeof buf, fp);
		if (ferror(fp))
			fatal("redo: failed to read %zu bytes from descriptor", sizeof buf);

		for (size_t i = 0; i < read; ++i) {
			if (buf[i])
				continue;
			if (!is_absolute(&ptr[1])) {
				/* if our path is relative we need to prefix it with the
				   root project directory or the path will be invalid */
				char *abs = concat(3, root, "/", &ptr[1]);
				if (update_target(abs, ptr[0]))
					rebuild = true;

				free(abs);
			} else {
				if (update_target(&ptr[1], ptr[0]))
					rebuild = true;
			}
			ptr = &buf[i+1];
		}

		if (read && buf[read-1]) {
			if (buf != ptr)
				memmove(buf, ptr, buf-ptr + sizeof buf);
			else
				die("redo: dependency file contains insanely long paths\n");
		}
	}

	fclose(fp);
	if (rebuild)
		return build_target(target);

	return 0;
}
