#!/usr/bin/env bats

load test_helper

@test "check if do-files marked with always really do execute always" {
	skip "Not yet implemented"
	run redo always
	[ $status -eq 0 ] && [ -e always_result ]
}
