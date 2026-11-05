#!/usr/bin/env bash
set -u
here=$(cd "$(dirname "$0")" && pwd)
: "${CHRONICLE:?set CHRONICLE to the chronicle binary}"
export CHRONICLE
pass=0
failed=()
for t in "$here"/t*.sh; do
  [ -e "$t" ] || continue
  name=$(basename "$t")
  if bash "$t" > /tmp/chronicle-it-$$.log 2>&1; then
    echo "ok   $name"
    pass=$((pass + 1))
  else
    echo "FAIL $name"
    sed 's/^/     /' /tmp/chronicle-it-$$.log
    failed+=("$name")
  fi
done
rm -f /tmp/chronicle-it-$$.log
echo "integration: $pass passed, ${#failed[@]} failed"
[ ${#failed[@]} -eq 0 ]
