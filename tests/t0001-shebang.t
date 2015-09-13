#!/bin/sh -e
# Copyright (c) 2015 Tharre
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

test_description='Test if shebang line is honored.

Requires /proc/<pid>/cmdline, so it may not work on some platforms'

. ./sharness.sh

cat > "shebang.do" <<'EOF'
#!/bin/sh
cat /proc/$$/cmdline | tr "\0" " " | cut -d' ' -f1,2
EOF

cat > "shebang2.do" <<'EOF'
cat /proc/$$/cmdline | tr "\0" " " | cut -d' ' -f1,2
EOF

cat > "echo.do" <<'EOF'
#!/usr/bin/env echo
EOF

test_expect_success "/bin/sh exists" "
    test -e /bin/sh
"

test_expect_success "/usr/bin/env exists" "
    test -e /usr/bin/env
"

test_expect_success "/proc/<pid>/cmdline reports correct shebang" "
    redo shebang | grep -q '/bin/sh shebang.do'
"

test_expect_success "'/bin/sh -e' is used if no shebang is specified" "
    redo shebang2 | grep -q 'bin/sh -e'
"

test_expect_success "'/usr/bin/env echo' works" "
    redo echo | grep -q echo.do
"

test_done
