if type "clang" > /dev/null; then
	PREF="clang"
else
	PREF="gcc"
fi
CC=${CC-$PREF}
CFLAGS="-g -Wall -Wextra -std=c99 -pedantic -Wno-gnu-statement-expression $CFLAGS"
LDFLAGS="$LDFLAGS"

if [ ! -n "$SH_BUILD" ]; then
	if [ -f "../config.local" ]; then
		redo-ifchange ../config.local
		. ../config.local
	else
		redo-ifcreate ../config.local
	fi
fi
