#!/bin/bash
#
# Each variant directory carries its own copy of the command line driver, the
# same way it carries its own copy of libgist. Nothing in the build stops those
# copies drifting apart, so check that they have not.

set -euo pipefail

cd "$(dirname "$0")/.."

drivers=$(ls */*/Main.cpp | sort)
count=$(echo "$drivers" | wc -l)

if [ "$count" -ne 10 ]; then
    echo "expected 10 driver copies, found $count" >&2
    echo "$drivers" >&2
    exit 1
fi

distinct=$(echo "$drivers" | xargs sha256sum | awk '{print $1}' | sort -u | wc -l)
if [ "$distinct" -ne 1 ]; then
    echo "the driver copies have drifted apart:" >&2
    echo "$drivers" | xargs sha256sum >&2
    exit 1
fi

echo "all $count driver copies are identical"
