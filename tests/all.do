#!/bin/sh -e
# Copyright (c) 2015 Tharre
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

# don't reuse existing redo session
unset REDO_ROOT
unset REDO_MAGIC
unset REDO_PARENT_TARGET

T=*.t

# pre-clean
rm -rf test-results

# T
for i in $T; do
	echo "*** $i ***"
	./$i
done

# aggregate-results
for f in test-results/*.counts; do
	echo "$f"
done | ./aggregate-results.sh

# cleanup
rm -rf "trash directory.*" test-results .prove
