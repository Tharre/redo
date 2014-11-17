#!/bin/sh -ex

. out/config.sh
$CC $CFLAGS -o out/util.o -c src/util.c
$CC $CFLAGS -o out/build.o -c src/build.c
$CC $CFLAGS -o out/filepath.o -c src/filepath.c
$CC $CFLAGS -o out/sha1.o -c src/sha1.c
$CC $CFLAGS -o out/redo.o -c src/redo.c
$CC -o out/redo out/redo.o out/util.o out/build.o out/filepath.o out/sha1.o $LDFLAGS

# TODO: just for convenience, should be removed as soon as redo can build itself
sudo install out/redo /usr/bin
sudo ln -sf /usr/bin/redo /usr/bin/redo-ifchange
sudo ln -sf /usr/bin/redo /usr/bin/redo-ifcreate
sudo ln -sf /usr/bin/redo /usr/bin/redo-always

echo "Finished compiling"
