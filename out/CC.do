redo-ifchange config.sh
. ./config.sh

exec >$3
cat <<-EOF
	redo-ifchange "\$SRCDIR/\$2.c"
	$CC $CFLAGS -MD -MF /dev/fd/1 -o \$3 -c "\$SRCDIR/\$2.c" \
			| sed 's/^[^:]*://g; s/ \\\\//g' | xargs redo-ifchange
EOF
chmod +x $3
