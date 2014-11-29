/* redo.c
 *
 * Copyright (c) 2014 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>

#include "build.h"
#include "util.h"
#include "dbg.h"
#include "filepath.h"


/* Returns the amount of digits a number n has in decimal. */
static inline unsigned digits(unsigned n) {
	return n ? 1 + digits(n/10) : n;
}

void prepare_env() {
	if (getenv("REDO_ROOT") && getenv("REDO_PARENT_TARGET")
	    && getenv("REDO_MAGIC"))
		return;

	/* create the dependency store if it doesn't already exist */
	if (mkdirp(".redo") && mkdirp(".redo/deps"))
		fprintf(stderr, "redo: creating dependency store ...\n");

	/* set REDO_ROOT */
	char *cwd = getcwd(NULL, 0);
	if (!cwd)
		fatal("redo: failed to obtain cwd");
	if (setenv("REDO_ROOT", cwd, 0))
		fatal("redo: failed to setenv() REDO_ROOT to %s", cwd);
	free(cwd);

	/* set REDO_MAGIC */
	srand(time(NULL));
	char magic_str[digits(UINT_MAX) + 1];
	sprintf(magic_str, "%u", rand());
	if (setenv("REDO_MAGIC", magic_str, 0))
		fatal("redo: failed to setenv() REDO_MAGIC to %s", magic_str);
}

int main(int argc, char *argv[]) {
	prepare_env();
	char *argv_base = xbasename(argv[0]);

	if (!strcmp(argv_base, "redo")) {
		if (argc < 2) {
			update_target("all", 'a');
		} else {
			for (int i = 1; i < argc; ++i)
				update_target(argv[i], 'a');
		}
		return EXIT_SUCCESS;
	} else {
		char ident;
		if      (!strcmp(argv_base, "redo-ifchange"))
			ident = 'c';
		else if (!strcmp(argv_base, "redo-ifcreate"))
			ident = 'e';
		else if (!strcmp(argv_base, "redo-always"))
			ident = 'a';
		else
			die("redo: argv set to unkown value\n");

		for (int i = 1; i < argc; ++i) {
			update_target(argv[i], ident);
			add_dep(argv[i], NULL, ident);
		}

		return EXIT_SUCCESS;
	}
}
