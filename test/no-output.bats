#!/usr/bin/env bats

load test_helper

@test "produce empty result" {
	run redo no-output_result
	[ $status -eq 0 ] && [ ! -e no-output_result ]
}
