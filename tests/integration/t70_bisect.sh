. "$(dirname "$0")/lib.sh"
ch init r >/dev/null
cd r
N=64
for i in $(seq 1 $N); do
  echo "line $i" >> calc.txt
  [ "$i" -eq 37 ] && echo "BUG" >> calc.txt
  ch add calc.txt; commit_at $((1700000000 + i)) "c$i"
  [ "$i" -eq 1 ] && first=$(cat .chronicle/refs/heads/main)
  [ "$i" -eq 37 ] && planted=$(cat .chronicle/refs/heads/main)
done
head_before=$(cat .chronicle/refs/heads/main)

expect_exit 1 ch bisect bad
assert_contains "$OUT" "no bisect in progress"
ch bisect start >/dev/null
ch bisect bad >/dev/null
expect_exit 0 ch bisect good "$first"
assert_contains "$OUT" "Bisecting: 63 candidates left"
[ -f .chronicle/bisect-work/calc.txt ] || fail "midpoint not exported"
out=$(ch bisect run sh -c '! grep -q BUG calc.txt')
assert_contains "$out" "first bad commit: $planted"
tests=$(echo "$out" | sed -n 's/^tests run: //p')
[ "$tests" -le 7 ] || fail "bisect used $tests tests (> ceil(log2 64)+1 = 7)"
assert_contains "$(ch bisect log)" "first bad commit $planted"
assert_eq "$head_before" "$(cat .chronicle/refs/heads/main)"
assert_contains "$(ch status)" "nothing to commit"
ch bisect reset >/dev/null
[ -f .chronicle/BISECT ] && fail "reset left BISECT"

ch bisect start >/dev/null
ch bisect bad HEAD >/dev/null
ch bisect good "$first" >/dev/null
for k in $(seq 1 10); do
  if grep -q BUG .chronicle/bisect-work/calc.txt 2>/dev/null; then o=$(ch bisect bad); else o=$(ch bisect good); fi
  case "$o" in *"first bad commit"*) break ;; esac
done
assert_contains "$o" "first bad commit: $planted"
ch bisect reset >/dev/null

ch bisect start >/dev/null; ch bisect bad HEAD >/dev/null; ch bisect good "$first" >/dev/null
out=$(ch bisect run sh -c 'n=$(wc -l < calc.txt); [ $((n % 2)) -eq 0 ] && exit 125; ! grep -q BUG calc.txt')
assert_contains "$out" "is skip"
case "$out" in *"first bad commit"*|*"only skipped commits left"*) ;; *) fail "skip run did not terminate cleanly: $out" ;; esac
ch bisect reset >/dev/null

ch bisect start >/dev/null; ch bisect bad HEAD >/dev/null; ch bisect good "$first" >/dev/null
expect_exit 1 ch bisect run sh -c 'exit 130'
assert_contains "$OUT" "aborting"
ch bisect reset >/dev/null

ch bisect start --strategy=weighted >/dev/null; ch bisect bad HEAD >/dev/null; ch bisect good "$first" >/dev/null
out=$(ch bisect run sh -c '! grep -q BUG calc.txt')
assert_contains "$out" "first bad commit: $planted"
ch bisect reset >/dev/null
finish
