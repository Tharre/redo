. ./config.sh

DEPS="redo-ifcreate.o build.o util.o filepath.o"
redo-ifchange $DEPS config.sh
$CC -o $3 $DEPS $LDFLAGS