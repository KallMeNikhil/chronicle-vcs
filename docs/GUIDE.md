# Chronicle: Setup, Test and Demo Guide

This guide takes you from a brand-new machine with only the Chronicle source
code on it to a full demonstration of every feature.

| Part | Contents | Time |
|---|---|---|
| [1](#part-1-prepare-the-system) | Prepare the system | 5–15 min |
| [2](#part-2-build-and-install) | Build and install | 2 min |
| [3](#part-3-configure) | Configure | 2 min |
| [4](#part-4-run-the-automated-checks) | Run the automated checks | 5 min |
| [5](#part-5-guided-feature-walkthrough) | Guided feature walkthrough | 30 min |
| [6](#part-6-short-evaluation-script) | Short evaluation script | 10 min |
| [7](#part-7-reference) | Reference: commands, exit codes, data structures | — |
| [8](#part-8-troubleshooting) | Troubleshooting | — |
| [9](#part-9-clean-up-and-uninstall) | Clean up and uninstall | 1 min |

Conventions used below:

- Commands are for a Linux `bash` shell. On Windows, that means the Ubuntu shell from WSL.
- `<word>` in expected output stands for something that varies, usually an id.
  Object ids depend on the exact file contents and timestamps.
- A short id is the first 8 hex characters of an object id.

---

## Part 1: Prepare the system

### 1.1 Supported platforms

Chronicle uses POSIX calls (`fork`, `lstat`, `fnmatch`) and Linux-style `stat`
fields, so it needs Linux. Windows works through WSL. macOS is not supported.

The project was built and tested on **Ubuntu under WSL2 with g++ 15.2, GNU Make
4.4 and git 2.53**. Other distributions and older compilers should work, but have
not been tried; see [Part 8](#part-8-troubleshooting) if the build complains.

### 1.2 Windows only: install WSL

Skip this on native Linux.

Open **PowerShell as Administrator** and run:

```powershell
wsl --install -d Ubuntu
```

Restart if asked, then open **Ubuntu** from the Start menu and choose a username
and password. Everything from here on happens inside that Ubuntu window. To
reopen it later:

```powershell
wsl -d Ubuntu
```

### 1.3 Install the tools

Required: `g++` with C++17, GNU `make`, `bash`, and the usual command-line tools
(`sed`, `awk`, `grep`, `sha256sum`).

Optional: `git` (a reference test compares against it, and skips itself when git
is missing) and `graphviz` (only to turn `graph --dot` output into a picture).

Ubuntu or Debian:

```bash
sudo apt update
```

```bash
sudo apt install -y build-essential git graphviz
```

Fedora:

```bash
sudo dnf install -y gcc-c++ make git graphviz
```

Arch:

```bash
sudo pacman -S --needed base-devel git graphviz
```

Check the versions. Use g++ 9 or newer, and git 2.29 or newer for SHA-256 repositories.

```bash
g++ --version && make --version && git --version
```

### 1.4 Put the source code in place

Use whichever applies.

**A. From GitHub:**

```bash
git clone https://github.com/KallMeNikhil/chronicle-vcs.git ~/chronicle-vcs
```

**B. From a zip or folder copy:** unpack it so that `Makefile` sits directly
inside `~/chronicle-vcs`.

**C. Already on your Windows drive:** copy it into the Linux filesystem. A copy on
`/mnt/c` also builds, but more slowly, and files there do not fully support
Linux permissions.

```bash
cp -r /mnt/c/Users/<you>/<path>/chronicle-vcs ~/chronicle-vcs
```

Then go there and confirm the layout:

```bash
cd ~/chronicle-vcs && ls
```

You should see `Makefile`, `README.md`, `bench`, `docs`, `scripts`, `src` and `tests`.

---

## Part 2: Build and install

### 2.1 Build

```bash
make -j"$(nproc)"
```

This produces `build/chronicle` and `build/unit_tests`. The compile flags are
`-std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror`, so a success means zero warnings.

```bash
./build/chronicle --version
```

Expected: `chronicle 0.1.0`

### 2.2 Install

There is no `make install`; the program is one self-contained binary, so copy it
onto your `PATH`. Choose one option.

**Just for you:**

```bash
mkdir -p ~/.local/bin && cp build/chronicle ~/.local/bin/
```

```bash
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc && source ~/.bashrc
```

**For all users:**

```bash
sudo install -m 755 build/chronicle /usr/local/bin/chronicle
```

Confirm it works from anywhere:

```bash
cd ~ && chronicle --version && chronicle --help
```

After any rebuild, repeat the copy. The scripts in `scripts/` always use
`build/chronicle` inside the project, so they do not need the install step.

---

## Part 3: Configure

Chronicle needs no setup to run. Without an identity, commits are authored as
`Unknown <unknown@localhost>`.

### 3.1 Set your identity

Environment variables apply to every repository, and take priority over per-repository settings:

```bash
echo 'export CHRONICLE_AUTHOR_NAME="Your Name" CHRONICLE_AUTHOR_EMAIL="you@example.com"' >> ~/.bashrc && source ~/.bashrc
```

Or set it for one repository, from inside that repository. There is no `config`
command; settings are plain `key=value` lines in `.chronicle/config`.

```bash
printf 'user.name=Your Name\nuser.email=you@example.com\n' >> .chronicle/config
```

### 3.2 All settings

Put these in `<repository>/.chronicle/config`.

| Key | Default | Meaning |
|---|---|---|
| `user.name`, `user.email` | `Unknown`, `unknown@localhost` | commit author |
| `core.maxfilebytes` | 67108864 (64 MiB) | `add` refuses larger files |
| `core.abbrev` | 8 | minimum short-id length in `log` and `graph` |
| `pack.window` | 10 | how many earlier objects `pack` tries as a delta base |
| `pack.maxdepth` | 50 | longest delta chain |
| `pack.minsaving` | 50 | keep a delta only if it is under (100 − this)% of the full size |
| `gc.graceseconds` | 0 | minimum age before `gc` deletes unreachable loose objects |

### 3.3 Environment variables

| Variable | Meaning |
|---|---|
| `CHRONICLE_AUTHOR_NAME`, `CHRONICLE_AUTHOR_EMAIL` | author identity |
| `CHRONICLE_AUTHOR_DATE` | fixed commit time, as `"<unix seconds> [+hhmm]"` |
| `TZ` | timezone used to read dates such as `2023-11-15T12:00` |
| `CHRONICLE` | which binary the test and demo scripts should run |

### 3.4 Ignore rules

Create `.chronicleignore` in the repository root: one glob per line, `#` starts a
comment, `*` and `?` work, and a trailing `/` means directories only. A pattern
matches the path from the root or just the file name. `.chronicle/` is always ignored.

```text
*.log
build/
secret?.txt
```

---

## Part 4: Run the automated checks

Run these from the project folder:

```bash
cd ~/chronicle-vcs
```

### 4.1 Unit and integration tests

```bash
make test
```

Expected at the end, with no line starting `FAIL`:

```text
53 tests, 0 failed checks
...
integration: 16 passed, 0 failed
```

What this proves:

| Area | Check |
|---|---|
| SHA-256 | NIST vectors, and `sha256sum` at block-boundary lengths |
| Object ids | `git hash-object` in a SHA-256 git repo, 100 files from 0 to 1 MiB |
| Radix sort, interpolation search | against `std::stable_sort` and `std::lower_bound`, on uniform and clustered ids |
| Trie, B+ tree | against simple oracles, with invariant checks during random inserts (B+ tree at order 4 and 64) |
| Diff, merge | `apply(diff(a,b),a) == b` on 10,000 random pairs; every diff3 rule; every tree-merge rule |
| Commit graph | merge base on linear, diamond, criss-cross and unrelated histories; clock skew |
| Blame, delta, pack | known attribution through merges; round trips; every id found after packing |
| Scenarios | 16 scripts in `tests/integration/` driving the real binary |

To run a part on its own:

```bash
./build/unit_tests blame
```

```bash
CHRONICLE=$PWD/build/chronicle bash tests/integration/t40_merge.sh && echo PASS
```

### 4.2 Memory-safety run

This rebuilds everything with AddressSanitizer and UBSan in `build-asan/`, then
runs the same suite. Expect the same two summary lines, and no `ERROR:` or `runtime error`.

```bash
make asan
```

### 4.3 Everything from a clean build

```bash
bash scripts/check.sh --asan
```

Expected last line: `check: OK`

### 4.4 Benchmarks

```bash
make bench
```

This takes under a minute plus compile time. It writes `bench/results/B*.csv` and
prints observed against theoretical growth exponents. Timings vary by machine;
the comparisons below should hold anywhere.

| ID | Comparison | Observed in one run |
|---|---|---|
| B4 | abbreviated id: trie vs linear scan | 0.07 vs 1.08 |
| B5 | blame at every revision: memo vs no memo | 1.37 vs 2.17 |
| B6 | time-range query: B+ tree vs linear scan | 0.03 vs 1.15 |
| B8 | bisect tests against log₂N | slope 1.00 |
| B9 | storage: pack vs loose objects | pack is about 4–5% of the size |
| B10 | staging index: `std::map` vs sorted vector | 1.32 vs 1.96 |

B1 and B2 come out above their theoretical value of 1. Once a table outgrows the
CPU cache, each lookup becomes a cache miss, so the time per lookup rises with
size. That is a real effect worth explaining, not a defect.

To run one group (`structures`, `graph` or `repo`) after `make bench` has built the binary:

```bash
./build/chronicle-bench bench/results graph
```

### 4.5 Scripted demos

```bash
bash scripts/demo_all.sh
```

Expected final line: `== demo complete`. The last `verify` in it reports corruption on purpose.

```bash
bash scripts/demo_bisect.sh 1000 4242
```

Expected final line: `DEMO OK`. It builds 1000 commits, which takes up to about 20
seconds. On the development machine this seed planted the bug at commit #388 and
found it in 10 tests, within the bound of 11.

Generate a larger repository to explore. The same arguments give identical commit ids.

```bash
bash scripts/gen_repo.sh 500 8 42 /tmp/genrepo
```

---

## Part 5: Guided feature walkthrough

This part uses `chronicle` by hand, in the order the features were built. It needs
the install from Part 2. Fixed timestamps keep the dates in the expected output
predictable. Run the steps in order, because later steps build on earlier ones.

Work in a Linux folder (not `/mnt/c`), because the steps use executable bits and symlinks.

### 5.0 Session setup

Run this once per terminal. `ct` is a small helper that commits at a fixed time.

```bash
export TZ=UTC CHRONICLE_AUTHOR_NAME="Ada" CHRONICLE_AUTHOR_EMAIL="ada@example.com"
```

```bash
ct() { CHRONICLE_AUTHOR_DATE="$1 +0000" chronicle commit -m "$2"; }
```

```bash
rm -rf ~/chronicle-eval && mkdir ~/chronicle-eval && cd ~/chronicle-eval
```

### 5.1 Content-addressed objects

Every object is named by the SHA-256 of `"<type> <size>\0" + content`.

```bash
chronicle init project && cd project
```

```bash
ls -A .chronicle && cat .chronicle/HEAD
```

Expected: `HEAD`, `config`, `objects`, `refs`, then `ref: refs/heads/main`.

```bash
printf 'hello\n' > hello.txt
```

```bash
id=$(chronicle hash-object -w hello.txt); echo "$id"
```

```bash
printf 'blob 6\0hello\n' | sha256sum
```

Both print the same 64-character id, which proves the hash and the header format.

```bash
chronicle cat-file -t "$id"; chronicle cat-file -s "$id"; chronicle cat-file -p "$id"
```

Expected: `blob`, `6`, `hello`.

Compare with real Git, if installed:

```bash
rm -rf /tmp/goracle && git init -q --object-format=sha256 /tmp/goracle
```

```bash
git --git-dir=/tmp/goracle/.git hash-object hello.txt
```

Expected: the same id again.

### 5.2 Staging, ignore rules, first commit

```bash
printf 'def area(r):\n    return 3.14 * r * r\n\ndef perimeter(r):\n    return 2 * 3.14 * r\n' > geometry.py
```

```bash
echo "# Geometry" > README.md
```

```bash
printf '*.log\nbuild/\n' > .chronicleignore
```

```bash
echo noise > debug.log
```

```bash
chronicle status
```

Expected:

```text
On branch main (no commits yet)
Untracked:
  .chronicleignore
  README.md
  geometry.py
  hello.txt
```

`debug.log` is missing because the ignore rule hides it.

```bash
chronicle add . && chronicle status
```

All four files now appear under `Staged:` as `new file:`.

```bash
ct 1700000000 "Initial commit"
```

Expected: `[main <id>] Initial commit`

```bash
chronicle status
```

Expected: `nothing to commit, working tree clean`

Committing with nothing staged is refused (exit status 1):

```bash
ct 1700000001 "empty"; echo "exit $?"
```

Look at what was stored:

```bash
chronicle cat-file -p HEAD
```

```bash
chronicle ls-tree -r HEAD
```

```bash
head -3 .chronicle/index
```

The first shows the `tree` and `author` lines, the second every tracked file, and
the third the text format of the staging index.

### 5.3 The three status sections

```bash
echo "Circle helpers." >> README.md; rm hello.txt; echo tmp > notes.txt
```

```bash
chronicle status
```

Expected sections: `Not staged:` with `modified:   README.md` and
`deleted:    hello.txt`, then `Untracked:` with `notes.txt`.

Throw the changes away. `reset --hard` restores tracked files and leaves untracked ones alone.

```bash
chronicle reset --hard && rm notes.txt && chronicle status
```

### 5.4 Abbreviated ids (trie)

```bash
head=$(cat .chronicle/refs/heads/main)
```

```bash
chronicle find "${head:0:6}"
```

Expected: `<full id> commit`

Prefixes under 4 characters are rejected as a usage error (exit 2):

```bash
chronicle find "${head:0:3}"; echo "exit $?"
```

### 5.5 Branches and the clean-tree rule

```bash
chronicle checkout -b feature
```

```bash
printf 'def area(r):\n    return 3.14 * r * r\n\ndef perimeter(r):\n    return 2 * 3.14 * r\n\ndef search(xs, x):\n    return x in xs\n' > geometry.py
```

```bash
chronicle add geometry.py && CHRONICLE_AUTHOR_NAME=Grace ct 1700086400 "Add search"
```

Now try to switch with unsaved work. Chronicle refuses rather than lose it:

```bash
echo dirty >> README.md; chronicle checkout main; echo "exit $?"
```

Expected: `chronicle: your local changes would be lost...` and `exit 1`.

```bash
chronicle reset --hard && chronicle checkout main && chronicle branch
```

Expected: `Switched to branch 'main'`, then the branch list with `* main`.

### 5.6 Line diff (Myers)

```bash
printf 'import math\n\ndef area(r):\n    return math.pi * r * r\n\ndef perimeter(r):\n    return 2 * 3.14 * r\n' > geometry.py
```

```bash
chronicle diff
```

Look for `--- a/geometry.py`, `+++ b/geometry.py`, an `@@` hunk header,
`+import math`, `-    return 3.14 * r * r` and `+    return math.pi * r * r`.

```bash
chronicle add geometry.py && chronicle diff --cached | head -5
```

After staging, plain `chronicle diff` is empty and `--cached` shows the change.

```bash
ct 1700172800 "Use math.pi"
```

### 5.7 Three-way merge and the graph

Both branches changed `geometry.py` in different places, so this merges cleanly:

```bash
CHRONICLE_AUTHOR_DATE="1700200000 +0000" chronicle merge feature
```

Expected: `Merge made by the three-way strategy: <id>`

```bash
cat geometry.py
```

The file has both `import math` and `def search`.

```bash
chronicle log --graph --oneline
```

Expected shape:

```text
*   <id> (HEAD -> main) Merge branch 'feature'
|\
* | <id> Use math.pi
| * <id> (feature) Add search
|/
* <id> Initial commit
```

The order comes from a heap keyed by date, and a commit can never appear before
its descendants, even when clocks are wrong.

### 5.8 Conflicts, abort and resolution

Create two branches that change the same line:

```bash
chronicle checkout -b conflict-a
```

```bash
echo "Geometry helpers (A)" > README.md; chronicle add README.md; ct 1700200000 "README A"
```

```bash
chronicle checkout -b conflict-b main
```

```bash
echo "Geometry helpers (B)" > README.md; chronicle add README.md; ct 1700200100 "README B"
```

```bash
CHRONICLE_AUTHOR_DATE="1700200200 +0000" chronicle merge conflict-a; echo "exit $?"
```

Expected: `CONFLICT (content): README.md`, a message saying the merge failed, and `exit 3`.

```bash
cat README.md
```

Expected:

```text
<<<<<<< ours (conflict-b)
Geometry helpers (B)
=======
Geometry helpers (A)
>>>>>>> theirs (conflict-a)
```

```bash
chronicle status
```

Expected: `Merging conflict-a; unresolved conflicts:` followed by `conflict:   README.md`.

Committing now is refused:

```bash
chronicle commit; echo "exit $?"
```

Abort returns everything to how it was:

```bash
chronicle merge --abort && cat README.md
```

Expected: the file reads `Geometry helpers (B)`.

Merge again and resolve it this time. `commit` needs no `-m` to finish a merge.

```bash
CHRONICLE_AUTHOR_DATE="1700200200 +0000" chronicle merge conflict-a
```

```bash
echo "Geometry helpers" > README.md && chronicle add README.md && chronicle status
```

Expected: `all conflicts resolved (run 'chronicle commit')`

```bash
CHRONICLE_AUTHOR_DATE="1700200300 +0000" chronicle commit
```

Expected: `[conflict-b <id>] Merge branch 'conflict-a'`

### 5.9 Fast-forward

`main` is an ancestor of `conflict-b`, so no merge commit is needed:

```bash
chronicle checkout main && chronicle merge conflict-b
```

Expected: `Fast-forward <id>..<id>`

```bash
chronicle log --graph --oneline
```

The graph now shows both merges. There are 7 commits in total.

### 5.10 Revision syntax, show, tree views

`~N` follows first parents, and `^N` picks the N-th parent.

```bash
chronicle log --oneline -n 1 HEAD^2
```

Expected: `<id> (conflict-a) README A`

```bash
chronicle show HEAD~2
```

This shows the first merge: a `Merge:` line, author, date, message and its diff.

```bash
chronicle diff HEAD~3 HEAD -- geometry.py
```

This compares two revisions. Equal subtree ids are skipped without being read,
which is the Merkle tree diff.

### 5.11 BlameFast

```bash
chronicle blame geometry.py
```

Expected, with three commits and two authors:

```text
<math.pi> (Ada 2023-11-16)  1) import math
<math.pi> (Ada 2023-11-16)  2) 
<initial> (Ada 2023-11-14)  3) def area(r):
<math.pi> (Ada 2023-11-16)  4)     return math.pi * r * r
<initial> (Ada 2023-11-14)  5) 
<initial> (Ada 2023-11-14)  6) def perimeter(r):
<initial> (Ada 2023-11-14)  7)     return 2 * 3.14 * r
<search>  (Grace 2023-11-15)  8) 
<search>  (Grace 2023-11-15)  9) def search(xs, x):
<search>  (Grace 2023-11-15) 10)     return x in xs
```

Lines from the feature branch are credited to Grace even though they arrived
through a merge. The memo's effect is measured in benchmark B5.

### 5.12 Object history index

```bash
chronicle history geometry.py
```

Expected: four events, newest first: `M` for the merge, `M` for `Use math.pi`,
`M` for `Add search` and `A` for `Initial commit`.

```bash
chronicle history --first README.md
```

Expected: the `Initial commit` line, then the full commit id.

The next query asks when a specific *content* appeared and when it stopped being current:

```bash
first=$(chronicle log --oneline | tail -1 | cut -d' ' -f1)
```

```bash
old=$(chronicle ls-tree "$first" geometry.py | awk '{print $3}')
```

```bash
chronicle history --blob "$old"
```

Expected: `A <id> 2023-11-14 Initial commit  [geometry.py] -- replaced in <id>`

If the index is deleted, it can be rebuilt with one graph walk:

```bash
rm .chronicle/history.idx && chronicle reindex
```

Expected: `history.idx rebuilt: <n> events`

### 5.13 Time travel (B+ tree)

Log range queries:

```bash
chronicle log --oneline --since 2023-11-15 --until 2023-11-16T23:59
```

Expected: `Use math.pi` and `Add search`.

```bash
chronicle log --oneline -- README.md
```

Expected: the four commits that changed `README.md`.

Read a file as it was at a moment in time. This never touches HEAD, the index or
your files. `at` follows `main`'s first-parent line, so `Add search` (which came
in through the merge) is not on it.

```bash
chronicle at 2023-11-15T12:00 geometry.py
```

Expected: the original five-line file.

```bash
chronicle at 2023-11-17T00:00 geometry.py
```

Expected: the `math.pi` version, without `search`.

A Unix timestamp also works:

```bash
chronicle at 1700000000 README.md
```

Expected: `# Geometry`

Restore a full read-only snapshot somewhere else:

```bash
chronicle export 2023-11-15T12:00 ~/chronicle-eval/snapshot
```

```bash
ls -lA ~/chronicle-eval/snapshot
```

Expected: `exported <id> (4 files) to ...`, then four read-only files.

```bash
chronicle status
```

Expected: `nothing to commit, working tree clean`. Nothing moved.

### 5.14 Structural sharing and graph export

```bash
chronicle stats --sharing
```

Expected: the object counts, then `structural sharing over the last 7 commits`,
the two entry counts and a `sharing ratio`. Commits share unchanged subtrees
instead of storing full copies.

```bash
chronicle graph
```

```bash
chronicle graph --dot > ../graph.dot && head -5 ../graph.dot
```

Expected: the second starts `digraph chronicle {`. With Graphviz installed, make a picture:

```bash
dot -Tpng ../graph.dot -o ../graph.png
```

### 5.15 Bisect

Build 64 commits with a bug planted at commit 37. Use a new repository:

```bash
cd ~/chronicle-eval && chronicle init lab && cd lab
```

```bash
for i in $(seq 1 64); do
  echo "line $i" >> calc.txt
  [ "$i" -eq 37 ] && echo BUG >> calc.txt
  chronicle add calc.txt
  CHRONICLE_AUTHOR_DATE="$((1700000000 + i)) +0000" chronicle commit -m "c$i" >/dev/null
done
```

```bash
first=$(chronicle log --oneline | tail -1 | cut -d' ' -f1)
```

```bash
chronicle bisect start && chronicle bisect bad HEAD && chronicle bisect good "$first"
```

Expected last line: `Bisecting: 63 candidates left; test <id> (exported to .chronicle/bisect-work)`

Let Chronicle test by itself. Each candidate is exported to a private folder, so
your working files are never touched. Exit 0 means good, 125 means skip, and 1–127 means bad.

```bash
chronicle bisect run sh -c '! grep -q BUG calc.txt'
```

Expected: `step N: <id> is good|bad` lines, then:

```text
first bad commit: <id>
    c37
tests run: <6 or 7>
```

The bound for 64 commits is ⌈log₂64⌉ + 1 = 7. Confirm the answer and see the decisions:

```bash
chronicle log --oneline | grep ' c37$'
```

```bash
chronicle bisect log | tail -3
```

```bash
chronicle bisect reset
```

The weighted strategy, which picks the commit that splits the ancestors most evenly, finds the same commit:

```bash
chronicle bisect start --strategy=weighted && chronicle bisect bad HEAD && chronicle bisect good "$first"
```

```bash
chronicle bisect run sh -c '! grep -q BUG calc.txt'
```

```bash
chronicle bisect reset
```

Exit code 125 marks untestable commits. Here every commit with an even line count is skipped:

```bash
chronicle bisect start && chronicle bisect bad HEAD && chronicle bisect good "$first"
```

```bash
chronicle bisect run sh -c 'n=$(wc -l < calc.txt); [ $((n % 2)) -eq 0 ] && exit 125; ! grep -q BUG calc.txt'
```

Expected: some `is skip` steps, then either `first bad commit:` or a list of remaining candidates.

```bash
chronicle bisect reset
```

Now the headline demo with 1000 commits:

```bash
(cd ~/chronicle-vcs && bash scripts/demo_bisect.sh 1000)
```

Expected: `planted` and `found` show the same id, at most 11 tests, and `DEMO OK`.
Without a seed argument a new position is picked each run.

### 5.16 Delta compression and packfiles

Still in the `lab` repository, which holds 64 versions of a growing file:

```bash
chronicle stats
```

```bash
chronicle log --oneline > ../log_before.txt
```

```bash
chronicle pack
```

Expected: `packed <n> objects (<d> as deltas) into pack-<hash>`, and a line
comparing loose and packed byte counts.

```bash
find .chronicle/objects -type f -path '*/[0-9a-f][0-9a-f]/*' | wc -l
```

Expected: `0`. No loose objects remain.

```bash
ls -l .chronicle/objects/pack
```

```bash
chronicle stats
```

Everything still reads back correctly through delta chains and the interpolation-searched index:

```bash
chronicle log --oneline | diff - ../log_before.txt && echo identical
```

```bash
chronicle at HEAD~40 calc.txt | tail -1
```

Expected: `identical`, then `line 24`.

### 5.17 Garbage collection

Create an object nothing refers to:

```bash
echo garbage > junk.txt; junk=$(chronicle hash-object -w junk.txt); rm junk.txt
```

A dry run changes nothing:

```bash
chronicle gc --dry-run && chronicle cat-file -t "$junk"
```

Expected: `[dry run]` lines, then `blob`.

A grace period protects recent objects:

```bash
chronicle gc --grace-seconds 3600
```

Expected: `kept by grace period: 1`

A plain `gc` removes it:

```bash
chronicle gc
```

Expected: `unreachable loose objects deleted: 1`

```bash
chronicle cat-file -t "$junk"; echo "exit $?"
```

Expected: `chronicle: missing object <id>` and `exit 4`.

```bash
chronicle log --oneline | diff - ../log_before.txt && echo identical
```

History is unchanged.

### 5.18 Integrity checking

```bash
chronicle verify
```

Expected: `checked <n> objects: 0 corrupt, 0 missing, 0 dangling`

Tamper with a stored object by hand:

```bash
echo new > n.txt && chronicle add n.txt && ct 1700001000 "add n"
```

```bash
nid=$(chronicle hash-object n.txt); f=.chronicle/objects/${nid:0:2}/${nid:2}
```

```bash
cp "$f" ../object.backup && chmod u+w "$f" && printf 'blob 4\0nEw\n' > "$f"
```

```bash
chronicle verify; echo "exit $?"
```

Expected: a `corrupt:` line naming the id, a count of 1 corrupt, a `repository is
damaged` message, and `exit 1`.

Reading it directly exits with 4:

```bash
chronicle cat-file -p "$nid"; echo "exit $?"
```

Restore it:

```bash
cp -f ../object.backup "$f" && chronicle verify
```

Now flip one byte in the middle of the pack. This permanently damages `lab`, so do it last:

```bash
p=$(ls .chronicle/objects/pack/*.pack); chmod u+w "$p"
```

```bash
printf 'X' | dd of="$p" bs=1 seek=200 conv=notrunc 2>/dev/null
```

```bash
chronicle verify; echo "exit $?"
```

Expected: `trailer checksum mismatch` among the output, and `exit 1`. Objects
whose delta chain passes through that byte are also listed as corrupt.

### 5.19 Reconciling two repositories (experimental)

There is no networking; two local folders stand in for two remotes.

```bash
cd ~/chronicle-eval && chronicle init a && cd a
```

```bash
printf 'one\ntwo\nthree\n' > f; chronicle add f; ct 1700000000 base; chronicle branch shared
```

```bash
cd .. && cp -r a b && cd a
```

```bash
echo a1 >> f; chronicle add f; ct 1700000100 a1
```

```bash
echo x > ax; chronicle add ax; ct 1700000200 a2
```

```bash
cd ../b && echo b1 > bfile; chronicle add bfile; ct 1700000150 b1
```

```bash
chronicle checkout -b topic
```

```bash
for i in 1 2 3; do echo "t$i" > "t$i"; chronicle add "t$i"; ct $((1700000300 + i)) "t$i"; done
```

```bash
chronicle checkout main && cd ../a
```

```bash
chronicle reconcile ../b
```

Expected:

```text
main: diverged (ahead 2, behind 1), merge base <id>
shared: identical
topic: only in other (5 commits)
```

Commit ids are comparable across repositories because they are content hashes.
`--fetch` copies missing objects and creates `<name>-remote` branches. Your own
branches are not touched.

```bash
chronicle reconcile ../b --fetch
```

```bash
chronicle branch
```

```bash
CHRONICLE_AUTHOR_DATE="1700000400 +0000" chronicle merge main-remote
```

### 5.20 Cherry-pick and patch-id (experimental)

```bash
t2=$(chronicle log --oneline topic-remote | awk '/ t2$/ {print $1}')
```

```bash
chronicle cherry-pick "$t2" && ls
```

Expected: `[main <id>] t2`, and the file `t2` appears.

Applying the same change again is detected by comparing patch-ids. The new commit
has a different id, but the same change:

```bash
chronicle cherry-pick "$t2"
```

Expected: `already applied as <id>`

### 5.21 Edge cases

Back in the first repository:

```bash
cd ~/chronicle-eval/project
```

**Detached HEAD:**

```bash
chronicle checkout HEAD~1 && chronicle status && chronicle branch
```

Expected: `HEAD is now at <id> README B (detached)`, `HEAD detached at <id>`, and
`* (HEAD detached at <id>)`.

```bash
chronicle checkout main
```

**Repository lock:** a second writer gets exit 5.

```bash
touch .chronicle/lock; chronicle add .; echo "exit $?"; rm .chronicle/lock
```

**Corrupt staging index:**

```bash
printf garbage > .chronicle/index
```

```bash
chronicle status; echo "exit $?"
```

Expected: an error suggesting `chronicle reset --hard HEAD`, and `exit 1`.

```bash
chronicle reset --hard HEAD && chronicle status
```

**Awkward file names:**

```bash
printf x > "with space.txt"; printf y > "$(printf 'new\nline')"; printf z > '100%.txt'
```

```bash
chronicle add . && ct 1700300000 "odd names" && rm "with space.txt" '100%.txt'
```

```bash
chronicle reset --hard >/dev/null && ls
```

The deleted files return, and the newline name round-trips through the text index.

**Symlinks and the executable bit:**

```bash
ln -s README.md link; printf '#!/bin/sh\necho hi\n' > run.sh; chmod +x run.sh
```

```bash
chronicle add link run.sh && ct 1700300100 "link and script"
```

```bash
chronicle ls-tree HEAD | grep -E 'link|run.sh'
```

Expected: `link` has mode `120000`, and `run.sh` has mode `100755`.

**Binary files:**

```bash
printf 'bin\0ary' > b.bin; chronicle add b.bin; ct 1700300200 "binary"
```

```bash
printf 'bin\0ARY' > b.bin; chronicle diff
```

Expected: `Binary files differ`

```bash
chronicle blame b.bin; echo "exit $?"
```

Expected: `chronicle: blame: binary file 'b.bin'` and `exit 1`.

```bash
chronicle reset --hard >/dev/null
```

**File size limit:**

```bash
echo 'core.maxfilebytes=10' >> .chronicle/config; printf '0123456789abcdef' > big.bin
```

```bash
chronicle add big.bin; echo "exit $?"
```

Expected: `file too large: 'big.bin'...` and `exit 1`.

```bash
sed -i '/maxfilebytes/d' .chronicle/config; rm big.bin
```

**Running from elsewhere:** `-C` acts as if started in that folder, and subfolders find their repository automatically.

```bash
cd ~ && chronicle -C ~/chronicle-eval/project log --oneline -n 2
```

---

## Part 6: Short evaluation script

If you have about 10 minutes, this order covers the strongest points. First do
Parts 1–2 and the session setup in 5.0, and have the commands ready.

| Min | Show | Commands | Point to make |
|---|---|---|---|
| 1 | Test suite | `make test` | 53 unit tests and 16 scenarios, zero compiler warnings |
| 1 | Content addressing | 5.1 | the id equals `sha256sum` and `git hash-object` |
| 2 | Merge and graph | 5.7, 5.8 | merge base + diff3, conflict workflow, heap-ordered log |
| 1 | BlameFast | 5.11 | attribution survives merges; memo gives O(N) instead of O(N²) (B5) |
| 1 | Time travel | 5.13 | B+ tree finds the commit; the read costs O(depth), not repo size |
| 2 | **Bisect (headline)** | `bash scripts/demo_bisect.sh 1000` | a bug in 1000 commits, found in about 10 tests |
| 1 | Pack and corruption | 5.16, 5.18 | delta storage; one flipped byte is detected |
| 1 | Benchmarks | `make bench` | measured exponents match the theory |

For a shorter version, `bash scripts/demo_all.sh` runs most of this unattended.

---

## Part 7: Reference

### 7.1 Commands

| Group | Commands |
|---|---|
| Set up | `init [dir]` |
| Stage and commit | `add <path>...`, `rm [--cached] <path>...`, `status`, `commit -m <msg>` |
| Branches | `branch [<name> [<rev>]] [-d <name>] [--list]`, `checkout <branch\|rev> [-b <new>]`, `reset --hard [<rev>]` |
| Inspect | `log [--oneline] [--graph] [--since T] [--until T] [-n N] [<rev>] [-- <path>]`, `show <rev>`, `diff [--cached] [<rev> [<rev>]] [-- <path>]`, `ls-tree [-r] <rev> [<path>]`, `graph [--dot] [<rev>...]` |
| Merge | `merge <branch> [--no-ff] [--abort] [--allow-unrelated]` |
| Line and object history | `blame <path> [<rev>]`, `history <path>`, `history --first <path>`, `history --blob <id>`, `reindex` |
| Time travel | `at <rev-or-time> <path>`, `export <rev-or-time> <dir>`, `stats [--sharing] [--last N]` |
| Bug hunting | `bisect start [--strategy=linear\|weighted]`, `bisect bad\|good\|skip [<rev>]`, `bisect run <cmd...>`, `bisect log`, `bisect reset` |
| Storage | `pack [--keep-loose]`, `gc [--dry-run] [--grace-seconds N]`, `verify` |
| Low level | `hash-object [-w] <file>`, `cat-file -t\|-s\|-p <id>`, `find <prefix>` |
| Experimental | `reconcile <dir> [--fetch]`, `cherry-pick <rev>` |

Global options: `-C <dir>`, `--help`, `--version`.

Revisions: `HEAD`, a branch name, a full id, or an id prefix of at least 4 hex
characters, followed by any chain of `~N` and `^N`.

Times: `YYYY-MM-DD`, `YYYY-MM-DDTHH:MM`, `YYYY-MM-DDTHH:MM:SS` (local time) or Unix seconds.

### 7.2 Exit codes

| Code | Meaning |
|---|---|
| 0 | success |
| 1 | general error (`verify` also uses this when it finds damage) |
| 2 | usage error |
| 3 | merge or cherry-pick conflicts |
| 4 | a command hit a corrupt or missing object |
| 5 | no repository found, or repository locked |

### 7.3 Repository layout

```text
.chronicle/
  HEAD                 current branch, or a commit id when detached
  config               key=value settings
  index                staging area (text, with a SHA-256 trailer)
  objects/ab/<62 hex>  loose objects
  objects/pack/        pack-<hash>.pack and .idx
  refs/heads/<branch>  branch tips
  history.idx          object history index
  MERGE_STATE          only during an unresolved merge or cherry-pick
  BISECT               only during a bisect session
  lock                 write lock
```

### 7.4 Where each data structure is used

| Feature | Structure | Algorithm | Cost | See it in |
|---|---|---|---|---|
| Object ids | — | SHA-256 (hand-written) | O(n) | 5.1 |
| Staging area | red-black tree (`std::map`) | in-order walk | O(log n) update | 5.2 |
| Abbreviated ids | 16-ary trie (hand-written) | prefix descent | O(prefix length) | 5.4 |
| Line diff | arrays + hash table for line interning | Myers | O(N·D) | 5.6 |
| Tree diff | k-ary Merkle tree | skip equal ids | O(changed paths) | 5.10 |
| Log order | binary heap (`std::priority_queue`) | Kahn topological walk | O((V+E) log V) | 5.7 |
| Merge base | DAG + hash sets | descending-generation marking | O(V+E) | 5.7 |
| File merge | change regions | diff3 | O(N·D) | 5.8 |
| Blame | origin vectors + memo hash map | parent-pass dynamic programming | each commit diffed once | 5.11 |
| Object history | hash maps | reverse index | O(1) + output | 5.12 |
| Time queries | B+ tree (hand-written) | leaf-chain range scan | O(log n + k) | 5.13 |
| Bisect | linearised DAG, bit vectors | binary search | O(log N) tests | 5.15 |
| Delta | hash index of 16-byte blocks | Rabin–Karp rolling hash | O(n + m) expected | 5.16 |
| Pack index | sorted id array | LSD radix sort, interpolation search | O(32·N); O(log log n) expected | 5.16 |
| GC, verify | stack + hash set | DFS mark and sweep | O(V+E) | 5.17, 5.18 |

Design details are in [DESIGN.md](DESIGN.md), and decisions taken while building are
in [DECISIONS.md](DECISIONS.md).

### 7.5 Known limits

- No rename tracking; a rename looks like a delete plus an add.
- Criss-cross merges use one chosen base (the newest), with a warning.
- Bisect's O(log N) assumes the bug stays present along the linearised order, which holds exactly on linear history.
- Delta base selection is a window heuristic, not an optimal choice.
- `merge` and `cherry-pick` need a clean tracked state.
- `bisect run` counts exit codes 126 and 127 (command cannot run, or not found) as *bad*, so check that your test command works first.
- Single process, no networking, no tags, rebase or stash.
- The optional MiniLang AST merge (M13) is not implemented.

---

## Part 8: Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `g++: command not found`, `make: command not found` | Tools missing, or you are in PowerShell rather than Ubuntu. Repeat 1.2–1.3. |
| Build stops on a warning treated as an error | Your compiler differs from the tested one. Build without `-Werror`: `make clean && make CXXFLAGS="-std=c++17 -O2"` |
| `Permission denied` running `scripts/...` | The scripts are not marked executable. Run them as `bash scripts/<name>.sh`. |
| `$'\r': command not found` | Windows line endings got into the scripts. Fix with `sed -i 's/\r$//' scripts/*.sh tests/integration/*.sh`. Cloning with git avoids this. |
| `chronicle: command not found` | Not on `PATH`. Repeat 2.2, or open a new terminal. |
| `build first: make` from a script | `build/chronicle` is missing. Run `make` in the project folder. |
| t03 prints `git lacks sha256 support` | git is older than 2.29. The test passes as skipped; upgrade git to run the comparison. |
| `make asan` crashes at start with a shadow-memory error | A known clash between older sanitizers and newer kernels. Run `sudo sysctl vm.mmap_rnd_bits=28` and try again. |
| `chronicle: repository is locked` (exit 5) | A command was interrupted. If nothing else is running, delete `.chronicle/lock`. |
| `index file is truncated or corrupt`, or `has a bad checksum` | Run `chronicle reset --hard HEAD`. |
| Commits show `Unknown <unknown@localhost>` | Identity not set. See 3.1. |
| Dates from `at` or `log --since` look shifted | Times are read in local time. Set `export TZ=UTC`, as in 5.0. |
| Exec bits or symlinks misbehave | The repository is on `/mnt/c`. Use a Linux folder such as `~/`. |
| `ct: command not found` | New terminal. Rerun the setup in 5.0. |

---

## Part 9: Clean up and uninstall

Remove the walkthrough files:

```bash
rm -rf ~/chronicle-eval /tmp/goracle /tmp/genrepo
```

Remove build output:

```bash
cd ~/chronicle-vcs && make clean
```

Remove the installed binary (whichever you used):

```bash
rm -f ~/.local/bin/chronicle
```

```bash
sudo rm -f /usr/local/bin/chronicle
```
