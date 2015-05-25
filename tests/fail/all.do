. ../include.sh

rm -rf fail

redo-ifchange fail >& /dev/null || true

assert "abort if do-script returns nonzero" << !
[ ! -e fail ]
!
