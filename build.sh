#!/bin/sh -e

. out/config.sh
$CC $CFLAGS -o out/util.o -c src/util.c
$CC $CFLAGS -o out/build.o -c src/build.c
$CC $CFLAGS -o out/redo.o -c src/redo.c
$CC $CFLAGS -o out/redo-ifchange.o -c src/redo-ifchange.c
$CC -o out/redo out/redo.o out/util.o out/build.o $LDFLAGS
$CC -o out/redo-ifchange out/redo.o out/util.o out/build.o $LDFLAGS

echo "Finished compiling"
