# Decisions taken where the spec is silent or self-contradictory

Spec decisions D-01…D-13 live in `docs/DESIGN.md` §16 and are final. The rows
below are implementation decisions (I-xx) taken under §0.1 ("choose the
simplest option that satisfies the hard constraints and record it here").

| ID | Decision | Why |
|---|---|---|
| I-01 | Object cache in `ObjectStore` is a `std::unordered_map<Id, ...>` capped at 512 entries, cleared when full (spec text says "ChainedHashMap"). | D-01 forbids custom hash tables; `unordered_map` is a chained hash table. |
| I-02 | Integration scenarios are proven in the milestone that delivers the feature (log in M5, merges in M6), not all in M3. | M3's DoD references features that only exist later. |
| I-03 | Bisect never re-tests the known-bad tip: it is last in the linearisation and the midpoint index is `⌊(n−1)/2⌋`. Candidate set is recomputed from the `BISECT` state at every step. | `⌊n/2⌋` with "bad ⇒ keep [0..mid]" does not terminate for n = 2. |
| I-04 | `history.idx` writing (append on commit/merge, `reindex`) is implemented in M7 together with blame; query commands in M8. | Blame's `H` is defined via `history.idx`. |
| I-05 | `BPlusTree::floor(key)` added (doubly linked leaves). `at <time>` uses key `(time, first-parent position)` and `floor((T, INT32_MAX))`. | "newest commit with time ≤ T" is a floor query; position makes equal times pick the latest. |
| I-06 | `stats --sharing` examines the 100 most recent commits in log order (`--last N` to change). | N unspecified. |
| I-07 | Conflicted paths keep *ours* in the index; `MERGE_STATE` lists unresolved paths; `add`/`rm` of a path marks it resolved. | No index stages in the spec's index format. |
| I-08 | Env `CHRONICLE_AUTHOR_DATE="<unix seconds> [+hhmm]"` overrides commit time. | Deterministic tests and demos for time travel/log. |
| I-09 | Racy-stat rule uses the index file's mtime as "index write time". | Format has no field for it. |
| I-10 | `history.idx` records file paths (blobs/symlinks) only; tree entries are implied. | Simplest form that answers every §10.6 query. |
| I-11 | Extra files `store/objstore.cpp`, `store/repo.cpp`, `store/state.cpp` (MERGE_STATE), `engine/worktree.cpp`, `engine/commitops.cpp`, `cli/common.cpp`, `cli/dispatch.cpp`. | Keeps every file below ~600 lines. |
| I-12 | Short ids printed by `log`/`blame`/`graph` use `core.abbrev` (8) characters, extended through the trie's `subtree_count` until unique. | §5.2 "shortest unique prefix (≥ 8 chars)". |
| I-13 | `add <dir>` (including `.`) also stages deletions of tracked files under that directory that no longer exist. | Makes `add .` mean "make the index match the tree" — the simplest reading of §9. |
| I-14 | `commit` with no `-m` is allowed only when finishing a merge (default message `Merge branch '<name>'`) or a conflicted cherry-pick (original message). | §10.4 step 4. |
| I-15 | `gc` builds one new pack of every reachable object (loose + packed), deletes old packs, deletes loose objects that are now packed and unreachable loose objects older than `gc.graceseconds`. With no reachable objects it writes no pack. | Simplest repack that satisfies §10.11. |
| I-16 | `log --since/--until` builds a B+ tree over the reachable commits for the requested tips and filters the date-ordered walk by the range-scan result. | §9 + §5.2. |
| I-17 | Time strings without a zone are local time (`mktime`); tests set `TZ=UTC`. | §9 `at`. |
| I-18 | `bisect run` treats exit codes 126/127 as *bad* (spec: 1–127 except 125). README warns. | Literal spec rule. |
| I-19 | `reset --hard` with no `<rev>` means `HEAD`. | Needed for "reset --hard HEAD" suggestions. |
| I-20 | `merge --allow-unrelated` merges against an empty base tree. | §10.3. |
| I-21 | Unified diff uses 3 lines of context and Git-style `a/` `b/` prefixes; `\ No newline at end of file` marker. | Conventional; unspecified. |
| I-22 | `blame` output short id is 8 characters; date is the author date in the commit's own timezone. | §10.5 columns. |
| I-23 | `graph` with no revisions draws all branches; `log --graph` draws the given rev (default HEAD). | §10.12. |
| I-24 | `patch-id` hashes, per file in path order, the line `path` then every `+`/`-` line with whitespace trimmed, with hunk headers/context removed. | §10.13 canonical form. |
| I-25 | Benchmarks B5/B8 build their repositories through the library API in a temp dir (much faster than thousands of CLI processes); `scripts/gen_repo.sh` remains for CLI-level demos. | §12.3. |
| I-26 | Myers keeps at most 32 Mi trace ints (~128 MiB); beyond that (or D > 10 000) the diff falls back to "replace whole file". | D = 10 000 would need ~400 MB of O(D²) trace. |
| I-27 | `merge` (and `cherry-pick`) require a clean tracked state (no staged or unstaged changes), like `checkout`. | Simplest safe rule; the spec only defines the clean rule for checkout. |
| I-28 | `at`/`export` argument: a string containing `-` that parses as a date is a time; otherwise it is tried as a revision first and, if that fails and it is all digits, as Unix seconds. | Digits are valid hex, so "1700000000" could also be an abbreviated id. |
| I-29 | `export` writes files read-only (0444, 0555 if executable); directories stay writable so the snapshot can be deleted. | "materialise read-only" without making cleanup painful. |
| I-30 | `history` output lists events of commits reachable from the refs, newest first in date-ordered topological order: `<A|M|D> <short id> <date> <summary>`; `--blob` also reports where the content was later replaced or deleted. | §10.6 leaves the format open. |
| I-31 | Manual bisect never checks anything out: after each mark the next commit to test is exported to `.chronicle/bisect-work/` and remembered as `next`; `bisect good/bad/skip` without a revision refer to it (HEAD before the first suggestion). `bisect reset` only removes the state (HEAD was never moved). | Keeps the "never touch the user's working tree" rule of `bisect run` for the manual flow too. |
| I-32 | In a delta pack entry, `payload_size` counts only the delta bytes (the 32 base-id bytes that follow the header are not included). | §10.10 is ambiguous. |
| I-33 | `verify` exits 1 when anything is corrupt or missing (§10.10's explicit rule); commands that *hit* a corrupt/missing object while working exit 4 (E_CORRUPT, §11.1). | The two sections disagree; the more specific rule wins for `verify`. |
| I-34 | Delta construction tries at most 16 base offsets per block hash. | Bounds the worst case on highly repetitive inputs; still a heuristic (D-08). |
| I-35 | The code contains no comments; only shebangs and `#pragma`/`#include` directives remain. Algorithms and complexity are explained in `docs/` instead. | Owner policy; overrides the "comment the *why*" item in §0.3. |
