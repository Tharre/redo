assert() {
	if source /dev/stdin; then
		echo "PASS: $1" >&2
	else
		echo "FAIL: $1" >&2
	fi
}
