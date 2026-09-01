#!/usr/bin/env python3
"""Generate a deterministic dataset for the happy-path test.

The objects are emitted in whatever form the variant's Object::Read expects:
whitespace separated numbers for the vector and integer variants, one word per
line for the string variant. Objects are deduplicated because the index does
not tolerate exact duplicates in the build set.
"""

import argparse
import random
import string
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kind", choices=("vector", "int", "str"), required=True)
    parser.add_argument("--rows", type=int, required=True)
    parser.add_argument("--dim", type=int, required=True)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()

    rng = random.Random(args.seed)
    seen = set()
    emitted = 0
    # Duplicates are rare at these sizes, so a generous cap is enough to keep a
    # pathological seed from spinning forever.
    for _ in range(args.rows * 100):
        if emitted == args.rows:
            break
        if args.kind == "vector":
            row = " ".join("%.4f" % rng.uniform(0.0, 100.0) for _ in range(args.dim))
        elif args.kind == "int":
            row = " ".join(str(rng.randint(0, 1000)) for _ in range(args.dim))
        else:
            # Object holds a fixed char[dim] padded with '!', so the word has to
            # fit and must not contain the padding character itself.
            length = rng.randint(max(2, args.dim // 2), args.dim)
            row = "".join(rng.choice(string.ascii_lowercase) for _ in range(length))
        if row in seen:
            continue
        seen.add(row)
        print(row)
        emitted += 1

    if emitted != args.rows:
        sys.exit("could only generate %d of %d distinct objects" % (emitted, args.rows))


if __name__ == "__main__":
    main()
