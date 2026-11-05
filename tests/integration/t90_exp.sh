. "$(dirname "$0")/lib.sh"
ch init a >/dev/null
cd a
printf 'one\ntwo\nthree\n' > f; ch add f; commit_at 1700000000 base
ch branch shared
cd "$T"
cp -r a b

cd "$T/a"
echo a1 >> f; ch add f; commit_at 1700000100 a1
echo x > ax; ch add ax; commit_at 1700000200 a2
cd "$T/b"
echo b1 > bfile; ch add bfile; commit_at 1700000150 b1
ch checkout -b topic >/dev/null
for i in 1 2 3; do echo "t$i" > "t$i"; ch add "t$i"; commit_at $((1700000300 + i)) "t$i"; done
ch checkout main >/dev/null

cd "$T/a"
out=$(ch reconcile ../b)
assert_contains "$out" "shared: identical"
assert_contains "$out" "main: diverged (ahead 2, behind 1), merge base"
assert_contains "$out" "topic: only in other (5 commits)"
expect_exit 0 ch reconcile ../b --fetch
assert_contains "$OUT" "fetched main -> main-remote"
assert_contains "$OUT" "fetched topic -> topic-remote"
assert_eq "$(cat "$T/b/.chronicle/refs/heads/topic")" "$(cat .chronicle/refs/heads/topic-remote)"
assert_eq "$(cat "$T/b/.chronicle/refs/heads/main")" "$(cat .chronicle/refs/heads/main-remote)"
assert_contains "$(ch log --oneline topic-remote)" "t3"
expect_exit 0 ch verify
out=$(ch reconcile ../b)
assert_contains "$out" "main-remote: only here"
assert_contains "$(ch log --oneline -n 1)" "a2"
expect_exit 0 ch merge main-remote
[ -e bfile ] || fail "merge of fetched branch did not bring bfile"

t2=$(ch log --oneline topic-remote | awk '/ t2$/ {print $1}')
expect_exit 0 ch cherry-pick "$t2"
[ -e t2 ] || fail "cherry-pick did not apply t2"
assert_eq 1 "$(ch cat-file -p HEAD | grep -c '^parent ')"
assert_contains "$(ch log -n 1)" "    t2"
expect_exit 0 ch cherry-pick "$t2"
assert_contains "$OUT" "already applied as"
expect_exit 0 ch cherry-pick HEAD~2
assert_contains "$OUT" "already applied"

ch checkout -b side >/dev/null
printf 'one\nSIDE\nthree\na1\n' > f; ch add f; commit_at 1700000500 "side edit"
sid=$(cat .chronicle/refs/heads/side)
ch checkout main >/dev/null
printf 'one\nMAIN\nthree\na1\n' > f; ch add f; commit_at 1700000600 "main edit"
expect_exit 3 ch cherry-pick "$sid"
assert_contains "$OUT" "CONFLICT (content): f"
assert_contains "$(ch status)" "Cherry-picking"
printf 'one\nBOTH\nthree\na1\n' > f; ch add f
expect_exit 0 ch commit
assert_eq 1 "$(ch cat-file -p HEAD | grep -c '^parent ')"
assert_contains "$(ch cat-file -p HEAD)" "side edit"

out=$(ch graph --dot)
assert_contains "$out" "digraph chronicle {"
assert_contains "$out" "rankdir=BT;"
assert_contains "$out" "\"branch:main\" [shape=box"
assert_contains "$out" " -> "
finish
