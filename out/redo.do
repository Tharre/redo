. ./config.sh

DEPS="redo.o build.o util.o filepath.o sha1.o DSV.o pcg.o"
redo-ifchange $DEPS config.sh
$CC -o $3 $DEPS $LDFLAGS
