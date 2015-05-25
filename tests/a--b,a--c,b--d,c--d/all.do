. ../include.sh

rm -rf a b c d

redo-ifchange a

assert "a--b,a--c,b--d,c--d" << !
[ "$(cat a)" = "abdcd" ]
!
