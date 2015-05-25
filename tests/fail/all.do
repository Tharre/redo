. ../include.sh

rm -rf fail

redo-ifchange fail 2> /dev/null || true

assert "abort if do-script returns nonzero" << !
[ ! -e fail ]
!
