. "$(dirname "$0")/lib.sh"
ch init r >/dev/null
cd r
expect_exit 1 ch log
echo 1 > f; ch add f; commit_at 1700000000 "first"
echo 2 > f; ch add f; commit_at 1700000100 "second"
ch branch feature
echo 3 > f; ch add f; commit_at 1700000200 "third"

out=$(ch log)
assert_contains "$out" "(HEAD -> main)"
assert_contains "$out" "Author: Test User <test@example.com>"
assert_contains "$out" "Date:   2023-11-14 22:16:40 +0000"
assert_contains "$out" "    third"
assert_eq 3 "$(ch log --oneline | wc -l)"
assert_eq "third second first" "$(ch log --oneline | awk '{print $NF}' | xargs)"
assert_eq 2 "$(ch log --oneline -n 2 | wc -l)"
assert_contains "$(ch log --oneline feature | head -1)" "(feature) second"
first_line=$(ch log --oneline | head -1)
short=${first_line%% *}
[ ${#short} -ge 8 ] || fail "short id shorter than core.abbrev"

out=$(ch graph)
assert_contains "$out" "* "
assert_contains "$out" "(HEAD -> main) third"
assert_contains "$out" "(feature) second"
finish
