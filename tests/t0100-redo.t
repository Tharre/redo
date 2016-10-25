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

test_done
