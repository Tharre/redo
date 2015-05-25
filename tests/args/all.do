. ../include.sh

assert "check arguments supplied to do-script" << !
redo-ifchange d/a.ext
!
