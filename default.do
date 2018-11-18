#!/bin/sh -e
# define all project wide variables

export ROOTDIR=$(pwd)
export SRCDIR=$ROOTDIR/src
export OUTDIR=$ROOTDIR/out
export VERSION="pre-0.01"

DESTDIR=${DESTDIR-/usr/local/bin}

if [ "$1" = "all" ]; then
	redo-ifchange "$OUTDIR/redo"
elif [ "$1" = "clean" ]; then
	rm -rf "$OUTDIR"/*.tmp "$OUTDIR"/*.o "$OUTDIR"/redo "$OUTDIR"/CC
	# autoconf stuff
	rm -rf autom4te.cache config.h.in configure config.status config.log config.h
elif [ "$1" = "install" ]; then
	redo-ifchange all
	mkdir -p "$DESTDIR"
	install "$OUTDIR/redo" "$DESTDIR"
	ln -sf "$DESTDIR/redo" "$DESTDIR/redo-ifchange"
	ln -sf "$DESTDIR/redo" "$DESTDIR/redo-ifcreate"
	ln -sf "$DESTDIR/redo" "$DESTDIR/redo-always"
	echo "Finished installing."
else
	echo "No such target."
	exit 1
fi
