. "$(dirname "$0")/lib.sh"
ch init r >/dev/null
cd r
echo base > f; mkdir d; echo x > d/x
ch add . ; ch commit -m base >/dev/null

expect_exit 0 ch branch feature
expect_exit 1 ch branch feature
assert_contains "$OUT" "already exists"
for bad in "a..b" "/a" "a/" "a.lock" "a b" "a//b"; do
  expect_exit 1 ch branch "$bad"
done
expect_exit 0 ch branch nested/name
out=$(ch branch)
assert_contains "$out" "* main"
assert_contains "$out" "  feature"
assert_contains "$out" "  nested/name"

expect_exit 0 ch checkout feature
assert_contains "$OUT" "Switched to branch 'feature'"
echo feat > f; rm d/x; echo y > d/y
ch add . ; ch commit -m feat >/dev/null
expect_exit 0 ch checkout main
assert_eq "base" "$(cat f)"
[ -e d/x ] || fail "d/x not restored"
[ -e d/y ] && fail "d/y not removed"
expect_exit 0 ch checkout feature
assert_eq "feat" "$(cat f)"

echo dirty > f
expect_exit 1 ch checkout main
assert_contains "$OUT" "local changes would be lost"
assert_eq "dirty" "$(cat f)"
ch add f
expect_exit 1 ch checkout main
expect_exit 0 ch reset --hard
assert_eq "feat" "$(cat f)"

ch checkout main >/dev/null
echo mine > d/y
expect_exit 1 ch checkout feature
assert_contains "$OUT" "untracked working tree files would be overwritten"
rm d/y

expect_exit 0 ch checkout -b topic feature
assert_eq "feat" "$(cat f)"
expect_exit 1 ch branch -d topic
assert_contains "$OUT" "current branch"
ch checkout main >/dev/null
expect_exit 0 ch branch -d topic
assert_contains "$OUT" "Deleted branch topic"
expect_exit 1 ch branch -d topic

expect_exit 0 ch reset --hard feature
assert_eq "$(cat .chronicle/refs/heads/feature)" "$(cat .chronicle/refs/heads/main)"
assert_eq "feat" "$(cat f)"
echo junk > untracked.txt
ch reset --hard HEAD~1 >/dev/null
assert_eq "base" "$(cat f)"
[ -e untracked.txt ] || fail "reset --hard removed an untracked file"

finish
