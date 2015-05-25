. ../include.sh

rm -rf a

redo-ifchange a

assert "a" << !
[ "$(cat a)" = "a" ]
!
