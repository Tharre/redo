/* build.c
 *
 * Copyright (c) 2014-2017 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#define _XOPEN_SOURCE 700
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include <libgen.h> /* dirname(), basename() */

#include "sha1.h"
#include "build.h"
#include "util.h"
#include "filepath.h"
#include "DSV.h"
#define _FILENAME "build.c"
#include "dbg.h"

typedef struct do_attr {
	char *specific;
	char *general;
	char *chosen;
} do_attr;

typedef struct dep_info {
	const char *target;
	char *path;
	unsigned char *hash;
	struct timespec ctime;
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
static void write_dep_information(dep_info *dep);
static int handle_ident(dep_info *dep, int ident);
static int handle_c(dep_info *dep);
static void update_dep_info(dep_info *dep, const char *target);


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
			if (!dep->hash) /* if the hash was retrieved, ctime was too */
				update_dep_info(dep, dep->target);

			write_dep_information(dep);
			goto exit;
		}

		die("%s couldn't be built as no suitable .do script exists\n",
				dep->target);
	}

	char *reltarget = get_relpath(dep->target);
	if (!reltarget)
		fatal("redo: failed to get realpath() of %s", reltarget);

	printf("\033[32mredo  \033[1m\033[37m%s\033[0m\n", reltarget);
	free(reltarget);

	/* remove old dependency record */
	if (remove(dep->path) && errno != ENOENT)
		fatal("redo: failed to remove %s", dep->path);

	char *prereq = concat(2, dep->path, ".prereq");
	if (remove(prereq) && errno != ENOENT)
		fatal("redo: failed to remove %s", prereq);

	free(prereq);

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
		unsigned char *old_hash = dep->hash;
		update_dep_info(dep, dep->target);
		if (old_hash)
			retval = memcmp(dep->hash, old_hash, 20);

		free(old_hash);

		write_dep_information(dep);
	} else {
		if (remove(temp_output) && errno != ENOENT)
			fatal("redo: failed to remove %s", temp_output);
	}

	/* depend on the .do script */
	dep_info dep2 = {
		.target = dep->target,
		.path = get_dep_path(doscripts->chosen),
	};

	if (!dep2.path)
		fatal("redo: failed to get realpath() of %s", doscripts->chosen);

	if (!fexists(dep2.path)) {
		update_dep_info(&dep2, doscripts->chosen);
		write_dep_information(&dep2);
		free(dep2.hash);
	}
	free(dep2.path);

	add_prereq_path(doscripts->chosen, dep->target, 'c');

	/* redo-ifcreate on specific if general was chosen */
	if (doscripts->general == doscripts->chosen)
		add_prereq_path(doscripts->specific, dep->target, 'e');

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

	char *abstarget = NULL;
	if (absdir)
		abstarget = concat(3, absdir, "/", xbasename(path));

	free(dirc);
	free(absdir);
	return abstarget;
}

/* Return the relative path against "REDO_ROOT" of target. Returns NULL if
   realpath() fails. */
static char *get_relpath(const char *target) {
	char *root = getenv("REDO_ROOT");
	char *abstarget = xrealpath(target);

	if (!abstarget)
		return NULL;

	char *path = xstrdup(relpath(abstarget, root));
	free(abstarget);
	return path;
}

/* Return the dependency record path of target. */
static char *get_dep_path(const char *target) {
	char *root = getenv("REDO_ROOT");
	char *dep_path;
	char *reltarget = get_relpath(target);
	if (!reltarget)
		return NULL;

	char *redodir = is_absolute(reltarget) ? "/.redo/abs/" : "/.redo/rel/";
	dep_path = concat(3, root, redodir, reltarget);

	/* create directory */
	mkpath(dep_path, 0755); /* TODO: should probably be somewhere else */

	free(reltarget);
	return dep_path;
}

/* Declare that parent depends on target in the specific way ident.
 * Parent must be a path pointing to a (maybe non-existent) file in a valid,
 * exisiting directory. Target can be any string.
 * This relation is saved in the dependency store like this:
 * .redo/{abs,rel}/<parent>.prereq:
 *     <ident>:<target>
 */
void add_prereq(const char *target, const char *parent, int ident) {
	char *base_path = get_dep_path(parent);
	if (!base_path)
		fatal("redo: failed to get realpath() of %s", parent);

	char *dep_path = concat(2, base_path, ".prereq");

	int fd = open(dep_path, O_WRONLY | O_APPEND | O_CREAT, 0644);
	if (fd < 0)
		fatal("redo: failed to open %s", dep_path);

	size_t bufsize = strlen(target)*2 + 3;
	char *buf = xmalloc(bufsize);

	buf[0] = ident;
	buf[1] = ':';
	size_t encoded_len = encode_string(buf+2, target) + 3;
	buf[encoded_len-1] = '\n';

	if (write(fd, buf, encoded_len) < (ssize_t) encoded_len)
		fatal("redo: failed to write to %s", dep_path);

	if (close(fd))
		fatal("redo: failed to close %s", dep_path);

	free(buf);
	free(dep_path);
	free(base_path);
}

/* Works like add_prereq(), except that it uses the relative path of target to
 * REDO_ROOT instead of just the target. Target must be a path pointing to a
 * (maybe non-existant) file in a valid existing directory.
 */
void add_prereq_path(const char *target, const char *parent, int ident) {
	char *reltarget = get_relpath(target);
	if (!reltarget)
		fatal("redo: failed to get realpath() of %s", target);

	add_prereq(reltarget, parent, ident);
	free(reltarget);
}

/* Update hash & ctime information stored in the given dep_info struct */
static void update_dep_info(dep_info *dep, const char *target) {
	FILE *fp = fopen(target, "rb");
	if (!fp)
		fatal("redo: failed to open %s", target);

	dep->hash = hash_file(fp);
	struct stat st;
	if (fstat(fileno(fp), &st))
		fatal("redo: failed to aquire stat() %s", target);

	dep->ctime = st.st_ctim;
	fclose(fp);
}

/* Write the dependency information into the specified path. */
static void write_dep_information(dep_info *dep) {
	FILE *fd = fopen(dep->path, "w+");
	if (!fd)
		fatal("redo: failed to open %s", dep->path);

	char hash[41];
	sha1_to_hex(dep->hash, hash);
	char *flags = (dep->flags & DEP_SOURCE) ? "s" : "l";

	int magic = atoi(getenv("REDO_MAGIC"));

	/* TODO: casting time_t to long long is probably not entirely portable */
	if (fprintf(fd, "%s:%lld.%.9ld:%010d:%s\n", hash,
			(long long)dep->ctime.tv_sec, dep->ctime.tv_nsec, magic, flags) < 0)
		fatal("redo: failed to write to %s", dep->path);

	if (fclose(fd))
		fatal("redo: failed to close %s", dep->path);
}

int update_target(const char *target, int ident) {
	dep_info dep = {
		.target = target,
		.path = get_dep_path(target),
	};

	if (!dep.path)
		return 1;

	int retval = handle_ident(&dep, ident);
	free(dep.path);
	free(dep.hash);

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
	struct dsv_ctx ctx_dep, ctx_prereq;
	int retval = 0;

	/* check if the dependency record exists and is valid */
	FILE *depfd = fopen(dep->path, "rb");
	if (!depfd) {
		if (errno == ENOENT) {
			/* dependency record does not exist */
			log_warn("%s ood: dependency record doesn't exist\n", dep->target);
			return build_target(dep);
		} else {
			fatal("redo: failed to open %s", dep->path);
		}
	}

	dsv_init(&ctx_dep, 4);

	if (dsv_parse_file(&ctx_dep, depfd)) {
		/* parsing failed */
		log_info("%s ood: parsing of dependency file failed\n", dep->target);
		retval = build_target(dep);
		goto exit;
	}

	FILE *targetfd = fopen(dep->target, "rb");
	if (!targetfd) {
		if (errno != ENOENT) {
			fatal("redo: failed to open %s", dep->target);
		} else if (ctx_dep.fields[3][0] == 's') {
			/* target is a source and must not be rebuild */
			retval = 1;
			goto exit2;
		} else {
			log_info("%s ood: target file nonexistent\n", dep->target);
			retval = build_target(dep);
			goto exit2;
		}
	}

	struct stat curr_st;
	if (sscanf(ctx_dep.fields[1], "%lld.%ld", (long long*)&dep->ctime.tv_sec,
				&dep->ctime.tv_nsec) < 2) {
		/* ctime parsing failed */
		log_info("%s ood: ctime parsing failed\n", dep->target);
		retval = build_target(dep);
		goto exit3;
	}

	if (fstat(fileno(targetfd), &curr_st))
		fatal("redo: failed to stat() %s", dep->target);

	/* store the hash now, as build_target() will need it for comparison */
	unsigned char *old_hash = xmalloc(20);
	hex_to_sha1(ctx_dep.fields[0], old_hash); /* TODO: error checking */
	dep->hash = old_hash;

	if (dep->ctime.tv_sec != curr_st.st_ctim.tv_sec
			|| dep->ctime.tv_nsec != curr_st.st_ctim.tv_nsec) {
		/* ctime doesn't match */
		dep->ctime = curr_st.st_ctim;

		/* so check the hash */
		dep->hash = hash_file(targetfd);

		if (memcmp(old_hash, dep->hash, 20)) {
			/* target hash doesn't match */
			log_info("%s ood: hashes don't match\n", dep->target);
			free(old_hash);
			retval = build_target(dep);
			goto exit3;
		}
		free(old_hash);

		/* update ctime hash */
		write_dep_information(dep);
	}

	/* make sure all prereq dependencies are met */
	char *prereq_path = concat(2, dep->path, ".prereq");
	FILE *prereqfd = fopen(prereq_path, "rb");
	if (!prereqfd) {
		if (errno != ENOENT)
			fatal("redo: failed to open %s", prereq_path);

		/* no .prereq file exists; so we don't do anything */
		goto exit4;
	}

	dsv_init(&ctx_prereq, 2);

	while (!dsv_parse_file(&ctx_prereq, prereqfd)) {
		char *target = make_abs(getenv("REDO_ROOT"), ctx_prereq.fields[1]);
		int outofdate = update_target(target, ctx_prereq.fields[0][0]);

		free(target);
		free(ctx_prereq.fields[0]);
		free(ctx_prereq.fields[1]);

		if (outofdate) {
			log_info("%s ood: subtarget(s) ood\n", dep->target);
			retval = build_target(dep);
			break;
		}
	}

	dsv_free(&ctx_prereq);
	fclose(prereqfd);
exit4:
	free(prereq_path);
exit3:
	fclose(targetfd);
exit2:
	for (size_t i = 0; i < ctx_dep.fields_count; ++i)
		free(ctx_dep.fields[i]);
exit:
	dsv_free(&ctx_dep);
	fclose(depfd);
	return retval;
}
