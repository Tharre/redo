. ../include.sh

assert "dependency-loop" << !
timeout 1 redo-ifchange a >& /dev/null
!
