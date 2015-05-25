. ../include.sh

rm -rf a b c

redo-ifchange a b

assert "a--c,b--c" << !
[ "$(cat a)" = "ac" ] &&
[ "$(cat b)" = "bc" ]
!
