. "$(dirname "$0")/lib.sh"
ch init r >/dev/null
cd r
assert_contains "$(ch status)" "no commits yet"
echo a > a; echo b > b; echo c > c
ch add a b c
ch commit -m base >/dev/null
assert_contains "$(ch status)" "nothing to commit, working tree clean"

echo a2 > a
rm b
echo n > new
echo c2 > c; ch add c
echo s > staged; ch add staged
out=$(ch status)
assert_contains "$out" "On branch main"
assert_contains "$out" "Staged:"
assert_contains "$out" "modified:   c"
assert_contains "$out" "new file:   staged"
assert_contains "$out" "Not staged:"
assert_contains "$out" "modified:   a"
assert_contains "$out" "deleted:    b"
assert_contains "$out" "Untracked:"
assert_contains "$out" "  new"

ch add b
assert_contains "$(ch status)" "deleted:    b"
ch commit -m "stuff" >/dev/null
out=$(ch status)
assert_not_contains "$out" "Staged:"
assert_contains "$out" "modified:   a"

ch rm --cached c >/dev/null
[ -e c ] || fail "rm --cached deleted the file"
assert_contains "$(ch status)" "deleted:    c"
ch rm staged >/dev/null
[ -e staged ] && fail "rm did not delete the file"
expect_exit 1 ch rm nosuchfile
expect_exit 1 ch add nosuchfile
assert_contains "$OUT" "did not match"

ch add . ; ch commit -m "clean up" >/dev/null
chmod +x a
assert_contains "$(ch status)" "modified:   a"
ch add a
assert_contains "$(cat .chronicle/index)" "100755"

expect_exit 1 ch add ../outside
assert_contains "$OUT" "outside the repository"

printf 'garbage' > .chronicle/index
expect_exit 1 ch status
assert_contains "$OUT" "reset --hard HEAD"
expect_exit 0 ch reset --hard HEAD
expect_exit 0 ch status

finish
