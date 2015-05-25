. ../include.sh

rm -rf a b c

redo-ifchange a

assert "a--b,a--c,b--c" << !
[ "$(cat a)" = "abcc" ]
!
