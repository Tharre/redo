. ../include.sh

rm -rf a d/b d/d/c

redo-ifchange a

assert "cwd" << !
[ "$(cat a)" == "abc" ]
!
