#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
CH=${CHRONICLE:-$here/../build/chronicle}
[ -x "$CH" ] || { echo "build first: make" >&2; exit 1; }
export TZ=UTC CHRONICLE_AUTHOR_EMAIL="demo@example.com"
work=$(mktemp -d "${TMPDIR:-/tmp}/chronicle-demo.XXXXXX")
trap 'chmod -R u+w "$work" 2>/dev/null; rm -rf "$work"' EXIT
step() { printf '\n\033[1m== %s\033[0m\n' "$*"; }
run() { printf '$ chronicle %s\n' "$*"; "$CH" "$@"; }
at() { CHRONICLE_AUTHOR_DATE="$1 +0000" "$CH" commit -m "$2" >/dev/null; }

cd "$work"
"$CH" init demo >/dev/null
cd demo

step "1. Branches, merge, conflict, graph"
export CHRONICLE_AUTHOR_NAME="Ada"
printf 'def area(r):\n    return 3.14 * r * r\n\ndef perimeter(r):\n    return 2 * 3.14 * r\n' > geometry.py
echo "# Geometry" > README.md
"$CH" add . ; at 1700000000 "Initial commit"
"$CH" checkout -b feature >/dev/null
export CHRONICLE_AUTHOR_NAME="Grace"
printf 'def area(r):\n    return 3.14 * r * r\n\ndef perimeter(r):\n    return 2 * 3.14 * r\n\ndef search(xs, x):\n    return x in xs\n' > geometry.py
"$CH" add geometry.py ; at 1700086400 "Add search"
"$CH" checkout main >/dev/null
export CHRONICLE_AUTHOR_NAME="Ada"
printf 'import math\n\ndef area(r):\n    return math.pi * r * r\n\ndef perimeter(r):\n    return 2 * 3.14 * r\n' > geometry.py
"$CH" add geometry.py ; at 1700172800 "Use math.pi"
CHRONICLE_AUTHOR_DATE="1700200000 +0000" run merge feature
run log --graph --oneline
"$CH" checkout -b conflict-a >/dev/null
echo "Geometry helpers (A)" > README.md; "$CH" add README.md; at 1700200000 "README A"
"$CH" checkout -b conflict-b main >/dev/null
echo "Geometry helpers (B)" > README.md; "$CH" add README.md; at 1700200100 "README B"
CHRONICLE_AUTHOR_DATE="1700200200 +0000" run merge conflict-a || true
cat README.md
echo "Geometry helpers" > README.md
"$CH" add README.md
CHRONICLE_AUTHOR_DATE="1700200300 +0000" run commit
run status

step "2. BlameFast"
"$CH" checkout main >/dev/null
run blame geometry.py
t0=$(date +%s%N); "$CH" blame geometry.py HEAD~1 >/dev/null; t1=$(date +%s%N)
echo "(blame at HEAD~1: $(( (t1 - t0) / 1000000 )) ms)"

step "3. Object history and time travel"
run history geometry.py
run history --first README.md
blob=$("$CH" ls-tree HEAD geometry.py | awk '{print $3}' | cut -f1)
run history --blob "${blob:0:12}"
run at 2023-11-15T12:00 geometry.py
run export 2023-11-15T12:00 "$work/snapshot"
ls -l "$work/snapshot"
run stats --sharing

step "4. Bisect (headline demo; N = 300 here, try scripts/demo_bisect.sh 1000)"
CHRONICLE="$CH" bash "$here/demo_bisect.sh" 300 7

step "5. Pack, stats, verify, corruption detection"
for i in $(seq 1 20); do seq 1 $((100 + i)) > numbers.txt; "$CH" add numbers.txt; at $((1700300000 + i)) "numbers $i"; done
run pack
run stats
run verify
pack=$(ls .chronicle/objects/pack/*.pack | head -1)
chmod u+w "$pack"
printf 'Z' | dd of="$pack" bs=1 seek=100 conv=notrunc 2>/dev/null
echo "(flipped one byte in $(basename "$pack"))"
run verify || echo "verify exit status: $? (corruption detected)"

if [ "${1:-}" = "--bench" ]; then
  step "6. Benchmarks"
  make -C "$here/.." bench
fi
step "demo complete"
