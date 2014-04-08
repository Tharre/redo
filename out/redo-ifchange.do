. ./config.sh

DEPS="redo-ifchange.o build.o util.o"
redo-ifchange $DEPS config.sh
$CC -o $3 $DEPS $LDFLAGS
