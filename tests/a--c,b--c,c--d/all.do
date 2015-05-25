. ../include.sh

rm -rf a b c d

redo-ifchange a b

assert "a--c,b--c,c--d" << !
[ "$(cat a)" = "acd" ] &&
[ "$(cat b)" = "bcd" ]
!
