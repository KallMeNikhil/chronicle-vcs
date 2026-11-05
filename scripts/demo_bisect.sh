#!/usr/bin/env bash
set -euo pipefail
N=${1:-1000}
SEED=${2:-$RANDOM}
here=$(cd "$(dirname "$0")" && pwd)
CH=${CHRONICLE:-$here/../build/chronicle}
[ -x "$CH" ] || { echo "build first: make (or set CHRONICLE)" >&2; exit 1; }
export TZ=UTC CHRONICLE_AUTHOR_NAME="Demo" CHRONICLE_AUTHOR_EMAIL="demo@example.com"
work=$(mktemp -d "${TMPDIR:-/tmp}/chronicle-bisect.XXXXXX")
trap 'rm -rf "$work"' EXIT
cd "$work"
"$CH" init repo >/dev/null
cd repo
RANDOM=$SEED
planted=$(( (RANDOM * 32768 + RANDOM) % (N - 1) + 2 ))
echo "building $N commits (bug planted at commit #$planted, seed $SEED)..."
start=$(date +%s)
planted_id=""
first_id=""
for i in $(seq 1 "$N"); do
  echo "result_$i = compute($i)" >> calc.txt
  if [ "$i" -eq "$planted" ]; then echo "BUG = 1 / 0" >> calc.txt; fi
  "$CH" add calc.txt
  CHRONICLE_AUTHOR_DATE="$((1700000000 + i * 60)) +0000" "$CH" commit -m "commit $i" >/dev/null
  if [ "$i" -eq 1 ]; then first_id=$(cat .chronicle/refs/heads/main); fi
  if [ "$i" -eq "$planted" ]; then planted_id=$(cat .chronicle/refs/heads/main); fi
done
echo "built in $(( $(date +%s) - start ))s"

"$CH" bisect start >/dev/null
"$CH" bisect bad HEAD >/dev/null
"$CH" bisect good "$first_id" >/dev/null
t0=$(date +%s%N)
out=$("$CH" bisect run sh -c '! grep -q BUG calc.txt')
t1=$(date +%s%N)
echo "$out" | sed 's/^/  /'
found=$(echo "$out" | sed -n 's/^first bad commit: //p')
tests=$(echo "$out" | sed -n 's/^tests run: //p')
bound=$(awk -v n="$N" 'BEGIN { l = log(n) / log(2); c = int(l); if (c < l) c++; print c + 1 }')
echo "planted : $planted_id"
echo "found   : $found"
echo "tests   : $tests (bound ceil(log2 $N)+1 = $bound), bisect time $(( (t1 - t0) / 1000000 )) ms"
"$CH" bisect reset >/dev/null
if [ "$found" = "$planted_id" ] && [ "$tests" -le "$bound" ]; then
  echo "DEMO OK"
else
  echo "DEMO FAILED" >&2
  exit 1
fi
