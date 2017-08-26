#!/bin/sh -e
# Copyright (c) 2016 Tharre
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

(
export BOOTSTRAP_BUILD=1

[ -f "./config.local" ] && . ./config.local

. out/config.sh
$CC $CFLAGS -o out/util.o -c src/util.c
$CC $CFLAGS -o out/build.o -c src/build.c
$CC $CFLAGS -o out/filepath.o -c src/filepath.c
$CC $CFLAGS -o out/sha1.o -c src/sha1.c
$CC $CFLAGS -o out/DSV.o -c src/DSV.c
$CC $CFLAGS -o out/redo.o -c src/redo.c
$CC $CFLAGS -o out/pcg.o -c src/pcg.c
$CC -o out/redo out/redo.o out/util.o out/build.o out/filepath.o out/sha1.o \
       out/DSV.o out/pcg.o $LDFLAGS
)

ln -sf redo out/redo-ifchange
ln -sf redo out/redo-ifcreate
ln -sf redo out/redo-always

export PATH="$(pwd)/out:$PATH"

redo

echo "Finished bootstrapping."
