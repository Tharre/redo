. ./config.sh

DEPS="redo-always.o build.o util.o filepath.o"
redo-ifchange $DEPS config.sh
$CC -o $3 $DEPS $LDFLAGS
