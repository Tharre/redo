#include <stdio.h>

#include "build.h"

int main(int argc, char *argv[]) {
	int i;
	for (i = 1; i < argc; ++i) {
		build_target(argv[i]);
	}
}
