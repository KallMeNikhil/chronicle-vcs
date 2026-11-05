#!/usr/bin/env bash
set -euo pipefail
N=${1:?usage: gen_repo.sh <n_commits> <files> <seed> [dir]}
F=${2:?files}
SEED=${3:?seed}
DIR=${4:-gen-repo-$N-$F-$SEED}
here=$(cd "$(dirname "$0")" && pwd)
CH=${CHRONICLE:-$here/../build/chronicle}
export TZ=UTC CHRONICLE_AUTHOR_NAME="Generator" CHRONICLE_AUTHOR_EMAIL="gen@example.com"
"$CH" init "$DIR" >/dev/null
cd "$DIR"
RANDOM=$SEED
mkdir -p src
for f in $(seq 1 "$F"); do printf 'file %d\n' "$f" > "src/file$f.txt"; done
"$CH" add . >/dev/null
CHRONICLE_AUTHOR_DATE="1700000000 +0000" "$CH" commit -m "initial" >/dev/null
for i in $(seq 2 "$N"); do
  side=0
  if [ $(( i % 25 )) -eq 0 ]; then side=1; "$CH" checkout -b "side$i" >/dev/null; fi
  f=$(( RANDOM % F + 1 ))
  if [ $(( RANDOM % 3 )) -eq 0 ]; then
    sed -i "1s/.*/file $f edited at $i/" "src/file$f.txt"
  else
    echo "line added in commit $i ($RANDOM)" >> "src/file$f.txt"
  fi
  "$CH" add "src/file$f.txt"
  when="$((1700000000 + i * 60)) +0000"
  if [ "$side" -eq 1 ]; then
    CHRONICLE_AUTHOR_DATE="$when" "$CH" commit -m "side change $i" >/dev/null
    "$CH" checkout main >/dev/null
    echo "main change $i" > "src/main$i.txt"; "$CH" add "src/main$i.txt"
    CHRONICLE_AUTHOR_DATE="$when" "$CH" commit -m "main change $i" >/dev/null
    CHRONICLE_AUTHOR_DATE="$when" "$CH" merge "side$i" >/dev/null
  else
    CHRONICLE_AUTHOR_DATE="$when" "$CH" commit -m "commit $i" >/dev/null
  fi
done
echo "generated $DIR: $("$CH" log --oneline | wc -l) commits, HEAD $(cat .chronicle/refs/heads/main)"
