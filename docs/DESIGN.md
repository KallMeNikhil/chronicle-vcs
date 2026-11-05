# Chronicle VCS — Final Build-Ready Specification

**Subject:** Data Structures (course 15B11CI311) + Data Structure Lab (15B17CI371), JIIT — project-based-learning mini project
**Team:** solo · **Language:** C++17 · **Platform:** Ubuntu VM · **Hash:** SHA-256 (self-contained implementation)
**Deliverables:** the codebase now; the project report is written later, after the code is complete (Section 17 prepares for it).

> **Legend used in this document**
> `[ORIGINAL]` = carried over from the original design doc (`chronicle-vcs-design.md`) · `[REVISED]` = original idea kept, details changed · `[ADDED]` = new decision/spec that fills a gap found in the audit.
> Every `D-xx` tag refers to a row in the Decision Register (Section 16). Decisions are **final**; do not reopen them while building.

---

## 0. Project rules (read first)

### 0.1 Authority
This file is the single source of truth. If something is not specified here, choose the simplest option that satisfies the hard constraints in 0.2, write the choice into `docs/DECISIONS.md` in the repo, and continue. Do not ask for clarification for anything this file already decides.

### 0.2 Hard constraints (non-negotiable)
1. **Language:** C++17, compiled with `g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror`. No Python anywhere (scripts are Bash only).
2. **Standard library policy (D-01, revised):** the full C++ standard library **and STL are allowed and preferred** wherever a standard container/algorithm already does the job (`std::unordered_map`, `std::map`, `std::set`, `std::unordered_set`, `std::vector`, `std::stack`, `std::queue`, `std::priority_queue`, `std::sort`, `std::stable_sort`, `std::lower_bound`, `std::function`, …). The project's purpose is a *version-control engine built from data-structure ideas*, not re-implementing textbook containers.
3. **Hand-write only what has no standard equivalent** and is central to the project or the syllabus: `HexTrie`, `BPlusTree`, LSD radix sort (counting-sort passes), interpolation search, Myers diff, Rabin–Karp delta encoder, SHA-256, and all DAG/merge/blame/bisect algorithms (Section 5.2). Everything else uses the STL. Do **not** write custom hash tables, heaps, stacks, queues, AVL/RB trees or sorting routines.
4. **Be explicit about which STL structure plays which role** (Section 5.1 table) and what it is underneath (e.g. `std::unordered_map` = chained hash table; `std::map` = red-black tree; `std::priority_queue` = binary heap; `std::stable_sort` = merge sort). The report maps these to the syllabus topics; the code uses the standard implementations.
5. **SHA-256 is a single self-contained file** (`src/core/sha256.cpp`, FIPS 180-4, ~120 lines) so the build needs no external library. No OpenSSL; no `sha256sum` calls in production code (tests may call `sha256sum`/`git` as oracles).
6. **No third-party libraries.** zlib is *not* used in the core; an optional `-DCHRONICLE_ZLIB` build flag (default off) may add it (D-05).
7. **No GUI / web front end.** Visualisation = terminal output plus `chronicle graph --dot` (plain text Graphviz DOT; Graphviz itself is optional and not a dependency).

### 0.3 Working rules
- Work **milestone by milestone** (Section 13). Do not start milestone *n+1* until milestone *n*'s Definition of Done passes (`make test` green).
- After each milestone: run `make test`, then `git commit -m "M<n>: <title>"`.
- Every data structure gets unit tests with invariant checkers *before* it is used by higher layers.
- Keep functions small; keep each `.cpp` under ~600 lines; comment the *why* of every non-obvious algorithm step and cite its complexity in a header comment.
- Never silently swallow errors: return `Status`/error codes up to `main`, print `chronicle: <message>` to stderr, use exit codes in Section 11.1.
- Do not add features not listed here. Stretch items are explicitly marked `EXPERIMENTAL`/`OPTIONAL`.

### 0.4 Repo layout to create (D-02)
```
chronicle/
├── Makefile                     # targets: all, test, bench, asan, clean
├── README.md
├── docs/ DESIGN.md (this file), DECISIONS.md
├── src/
│   ├── main.cpp                 # arg parsing + command dispatch
│   ├── cli/                     # one file per command (cmd_add.cpp, cmd_commit.cpp, …)
│   ├── ds/                      # ONLY structures with no STL equivalent: trie.hpp bplustree.hpp
│   │                            # radixsort.hpp interpsearch.hpp  (everything else = STL)
│   ├── core/                    # sha256.cpp/.hpp  id.hpp  varint.hpp  status.hpp  strutil.hpp  timeutil.hpp
│   ├── store/                   # object.cpp  loose.cpp  pack.cpp  packidx.cpp  delta.cpp
│   │                            # refs.cpp  index.cpp  config.cpp  ignore.cpp  lock.cpp
│   ├── engine/                  # dag.cpp  revparse.cpp  logwalk.cpp  diff.cpp  treediff.cpp  merge.cpp
│   │                            # blame.cpp  history.cpp  timetravel.cpp  bisect.cpp  gc.cpp  fsck.cpp  graphview.cpp
│   ├── exp/                     # reconcile.cpp  cherrypick.cpp  patchid.cpp  astmerge/ (optional)
│   └── util/                    # fs.cpp (filesystem wrappers)  proc.cpp (fork/exec wrapper)
├── tests/
│   ├── framework.hpp            # tiny CHECK/CHECK_EQ macros, test registry (no external framework)
│   ├── unit/                    # test_<module>.cpp
│   └── integration/             # *.sh scenario scripts (bash)
├── bench/                       # bench_*.cpp  + results/ (CSV, git-ignored)
└── scripts/                     # gen_repo.sh  demo_bisect.sh  demo_all.sh
```

---

## 1. Project Definition

### 1.1 Origin and originality `[ORIGINAL][REVISED]`
ForgeVCS was a Git clone built on a Merkle DAG: immutable, content-addressed objects (blobs → trees → commits), linked into a directed acyclic graph that records history. The point was never to reproduce Git's UX but to understand *why* Git works — hashing gives integrity and deduplication for free, immutability gives safe history, and the DAG shape (not a tree) is what makes merging possible at all.

Because that exact framing is not unique to this project, the plain "Git clone" version is not submitted. Nothing about the Merkle DAG idea is wrong — it is the right foundation. What was missing was a **thesis beyond storage**: what do you do with an immutable object graph once you have one? Chronicle's answer is in 1.2, and it is realised through the five add-on ideas evaluated in Section 2.

### 1.2 Name, pitch, thesis `[ORIGINAL]`
- **Name:** *Chronicle* (kept — reads as "a record of history", every CLI verb fits it). Alternative considered: *Anamnesis* ("recollection"; fits time-travel/blame/bisect) — rejected because it is harder to say in a viva and doesn't telegraph "version control".
- **One-line pitch:** A version-control engine where the immutable object graph isn't just storage — it's the data structure you compute on to debug, merge, and optimize history.
- **Central thesis:** Every advanced feature (blame, bisect, structural merge, delta compression, multi-remote reconciliation) is a different *traversal or query* over the same Merkle DAG. The project builds that DAG properly once, then shows genuinely different algorithms operating on it.
- **Why Git alone doesn't satisfy the course goal:** Git *uses* these structures but hides them behind decades of optimized C plumbing. Reimplementing the object model and graph algorithms from scratch, owning memory layout, is what makes this a data-structures project instead of a "learn Git's CLI" project.
- **Real-life mapping (required by the course PBL rule):** a version-control system is a real application every software team uses daily; each data structure in Section 5 is chosen because the real problem demands it (e.g., the staging area must be iterated in sorted path order → red-black tree (`std::map`); abbreviated commit ids must resolve by prefix → trie).

### 1.3 Scale `[ADDED]`
Target repositories: up to ~10⁴ commits and ~10⁵ objects with files ≤ 64 MiB (configurable). The implementation is single-process, single-machine, no networking.

---

## 2. Evaluation of the Five Candidate Ideas `[ORIGINAL]`

| Idea | Core concept | DSA depth | Uniqueness | Scope risk | Verdict |
|---|---|---|---|---|---|
| **Chronos** | Object-store-level time travel + auto-bisect | High (persistent structures, binary search over DAG) | High — most VCS projects never touch this | Medium | Keep, split into CORE (time travel) + ADVANCED (bisect) |
| **MergeMind** | AST-aware three-way merge | High (tree edit distance) | High | High — full parser is a project on its own | Keep, heavily scoped down (EXPERIMENTAL, toy grammar) |
| **PackSmith** | Packfile format, delta encoding | Very high (real Git internals) | Highest — "proves you understand Git" | High if Git-binary-compatible | Keep, drop hard compatibility requirement |
| **BlameFast** | Efficient blame via reverse diff + memoization | Medium-high, self-contained | Medium | Low — well-scoped | Keep as-is, strong CORE candidate |
| **Shardgraph** | Multi-remote DAG reconciliation (LCA, topo sort, cherry-pick) | High (classic graph algorithms) | Medium (standard algorithms, rarely applied to VCS) | Low if simulated, not networked | Fold into the core History Engine; keep only the thin "reconcile two DAGs" layer as EXPERIMENTAL |

**Key correction `[ORIGINAL]`:** Shardgraph isn't a sixth subsystem. Once proper merge (needs LCA) and proper log (needs topological order) exist, 80 % of Shardgraph already exists. The remaining 20 % — reconciling two independently-grown DAGs — is a thin, high-value layer on top.

---

## 3. Syllabus Alignment `[ADDED]`

Source: *Data Structures* syllabus (modules 1–6; CO C210.1–C210.5) and *Data Structure Lab* syllabus (C270.1–C270.6). PBL rule from both: "develop a project using concepts of data structures; assessed on how data structures are used and mapped to a real-life problem; C/C++/Java."

| Syllabus topic | Used in Chronicle? | Where / why (or why not) |
|---|---|---|
| Array, linked list, stack, queue (M1) | **Yes (STL)** | `std::vector` (arrays, adjacency lists), `std::stack` (iterative DFS), `std::queue` (BFS, Kahn), `std::list`/`vector` for chains |
| Searching: binary, interpolation (M2) | **Yes** | Pack-index lookup: **hand-written** interpolation search (ids are uniformly distributed) with `std::lower_bound` as baseline |
| Median search | No | No natural use; not forced |
| Hashing: hash table, chaining, probing (M2) | **Yes** | `std::unordered_map`/`unordered_set` (chaining in libstdc++): object index, refs, history index, blame memo, visited sets. Probing: optional ~40-line linear-probing set used only in benchmark B2 to compare against chaining |
| Sorting: merge, radix, count (M2) | **Yes** | `std::stable_sort` (merge sort) for tree entries/paths; **hand-written** LSD radix sort with counting-sort passes (pack index ids) |
| Quick / bucket sort | No | Not needed; benchmark may compare against merge sort optionally |
| Binary tree, k-ary tree, traversals (M3) | **Yes** | Tree *objects* are k-ary Merkle trees; tree walk = traversal; AST for experimental merge |
| Priority queue using binary heap, heap sort (M3) | **Yes (STL)** | `std::priority_queue` (binary heap): date-ordered log walk, Kahn tie-breaking for bisect; `std::make_heap/sort_heap` in tests |
| Threaded BT, binomial heap, Fibonacci heap (M3) | No | No genuine use |
| BST, AVL, Red-black tree (M4) | **Yes (STL RB tree)** | Staging index = `std::map<path, IndexEntry>` (a red-black tree; in-order iteration = canonical tree order). AVL not separately used |
| B tree / B+ tree (M4) | **Yes (B+)** | `BPlusTree` time index: `log --since/--until`, "repo state as of time T", date-bounded bisect |
| Graph: adjacency list, DFS, BFS (M5) | **Yes** | Commit DAG; reachability, GC, merge-base, topological order |
| Shortest path, MST (M5) | No | No natural use in a commit DAG; not forced |
| Interval / segment tree (M6) | No | Not used (blame uses arrays of line origins) |
| Tries (M6) | **Yes** | Hex-trie for abbreviated object-id resolution and ambiguity detection |
| Suffix tree / suffix array (M6) | No | No genuine use |
| Complexity analysis (M2) | **Yes** | Section 12.3: empirical complexity validation by benchmarks |
| Union-Find | Not in syllabus; **rejected** | No dynamic-connectivity need (see 15.1) |

Honesty rule: the report must state clearly which syllabus structures are *not* used and why; do not retrofit them.

---

## 4. Final Feature Set

Priority tiers `[ORIGINAL]`: **CORE** = must ship; **ADVANCED** = ship after CORE; **EXPERIMENTAL** = only after everything else; **FUTURE** = explicitly excluded. Rows marked `[ADDED]`/`[REVISED]` differ from the original table.

| # | Feature | Main DS | Algorithm | Complexity | Priority |
|---|---|---|---|---|---|
| F1 | Object store (blob/tree/commit) | Hash table (id → object location) | SHA-256 content hashing | O(1) expected lookup | CORE |
| F2 | Merkle DAG / commit graph | DAG, adjacency list + generation numbers `[ADDED]` | — | O(1) edge insert | CORE |
| F3 | init / add / status / commit / log | Staging index = **`std::map` (red-black tree)** `[REVISED]`, linked list of commits | diff vs last tree; date-ordered topological walk with **`std::priority_queue`** `[ADDED]` | O(files changed); log O((V+E) log V) | CORE |
| F4 | Branch / checkout / reset --hard | Hash map (branch name → commit id) | tree materialisation | O(1) ref ops | CORE |
| F5 | Diff (line-level) and tree diff | Arrays, interned-line hash table | Myers O(ND); Merkle tree diff skips equal subtrees `[ADDED]` | O(N·D); tree diff O(changed) | CORE |
| F6 | Merge (three-way, line-based, tree-level) | DAG | merge-base (best common ancestors) + diff3 | O(V+E) for bases | CORE |
| F7 | BlameFast | Line-origin arrays, memo hash map | Parent-pass blame via Myers, DP over path history | Σ over commits touching path of O(N·D); memo amortises repeats | CORE |
| F8 | History Engine (log ordering, ancestor queries, divergence) | DAG, visited-set (`std::unordered_set`) | DFS/BFS, Kahn topological sort, merge-base, generation-pruned ancestry | O(V+E) | CORE |
| F9 | Abbreviated-id resolution `[ADDED]` | **Trie** (16-ary, hex) | prefix descent, ambiguity count | O(len) | CORE |
| F10 | Time travel / arbitrary-state reconstruction (`at`, `export`) | Persistent (immutable, structurally shared) tree; **B+ tree** time index `[REVISED]` | root-tree walk from any commit; time → commit via B+ lookup | O(size of subtree touched); lookup O(log n) | ADVANCED |
| F11 | Object history index ("when did X first appear/change") | Reverse index: path → events, blob id → commits (hash tables); persisted `history.idx` | built per commit, or backfilled by one DAG walk | O(1) query after O(V+E) build | ADVANCED |
| F12 | Automated bisect | Linearised commit list from DAG (+ optional weighted midpoint) | binary search + test execution (`fork/exec`) | O(log N) test runs, conditional on contiguity | ADVANCED |
| F13 | Delta compression + packfile + pack index `[REVISED]` | Hash index of base blocks (`std::unordered_map`), delta chains, sorted id array | Rabin–Karp rolling-hash delta, window-based base selection (heuristic, **not** claimed optimal), **LSD radix sort**, **interpolation search** | selection O(N·W); idx build O(32·N); lookup expected O(log log N) | ADVANCED |
| F14 | Garbage collection + fsck | Reachability set (`std::unordered_set`) | DFS/BFS from all refs | O(V+E) | ADVANCED |
| F15 | Multi-remote reconciliation (simulated, local paths) + cherry-pick + patch-id | Two DAGs, commit-id hash sets | set difference, merge-base across DAGs, patch-id equivalence detection | O(V+E) per DAG | EXPERIMENTAL |
| F16 | Structural (AST) merge — toy grammar `MiniLang` | AST (small subset grammar) | Zhang–Shasha tree edit distance on top-level items | O(n·m) restricted trees | EXPERIMENTAL / OPTIONAL |
| F17 | Terminal DAG graph + DOT export `[ADDED]` | DAG | lane assignment over topological order | O(V+E) | CORE (ASCII) / ADVANCED (DOT) |
| F18 | Benchmarks + complexity validation `[ADDED]` | — | timing harness, log-log slope estimate | — | ADVANCED |
| F19 | Git-packfile binary compatibility | — | — | — | **FUTURE WORK** (excluded) |

---

## 5. Data-Structure Catalogue

### 5.1 STL structures used (no custom implementation) `[REVISED]`
| Role in Chronicle | STL type | What it is underneath | Syllabus topic |
|---|---|---|---|
| Object-location index, refs (branch → id), history index (`path → events`, `blob id → commits`), blame memo `(commit idx, path id) → origin vector`, interned lines for diff, config | `std::unordered_map` | hash table with chaining | hashing |
| Visited sets (DFS/BFS/GC/merge-base/log walk), commit-id sets for reconcile | `std::unordered_set` | hash set | hashing |
| Staging index keyed by repo-relative path (bytewise order; in-order iteration gives tree-object order for free) | `std::map<std::string, IndexEntry>` | red-black tree | BST / balanced BST |
| Date-ordered log walk (max-heap on time, tie-break generation, id); Kahn tie-breaking for bisect (min-heap) | `std::priority_queue` | binary heap | priority queue |
| Iterative DFS, GC mark, merge-base ancestor walks | `std::stack` | stack | stack |
| BFS, Kahn queue, DAG loading | `std::queue` | queue | queue |
| Commit DAG adjacency (`parents`, `children`), tree entries, lines of a file, blame origin vectors, history events | `std::vector` | dynamic array | arrays / lists |
| Delta chains, pack candidate ordering | `std::vector` / `std::list` | | linked list |
| Tree-entry sorting, path sorting, generic ordering | `std::stable_sort` / `std::sort` | merge sort / introsort | sorting |
| Binary search baselines, time lookups on sorted vectors | `std::lower_bound` | binary search | searching |
| Weighted-bisect ancestor sets | `std::vector<bool>` or `std::bitset`-style `std::vector<uint64_t>` | bit vector | arrays |

### 5.2 Hand-written structures and algorithms (only where no STL equivalent exists) `[ADDED]`
These are the parts the DSA report should explain in depth. They live in `src/ds/` (structures) or `src/engine/`, `src/store/` (algorithms).

**`HexTrie`** (`trie.hpp`) — abbreviated-id resolution.
- 16-ary trie over lowercase hex digits; node pool in `std::vector` (child indices `int32`, −1 = none); each node stores `subtree_count`.
- Ops: `insert(id)`, `resolve(prefix)` → `{NONE | UNIQUE(id) | AMBIGUOUS(count, up to 10 candidate ids)}`. O(prefix length). Minimum prefix length 4.
- Use: `revparse` for abbreviated ids; `chronicle find <prefix>`; shortest unique prefix (≥ 8 chars) in log output via `subtree_count`.

**`BPlusTree<K,V>`** (`bplustree.hpp`) — time index.
- Order `m` = max keys per node (default 64; tests also run `m=4` to force many splits). Internal nodes hold separator keys; **all values in leaves**; leaves linked left→right. Composite key `(int64 time, int32 commit_idx)` makes keys unique.
- Ops: `insert` (split upward, iterative), `bulkLoad(sorted pairs, fill=0.7)`, `lowerBound(key)`, `rangeScan(lo, hi)` via leaf chain. **No delete** (history is append-only; the tree is rebuilt on load/GC). Invariants (tested): all leaves same depth; node fill between ⌈m/2⌉ and m except root; leaf chain strictly ascending.
- Use: `log --since/--until`, `at <time>`, date-bounded bisect.

**LSD radix sort** (`radixsort.hpp`): `radixSortIds(vector<IdEntry>&)` over the 32 key bytes, each pass a counting sort on one byte (256 counters), stable, O(32·(n+256)). Used to build pack indexes.

**Interpolation search** (`interpsearch.hpp`): `interpolationSearch(vector<IdEntry>&, id)` uses the first 8 id bytes as a big-endian `uint64`; estimates `pos = lo + (target−key[lo])·(hi−lo)/(key[hi]−key[lo])` using `unsigned __int128` arithmetic; after 24 probes or when the range stops shrinking it falls back to `std::lower_bound`. Expected O(log log n) on uniform ids (SHA-256 ids are uniform).

**`Dag`** (`engine/dag.cpp`) — the commit graph.
- Nodes = commits with dense `int` index. `struct CommitNode { Id id; int64 time; vector<int> parents; vector<int> children; int32 generation; }`. `generation = 1 + max(parent generations)` (root = 1), computed in one topological pass. id → idx via `std::unordered_map`.
- Loaded lazily by BFS from all refs + HEAD; cached for the process lifetime only.
- Generation numbers bound ancestry checks: `isAncestor(a,b)` prunes any node whose generation < generation(a).

**Original coverage notes `[ORIGINAL]` and what changed**
- **Hash table** — essential everywhere (object store, refs, blame line-map, reverse index); the backbone of content addressing.
- **DAG / adjacency list** — the commit graph; *this is the project*.
- **Linked list** — delta chains and parent-pointer walks; honest note for the report: arrays (vectors) often beat linked lists for commit sequences in practice.
- **Persistent (immutable) tree** — per-commit file-tree snapshots with structural sharing; essential to the time-travel thesis.
- **Priority queue** — originally "lightly justified, optional" (delta-base ranking). **[REVISED]** the delta heuristic does not need it, but the date-ordered log walk and deterministic topological ordering do, so it is CORE (via `std::priority_queue`).
- **Stack/Queue** — DFS uses a stack, BFS a queue; used in history traversal, GC, bisect. Essential, not glamorous — say so plainly in the report.
- **AST (tree)** — only for the experimental structural-merge subsystem; toy grammar.
- **Union-Find** — investigated and **rejected as forced** (nothing needs dynamic connectivity; reachability covers every case).
- **Trie** — originally rejected (no prefix-search requirement). **[REVISED → ADOPTED]** because abbreviated-id resolution is a real prefix-search problem and the syllabus lists tries.

---

## 6. Algorithms and Complexity `[ORIGINAL]` + `[ADDED]` precision

| Algorithm | Purpose | Complexity | Notes |
|---|---|---|---|
| SHA-256 | content addressing | O(n) bytes | Hand-written; verified by NIST vectors (Section 12.1) |
| DFS / BFS | reachability (GC), history traversal, ancestry | O(V+E) | iterative, `std::stack`/`std::queue`, `std::unordered_set` visited |
| Kahn topological sort | log order, bisect linearisation | O(V+E) (O((V+E) log V) with heap tie-breaking) | deterministic tie-break: newer `time` first, then smaller id |
| Merge base (best common ancestors) | merge | O(V+E) | exact algorithm in 10.3 |
| Binary search on linearised DAG (bisect) | bug localisation | O(log N) test runs | **conditional**: bug region must be contiguous in the chosen linearisation; stated explicitly in the report |
| Myers diff | line diff | O(N·D) time, O(D²) trace memory | D capped at 10 000 → fallback "replace whole file" |
| Merkle tree diff | tree-level diff | O(changed paths) | equal subtree ids skipped |
| diff3 merge | three-way file merge | O(N·D) | overlap rules in 10.4 |
| Parent-pass blame (DP over path history) | blame | see 10.5 | memoised per `(commit, path)` |
| Rabin–Karp rolling hash delta | delta encoding | O(n+m) expected | block size 16 |
| Interpolation search | pack idx lookup | expected O(log log n), worst O(n) → bounded by fallback | |
| LSD radix sort | build pack idx | O(32·N) | |
| Tree edit distance (Zhang–Shasha) | experimental AST merge | O(n·m·min(depth,leaves)²) | bounded to small trees (≤ 200 nodes) |
| Patch-id | cherry-pick detection | O(diff size) | 10.12 |

**LCA note `[ORIGINAL]`:** an ancestor-set approach is O(V+E); a sparse-table LCA may be *mentioned* as a possible optimisation but is **not** implemented.

---

## 7. Architecture

### 7.1 Feature dependency `[ORIGINAL]`
```
Object Store (hash table)
        │
        ▼
   Merkle DAG (commit graph)
        │
        ▼
  History Engine (DFS/BFS, topo sort, merge-base, generation numbers)
   ┌────────┼─────────┬───────────┐
   ▼        ▼         ▼           ▼
Blame   Time Travel  Bisect   Merge (line + tree)
   │        │         │           │
   └────────┴─────────┴───────────┘
                │
                ▼
     [experimental layer]
   AST Merge   Multi-Remote Reconcile   Delta/Pack + GC
```
Everything above the "experimental layer" line is one connected engine — nothing is a bolt-on. (Delta/Pack/GC are ADVANCED, not experimental; the diagram keeps the original grouping.)

### 7.2 Layers `[ADDED]`
1. **Core utilities** — `Status`, ids, SHA-256, varints, string/time helpers.
2. **Data structures** — Section 5.
3. **Store** — loose objects, packs, refs, index, config, ignore rules, repo lock.
4. **Engine** — DAG, revision parsing, diff, merge, blame, history, bisect, time travel, GC/fsck, graph view.
5. **CLI** — thin command handlers; no algorithm lives here.
6. **Experimental** — reconcile, cherry-pick, patch-id, AST merge.

### 7.3 Core interfaces (sketch — keep signatures stable)
```cpp
struct Id { uint8_t b[32]; /* hex(), fromHex(), operator==, hash64() */ };
struct Status { int code; std::string msg; bool ok() const; };            // never throw across module boundaries

class ObjectStore {                    // facade over loose + packs
 public:
  Status put(ObjType t, const std::string& payload, Id* out);             // idempotent
  Status get(const Id& id, ObjType* t, std::string* payload);            // resolves deltas
  bool   has(const Id& id);
  Status forEachId(/*callback via simple function pointer + void* ctx*/);
};
struct TreeEntry { std::string name; uint32_t mode; Id id; };
struct Commit    { Id tree; std::vector<Id> parents; std::string author; int64_t time; std::string tz; std::string message; };
class Index { std::map<std::string, IndexEntry> entries; Status load(); Status save(); };
class Dag   { int idxOf(const Id&); const CommitNode& node(int); bool isAncestor(int a,int b); /* … */ };
```
(`std::function` callbacks are fine.)

---

## 8. Data Formats and On-Disk Layout `[ADDED]` (D-03, D-04)

### 8.1 Layout
```
<worktree>/.chronicle/
  HEAD                    "ref: refs/heads/main\n"   or   "<64 hex>\n" (detached)
  config                  key=value lines
  index                   staging index (text)
  objects/ab/<62 hex>     loose objects (first 2 hex chars = directory)
  objects/pack/pack-<hex>.pack  and  pack-<hex>.idx
  objects/tmp/            temp files for atomic writes
  refs/heads/<branch>     "<64 hex>\n"
  history.idx             object-history index (append-only text)
  MERGE_STATE             exists only during an unresolved merge
  BISECT                  exists only during a bisect session
  lock                    repository write lock (created O_EXCL)
```
All writes to HEAD, refs, index, MERGE_STATE, BISECT use *write temp + fsync + rename* in the same directory. Directory names inside `refs/heads/` may contain `/` (nested dirs created on demand).

### 8.2 Objects (loose)
- File bytes = `"<type> <decimal size>\0" + payload`, uncompressed. **Object id = SHA-256 of exactly these bytes** (D-04). Types: `blob`, `tree`, `commit`.
- This header scheme is identical to Git's, so for validation: in a scratch repo created with `git init --object-format=sha256`, `git hash-object <file>` must equal `chronicle hash-object <file>` for the same bytes. (Compatibility of *packs/other formats* is not a goal.)
- Writing an object that already exists is a no-op. Reading verifies `sha256(file bytes) == id` and returns `E_CORRUPT` on mismatch.
- **Blob payload** = raw file bytes. **Symlinks** are stored as blobs containing the link target, with tree mode `120000`.
- **Tree payload** = concatenation of entries `"<mode> <name>\0" + <32 raw id bytes>`, sorted by `name` bytewise (no special directory-slash rule), modes `100644`, `100755`, `40000` (directory), `120000` (symlink). Forbidden names: empty, `.`, `..`, `.chronicle`, anything containing `/` or NUL. **Empty directories are not tracked.**
- **Commit payload** (text):
```
tree <64hex>
parent <64hex>          (zero or more lines; first = mainline parent)
author <name> <<email>> <unix_seconds> <+hhmm|-hhmm>

<message, UTF-8, always ends with "\n">
```
  Author name/email come from `config` (`user.name`, `user.email`) or env `CHRONICLE_AUTHOR_NAME`/`CHRONICLE_AUTHOR_EMAIL`, default `Unknown <unknown@localhost>`. There is no separate committer line.

### 8.3 Index file
```
CHRONICLE-INDEX 1
<mode>\t<id hex>\t<size>\t<mtime_ns>\t<escaped path>
...
#SHA256 <hex of all preceding bytes>
```
Path escaping: percent-encode `%`, TAB, LF, CR, and bytes < 0x20 or 0x7F as `%XX`. Entries sorted by path. In memory the index is a `std::map`. **Stat cache rule:** if `(size, mtime_ns)` of a working file equals the index entry *and* `mtime_ns` is older than (index write time − 2 s), the file is assumed unchanged; otherwise it is rehashed ("racy timestamp" guard).

### 8.4 history.idx
```
CHRONICLE-HISTORY 1
<commit id>\t<A|M|D>\t<mode>\t<blob/tree id or ->\t<escaped path>
```
One line per path whose entry differs from the commit's **first parent** (all paths for a root commit are `A`). Appended on every commit/merge; `chronicle reindex` rebuilds it by one DAG walk (also used if the file is missing or its last line is truncated).

### 8.5 config keys and defaults
| Key | Default | Meaning |
|---|---|---|
| `user.name`, `user.email` | see 8.2 | author |
| `core.maxfilebytes` | 67108864 | files larger are refused by `add` |
| `core.abbrev` | 8 | minimum abbreviated id length in output |
| `pack.window` | 10 | candidate bases examined per object |
| `pack.maxdepth` | 50 | max delta chain length |
| `pack.minsaving` | 50 | a delta is accepted only if its size is below (100 − `pack.minsaving`) % of the object's full size, i.e. < 50 % by default |
| `gc.graceseconds` | 0 | age before unreachable loose objects may be deleted |

### 8.6 `.chronicleignore`
One glob per line, `#` comments, blank lines ignored, trailing `/` = directories only, `*` and `?` supported (`fnmatch(3)` may be used), patterns match the path relative to the repo root and also the basename. `.chronicle/` is always ignored.

### 8.7 Revision syntax
`HEAD`, `<branch>`, full 64-hex id, abbreviated id (≥ 4 hex, via trie), each followed by any chain of `~N` (N-th first-parent ancestor) and `^N` (N-th parent; `^` = `^1`). A name that is both a branch and a valid hex string resolves to the branch unless it is exactly 64 hex chars. Ambiguous abbreviations are an error listing up to 10 candidates.

---

## 9. CLI Specification `[ADDED]` (D-06)

Binary name `chronicle`. Global: `-C <dir>` run as if started in `<dir>`; `--help`; `--version`. Output goes to stdout; diagnostics to stderr. Exit codes in 11.1.

| Command | Behaviour |
|---|---|
| `init [dir]` | Create `.chronicle/` (HEAD → `refs/heads/main`). Refuses if one exists. |
| `add <path>...` | Stage files/dirs recursively (`.` = everything not ignored). Directories are walked; ignored paths skipped. Removing a deleted tracked file from the index requires `add` of that path too (stages the deletion). |
| `rm [--cached] <path>...` | Unstage and (unless `--cached`) delete from working tree. |
| `status` | Three sections: *Staged* (HEAD vs index), *Not staged* (index vs working tree: modified/deleted), *Untracked*. Also shows branch, or `detached at <id>`, or `merging <branch>` with conflicted paths. |
| `commit -m <msg>` | Build tree objects from the index (`std::map` in-order), create commit with parent(s) from HEAD (+ `MERGE_STATE` theirs), advance branch, append `history.idx`. Refuses with nothing staged (unless merge) or unresolved conflicts. |
| `log [--oneline] [--graph] [--since T] [--until T] [-n N] [<rev>] [-- <path>]` | Date-ordered topological walk (10.2). `--since/--until` use the B+ time index. `-- <path>` filters to commits touching the path via `history.idx`. |
| `show <rev-or-id>` | For a commit: header + diff vs first parent. For a blob/tree id: content/listing. |
| `diff [--cached] [<rev> [<rev>]] [-- <path>]` | Unified diff: working vs index (default), index vs HEAD (`--cached`), or between two revisions. Binary files print `Binary files differ`. |
| `branch [<name> [<rev>]] [-d <name>] [--list]` | Create/delete/list branches. Cannot delete the current branch. |
| `checkout <branch\|rev> [-b <newbranch>]` | Switch HEAD and working tree. Requires a clean tracked state (10.8). Revision that is not a branch → detached HEAD. |
| `reset --hard <rev>` | Move current branch (or detached HEAD) to `<rev>`, reset index and working tree to it. Also used to abort a merge. |
| `merge <branch> [--no-ff] [--abort]` | Three-way merge (10.4). `--abort` = `reset --hard HEAD` + remove `MERGE_STATE`. |
| `blame <path> [<rev>]` | Per-line attribution: `<short id> (<author> <date>) <lineno>) <text>`. |
| `history <path>` / `history --first <path>` / `history --blob <id>` | Object-history queries (10.6). |
| `at <rev-or-time> <path>` | Print file content as of a revision or time (10.7). Time = `YYYY-MM-DD[THH:MM[:SS]]` (local time) or Unix seconds. |
| `export <rev-or-time> <dir>` | Materialise a whole tree read-only into `<dir>` without touching HEAD/index/working tree. |
| `bisect start \| bad [<rev>] \| good [<rev>] \| skip [<rev>] \| run <cmd…> \| reset \| log` | Automated bisect (10.9). |
| `cat-file -t\|-s\|-p <id>` | Type / size / pretty-print of an object. |
| `hash-object [-w] <file>` | Print the blob id (and store it with `-w`). |
| `ls-tree [-r] <rev> [<path>]` | List tree entries. |
| `find <prefix>` | Resolve an abbreviated id via the trie; prints id and type or the ambiguity list. |
| `pack` | Delta-compress and pack all loose objects (10.10). |
| `gc [--dry-run] [--grace-seconds N]` | Remove unreachable objects, repack (10.11). |
| `verify` | fsck: recompute every object hash, check links, report dangling objects (10.11). |
| `reindex` | Rebuild `history.idx`. |
| `graph [--dot] [<rev>…]` | ASCII DAG (default) or Graphviz DOT text on stdout (10.12). |
| `stats [--sharing]` | Counts of loose/packed objects, sizes, pack savings; with `--sharing`, structural-sharing report (10.7). |
| `reconcile <other-repo-dir> [--fetch]` *(EXPERIMENTAL)* | Compare/sync two local repos (10.13). |
| `cherry-pick <rev>` *(EXPERIMENTAL)* | Apply a commit's change onto HEAD (10.13). |

---

## 10. Subsystem Specifications

### 10.1 Object store engine `[ADDED]`
- `put`: compute id; if loose file or any pack contains it → return; else write `objects/tmp/<random>` then `rename` to `objects/ab/<rest>` (create dir if needed, mode 0755 dirs, 0444 files).
- `get`: loose first; else search each pack's index (interpolation search). Delta objects resolved iteratively: follow base links up to `pack.maxdepth+5`, applying deltas from the base outward; cache resolved objects in a bounded `ChainedHashMap` (max 512 entries; cleared when full).
- Loose objects are never modified. Pack files are written to a temp name then renamed; index file written last.

### 10.2 History engine (log walk, ancestry) `[ORIGINAL][ADDED]`
**Date-ordered topological walk** (used by `log`):
1. From the requested tips, compute the reachable set by BFS (`std::unordered_set`); for each node count `pending_children` = children inside the set.
2. Heap = max-heap by `(time, generation, id)`. Push tips with `pending_children == 0`.
3. Pop → emit; for each parent decrement `pending_children`; when it reaches 0, push.
4. Result: newest-first order where a commit never appears before any of its descendants, even with skewed clocks. O((V+E) log V).

`isAncestor(a, b)`: BFS/DFS from `b` through parents, pruning nodes with `generation < generation(a)`; O(visited).

### 10.3 Merge base (best common ancestors) `[ADDED]`
```
A = ancestors(a) ∪ {a};  B = ancestors(b) ∪ {b};  C = A ∩ B                (unordered_set lookups)
mark = empty unordered_set
for c in C processed in DESCENDING generation order:
    if c in mark: continue
    best.add(c)                           # no already-processed common node is a descendant of c? → c is maximal
    mark all proper ancestors of c        # BFS over parents, stop at nodes already marked
return best                               # usually exactly one element
```
- 1 base → use it. 0 bases → unrelated histories → error unless `--allow-unrelated` (then merge against an empty tree).
- **Several bases (criss-cross):** choose the base with the greatest `time`, ties → smallest id; print `warning: <n> merge bases, using <id>`. A recursive "virtual base" merge is FUTURE WORK. (D-07)

### 10.4 Merge (three-way) `[ORIGINAL][ADDED]`
Algorithm for `merge <branch>`:
1. Resolve `ours` = HEAD commit, `theirs` = branch tip. If `theirs` is an ancestor of `ours` → "Already up to date". If `ours` is an ancestor of `theirs` and not `--no-ff` → **fast-forward** (move branch, update working tree like checkout).
2. Else find base (10.3); run **tree-level three-way merge** over `(base, ours, theirs)` tree ids, per path:
   - all three equal → keep; `base == ours` → take theirs; `base == theirs` → take ours; `ours == theirs` → take either.
   - all three are trees → recurse (skip when ids equal).
   - all three are regular files → **diff3** (below). Mode changes: if only one side changed the mode, take it; both changed differently → conflict.
   - modify/delete (one side deleted, other changed) → conflict, keep the modified version in the working tree.
   - add/add with different content → conflict (markers via diff3 against an empty base).
   - file vs directory → conflict, keep ours, report.
   - Renames are **not** detected (appear as delete + add). Documented limitation.
3. Clean result → create merge commit with parents `[ours, theirs]`, message `Merge branch '<name>'`.
4. Conflicts → write marker files to the working tree, stage clean results, write `MERGE_STATE` (theirs id, branch name, list of conflicted paths), print `CONFLICT (<kind>): <path>`, exit 3. `commit` is refused until every conflicted path has been re-`add`ed (any content, even with markers, is accepted — resolution is the user's responsibility, as in Git). Then `commit` (no `-m` needed; default message) creates the two-parent merge commit and removes `MERGE_STATE`.

**diff3 rule set (final):**
- Compute Myers diffs `base→ours` and `base→theirs`; convert each to *change regions* `[bs, be)` in base lines → replacement lines (pure insertion has `bs == be`).
- Two regions **overlap** if `a.bs < b.be && b.bs < a.be`, **or** `a.bs == b.bs` (same-position insertion/start). Regions that merely touch (`a.be == b.bs`) are independent.
- Process base left-to-right. Non-overlapping region → apply as is. Overlapping cluster (transitively merged) → compute both sides' resulting text for the cluster's base span; identical → take it; otherwise **conflict**.
- Conflict text:
```
<<<<<<< ours (<current branch name or HEAD>)
...ours lines...
=======
...theirs lines...
>>>>>>> theirs (<other branch name>)
```
- Binary files (NUL byte in first 8000 bytes) with different content on both sides → conflict, keep ours (no markers).
- Line endings are preserved byte-exact; final-newline state is part of the last line's identity.

### 10.5 BlameFast `[ORIGINAL][ADDED]`
Goal: for path `p` at revision `R`, assign every line the commit that introduced it.
- `H` = list of commits (from `history.idx`) that changed `p` and are ancestors of `R`, in topological order, **oldest first**. Merge commits count when `p` differs from the first parent; handle all parents per the next rule.
- DP in that order. For commit `C` with file lines `L(C)`:
  - If `p` is absent in all parents (added) → every line blamed to `C`.
  - Else for each parent `Pi` that contains `p`: compute Myers `Pi.p → C.p`; lines of `C` that map to *unchanged* lines of `Pi` inherit `origin[Pi]` for that line. A line matched in several parents takes the **first parent** that matches. Unmatched lines → blamed to `C`.
  - Result `origin[C]` = vector of commit indices, one per line.
- **Memoisation (the "Fast" in BlameFast):** key `(commit idx, path id)` → origin vector in a `std::unordered_map`. If `p`'s blob id at `C` equals its blob id at the first parent, `origin[C]` aliases the parent's vector (no diff). Computation for `R` reuses everything already computed for earlier revisions in the same process.
- Complexity: Σ over commits in `H` of O(N·D); naive recomputation per query would be O(|H|·N·D) *per revision asked*. Benchmark B5 demonstrates the amortisation (blame at K successive revisions).
- Out of scope: rename/copy tracking, `-L` ranges.
- Output columns: `<short id> (<author> <YYYY-MM-DD>) <line no>) <text>`.

### 10.6 Object history index `[ORIGINAL][ADDED]`
Format in 8.4. In memory (loaded on demand): `std::unordered_map<path, vector<Event>>` and `std::unordered_map<blobid, vector<commit idx>>`.
- `history <path>` → events newest-first with `A/M/D` and short ids.
- `history --first <path>` → first commit where the path appears.
- `history --blob <id>` → every commit/path where that exact content id appears (answers "when did this content first appear / disappear?").
- Query time O(1) hash lookup + O(output); build O(V+E) (`reindex`).

### 10.7 Time travel and structural sharing `[ORIGINAL][ADDED]`
- `at <rev-or-time> <path>`: if a time is given, use `BPlusTree.lowerBound` to find the newest commit with `time ≤ T` **on the current branch's first-parent history**; ties/skew → pick the topologically latest among equal-time commits. Then walk the root tree to `<path>` (reading only the trees along the path) and print the blob. Cost = O(depth) object reads, not repo size.
- `export`: recursive materialisation of the tree into `<dir>` (must not exist or be empty); modes applied (exec bit); symlinks recreated.
- **Structural-sharing report** (`stats --sharing`): for the last N commits compute (a) total tree entries if each commit stored a full snapshot, (b) unique tree+blob objects actually stored; print the ratio. This quantifies "no full copy per commit".
- "Time travel" differs from `checkout` in that it is read-only and never moves HEAD/index/working tree.

### 10.8 Checkout / reset rules `[ADDED]`
- `checkout` requires: no staged changes and no modified/deleted tracked files; otherwise abort with a list (`--force` is not provided). Untracked files that would be overwritten → abort. 
- Procedure: compute tree diff `current HEAD tree → target tree`; delete removed files (remove now-empty parent dirs), write added/modified files, rebuild index, update HEAD. 
- `reset --hard` discards everything unconditionally.

### 10.9 Bisect `[ORIGINAL][ADDED]`
State file `BISECT`: `bad <id>`, `good <id>` lines, `skip <id>` lines, `orig <HEAD content>`.
- **Candidate set** = `ancestors(bad) ∪ {bad}` minus `ancestors(any good)` minus the goods. 
- **Linearisation:** Kahn topological order of the candidate subgraph (tie-break oldest `time` first, then id). 
- **Strategy `linear` (default):** test the middle element of the linearised list (index `⌊n/2⌋`). Assumes the "bad" property is monotone along this order — **explicit assumption, must be stated in the report**; with merges the result is still *a* valid first-bad commit only if that holds. `bad` ⇒ keep `[0..mid]`; `good` ⇒ keep `[mid+1..n)`.
- **Strategy `weighted` (`--strategy=weighted`):** pick the candidate whose number of in-set ancestors (including itself) is closest to `n/2` (Git-style), computed with bit-vector rows (`std::vector<uint64_t>`) (O(V²/64) memory) — only when `n ≤ 5000`; otherwise fall back to `linear` with a warning.
- **Skip:** a skipped midpoint is replaced by the nearest untested candidate (alternating outwards); if all remaining candidates are skipped print the remaining set and stop.
- **Termination:** one candidate left = **first bad commit**; print it with `show`.
- **`bisect run <cmd…>`:** for each midpoint, `export` the commit into `.chronicle/bisect-work/` (never touching the user's working tree), `fork` + `chdir` + `execvp(cmd)`, `waitpid`. Exit code `0` → good; `125` → skip; `1–127` (except 125) → bad; `≥128` or killed by signal → abort the session with an error. Test run count and each step printed.
- `bisect reset` removes the state file; `bisect log` prints the decisions so far.
- **Demo** (`scripts/demo_bisect.sh`): generate N commits (default 1000) each appending a line to `calc.txt`; at a random commit inject a "bug" line; the test command greps for it; run `bisect run`; print planted vs found commit and the number of tests (expected ≤ ⌈log₂ N⌉ + 1). This is the **strongest live demo**.

### 10.10 Delta compression and packfiles `[ORIGINAL][REVISED]`
**Candidate selection (heuristic, not claimed optimal — D-08):**
1. Walk all reachable commits' trees once to collect, for each blob/tree id, a *name hint* = hash of its last path component (first path seen).
2. Create entries `(type, name_hint, size, id)`; sort with merge sort by `(type, name_hint, size descending)`.
3. Process entries **in sorted-list order** (so every base is processed before the objects that may use it, which keeps chains acyclic): for each entry, try as bases the previous `pack.window` entries of the same type whose chain depth is < `pack.maxdepth`. Keep the base yielding the smallest delta; accept it if the delta is smaller than (100 − `pack.minsaving`) % of the object's size; otherwise store the object in full.
4. No claim of minimum spanning optimality; the report must say "heuristic, window-based".

**Delta format (own, simple):** delta payload = `varint result_size` then instructions:
- `0x01 <varint offset> <varint length>` — COPY `length` bytes from the base at `offset`.
- `0x02 <varint length> <length literal bytes>` — INSERT literal.
**Delta construction:** index the base in 16-byte blocks (`std::unordered_map<uint64 blockhash, vector<offset>>`); scan the target with a rolling Rabin–Karp hash (polynomial, mod 2⁶⁴, base 257); on a hash hit verify bytes, extend the match forward and backward, emit COPY; unmatched bytes accumulate into INSERT. Matches shorter than 16 bytes are never emitted as COPY.

**Pack file:** `"CPK1"`, `uint32 BE version=1`, `uint32 BE count`, then entries, then a 32-byte SHA-256 trailer of everything before it. Entry: `uint8 kind` (1 blob, 2 tree, 3 commit, 4 delta) + `varint payload_size`; for delta: 32 raw bytes base id then delta payload; otherwise the raw payload (uncompressed; optional zlib only under `-DCHRONICLE_ZLIB`).
**Pack index:** `"CIDX"`, `uint32 BE version=1`, `uint32 BE count`, then `count` records of `32-byte id + uint64 BE offset` **sorted by id** (built with LSD radix sort), then a 32-byte SHA-256 trailer. Lookup = interpolation search (5.2). The index is loaded into memory (`vector`) on first use.
After a successful pack: loose objects that are now in the pack are deleted (unless `--keep-loose`).

### 10.11 GC and fsck `[ORIGINAL][ADDED]`
- **Roots:** all `refs/heads/*`, `HEAD` (if detached), `MERGE_STATE` theirs id, `BISECT` ids, and every blob/tree referenced by the current index.
- **Mark:** DFS/BFS over commit → tree → blob/tree edges using `std::unordered_set` visited. O(V+E).
- **Sweep:** delete unreachable loose objects older than `gc.graceseconds`; rewrite packs so they contain only reachable objects (delta bases of kept objects are always kept); delete old packs after the new pack+index are renamed into place. `--dry-run` prints counts and changes nothing. A `gc.lock` file prevents concurrent runs.
- **fsck (`verify`):** for each loose/packed object recompute the hash; check that every referenced parent/tree/blob exists; report `dangling` (unreachable) objects separately from `corrupt` ones; exit 1 if anything is corrupt or missing.

### 10.12 Graph visualisation `[ADDED]`
- **ASCII (`log --graph`, `graph`)**: walk in the 10.2 order, maintain `lanes` = vector of "expected next commit id" per column. For each commit: find its lane (first lane expecting it, else new lane at the right); print `*` in that lane and `|` in others; parents: first parent keeps the lane, extra parents open new lanes drawn with `\` on the following row; lanes that converge are closed with `/`. Example:
```
*   a1b2c3d4 (HEAD -> main) Merge branch 'feature'
|\
| * 9f8e7d6c (feature) Add search
* | 4c5d6e7f Fix typo
|/
* 1a2b3c4d Initial commit
```
- **DOT (`graph --dot`)**: `digraph chronicle { rankdir=BT; "<short>" [label="<short>\n<msg first line>"]; "<child>" -> "<parent>"; }`. Branch tips are extra boxed nodes with edges to their commits. Output only; no rendering dependency.
- Decorations `(HEAD -> main, other)` come from the refs map.

### 10.13 EXPERIMENTAL — reconcile, cherry-pick, patch-id `[ORIGINAL][ADDED]`
Simulates "multiple remotes" with **two local repository directories** (no networking).
- `reconcile <other>`: load both DAGs (ids are comparable because both use SHA-256 content addressing). Report per branch name: *identical*, *ahead N*, *behind M*, *diverged (ahead N, behind M)*, using commit-id hash-set differences and the merge base. With `--fetch`, copy all objects reachable from the other repo's branch tips that are missing locally into the local object store, and create/advance `refs/heads/<name>-remote` — never touches local branches.
- `patch-id(commit)`: SHA-256 of a canonical form of the commit's diff vs its first parent: for each file (path order) the hunks' `+`/`−` lines with line numbers and context stripped and whitespace-trimmed. Equal patch-ids ⇒ "the same change".
- `cherry-pick <rev>`: three-way merge with `base = parent(rev)`, `ours = HEAD`, `theirs = rev`; success → new commit with the original message and a single parent HEAD; conflicts handled like merge. Before applying, scan `merge-base..HEAD` patch-ids; if equal to `rev`'s, print `already applied as <id>` and do nothing (this is the **cherry-pick detection**).

### 10.14 EXPERIMENTAL / OPTIONAL — structural merge for `MiniLang` (F16)
Do this only after M12. Grammar (complete; anything else is `unsupported`, falling back to line merge):
```
program   := { func }
func      := "func" IDENT "(" [ IDENT { "," IDENT } ] ")" block
block     := "{" { stmt } "}"
stmt      := "let" IDENT "=" expr ";" | IDENT "=" expr ";" | "return" expr ";"
           | "if" "(" expr ")" block [ "else" block ] | "while" "(" expr ")" block | expr ";"
expr      := term { ("+"|"-"|"*"|"/"|"<"|">"|"=="|"!=") term }
term      := NUMBER | IDENT | IDENT "(" [ expr { "," expr } ] ")" | "(" expr ")"
```
Files with extension `.mini` are parsed to ASTs. Merge operates on **top-level `func` items**: match functions across base/ours/theirs by name; a function renamed on one side is matched by Zhang–Shasha similarity (normalised edit distance ≤ 0.4); if each function was changed by at most one side, take the changed version regardless of where it moved in the file (a conflict the line-based merge would report); a function changed by both sides is merged at line level inside that function only. Output is re-printed canonically. Cap AST size at 200 nodes per function (else line merge). This subsystem is never required for submission.

---

## 11. Errors, Edge Cases, Concurrency, Security `[ADDED]`

### 11.1 Exit codes and messages
`0` success · `1` general error (message on stderr, `chronicle: <text>`) · `2` usage error · `3` merge/cherry-pick conflicts · `4` corruption detected (`E_CORRUPT`) · `5` repository not found / locked.

### 11.2 Edge-case rules (all must have a test)
| Situation | Required behaviour |
|---|---|
| `commit` on an empty repo / nothing staged | error (exit 1) except first commit with staged files |
| Detached HEAD commit | allowed; warns that the commit is not on a branch |
| Missing/corrupt object | `E_CORRUPT`, exit 4, names the id and the operation |
| Truncated `index` or bad checksum | error; suggests `chronicle reset --hard HEAD` |
| Binary file in diff/merge/blame | diff prints "Binary files differ"; blame treats as one opaque line group → error `blame: binary file` |
| File > `core.maxfilebytes` | `add` refuses with the path and limit |
| Files with spaces/newlines in names | fully supported via escaping in index/history |
| Symlinks | stored as mode 120000; never followed during `add`/walk |
| File mode | only the executable bit is tracked |
| Permission denied while reading | skip that file, report at end, exit 1 |
| Concurrent chronicle processes | write commands create `.chronicle/lock` with `O_CREAT|O_EXCL`; if present: exit 5 "repository is locked (stale lock? remove .chronicle/lock)" |
| Unrelated histories | error unless `--allow-unrelated` |
| Branch name validity | `[A-Za-z0-9._/-]`, no `..`, no leading `/` or `-`, no trailing `/` or `.lock` |
| Clock skew (child older than parent) | log/bisect rely on topology (generation numbers), never on time alone |
| Hash collision of ids | not handled (SHA-256; acknowledged as out of scope) |

### 11.3 Concurrency and durability
Single-process model. All ref/index writes are temp+fsync+rename (crash-safe). Loose object writes are idempotent. A crash mid-pack leaves only temp files (cleaned by `gc`).

### 11.4 Security
Paths are validated: no `..` components, no absolute paths, nothing resolving inside `.chronicle/`, when adding or checking out (prevents a crafted tree from writing outside the worktree). `bisect run` executes the user's own command — documented, no sandbox.

---

## 12. Testing, Validation and Benchmarks `[ADDED]` (D-09)

### 12.1 Unit tests (`make test`)
Tiny framework in `tests/framework.hpp` (registry + `CHECK`, `CHECK_EQ`). Each DS has an **invariant checker** called after every random operation in property tests with fixed seeds, compared against a naive oracle built on `std::vector`:
- STL containers are **not** unit-tested; only the hand-written pieces are (below), plus the optional benchmark-only probing set.
- `HexTrie`: unique/ambiguous/none results vs linear scan oracle.
- `BPlusTree` (`m=4` and `m=64`): invariants, leaf chain, range scan equals sorted oracle.
- Sorting: `radixSortIds` equals `std::stable_sort` result (ties stable); searching: interpolation == `std::lower_bound` for 10⁵ random ids and for adversarial clustered ids.
- **SHA-256 vectors:** `""` → `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`; `"abc"` → `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`; one million `'a'` → `cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0`; also block-boundary lengths 55, 56, 63, 64, 65 compared with `sha256sum`.
- Object encode/decode round trips; tree sort order; commit parse/serialize.
- Myers: `apply(diff(a,b), a) == b` for 10⁴ random line-array pairs (including empty files, no trailing newline).
- diff3: crafted cases for every rule in 10.4 (touching regions merge cleanly; same-position inserts conflict; identical changes merge).
- Delta: `apply(delta(base,target), base) == target` on random and structured inputs; pack/idx round trip; lookup of every id.
- Merge-base: hand-built DAGs (linear, diamond, criss-cross, unrelated) with known answers.
- Blame: scripted histories with known attribution, including merges.

### 12.2 Integration tests (`tests/integration/*.sh`)
Each script builds a temp repo, runs the CLI, asserts output/exit codes; run by `make test`. Required scenarios: init/add/commit/log; status states; branch/checkout (clean-tree refusal); fast-forward; clean merge; conflicting merge → resolve → commit; merge abort; blame with a merge; history queries; `at`/`export` by time; bisect run (planted bug, test count ≤ ⌈log₂N⌉+1); pack then read all objects; gc dry-run vs real; verify detects a deliberately corrupted object; abbreviated-id ambiguity; ignore rules; weird filenames; reconcile + cherry-pick detection (EXPERIMENTAL).
**Oracle test (skipped if `git` is missing):** create a repo with `git init --object-format=sha256` and compare `hash-object` for ~100 files of sizes 0…1 MiB.

### 12.3 Benchmarks and complexity validation (`make bench`)
Generator `scripts/gen_repo.sh <n_commits> <files> <seed>` (deterministic). Each benchmark runs sizes n = 1k, 2k, 4k, 8k, 16k, … and writes CSV to `bench/results/`; a small post-processing step **inside the bench binary** fits the slope of log(time) vs log(n) and prints `observed exponent ≈ x` next to the theoretical one.
| ID | Comparison |
|---|---|
| B1 | object lookup: chained hash vs linear-probing hash vs sorted array + binary search vs interpolation search |
| B2 | visited-set: `std::unordered_set` (chaining) vs a ~40-line benchmark-only linear-probing set, in full-DAG traversal |
| B3 | log walk: heap-based vs "collect all and merge-sort" |
| B4 | abbreviated id resolve: trie vs linear scan of all ids |
| B5 | blame at K successive revisions: with memo vs without memo |
| B6 | time-range query: B+ tree range scan vs linear scan of commits |
| B7 | diff time vs file size and edit distance D |
| B8 | bisect: tests used vs N (expect ≈ log₂ N) |
| B9 | storage: total bytes loose vs packed (delta ratio) on a generated edit history |
| B10 | staging index: `std::map` vs sorted-vector-insert for 10⁵ paths (insert + ordered iteration) |

### 12.4 Quality gates
`make asan` (AddressSanitizer + UBSan) must run the whole test suite clean. No compiler warnings. Valgrind optional.

---

## 13. Milestones and Definition of Done `[ADDED]` (D-10)

| M | Deliverable | Definition of Done |
|---|---|---|
| M0 | Skeleton: Makefile, framework, `Status`, string/time/varint utils | `make test` runs and passes an empty suite; CI-style script `scripts/check.sh` runs build + tests |
| M1 | SHA-256; `radixSortIds`; `interpolationSearch`; `Id`/hex utils | All SHA vectors pass; radix/interpolation tests pass |
| M2 | Object model: blob/tree/commit encode/decode, loose store, `hash-object`, `cat-file`, `init` | Round-trip tests; oracle test vs `git --object-format=sha256` passes |
| M3 | `HexTrie` (staging index = `std::map`); index load/save; `add`, `rm`, `status`, `commit`, refs, `branch`, `checkout`, `reset --hard`, revparse; ignore rules; repo lock | Integration scenarios 1–6 pass |
| M4 | Myers diff, unified output, tree diff, `diff`, `show`, `ls-tree` | diff round-trip property test passes |
| M5 | DAG, generation numbers, date-ordered walk, merge-base, `log` (+`--oneline`, `-n`), ASCII graph | `log` on a hand-built DAG matches expected; criss-cross base rule tested |
| M6 | Three-way merge: tree-level, diff3, conflicts, `MERGE_STATE`, `merge --abort`, fast-forward | All merge scenarios + diff3 unit rules pass |
| M7 | **BlameFast** + memo + history index prerequisites | Blame scripted-history tests pass (this completes the **minimum defensible submission**: M0–M7) |
| M8 | `history.idx`, `history` queries, `BPlusTree`, `log --since/--until`, `at`, `export`, `stats --sharing` | Time/history integration tests pass |
| M9 | Bisect (`linear` + `weighted`), `bisect run`, `demo_bisect.sh` | Demo finds planted bug in ≤ ⌈log₂N⌉+1 tests for N = 1000 |
| M10 | Delta, pack, pack idx, interpolation lookups, `pack`, `gc`, `verify`, `stats` | Pack round-trip; gc keeps all reachable; corruption detected |
| M11 | Benchmarks B1–B10 with complexity-fit output | `make bench` produces CSV + summary |
| M12 | EXPERIMENTAL: `reconcile`, `cherry-pick`, `patch-id`, DOT export | Integration tests pass |
| M13 | OPTIONAL: MiniLang AST merge | Only if time remains; tests for the grammar and function-level merge |
| M14 | Polish: README, `scripts/demo_all.sh`, asan clean, DECISIONS.md | `make asan` clean; demo script runs end-to-end |

**Minimum complete submission = M0–M7** (original guidance: *object store → Merkle DAG → basic commit/log/diff/merge → BlameFast is a complete, defensible submission even if nothing ADVANCED gets finished*). Recommended target = M0–M12.

---

## 14. Why This Is a DSA Project (viva-ready) `[ORIGINAL]`

| Problem | Structure | Algorithm | Result |
|---|---|---|---|
| Fast object retrieval | Hash table | Hashing | Expected O(1) lookup |
| History traversal / log | DAG + heap | DFS/BFS/topo sort | O(V+E) (O((V+E) log V) date-ordered) |
| Merge base finding | DAG | best-common-ancestor marking | O(V+E) |
| Bug localisation | Linearised DAG | Binary search | O(log N) tests |
| Efficient blame | Line-origin arrays + memo table | Parent-pass diff, memoisation | Avoids O(commits·filesize) re-walks |
| Storage efficiency | Persistent tree | Structural sharing | No full-copy per commit |
| Delta compression | Rolling-hash block index | Heuristic window selection | Reduced storage, explicitly non-optimal |
| **Staging area ordered iteration** `[ADDED]` | Red-black tree (`std::map`) | in-order walk | O(log n) update, sorted order for free |
| **Abbreviated ids** `[ADDED]` | Trie | prefix descent | O(len) resolve + ambiguity detection |
| **Date queries** `[ADDED]` | B+ tree | range scan over leaf chain | O(log n + k) |
| **Pack lookup** `[ADDED]` | Radix-sorted array | interpolation search | expected O(log log n) |

### 14.1 Demo plan `[ADDED]`
1. `scripts/demo_all.sh` creates a repo with a few branches, a merge, a conflict, and shows `log --graph`.
2. `blame` on a file with a merge in its history; show that a second blame at an older revision is instant (memo).
3. `history --first` / `history --blob` queries; `at --time` and `export`.
4. **Headline demo:** `scripts/demo_bisect.sh 1000` — find the planted regression live, print the number of test runs.
5. `chronicle pack`, then `stats` to show storage reduction; `verify`; corrupt a byte by hand and show detection.
6. `make bench` summary table showing observed vs theoretical complexity exponents.

### 14.2 Likely viva questions to be able to answer `[ADDED]`
Why a DAG and not a tree? Why chaining (`unordered_map`) for the object index, and how does it compare with open addressing (benchmark B2)? Why a red-black tree (`std::map`) for the staging index, and why does it need sorted order? Why interpolation search on SHA-256 ids and when does it degrade? What exactly is assumed for bisect's O(log N)? Why can two commits have several merge bases? What is *not* optimal in the delta heuristic?

---

## 15. Rejections, Scope and Future Work

### 15.1 What to reject `[ORIGINAL]` (revised where noted)
- **Full Git packfile binary compatibility** — huge reverse-engineering effort for marginal DSA payoff beyond what delta encoding teaches. (Own pack format instead.)
- **General multi-language AST parsing for MergeMind** — one toy grammar only; a real parser is a compilers course.
- **Real networked distributed remotes** — algorithms (merge-base across DAGs, cherry-pick detection) are the point; sockets add infrastructure risk with zero DSA value. (Simulated with two local directories.)
- **Union-Find** — no genuine use in this design; forcing it would read as checkbox-filling. ~~**Trie**~~ — *originally rejected; now adopted for abbreviated-id resolution (5.1).*
- **GUI** — out of scope; terminal visualisation (+ DOT text) is enough.
- **(added)** rename detection, recursive virtual merge base, `rebase`, `stash`, tags, packed refs, shallow clones, hooks, submodules, large-file chunking.

### 15.2 Final recommended scope `[ORIGINAL]`
- **CORE:** object store, Merkle DAG, init/add/commit/log/branch/checkout, line-based diff & three-way merge, BlameFast, History Engine (DFS/BFS/topo/merge-base).
- **ADVANCED:** time travel + object history index, automated bisect, delta compression + GC.
- **EXPERIMENTAL:** scoped AST structural merge, multi-remote reconciliation (simulated).
- **FUTURE WORK:** Git-binary packfile compatibility, multi-language parsing, real networked remotes, GUI.

### 15.3 Limitations to state in the report `[ADDED]`
No rename tracking; criss-cross merges use a single chosen base; bisect's O(log N) is conditional; delta selection is heuristic; single-process only; no tags/rebase/stash; SHA-256 only (no collision handling); text-oriented diff/blame.

---

## 16. Decision Register and Change Log vs the Original Document

| ID | Decision | Reason |
|---|---|---|
| D-01 | C++17 with the **full STL allowed**; hand-write only trie, B+ tree, radix sort, interpolation search, diff/delta/merge algorithms, SHA-256 | Owner's revision: focus on the VCS and its architecture, not re-implementing containers; syllabus topics are covered through the standard implementations plus the hand-written parts |
| D-02 | Repo layout in 0.4; Makefile build; Bash scripts, no Python | Ubuntu VM, owner preference |
| D-03 | Text index/history formats with SHA-256 trailer; atomic temp+rename writes | Debuggable, crash-safe, simple |
| D-04 | Object id = SHA-256 of `"<type> <size>\0"+payload`, loose objects uncompressed | Git-style header allows validation against `git --object-format=sha256`; no zlib dependency |
| D-05 | zlib optional (`-DCHRONICLE_ZLIB`, off); own delta format is the compression story | Keeps "no external libs" |
| D-06 | Final CLI in Section 9 (adds `reset --hard`, `find`, `at`, `export`, `reindex`, `graph`, `verify`, `stats`, `reconcile`, `cherry-pick`) | Original only said the verbs "fit" |
| D-07 | Criss-cross: choose newest base + warning | Deterministic and honest; recursive base = future work |
| D-08 | Delta base selection = sorted window heuristic, accept if < 50 % | Explicitly heuristic as in original |
| D-09 | Custom test framework + oracle test vs git; benchmarks with exponent fit | Course evaluates efficiency analysis |
| D-10 | Milestones M0–M14; minimum submission M0–M7 | Original "build first" guidance |
| D-11 | Trie adopted (abbreviated ids), `std::map` (RB tree) for staging index, B+ tree for time index, `std::priority_queue` for log walk, interpolation search + radix sort for pack index | Syllabus coverage **only where each has a genuine use** |
| D-12 | Union-Find stays rejected; shortest-path/MST/segment/suffix structures unused and declared unused | Honesty over checkbox coverage |
| D-13 | Terminal-only UI + DOT export; no web front end | Owner: no/negligible front end |

**Change log (original → this document):** Parts A–L all retained. Changed: Trie rejection reversed; priority-queue justification moved from delta ranking to log walk; "SHA-based hashing" fixed to SHA-256; BlameFast gained a precise DP + memo definition; merge gained tree-level rules, diff3 overlap rules, conflict workflow; bisect gained candidate/linearisation/skip/exit-code rules; time travel gained B+ time lookup and sharing report; delta/pack gained formats; added formats, CLI, edge cases, tests, benchmarks, milestones, syllabus map. Removed: nothing except conversational remarks ("a friend took that framing", "your own brief") rewritten neutrally in 1.1/15.1.

---

## 17. Preparing the Project Report (written later) `[ADDED]`

| Report chapter | Take from |
|---|---|
| Abstract, motivation, real-life mapping | 1.2, 1.3 |
| Literature / background | 2, 5 (theory notes), Git internals references |
| Requirements and constraints | 0.2, 3 |
| System design | 7, 8 |
| Data structures and algorithms | 5, 6, 10 |
| Implementation notes | 9, 10, 13 |
| Testing and validation | 12.1–12.2 |
| Performance evaluation | 12.3 (use the generated CSVs and exponent fits) |
| Limitations and future work | 15.3, 15.1 |
| Syllabus coverage (honest) | 3 |

Suggested references: Cormen et al., *Introduction to Algorithms*; Horowitz–Sahni–Mehta, *Fundamentals of Data Structures in C++*; Mehta–Sahni, *Handbook of Data Structures and Applications* (course texts); E. Myers, "An O(ND) Difference Algorithm and Its Variations" (1986); FIPS 180-4 (SHA-256); Git documentation (object model, packfile format) as background; Zhang & Shasha (1989) for tree edit distance (experimental module).

---

## Appendix A. Original Pitch Material `[ORIGINAL]` (kept in full; the 3-minute version expanded)

- **Name:** Chronicle
- **One-liner:** A version-control engine where the object graph is the computation, not just the storage.
- **30-sec:** Chronicle stores every version as an immutable, content-addressed object in a Merkle DAG — like Git, but built from scratch so every operation (blame, merge, bug-hunting) is implemented as an explicit graph or tree algorithm rather than hidden library code.
- **1-min:** Add history traversal (DFS/BFS/topological sort) for logs and ancestry, merge-base finding for merging, binary search over a linearised DAG for automatic bug-commit discovery, and a reverse index that lets you query the object store's own history — not just the commit history.
- **3-min:** Everything above, plus the advanced layer — persistent trees for zero-copy snapshots, heuristic delta compression for storage, and (experimental) a toy-grammar AST merge that resolves conflicts a line-based Git can't; and the supporting structures: a red-black-tree (`std::map`) staging area, a trie for abbreviated ids, a B+ tree time index, a `std::priority_queue`-driven log walk, and an interpolation-searched pack index — each measured against a baseline by the built-in benchmark suite.
- **Why not Git:** Git hides these structures behind mature C code; here you own and can defend every data-structure choice.
- **Why DSA:** every core feature maps directly to a named structure + algorithm + complexity bound (Section 14 table).
- **Most innovative feature:** the object-store history index (querying the store itself as persistent history, not just commits).
- **Most difficult:** delta-base selection heuristic + AST structural merge.
- **Strongest demo:** automated bisect finding a planted regression live, in seconds.
- **Build first:** object store → Merkle DAG → basic commit/log/diff/merge → BlameFast. That alone is a complete, defensible submission even if nothing ADVANCED gets finished.

---
*End of Chronicle specification.*
