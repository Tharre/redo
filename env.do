echo "Checking if environment changed ..." 2>&1
printf '%s' "$REDO_TEST" | redo-stamp
echo "c" > $3
