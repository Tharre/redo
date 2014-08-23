if type "clang" > /dev/null; then
	PREF="clang"
else
	PREF="gcc"
fi
CC=${CC-$PREF}
CFLAGS="-g -Wall -Wextra -std=c99 -pedantic -Wno-gnu-statement-expression"
LDFLAGS="-lm"
