. ../include.sh

rm -rf a

redo a

assert "always" << !
[ "$(cat a)" = "??" ]
!
