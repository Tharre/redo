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
	const char *target;
	char *path;
	unsigned char *old_hash;
	unsigned char *new_hash;
	unsigned int magic;
	int32_t flags;
#define DEP_SOURCE (1 << 1)
} dep_info;

static do_attr *get_doscripts(const char *target);
static void free_do_attr(do_attr *thing);
static char **parse_shebang(char *target, char *doscript, char *temp_output);
static char **parsecmd(char *cmd, size_t *i, size_t keep_free);
static char *get_relpath(const char *target);
static char *xrealpath(const char *path);
static char *get_dep_path(const char *target);
static void write_dep_header(dep_info *dep);
static int handle_ident(dep_info *dep, int ident);
static int handle_c(dep_info *dep);
static unsigned char *hash_file(const char *target);


/* Build given target, using it's .do script. */
static int build_target(dep_info *dep) {
	int retval = 1;

	/* get the .do script which we are going to execute */
	do_attr *doscripts = get_doscripts(dep->target);
	if (!doscripts->chosen) {
		if (fexists(dep->target)) {
			/* if our target file has no .do script associated but exists,
			   then we treat it as a source */
			dep->flags |= DEP_SOURCE;
			if (!dep->new_hash)
				dep->new_hash = hash_file(dep->target);

			write_dep_header(dep);
			goto exit;
		}

		die("%s couldn't be built as no suitable .do script exists\n",
				dep->target);
	}

	char *reltarget = get_relpath(dep->target);
	printf("\033[32mredo  \033[1m\033[37m%s\033[0m\n", reltarget);
	free(reltarget);

	/* remove old dependency record */
	if (remove(dep->path) && errno != ENOENT)
		fatal("redo: failed to remove %s", dep->path);

	char *temp_output = concat(2, dep->target, ".redoing.tmp");

	pid_t pid = fork();
	if (pid == -1) {
		/* failure */
		fatal("redo: failed to fork() new process");
	} else if (pid == 0) {
		/* child */

		char *abstemp = xrealpath(temp_output);
		if (!abstemp)
			fatal("redo: failed to get realpath() of %s", temp_output);

		/* change directory to our target */
		char *dirc = xstrdup(doscripts->chosen);
		char *ddoscript = dirname(dirc);
		if (chdir(ddoscript) == -1)
			fatal("redo: failed to change directory to %s", ddoscript);

		free(dirc);

		char **argv = parse_shebang(xbasename(dep->target),
				xbasename(doscripts->chosen), abstemp);

		/* set "REDO_PARENT_TARGET" */
		if (setenv("REDO_PARENT_TARGET", dep->target, 1))
			fatal("redo: failed to setenv() REDO_PARENT_TARGET to %s",
					dep->target);

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
		fatal("redo: waitpid() failed");

	/* check how our child exited */
	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status))
			die("redo: invoked .do script %s failed: %d\n", doscripts->chosen,
			    WEXITSTATUS(status));
	} else {
		/* something very wrong happened with the child */
		die("redo: invoked .do script did not terminate correctly\n");
	}

	/* check if our output file is > 0 bytes long */
	if (fsize(temp_output) > 0) {
		if (rename(temp_output, dep->target))
			fatal("redo: failed to rename %s to %s", temp_output, dep->target);

		/* recalculate hash after successful build */
		free(dep->new_hash);
		dep->new_hash = hash_file(dep->target);
		if (dep->old_hash)
			retval = memcmp(dep->new_hash, dep->old_hash, 20);

		write_dep_header(dep);
	} else {
		if (remove(temp_output) && errno != ENOENT)
			fatal("redo: failed to remove %s", temp_output);
	}

	/* depend on the .do script */
	dep_info dep2 = {
		.magic = dep->magic,
		.target = dep->target,
		.path = get_dep_path(doscripts->chosen),
	};

	if (!fexists(dep2.path)) {
		dep2.new_hash = hash_file(doscripts->chosen);
		write_dep_header(&dep2);
		free(dep2.new_hash);
	}
	free(dep2.path);

	add_dep(doscripts->chosen, dep->target, 'c');

	/* redo-ifcreate on specific if general was chosen */
	if (doscripts->general == doscripts->chosen)
		add_dep(doscripts->specific, dep->target, 'e');

	free(temp_output);
exit:
	free_do_attr(doscripts);

	return retval;
}

/* Read and parse shebang and return an argv-like pointer array containing the
   arguments. If no valid shebang could be found assume "/bin/sh -e" instead. */
static char **parse_shebang(char *target, char *doscript, char *temp_output) {
	FILE *fp = fopen(doscript, "rb");
	if (!fp)
		fatal("redo: failed to open %s", doscript);

	char *buf = xmalloc(1024);

	buf[ fread(buf, 1, 1023, fp) ] = '\0';
	if (ferror(fp))
		fatal("redo: failed to read from %s", doscript);

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

	argv[i++] = doscript;
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

/* Return a struct with all the possible .do scripts, and the chosen one. */
static do_attr *get_doscripts(const char *target) {
	do_attr *ds = xmalloc(sizeof(do_attr));

	ds->specific = concat(2, target, ".do");
	char *dirc = xstrdup(target);
	char *dt = dirname(dirc);

	ds->general = concat(4, dt, "/default", take_extension(target), ".do");
	free(dirc);

	if (fexists(ds->specific))
		ds->chosen = ds->specific;
	else if (fexists(ds->general))
		ds->chosen = ds->general;
	else
		ds->chosen = NULL;

	return ds;
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

/* Return the dependency record path of target. */
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
		die("redo: newlines in targets are not supported\n");

	int fd = open(dep_path, O_WRONLY | O_APPEND);
	if (fd < 0) {
		if (errno != ENOENT)
			fatal("redo: failed to open %s", dep_path);

		/* no dependency record was found, so we create one */
		fd = open(dep_path, O_WRONLY | O_APPEND | O_CREAT, 0644);
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

/* Hash the target file, returning a pointer to the heap allocated hash. */
static unsigned char *hash_file(const char *target) {
	FILE *in = fopen(target, "rb");
	if (!in)
		fatal("redo: failed to open %s", target);

	unsigned char *hash = xmalloc(20);

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

	return hash;
}

static void sha1_to_hex(const unsigned char *sha1, char *buf) {
	static const char hex[] = "0123456789abcdef";

	for (int i = 0; i < 20; ++i) {
		char *pos = buf + i*2;
		*pos++ = hex[sha1[i] >> 4];
		*pos = hex[sha1[i] & 0xf];
	}
}

static void hex_to_sha1(const char *s, unsigned char *sha1) {
	static const char hex[] = "0123456789abcdef";

	for (; *s; s += 2, ++sha1)
		*sha1 = ((strchr(hex, *s) - hex) << 4) + strchr(hex, *(s+1)) - hex;
}

/* Write the dependency information into the specified path. */
static void write_dep_header(dep_info *dep) {
	mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
	int out = open(dep->path, O_WRONLY | O_CREAT, mode);
	if (out < 0)
		fatal("redo: failed to open %s", dep->path);

	char buf[60];
	sprintf(buf, "%010u", dep->magic);
	buf[10] = '\t';
	sha1_to_hex(dep->new_hash, buf+11);
	buf[51] = '\t';
	memset(buf+52, '-', 7);
	if (dep->flags & DEP_SOURCE)
		buf[52] = 'S';
	buf[59] = '\n';

	if (write(out, buf, sizeof buf) < (ssize_t) sizeof buf)
		fatal("redo: failed to write dependency record to '%s'", dep->path);

	if (close(out))
		fatal("redo: failed to close %s", dep->path);
}

int update_target(const char *target, int ident) {
	dep_info dep = {
		.magic = atoi(getenv("REDO_MAGIC")),
		.target = target,
		.path = get_dep_path(target),
	};

	int retval = handle_ident(&dep, ident);
	free(dep.path);
	free(dep.old_hash);
	free(dep.new_hash);

	return retval;
}

static int handle_ident(dep_info *dep, int ident) {
	switch(ident) {
	case 'a':
		return build_target(dep);
	case 'e':
		if (fexists(dep->target))
			return build_target(dep);

		return 0;
	case 'c':
		return handle_c(dep);
	default:
		die("redo: unknown identifier '%c'\n", ident);
	}
}

static int handle_c(dep_info *dep) {
	FILE *fp = fopen(dep->path, "rb");
	if (!fp) {
		if (errno == ENOENT)
			/* dependency record does not exist */
			return build_target(dep);
		else
			fatal("redo: failed to open %s", dep->path);
	}

	char buf[FILENAME_MAX];

	if (fread(buf, 1, HEADERSIZE, fp) < HEADERSIZE)
		fatal("redo: failed to read %zu bytes from %s", HEADERSIZE, dep->path);

	errno = 0;
	buf[10] = '\0';
	long magic = strtol(buf, NULL, 10);
	if (errno)
		return build_target(dep);

	if (!fexists(dep->target)) {
		if (buf[52] == 'S') /* source flag set */
			/* target is a source and must not be rebuild */
			return 1;
		else
			return build_target(dep);
	}

	if (magic == dep->magic)
		/* magic number matches */
		return 1;

	dep->old_hash = xmalloc(20);
	buf[51] = '\0';
	hex_to_sha1(buf+11, dep->old_hash); /* TODO: error checking */
	dep->new_hash = hash_file(dep->target);

	if (memcmp(dep->old_hash, dep->new_hash, 20))
		return build_target(dep);

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
				die("redo: dependency record contains insanely long paths\n");
		}
	}

	fclose(fp);
	if (rebuild)
		return build_target(dep);

	return 0;
}
