. ../include.sh

rm -rf a b

# we use redo here, as redo-ifchange is smart enough not to rebuild the same
# file twice in the same redo session

redo a
redo a

touch b

redo a

assert "ifcreate" << !
[ "$(cat a)" = "baa" ]
!
