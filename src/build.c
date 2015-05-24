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

#define HEADERSIZE 60

typedef struct do_attr {
	char *specific;
	char *general;
	char *chosen;
} do_attr;

typedef struct dep_info {
	char *path;
	unsigned int magic;
	unsigned char hash[20];
	int32_t flags;
#define DEP_SOURCE (1 << 1)
} dep_info;

static do_attr *get_dofiles(const char *target);
static void free_do_attr(do_attr *thing);
static char **parse_shebang(char *target, char *dofile, char *temp_output);
static char **parsecmd(char *cmd, size_t *i, size_t keep_free);
static char *get_relpath(const char *target);
static char *get_dep_path(const char *target);
static void write_dep_info(dep_info *dep);
static int handle_c(const char *target);
static void hash_file(const char *target, unsigned char *hash);


/* Build given target, using it's do-file. */
int build_target(const char *target) {
	dep_info dep;
	int retval = 1;

	dep.path = get_dep_path(target);
	dep.magic = atoi(getenv("REDO_MAGIC"));
	dep.flags = 0;

	/* get the do-file which we are going to execute */
	do_attr *dofiles = get_dofiles(target);
	if (!dofiles->chosen) {
		if (fexists(target)) {
			/* if our target file has no do file associated but exists,
			   then we treat it as a source */
			dep.flags |= DEP_SOURCE;
			hash_file(target, dep.hash);
			write_dep_info(&dep);
			goto exit;
		}

		die("%s couldn't be built as no suitable do-file exists\n", target);
	}

	char *reltarget = get_relpath(target);
	printf("\033[32mredo  \033[1m\033[37m%s\033[0m\n", reltarget);
	free(reltarget);

	/* get the old hash (if any) */
	FILE *fp = fopen(dep.path, "rb");
	if (!fp) {
		if (errno != ENOENT)
			fatal("redo: failed to open %s\n", dep.path);
		memset(dep.hash, 0, 20); /* FIXME */
	} else {
		if (fseek(fp, sizeof(unsigned), SEEK_SET))
			fatal("redo: fseek() failed");
		if (fread(dep.hash, 1, 20, fp) < 20)
			fatal("redo: failed to read stuff");
		fclose(fp);
	}

	/* remove old dependency file */
	if (remove(dep.path))
		if (errno != ENOENT)
			fatal("redo: failed to remove %s", dep.path);

	char *temp_output = concat(2, target, ".redoing.tmp");

	pid_t pid = fork();
	if (pid == -1) {
		/* failure */
		fatal("redo: failed to fork() new process");
	} else if (pid == 0) {
		/* child */

		/* change directory to our target */
		char *dirc = xstrdup(dofiles->chosen);
		char *ddofile = dirname(dirc);
		if (chdir(ddofile) == -1)
			fatal("redo: failed to change directory to %s", ddofile);

		free(dirc);

		char **argv = parse_shebang(xbasename((char*)target),
				xbasename(dofiles->chosen), xbasename(temp_output));

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

		unsigned char new_hash[20];
		hash_file(target, new_hash);
		retval = memcmp(new_hash, dep.hash, 20);
		if (retval)
			memcpy(dep.hash, new_hash, 20);

		write_dep_info(&dep);
	} else {
		if (remove(temp_output))
			if (errno != ENOENT)
				fatal("redo: failed to remove %s", temp_output);
	}

	free(dep.path);

	/* depend on the do-file */
	dep.flags = 0;
	dep.path = get_dep_path(dofiles->chosen);
	if (!fexists(dep.path)) {
		hash_file(dofiles->chosen, dep.hash);
		write_dep_info(&dep);
	}
	add_dep(dofiles->chosen, target, 'c');

	/* redo-ifcreate on specific if general was chosen */
	if (dofiles->general == dofiles->chosen)
		add_dep(dofiles->specific, target, 'e');

	free(temp_output);
exit:
	free(dep.path);
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
static do_attr *get_dofiles(const char *target) {
	do_attr *dofiles = xmalloc(sizeof(do_attr));

	dofiles->specific = concat(2, target, ".do");
	dofiles->general = concat(3, "default", take_extension(target), ".do");

	if (fexists(dofiles->specific))
		dofiles->chosen = dofiles->specific;
	else if (fexists(dofiles->general))
		dofiles->chosen = dofiles->general;
	else
		dofiles->chosen = NULL;

	return dofiles;
}

/* Free the do_attr struct. */
static void free_do_attr(do_attr *thing) {
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

	char *path = xstrdup(relpath(abstarget, root));
	free(abstarget);
	return path;
}

/* Return the dependency file path of target. */
static char *get_dep_path(const char *target) {
	char *root = getenv("REDO_ROOT");
	char *reltarget = get_relpath(target);
	char *dep_path;

	if (is_absolute(reltarget)) {
		dep_path = concat(3, root, "/.redo/abs/", reltarget);
	} else {
		dep_path = concat(3, root, "/.redo/rel/", reltarget);
	}

	/* create directory */
	mkpath(dep_path, 0755); /* TODO: should probably be somewhere else */

	free(reltarget);
	return dep_path;
}

/* Declare that `parent` depends on `target`. */
void add_dep(const char *target, const char *parent, int ident) {
	char *dep_path = get_dep_path(parent);

	if (strchr(target, '\n'))
		fatal("Newlines in targets are not supported.");

	int fd = open(dep_path, O_WRONLY | O_APPEND);
	if (fd < 0) {
		if (errno != ENOENT)
			fatal("redo: failed to open %s", dep_path);

		/* no dependency record was found, so we create one */
		mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
		fd = open(dep_path, O_WRONLY | O_APPEND | O_CREAT, mode);
		if (fd < 0)
			fatal("redo: failed to open %s", dep_path);
	}

	char garbage[HEADERSIZE];
	memset(garbage, 'Z', HEADERSIZE);

	/* skip header */
	if (lseek(fd, 0, SEEK_END) < (off_t) HEADERSIZE)
		pwrite(fd, garbage, HEADERSIZE, 0);

	char *reltarget = get_relpath(target);
	int bufsize = strlen(reltarget) + 3;
	char *buf = xmalloc(bufsize);
	buf[0] = ident;
	buf[1] = '\t';
	strcpy(buf+2, reltarget);
	buf[bufsize-1] = '\n';
	if (write(fd, buf, bufsize) < bufsize)
		fatal("redo: failed to write to %s", dep_path);

	if (close(fd))
		fatal("redo: failed to close %s", dep_path);

	free(buf);
	free(reltarget);
	free(dep_path);
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

void sha1_to_hex(const unsigned char *sha1, char *buf) {
	static const char hex[] = "0123456789abcdef";

	for (int i = 0; i < 20; ++i) {
		char *pos = buf + i*2;
		*pos++ = hex[sha1[i] >> 4];
		*pos = hex[sha1[i] & 0xf];
	}
}

/* Write the dependency information into the specified path. */
static void write_dep_info(dep_info *dep) {
	mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
	int out = open(dep->path, O_WRONLY | O_CREAT, mode);
	if (out < 0)
		fatal("redo: failed to open %s", dep->path);

	char buf[60];
	sprintf(buf, "%010u", dep->magic);
	buf[10] = '\t';
	sha1_to_hex(dep->hash, buf+11);
	buf[51] = '\t';
	memset(buf+52, '-', 7);
	buf[59] = '\n';

	if (write(out, buf, sizeof buf) < (ssize_t) sizeof buf)
		fatal("redo: failed to write dependency info to '%s'", dep->path);

	if (close(out))
		fatal("redo: failed to close %s", dep->path);
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

	char buf[FILENAME_MAX];

	if (fread(buf, 1, HEADERSIZE, fp) < HEADERSIZE)
		fatal("redo: failed to read %zu bytes from %s", HEADERSIZE, dep_path);

	free(dep_path);

	errno = 0;
	buf[10] = '\0';
	long magic = strtol(buf, NULL, 10);
	if (errno)
		return build_target(target);

	if (!fexists(target)) {
		if (buf[52] == 'S') /* source flag set */
			/* target is a source and must not be rebuild */
			return 1;
		else
			return build_target(target);
	}

	if (magic == (unsigned) atoi(getenv("REDO_MAGIC")))
		/* magic number matches */
		return 1;

	unsigned char hash[20];
	char char_hash[40];

	hash_file(target, hash);
	sha1_to_hex(hash, char_hash);
	buf[51] = '\0';
	if (memcmp(char_hash, buf+11, 40))
		return build_target(target);

	char *ptr;
	char *root = getenv("REDO_ROOT");
	bool rebuild = false;

	while (!feof(fp)) {
		ptr = buf;

		size_t read = fread(buf, 1, sizeof buf, fp);
		if (ferror(fp))
			fatal("redo: failed to read %zu bytes from descriptor", sizeof buf);

		for (size_t i = 0; i < read; ++i) {
			if (buf[i] != '\n')
				continue;
			buf[i] = '\0';
			if (!is_absolute(&ptr[2])) {
				/* if our path is relative we need to prefix it with the
				   root project directory or the path will be invalid */
				char *abs = concat(3, root, "/", &ptr[2]);
				if (update_target(abs, ptr[0]))
					rebuild = true;

				free(abs);
			} else {
				if (update_target(&ptr[2], ptr[0]))
					rebuild = true;
			}
			ptr = &buf[i+1];
		}

		if (read && buf[read-1] != '\n') {
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
