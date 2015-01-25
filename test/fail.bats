#!/usr/bin/env bats

load test_helper

@test "invoke with target without do-file" {
	run redo this-does-not-exist
	[ $status -ne 0 ]
}

@test "invoke with failing do-file" {
	run redo fail
	sync
	[ $status -ne 0 ] && [ ! -e fail_result ]
}
