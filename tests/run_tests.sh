#!/bin/bash
#
# Build every variant and run the happy path against each. CI runs the variants
# as separate jobs; this is the same thing in one go, for local use.
#
# usage: tests/run_tests.sh [build-dir]

set -euo pipefail

cd "$(dirname "$0")/.."
builddir=${1:-build}

cmake -S . -B "$builddir" -DCMAKE_BUILD_TYPE=Release
cmake --build "$builddir" -j"$(getconf _NPROCESSORS_ONLN)"

tests/check_drivers.sh

variants="MTree MTree_int MTree_str MxTree MxTree_int MxTree_str
          MxTree_SLIM MxTree_SLIM_str MxTree_SPLIT MxTree_SPLIT_str"

failed=""
for variant in $variants; do
    if ! tests/happy_path.sh "$variant" "$builddir"; then
        failed="$failed $variant"
    fi
    echo
done

if [ -n "$failed" ]; then
    echo "failed:$failed"
    exit 1
fi

echo "every variant agrees with a linear scan"
