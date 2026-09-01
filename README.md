# MxTree

[![CI](https://github.com/jsc0218/MxTree/actions/workflows/ci.yml/badge.svg)](https://github.com/jsc0218/MxTree/actions/workflows/ci.yml)

A disk-based **metric space secondary index** in C++: the M-tree, and the
MX-tree this repository is named after, from

> Shichao Jin, Okhee Kim, Wenya Feng.
> *MX-tree: A Double Hierarchical Metric Index with Overlap Reduction.*
> ICCSA (5) 2013: 574–589.

It answers two queries over data that has no coordinates and no ordering:
**range** (everything within distance *r* of a query object) and **k-NN**.

## The idea, in database terms

A B-tree needs a total order on the key. A GiST index needs less than that: a
predicate it can evaluate against an entire subtree, so a search knows when it
may skip one. A metric index needs less still — **only a distance function that
obeys the triangle inequality**. No coordinates, no order, no dimensions.

That is why the same tree here indexes three unrelated object types: float
vectors under L2, integer vectors under L2, and strings under Levenshtein edit
distance. Nothing in the tree knows what an object *is*. It only ever calls
`Object::distance`.

Pruning is the triangle inequality and nothing else. Every entry stores a
routing object and a covering radius that bounds everything beneath it, so when

```
d(query, routing) - covering_radius > search_radius
```

the entire subtree is discarded without one further distance computation. That
matters because in a metric space the distance function *is* the cost: for edit
distance over strings it is an O(nm) dynamic program, so distance computations
are the thing you count. Each command here reports how many it did.

### This is GiST — but not PostgreSQL's GiST

Both descend from the same 1995 Hellerstein, Naughton and Pfeffer paper, and
then diverge completely. PostgreSQL turned GiST into an access method: you
write C functions and register them in an operator class under fixed support
function numbers. Berkeley's `libgist`, vendored in each `Gist/` directory
here, is the original research library, where you subclass C++ classes instead.
They share the idea and no code.

The correspondence is close enough to read this repository as an operator class:

| here | PostgreSQL GiST support function |
| --- | --- |
| `MTpred::Consistent` | `consistent` (1) |
| `MTnode::Union` | `union` (2) |
| `MTentry::Compress` | `compress` (3) |
| `MTentry::Decompress` | `decompress` (4) |
| `MTentry::Penalty`, `MTnode::SearchMinPenalty` | `penalty` (5) |
| `MTnode::PickSplit`, `MXTnode::PickSplit` | `picksplit` (6) |
| `MTentry::IsEqual` | `same` (7) |
| `MT::TopSearch` | `distance` (8), i.e. `ORDER BY col <-> value` |
| `MTfile`, `MXTfile` (`: GiSTstore`) | nothing — the server owns storage |

That last row is the real difference. Roughly a third of the code here is a
buffer manager, a page layout and a free list, because a research prototype has
to bring its own. Porting to PostgreSQL would mean deleting it, not translating
it.

### Where the paper's contribution sits

`consistent` and `union` decide whether an index is **correct**: get them wrong
and queries return wrong answers. `penalty` and `picksplit` decide whether it is
**any good**: get them wrong and queries are merely slow, because subtrees
overlap and a search has to descend several of them.

The M-tree's weakness is squarely in the second group. Sibling subtrees are
balls that overlap, and overlap is what forces multi-path descent. The MX-tree's
double hierarchy and its overlap reduction live entirely in `Split`, `Promote`,
`PickSplit` and `Trade` — the efficiency half of the interface. That is also the
half PostgreSQL lets you change freely without risking a wrong answer.

### Why this is not a PostgreSQL extension

Core PostgreSQL has no *generic* metric-space opclass — nothing that says "here
is a distance function satisfying the triangle inequality, give me an index".
The distance-capable GiST opclasses that ship are each welded to one type:
`point`, `cube`, `pg_trgm`, `btree_gist`. pgvector, the obvious modern
comparison, uses HNSW and IVFFlat rather than GiST, and is Euclidean/inner
product/cosine rather than arbitrary metrics.

So an M-tree operator class taking a user-supplied distance function would be a
genuinely useful thing to build. This repository is the algorithm half of it.

## Quick start

Builds on Linux and other POSIX systems. Requires CMake 3.16+, a C++17 compiler
and Python 3 for the test data.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

That produces one executable per variant in `build/`. Generate a dataset and
index it:

```sh
python3 tests/make_dataset.py --kind vector --rows 5000 --dim 8 --seed 1 > /tmp/data.txt

build/MxTree build --data /tmp/data.txt --dim 8 --index /tmp/mx
build/MxTree range --data /tmp/data.txt --dim 8 --index /tmp/mx --query 0 --radius 25
build/MxTree knn   --data /tmp/data.txt --dim 8 --index /tmp/mx --query 0 -k 5
```

`--query` names a row of the dataset to use as the query object, so the same
command works whether objects are vectors or strings. `range` prints matching
row numbers, `knn` prints neighbour distances, and both report the number of
distance computations on stderr.

Every command has a brute-force twin — `scan-range` and `scan-knn` — that
answers the identical question with a linear scan and no index. Comparing the
two is how the test suite checks the tree:

```sh
diff <(build/MxTree range      --data /tmp/data.txt --dim 8 --index /tmp/mx --query 0 --radius 25 2>/dev/null) \
     <(build/MxTree scan-range --data /tmp/data.txt --dim 8                 --query 0 --radius 25 2>/dev/null)
```

`build/MxTree --help` lists the rest, including `stats` and `check` and the
split-policy knobs (`--min-util`, `--promote`, `--secondary`).

Two things the algorithm requires of you:

- **Deduplicate before building.** Exact duplicates in the build set are not
  handled.
- A super node holds roughly ten times a normal node (`MAX_ENTRY_NUM` in the
  driver).

## The ten variants

Each directory is a complete, self-contained program: its own copy of libgist,
its own copy of the tree. They are near-duplicates on purpose — each one changes
internals rather than parameterising them — so CI builds and tests all ten
separately.

| directory | tree | object | distance |
| --- | --- | --- | --- |
| `MTree` | M-tree | double vector | L2 |
| `MTree_int` | M-tree | int vector | L2 |
| `MTree_str` | M-tree | string | edit distance |
| `MxTree` | MX-tree | double vector | L2 |
| `MxTree_int` | MX-tree | int vector | L2 |
| `MxTree_str` | MX-tree | string | edit distance |
| `MxTree_SLIM` | MX-tree, alternative split | double vector | L2 |
| `MxTree_SLIM_str` | MX-tree, alternative split | string | edit distance |
| `MxTree_SPLIT` | MX-tree, alternative split | double vector | L2 |
| `MxTree_SPLIT_str` | MX-tree, alternative split | string | edit distance |

`MTree*` is the baseline M-tree and the only family with bulk loading
(`BulkLoad.cpp`). `MxTree*` adds the double hierarchy, the super-node bitmap and
the trade operation. The `SLIM` and `SPLIT` directories are the alternatives
that were measured against it — they differ in `MXTree::Split`,
`MXTnode::PickSplit`, `Promote` and the minimum-spanning-tree helper. The plain
`MxTree*` trio is the reference implementation and is the only one that also
carries a vantage-point tree (`VPTree`).

## Correctness

`tests/happy_path.sh` builds an index over 500 generated objects and then, for
several query rows, checks that the index and a linear scan return **exactly**
the same thing: identical row numbers for range queries, identical distances for
1-NN, 5-NN and 20-NN. The radius is placed halfway between the 20th and 21st
neighbour, so results are a useful size and nothing sits on the boundary where an
inclusive and an exclusive test would disagree.

Both sides call the same `Object::distance`, so agreement is a real check on the
tree rather than on the metric: an over-eager prune loses rows, a covering radius
that is too small loses rows, and one that is stale adds them.

```sh
tests/run_tests.sh          # build everything and test all ten variants
```

CI runs the ten variants as separate jobs, plus four of them again under
AddressSanitizer and UndefinedBehaviorSanitizer.

## Notes on the port

The sources were written for Visual Studio and had not been built on a POSIX
toolchain. Getting them to compile and pass was mostly mechanical:

- The MSVC C runtime calls now use their POSIX spellings directly: `O_BINARY`
  is dropped, `_lseeki64` is `lseek`, `S_IREAD`/`S_IWRITE` are
  `S_IRUSR`/`S_IWUSR`. Only `filelength` has no POSIX equivalent, so
  `compat/mxtree_platform.h` keeps a small `lseek`-based version of it.
- `MTorderedlist<T>` called a base-class member unqualified, which older MSVC
  accepted and two-phase name lookup does not.
- The original `Makefile`s were the 1997 Bologna ones: they indent recipes with
  spaces, and link a prebuilt `$(HOME)/GiST/libGiST.a` that is not in the
  repository. They are replaced by `CMakeLists.txt`.
- The `.sln` and `.vcproj` files are gone. They only ever added `..\Gist` to the
  include path, so they could not have found the compatibility header, and
  nothing built or tested them. The tree targets Linux and other POSIX systems
  only, and there are no longer any `#ifdef _WIN32` branches to keep in step
  with the code they guard.

Two real bugs surfaced once the tests could run, both fixed:

- **k-NN could write past the end of its result array.** `MT::TopSearch` scanned
  for an insertion point with an unbounded `while (dists[j] <= grade) j++`. Once
  the array is full, `Consistent` still admits an object whose distance *ties*
  with the current kth, and such an object compares equal to every slot, so the
  scan runs off the end. Continuous distances effectively never tie, which is
  why only the edit-distance variants crashed.
- **`GiSTcompressedEntry::key` was allocated with `new[]` and freed with scalar
  `delete`.** Undefined behaviour that glibc tolerates and AddressSanitizer does
  not.

`Main.cpp` used to be a research driver with hardcoded Windows paths to a SIFT
dataset and two of its three `main` functions commented out. It is now the
command line program described above, identical in all ten directories and
checked by `tests/check_drivers.sh`.

## License

MIT, see [LICENSE](LICENSE) — with the caveat recorded there: the vendored
Berkeley libgist and Bologna M-tree sources carry their own **non-commercial**
grant, so commercial use is not advised.
