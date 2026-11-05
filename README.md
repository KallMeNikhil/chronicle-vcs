# Chronicle

A version-control engine in C++17, written from scratch, where the immutable
object graph is not only storage but the data structure every feature is
computed on. Blame, bisect, merge, time travel, delta compression and
reconciliation are each a different traversal or query over the same
SHA-256 Merkle DAG.

Data Structures (15B11CI311) + Data Structure Lab (15B17CI371), JIIT —
project-based-learning mini project. To install, test and demonstrate it from
scratch on a new machine, follow [docs/GUIDE.md](docs/GUIDE.md). The full specification is
[docs/DESIGN.md](docs/DESIGN.md); the build plan is
[docs/BLUEPRINT.md](docs/BLUEPRINT.md); every decision taken where the spec
was silent or contradictory is in [docs/DECISIONS.md](docs/DECISIONS.md).

## Build

Requirements: Linux (Ubuntu), `g++` with C++17, GNU `make`, `bash`. No
third-party libraries, no Python. `git` and `sha256sum` are only used by
tests as reference oracles.

```bash
make            # build/chronicle and build/unit_tests
make test       # unit tests + integration scenarios
make asan       # the whole suite under AddressSanitizer + UBSan
make bench      # benchmarks B1-B10 -> bench/results/*.csv + exponent summary
bash scripts/check.sh --asan   # clean build, tests, sanitizer run
```

Everything compiles with `-std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror`.

On Windows, build and run inside WSL Ubuntu.

## Usage

```bash
chronicle init demo && cd demo
echo "hello" > a.txt
chronicle add .
chronicle commit -m "first"
chronicle checkout -b feature
...
chronicle merge feature            # fast-forward or three-way (diff3)
chronicle log --graph --oneline
chronicle blame a.txt
chronicle history --first a.txt    # object history index queries
chronicle at 2024-05-01 a.txt      # file as of a date (B+ tree time index)
chronicle export HEAD~3 /tmp/snap  # read-only materialisation
chronicle bisect start; chronicle bisect bad; chronicle bisect good <rev>
chronicle bisect run ./test.sh     # never touches your working tree
chronicle pack; chronicle stats; chronicle verify; chronicle gc
```

| Command | Purpose |
|---|---|
| `init [dir]` | create `.chronicle/` |
| `add`, `rm [--cached]`, `status`, `commit -m` | staging (red-black tree index) and commits |
| `branch`, `checkout [-b]`, `reset --hard` | refs and working-tree switches (clean-tree rule) |
| `diff [--cached] [<rev> [<rev>]] [-- path]`, `show`, `ls-tree [-r]` | Myers diff, Merkle tree diff |
| `log [--oneline] [--graph] [--since T] [--until T] [-n N] [rev] [-- path]` | heap-ordered topological walk |
| `graph [--dot]` | ASCII lanes or Graphviz DOT text |
| `merge <branch> [--no-ff] [--abort] [--allow-unrelated]` | merge-base + tree-level three-way merge |
| `blame <path> [rev]` | BlameFast (parent-pass DP + memo) |
| `history <path>`, `--first <path>`, `--blob <id>` | object history index |
| `at <rev-or-time> <path>`, `export <rev-or-time> <dir>`, `stats [--sharing]` | time travel, structural sharing |
| `bisect start [--strategy=weighted] / bad / good / skip / run / reset / log` | automated bisect |
| `cat-file -t/-s/-p`, `hash-object [-w]`, `find <prefix>` | plumbing; trie-based abbreviated ids |
| `pack`, `gc [--dry-run] [--grace-seconds N]`, `verify`, `reindex` | delta packs, GC, fsck |
| `reconcile <dir> [--fetch]`, `cherry-pick <rev>` | EXPERIMENTAL: two-repo reconciliation, patch-id |

Revisions: `HEAD`, branch names, full ids, abbreviated ids (at least 4 hex
characters), with any chain of `~N` / `^N`. Exit codes: 0 ok, 1 error,
2 usage, 3 merge conflict, 4 corrupt or missing object, 5 no repository or
repository locked. Environment: `CHRONICLE_AUTHOR_NAME`,
`CHRONICLE_AUTHOR_EMAIL`, `CHRONICLE_AUTHOR_DATE="<unix> [+hhmm]"`.

## Demos

```bash
bash scripts/demo_all.sh            # branches, merge, conflict, graph, blame, history,
                               # time travel, bisect, pack/stats/verify + corruption
bash scripts/demo_bisect.sh 1000    # headline demo: find a planted bug in 1000 commits
bash scripts/gen_repo.sh 500 8 42   # deterministic generated repository
```

`demo_bisect.sh 1000` builds 1000 commits, plants a bug at a random one and
runs `chronicle bisect run`. A run with seed 4242 found the planted commit
#388 exactly, in 10 tests (bound ⌈log₂ 1000⌉ + 1 = 11), with 51 ms of
bisect time.

## Data structures and where they are used

| Structure | Implementation | Used for |
|---|---|---|
| Hash table (chaining) | `std::unordered_map` / `unordered_set` | object cache, DAG id -> index, history index, blame memo, visited sets, delta block index |
| Red-black tree | `std::map` | staging index (in-order walk = tree order) |
| Binary heap | `std::priority_queue` | date-ordered log walk, Kahn tie-break in bisect |
| Stack / queue | `std::stack` / `std::queue` | DFS (GC mark, ancestry, blame), BFS (DAG load), Kahn |
| Merge sort | `std::stable_sort` | pack base-candidate ordering, path ordering |
| **16-ary trie** (hand-written) | `src/ds/trie.hpp` | abbreviated id resolution + ambiguity |
| **B+ tree** (hand-written) | `src/ds/bplustree.hpp` | `at <time>`, `log --since/--until` |
| **LSD radix sort** (hand-written) | `src/ds/radixsort.hpp` | pack index construction |
| **Interpolation search** (hand-written) | `src/ds/interpsearch.hpp` | pack index lookup |
| **SHA-256** (hand-written) | `src/core/sha256.cpp` | content addressing |
| **Algorithms** (hand-written) | `src/engine/`, `src/store/delta.cpp` | Myers diff, diff3, merge base, BlameFast, bisect, Rabin–Karp delta, GC, fsck |

Not used, and deliberately so (spec §3): median search, threaded trees,
binomial and Fibonacci heaps, AVL as a separate structure, shortest paths,
MST, interval/segment trees, suffix structures, Union-Find.

## Testing

* **Unit tests** (`tests/unit`, own framework in `tests/framework.hpp`):
  * SHA-256 against NIST vectors and `sha256sum` at block-boundary lengths.
  * Radix sort vs `std::stable_sort`; interpolation search vs `std::lower_bound` on 10⁵ uniform and clustered ids.
  * Trie vs a linear-scan oracle; B+ tree invariants after every random insert, at m = 4 and m = 64.
  * Myers `apply(diff(a,b),a) == b` on 10⁴ random pairs; every diff3 rule; tree-merge rules.
  * Merge base on linear, diamond, criss-cross and unrelated DAGs; blame attribution through merges, with the memo on and off.
  * Delta and pack round trips.
* **Integration tests** (`tests/integration/t*.sh`): 16 scenario scripts covering every scenario listed in spec §12.2, plus the edge cases of §11.2. That includes the `git hash-object --object-format=sha256` oracle on 100 files from 0 to 1 MiB.
* `make asan` runs everything under AddressSanitizer + UBSan.

## Benchmarks (`make bench`)

Each benchmark writes a CSV to `bench/results/` and fits the slope of
log(time) against log(n). The numbers below are from one run under WSL2 on
the development machine.

| ID | Variant | Observed | Theory |
|---|---|---|---|
| B1 | n lookups: chained hash / linear probing / binary search / interpolation | 1.55 / 1.44 / 1.33 / 1.40 | 1 / 1 / 1+ / 1+ |
| B2 | DAG traversal visited set: `unordered_set` / linear probing | 1.40 / 1.52 | 1 |
| B3 | log walk: heap topological walk / collect + merge sort | 1.12 / 1.19 | 1+ |
| B4 | abbreviated id: trie / linear scan | 0.09 / 1.06 | 0 / 1 |
| B5 | blame at every revision: memo / no memo | 1.29 / 2.13 | 1 / 2 |
| B6 | time-range query: B+ tree / linear scan | 0.11 / 1.16 | 0 / 1 |
| B7 | diff vs N (D fixed) / Myers core vs D (N fixed) | 1.25 / 1.44 | 1 / 1..2 |
| B8 | bisect tests vs N: slope against log₂ N | 1.00 | 1 (tests = log₂ N, always ≤ bound) |
| B9 | storage: loose / packed | 1.08 / 1.00 | 1 / 1; the pack is 4–5 % of loose size |
| B10 | staging index: `std::map` / sorted-vector insert | 1.27 / 1.95 | 1+ / 2 |

The exponents for B1 and B2 sit above the theory because of the memory
hierarchy: once a table outgrows the CPU caches (around 2¹⁸ entries) every
probe is a cache miss. Interpolation search uses fewer probes than binary
search (about 2–4 instead of about 20), but each probe costs a 128-bit
division and a cache miss, so it is only faster at small and medium n.
These are effects to discuss in the report, not bugs.

## Limitations (spec §15.3)

* No rename tracking.
* A criss-cross merge uses one chosen base (the newest) instead of a recursive virtual base.
* Bisect's O(log N) assumes the "bad" property is monotone along the chosen linearisation.
* Delta base selection is a window heuristic, not optimal.
* Single process only; no tags, rebase or stash.
* SHA-256 only; hash collisions are not handled.
* Diff, merge and blame are text-oriented.
* `bisect run` treats exit codes 126/127 (cannot execute, not found) as *bad*, per the spec.
* The optional MiniLang AST merge (M13) is not implemented.
