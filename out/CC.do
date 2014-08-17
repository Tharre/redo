redo-ifchange config.sh
. ./config.sh

exec >$3
cat <<-EOF
	redo-ifchange \$SRCDIR/\$2.c
	IFS= read DEPS << END
	\$($CC $CFLAGS -MD -MF /dev/fd/1 -o \$3 -c \$SRCDIR/\$2.c)
	END
	redo-ifchange \${DEPS#*:}
EOF
chmod +x $3
