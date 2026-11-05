. "$(dirname "$0")/lib.sh"
ch init r >/dev/null
cd r

expect_exit 1 ch commit -m "empty"
assert_contains "$OUT" "nothing to commit"
expect_exit 2 ch commit
echo one > a.txt
mkdir -p src/deep
echo two > src/deep/b.txt
expect_exit 0 ch add .
expect_exit 0 ch commit -m "first"
assert_contains "$OUT" "[main "
assert_contains "$OUT" "] first"
expect_exit 1 ch commit -m "again"
assert_contains "$OUT" "nothing to commit"
head_id=$(cat .chronicle/refs/heads/main)
assert_eq 64 "${#head_id}"
assert_eq "commit" "$(ch cat-file -t "$head_id")"
assert_contains "$(ch cat-file -p "$head_id")" "author Test User <test@example.com>"

assert_eq "commit" "$(ch cat-file -t "${head_id:0:10}")"
expect_exit 1 ch cat-file -t "${head_id:0:3}"
expect_exit 0 ch find "${head_id:0:6}"
assert_contains "$OUT" "$head_id commit"

echo more >> a.txt
ch add a.txt
expect_exit 0 ch commit -m "second"
p1=$(ch cat-file -p HEAD~1 | head -1)
p2=$(ch cat-file -p HEAD^ | head -1)
p3=$(ch cat-file -p "$head_id" | head -1)
assert_eq "$p3" "$p1"
assert_eq "$p3" "$p2"
expect_exit 1 ch cat-file -t HEAD~5

ch checkout "$head_id" >/dev/null || fail "detached checkout"
echo d > d.txt
ch add d.txt
expect_exit 0 ch commit -m "detached work"
assert_contains "$OUT" "detached HEAD"
assert_contains "$OUT" "not on any branch"
ch checkout main >/dev/null 2>&1 || fail "back to main"
[ -e d.txt ] && fail "d.txt should be removed when leaving the detached commit"

touch .chronicle/lock
expect_exit 5 ch add a.txt
assert_contains "$OUT" "repository is locked"
rm .chronicle/lock
expect_exit 0 ch add a.txt

finish
