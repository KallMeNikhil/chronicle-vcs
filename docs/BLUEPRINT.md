# Chronicle — Implementation Blueprint

This is the build plan derived from `docs/DESIGN.md` (the spec). Where the spec
and this file disagree, the spec wins, except for the resolutions listed in
section (d), which are also recorded in `docs/DECISIONS.md`.

## (a) Architecture in one page

Chronicle is a content-addressed version-control engine. Every piece of state
that matters — file contents, directory listings, commits — is an immutable
object whose name is the SHA-256 of `"<type> <size>\0" + payload`. Because an
object's name is a function of its bytes, the store deduplicates for free, any
corruption is detectable by rehashing, and a commit id transitively pins the
whole snapshot below it (a Merkle DAG).

Six layers, each depending only on the ones above it:

1. **core** — `Status` (error value, never thrown across modules), `Id`
   (32 raw bytes + hex), self-contained FIPS 180-4 SHA-256, LEB128 varints,
   string escaping and time parsing/formatting.
2. **ds** — only the structures the STL lacks: `HexTrie` (16-ary trie for
   abbreviated ids), `BPlusTree<K,V>` (time index, leaves linked), LSD radix
   sort over 32-byte ids, interpolation search over a sorted id array.
3. **store** — on-disk formats: loose objects, packs + pack index + own
   delta format, refs/HEAD, the staging index (`std::map`, a red-black tree,
   whose in-order walk is exactly tree-object order), config, ignore rules,
   repository lock. `ObjectStore` is the facade (loose first, then packs).
4. **engine** — everything that *computes* on the graph: the commit `Dag`
   (dense indices, parent/child adjacency vectors, generation numbers),
   revision parsing, the heap-driven date-ordered topological walk, merge-base
   by descending-generation marking, Myers diff, Merkle tree diff, tree-level
   three-way merge + diff3, BlameFast (parent-pass DP with an
   `unordered_map` memo), the object history index, time travel (B+ tree time
   lookup + read-only materialisation), bisect, GC/fsck, ASCII/DOT graph,
   worktree materialisation and status.
5. **cli** — one thin handler per command: parse flags, call the engine,
   print, map `Status` to an exit code.
6. **exp** — reconcile two local repositories, patch-id, cherry-pick.

The thesis — "the immutable object graph is the data structure you compute
on" — shows up as: log = Kahn walk with a max-heap; merge = merge-base over
the DAG + Merkle tree diff that skips equal subtree ids; blame = DP over the
path's history in topological order; bisect = binary search over a Kahn
linearisation of a candidate subgraph; GC = DFS mark from roots; time travel
= B+ tree lookup then an O(depth) root-tree walk.

Build/test environment: Ubuntu (WSL2 on the owner's machine), g++ 15,
GNU make. Integration scripts run repositories under `mktemp -d` (native
Linux filesystem so exec bits and symlinks behave).

## (b) Module dependency order

```
core/status, core/id, core/strutil, core/varint, core/timeutil
  -> core/sha256
  -> ds/radixsort, ds/interpsearch, ds/trie, ds/bplustree
  -> util/fs, util/proc
  -> store/object -> store/loose -> store/delta -> store/packidx -> store/pack
  -> store/objstore (facade) -> store/config, store/refs, store/index,
     store/ignore, store/lock -> store/repo
  -> engine/dag -> engine/revparse -> engine/logwalk
  -> engine/diff -> engine/treediff -> engine/worktree
  -> engine/merge -> engine/history -> engine/blame
  -> engine/timetravel -> engine/bisect -> engine/gc, engine/fsck
  -> engine/graphview
  -> exp/patchid -> exp/cherrypick, exp/reconcile
  -> cli/* -> main.cpp
```

## (c) Milestones: files and proving tests

| M | Files created | Proof of done |
|---|---|---|
| M0 | `Makefile`, `tests/framework.hpp`, `tests/unit/test_main.cpp`, `tests/unit/test_core.cpp`, `src/core/{status,strutil,timeutil,varint}.hpp`, `src/main.cpp` (stub `--version`), `scripts/check.sh`, `tests/integration/run_all.sh`, `.gitignore` | `make test` green; `scripts/check.sh` builds + tests |
| M1 | `src/core/{sha256.hpp,sha256.cpp,id.hpp}`, `src/ds/{radixsort.hpp,interpsearch.hpp}`, `tests/unit/{test_sha256,test_radix_interp}.cpp` | NIST vectors (`""`, `abc`, 10⁶×`a`), lengths 55/56/63/64/65 vs `sha256sum`; radix == `std::stable_sort`; interpolation == `std::lower_bound` (10⁵ uniform + clustered) |
| M2 | `src/util/fs.{hpp,cpp}`, `src/store/{object,loose,objstore,config,repo}.{hpp,cpp}`, `src/cli/{cmd_init,cmd_hash_object,cmd_cat_file}.cpp`, `src/cli/cli.hpp`, `tests/unit/test_object.cpp`, `tests/integration/t02_objects.sh`, `tests/integration/t03_git_oracle.sh` | encode/decode round trips; tree order; oracle vs `git init --object-format=sha256` (100 files, 0…1 MiB) |
| M3 | `src/ds/trie.hpp`, `src/store/{refs,index,ignore,lock}.{hpp,cpp}`, `src/engine/{revparse,worktree}.{hpp,cpp}`, `src/cli/{cmd_add,cmd_rm,cmd_status,cmd_commit,cmd_branch,cmd_checkout,cmd_reset,cmd_find}.cpp`, `tests/unit/{test_trie,test_index}.cpp`, `tests/integration/t1x_*.sh` | trie vs linear oracle; index round trip incl. weird names + checksum failure; integration: init/add/commit, status states, branch/checkout clean-tree refusal, reset --hard, ignore rules, weird filenames, lock, abbreviated-id ambiguity |
| M4 | `src/engine/{diff,treediff}.{hpp,cpp}`, `src/cli/{cmd_diff,cmd_show,cmd_ls_tree}.cpp`, `tests/unit/test_diff.cpp`, `tests/integration/t20_diff.sh` | `apply(diff(a,b),a)==b` for 10⁴ random pairs; tree diff skips equal subtrees |
| M5 | `src/engine/{dag,logwalk,graphview}.{hpp,cpp}`, `src/cli/{cmd_log,cmd_graph}.cpp`, `tests/unit/test_dag.cpp`, `tests/integration/t30_log.sh` | hand-built DAG log order (incl. clock skew); merge-base on linear/diamond/criss-cross/unrelated |
| M6 | `src/engine/merge.{hpp,cpp}`, `src/cli/cmd_merge.cpp`, `tests/unit/test_diff3.cpp`, `tests/integration/t4x_merge*.sh` | every diff3 rule; ff, clean merge, conflict→resolve→commit, abort |
| M7 | `src/engine/{history,blame}.{hpp,cpp}`, `src/cli/{cmd_blame,cmd_reindex}.cpp`, `tests/unit/test_blame.cpp`, `tests/integration/t50_blame.sh` | scripted histories with known attribution incl. a merge; memo aliasing |
| M8 | `src/ds/bplustree.hpp`, `src/engine/timetravel.{hpp,cpp}`, `src/cli/{cmd_history,cmd_at,cmd_export,cmd_stats}.cpp`, `tests/unit/test_bplustree.cpp`, `tests/integration/t60_time.sh` | B+ invariants for m=4/m=64 vs oracle; history queries; `at`/`export` by time; `log --since/--until` |
| M9 | `src/util/proc.{hpp,cpp}`, `src/engine/bisect.{hpp,cpp}`, `src/cli/cmd_bisect.cpp`, `scripts/demo_bisect.sh`, `tests/integration/t70_bisect.sh` | demo finds planted bug in ≤ ⌈log₂N⌉+1 tests, N=1000 |
| M10 | `src/store/{delta,pack,packidx}.{hpp,cpp}`, `src/engine/{gc,fsck}.{hpp,cpp}`, `src/cli/{cmd_pack,cmd_gc,cmd_verify}.cpp`, `tests/unit/test_delta_pack.cpp`, `tests/integration/t80_pack_gc.sh` | delta round trip; pack/idx round trip + lookup of every id; gc keeps reachable; corruption detected (exit 4) |
| M11 | `bench/bench_main.cpp`, `bench/bench_*.cpp`, `scripts/gen_repo.sh` | `make bench` writes CSVs + exponent summary |
| M12 | `src/exp/{patchid,cherrypick,reconcile}.{hpp,cpp}`, `src/cli/{cmd_cherry_pick,cmd_reconcile}.cpp`, DOT in `graphview.cpp`, `tests/integration/t90_exp.sh` | reconcile states, `--fetch`, cherry-pick + detection, DOT output |
| M13 | optional MiniLang AST merge (only if time remains) | grammar + function-level merge tests |
| M14 | `README.md`, `scripts/demo_all.sh`, final `DECISIONS.md` | `make asan` clean; demo runs end-to-end |

## (d) Contradictions, ambiguities, risks — and resolutions

1. **"bounded `ChainedHashMap`" (10.1) vs D-01 (no custom hash tables).**
   Resolution: the resolved-object cache is a `std::unordered_map` (which *is*
   a chained hash table) capped at 512 entries, cleared when full.
2. **M3 DoD "integration scenarios 1–6"** but scenario 4–6 (fast-forward,
   merge, conflict) need M6 and scenario 1 includes `log` (M5).
   Resolution: M3 proves the scenarios that only need M3 features
   (init/add/commit, status, branch/checkout, reset, ignore, weird names,
   lock, ambiguity); `log` is asserted from M5 and merge scenarios from M6.
3. **Bisect midpoint `⌊n/2⌋` with "bad ⇒ keep [0..mid]"** never terminates for
   n = 2 when the known-bad commit is the last element (mid = 1 is the known
   bad). Resolution: the known-bad tip is always last in the linearisation and
   is never re-tested; the midpoint is `⌊(n−1)/2⌋` over the list. Also the
   candidate set is recomputed from the BISECT state each step (exactly the
   spec's definition), which equals list slicing on linear history.
4. **BlameFast (M7) needs `history.idx`, which the table places in M8.**
   Resolution: writing/appending/rebuilding `history.idx` lands in M7;
   the `history` query commands land in M8.
5. **`at` "lowerBound … newest commit with time ≤ T"** — a lower bound finds
   the first key ≥ T, not the last ≤ T. Resolution: the B+ tree also offers
   `floor(key)` (leaves are doubly linked); key = `(time, position on the
   first-parent chain)` so equal times resolve to the topologically latest.
6. **Tree order is plain bytewise** (no Git directory-slash rule), so tree and
   commit ids differ from Git's; only blob ids are oracle-comparable. This is
   what the spec intends (oracle test uses `hash-object` only).
7. **`stats --sharing` "last N commits"** with N unspecified. Resolution: the
   100 most recent commits in log order from HEAD (`--last N` overrides).
8. **Conflicted paths in the index.** The spec has no index stages.
   Resolution: the index keeps *ours* for conflicted paths; `MERGE_STATE`
   holds the list of still-unresolved paths and `add` removes a path from it.
9. **Deterministic timestamps for tests/demos.** Not specified. Resolution:
   env `CHRONICLE_AUTHOR_DATE="<unix> [+hhmm]"` overrides the commit time
   (analogous to Git's `GIT_AUTHOR_DATE`).
10. **"index write time"** for the racy-stat rule is not stored in the
    format. Resolution: use the index file's own mtime.
11. **`history.idx` granularity.** Resolution: one line per changed *file*
    path (blob or symlink); directories are implied by their files.
12. **Extra source files** beyond the 0.4 list: `store/objstore.cpp`
    (facade), `store/repo.cpp` (repo discovery/context),
    `engine/worktree.cpp` (status + checkout materialisation). They keep the
    listed files under ~600 lines; the listed names all still exist.
13. **Bisect exit 126/127** (cannot execute / not found) count as *bad* per
    the spec's literal 1–127 rule; the README warns about it.
14. **Risk: platform.** The owner's machine is Windows; the spec targets
    Ubuntu. Everything is built and tested in WSL2 Ubuntu (g++ 15) with
    test repositories in `/tmp` so POSIX semantics hold.
15. **Risk: `-Wpedantic` vs `unsigned __int128`** (needed by interpolation
    search). Resolution: declared through `__extension__`.
