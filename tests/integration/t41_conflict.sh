. "$(dirname "$0")/lib.sh"
ch init r >/dev/null
cd r
printf 'a\nb\nc\n' > f; echo keep > k; ch add . ; commit_at 1700000000 base
ch branch topic
printf 'a\nOURS\nc\n' > f; ch add f; commit_at 1700000100 ours
ch checkout topic >/dev/null
printf 'a\nTHEIRS\nc\n' > f; ch add f; commit_at 1700000200 theirs
ch checkout main >/dev/null
before=$(cat .chronicle/refs/heads/main)

expect_exit 3 ch merge topic
assert_contains "$OUT" "CONFLICT (content): f"
[ -f .chronicle/MERGE_STATE ] || fail "MERGE_STATE missing"
assert_contains "$(cat f)" "<<<<<<< ours (main)"
assert_contains "$(cat f)" ">>>>>>> theirs (topic)"
assert_contains "$(ch status)" "Merging topic; unresolved conflicts"
expect_exit 1 ch commit
assert_contains "$OUT" "unresolved conflicts"
expect_exit 1 ch checkout topic

expect_exit 0 ch merge --abort
[ -f .chronicle/MERGE_STATE ] && fail "abort left MERGE_STATE"
assert_eq "a OURS c" "$(cat f | xargs)"
assert_eq "$before" "$(cat .chronicle/refs/heads/main)"
expect_exit 1 ch merge --abort

expect_exit 3 ch merge topic
printf 'a\nRESOLVED\nc\n' > f
ch add f
assert_contains "$(ch status)" "all conflicts resolved"
expect_exit 0 ch commit
[ -f .chronicle/MERGE_STATE ] && fail "commit left MERGE_STATE"
assert_eq 2 "$(ch cat-file -p HEAD | grep -c '^parent ')"
assert_contains "$(ch cat-file -p HEAD)" "Merge branch 'topic'"

ch checkout -b md1 >/dev/null
ch rm k >/dev/null; commit_at 1700000300 "delete k"
ch checkout main >/dev/null
echo changed > k; ch add k; commit_at 1700000400 "modify k"
expect_exit 3 ch merge md1
assert_contains "$OUT" "CONFLICT (modify/delete): k"
assert_eq "changed" "$(cat k)"
ch add k
expect_exit 0 ch commit -m "keep k"
finish
