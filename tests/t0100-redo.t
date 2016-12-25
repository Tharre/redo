#!/bin/sh -e
# Copyright (c) 2015-2016 Tharre
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

test_description='Redo tests'

. ./sharness.sh

cat > "a.do" <<'EOF'
#!/bin/sh -e
redo-ifchange b
echo "a" | cat - b > $3
EOF

cat > "b.do" <<'EOF'
#!/bin/sh -e
redo-ifchange c
[ -e "b" ] && exit 1
echo "b" | cat - c > $3
EOF

cat > "c.do" <<'EOF'
#!/bin/sh -e
redo-ifchange s
echo "c" > $3
EOF

test_expect_success "redo shortcuts" "
    echo "s1" > s &&
    redo a &&
    echo "s2" > s &&
    redo a
"

cat > "nonexistant.do" << 'EOF'
#!/bin/sh -e
redo-ifchange nonexistant2
cat nonexistant2 > $3
EOF

cat > "nonexistant2.do" <<'EOF'
#!/bin/sh -e
[ -e "dir/b" ] && redo-ifchange dir/b && cat dir/b > $3
echo "b" >> $3
EOF

test_expect_success "redo should succeed despite missing directories" "
    mkdir dir &&
    echo a > dir/b &&
    redo nonexistant &&
    rm -rf dir &&
    redo nonexistant
"

test_done
