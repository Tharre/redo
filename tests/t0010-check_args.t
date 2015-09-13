#!/bin/sh -e
# Copyright (c) 2015 Tharre
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

test_description='Check arguments supplied to the do-script'

. ./sharness.sh

mkdir d &&
cat > "d/check_args.ext1.ext2.do" <<'EOF'
#!/bin/sh -e
[ "$1" = "check_args.ext1.ext2" ]
[ "$2" = "check_args.ext1" ]
[ "$3" != "check_args.ext1.ext2" ]
EOF

test_expect_success "check arguments" "
    redo d/check_args.ext1.ext2
"

test_done
