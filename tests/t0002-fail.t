#!/bin/sh -e
# Copyright (c) 2015 Tharre
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

test_description='Abort if do-script returns nonzero'

. ./sharness.sh

cat > "fail.do" <<'EOF'
#!/bin/sh -e
echo "fail" > $3
exit 1
EOF

test_expect_success "return nonzero" "
    test_must_fail redo fail
"

test_expect_success "target file was not created" "
    test_must_fail test -e fail
"

test_done
