#!/bin/sh -ex

export SH_BUILD=1

if [ -f "./config.local" ]; then
	. ./config.local
fi
. out/config.sh
$CC $CFLAGS -o out/util.o -c src/util.c
$CC $CFLAGS -o out/build.o -c src/build.c
$CC $CFLAGS -o out/filepath.o -c src/filepath.c
$CC $CFLAGS -o out/sha1.o -c src/sha1.c
$CC $CFLAGS -o out/DSV.o -c src/DSV.c
$CC $CFLAGS -o out/redo.o -c src/redo.c
$CC -o out/redo out/redo.o out/util.o out/build.o out/filepath.o out/sha1.o \
       out/DSV.o $LDFLAGS

# TODO: just for convenience, should be removed as soon as redo can build itself
sudo install out/redo /usr/local/bin
sudo ln -sf /usr/local/bin/redo /usr/local/bin/redo-ifchange
sudo ln -sf /usr/local/bin/redo /usr/local/bin/redo-ifcreate
sudo ln -sf /usr/local/bin/redo /usr/local/bin/redo-always

echo "Finished compiling"
