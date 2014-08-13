#!/bin/sh -ex

. out/config.sh
$CC $CFLAGS -o out/util.o -c src/util.c
$CC $CFLAGS -o out/build.o -c src/build.c
$CC $CFLAGS -o out/filepath.o -c src/filepath.c
$CC $CFLAGS -o out/redo.o -c src/redo.c
$CC $CFLAGS -o out/redo-ifchange.o -c src/redo-ifchange.c
$CC -o out/redo out/redo.o out/util.o out/build.o out/filepath.o $LDFLAGS
$CC -o out/redo-ifchange out/redo-ifchange.o out/util.o out/build.o out/filepath.o $LDFLAGS

# TODO: just for convenience, should be removed as soon as redo can build itself
sudo install out/redo out/redo-ifchange /usr/bin/

echo "Finished compiling"
