#!/bin/bash
#
# Happy path for one variant: build an index over a generated dataset, then
# check that every query the index answers is exactly what a linear scan over
# the same data returns.
#
# The index and the scan share the same Object::distance, so agreement is a
# real correctness check on the tree: an over-eager prune drops rows, a broken
# covering radius adds them.
#
# usage: tests/happy_path.sh <variant> <build-dir>

set -euo pipefail

variant=${1:?usage: tests/happy_path.sh <variant> <build-dir>}
builddir=${2:?usage: tests/happy_path.sh <variant> <build-dir>}

here=$(cd "$(dirname "$0")" && pwd)
binary="$builddir/$variant"

if [ ! -x "$binary" ]; then
    echo "no executable at $binary" >&2
    exit 1
fi

# The object type is baked into each variant rather than chosen at runtime.
case "$variant" in
    *_str) kind=str;    dim=12 ;;
    *_int) kind=int;    dim=8  ;;
    *)     kind=vector; dim=8  ;;
esac

rows=500
neighbours=20

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

data="$work/data.txt"
index="$work/index"

python3 "$here/make_dataset.py" --kind "$kind" --rows "$rows" --dim "$dim" --seed 7 > "$data"

echo "== $variant: $rows $kind objects of dimension $dim"

"$binary" build --data "$data" --dim "$dim" --index "$index"
"$binary" check --index "$index" --dim "$dim"

failures=0

fail() {
    echo "FAIL: $*"
    failures=$((failures + 1))
}

for query in 0 17 123 499; do
    # Put the radius halfway between the 20th and 21st neighbour so the result
    # set is a decent size and no object sits exactly on the boundary, where an
    # inclusive and an exclusive test would disagree.
    "$binary" scan-knn --data "$data" --dim "$dim" \
        --query "$query" -k $((neighbours + 1)) 2>/dev/null > "$work/knn.txt"
    radius=$(awk -v n="$neighbours" \
        'NR==n {previous=$1} NR==n+1 {printf "%.9f\n", (previous+$1)/2; exit}' "$work/knn.txt")

    "$binary" range --data "$data" --dim "$dim" --index "$index" \
        --query "$query" --radius "$radius" 2>/dev/null > "$work/range.index"
    "$binary" scan-range --data "$data" --dim "$dim" \
        --query "$query" --radius "$radius" 2>/dev/null > "$work/range.scan"

    if ! diff -u "$work/range.scan" "$work/range.index" > "$work/range.diff"; then
        fail "range from row $query at radius $radius disagrees with the scan"
        sed -n '1,20p' "$work/range.diff"
    fi

    matched=$(wc -l < "$work/range.scan")
    if [ "$matched" -lt 2 ] || [ "$matched" -ge "$rows" ]; then
        fail "range from row $query matched $matched of $rows rows, so it is not selective"
    fi

    for k in 1 5 20; do
        "$binary" knn --data "$data" --dim "$dim" --index "$index" \
            --query "$query" -k "$k" 2>/dev/null > "$work/knn.index"
        "$binary" scan-knn --data "$data" --dim "$dim" \
            --query "$query" -k "$k" 2>/dev/null > "$work/knn.scan"

        if ! diff -u "$work/knn.scan" "$work/knn.index" > "$work/knn.diff"; then
            fail "$k-NN from row $query disagrees with the scan"
            sed -n '1,20p' "$work/knn.diff"
        fi
    done

    echo "   row $query: radius $radius matched $matched rows, 1/5/20-NN agree"
done

if [ "$failures" -ne 0 ]; then
    echo "$variant: $failures check(s) failed"
    exit 1
fi

echo "$variant: index and linear scan agree on every query"
