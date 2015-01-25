teardown() {
	rm -rf .redo/

	# remove all helper files
	for i in *.bats; do
		rm -f "${i%%.*}_result"
	done
}

setup() {
	cd "$BATS_TEST_DIRNAME"
}
