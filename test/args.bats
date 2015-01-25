#!/usr/bin/env bats

load test_helper

@test "check arguments supplied to .do files" {
	run redo args_test.ext
	[ $status -eq 0 ]
}
