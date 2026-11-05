. "$(dirname "$0")/lib.sh"
ch init r >/dev/null
cd r
printf '1\n2\n3\n4\n5\n' > f; ch add f; commit_at 1700000000 "Initial commit"
ch branch feature
ch checkout feature >/dev/null
printf 'x\n' > g; ch add g; commit_at 1700000100 "Add g"
ch checkout main >/dev/null

expect_exit 0 ch merge feature
assert_contains "$OUT" "Fast-forward"
assert_eq "$(cat .chronicle/refs/heads/feature)" "$(cat .chronicle/refs/heads/main)"
[ -e g ] || fail "ff did not update the working tree"
expect_exit 0 ch merge feature
assert_contains "$OUT" "Already up to date"

ch checkout feature >/dev/null
printf '1\n2\n3\n4\nFIVE\n' > f; ch add f; commit_at 1700000300 "Add search"
ch checkout main >/dev/null
printf 'ONE\n2\n3\n4\n5\n' > f; ch add f; commit_at 1700000200 "Fix typo"
expect_exit 0 ch merge feature
assert_contains "$OUT" "Merge made"
assert_eq "ONE 2 3 4 FIVE" "$(cat f | xargs)"
head=$(cat .chronicle/refs/heads/main)
assert_eq 2 "$(ch cat-file -p "$head" | grep -c '^parent ')"
assert_contains "$(ch log -n 1)" "Merge branch 'feature'"
assert_contains "$(ch status)" "nothing to commit"

out=$(ch log --graph --oneline -n 4)
line1=$(echo "$out" | sed -n 1p); line2=$(echo "$out" | sed -n 2p)
assert_contains "$line1" "*   "
assert_contains "$line1" "Merge branch 'feature'"
assert_eq '|\' "$line2"
assert_contains "$out" "| * "
assert_contains "$out" "* | "

ch branch quick
ch checkout quick >/dev/null
echo q > q; ch add q; commit_at 1700000400 "quick"
ch checkout main >/dev/null
expect_exit 0 ch merge quick --no-ff
assert_eq 2 "$(ch cat-file -p HEAD | grep -c '^parent ')"

echo dirty >> f
expect_exit 1 ch merge feature
assert_contains "$OUT" "uncommitted changes"
ch reset --hard >/dev/null

cd "$T"; ch init u >/dev/null; cd u
echo a > a; ch add a; commit_at 1700000000 "a-root"
cd "$T"; ch init u2 >/dev/null; cd u2
echo b > b; ch add b; commit_at 1700000000 "b-root"
cp -r "$T/u2/.chronicle/objects/." "$T/u/.chronicle/objects/" 2>/dev/null
broot=$(cat .chronicle/refs/heads/main)
cd "$T/u"
ch branch unrel "$broot"
expect_exit 1 ch merge unrel
assert_contains "$OUT" "unrelated histories"
expect_exit 0 ch merge unrel --allow-unrelated
[ -e a ] && [ -e b ] || fail "unrelated merge did not combine files"
finish
