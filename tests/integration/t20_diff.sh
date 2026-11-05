. "$(dirname "$0")/lib.sh"
ch init r >/dev/null
cd r
printf 'one\ntwo\nthree\n' > f.txt
mkdir d; echo inner > d/in.txt
ch add . ; ch commit -m first >/dev/null
first=$(cat .chronicle/refs/heads/main)

printf 'one\nTWO\nthree\n' > f.txt
out=$(ch diff)
assert_contains "$out" "diff --chronicle a/f.txt b/f.txt"
assert_contains "$out" "--- a/f.txt"
assert_contains "$out" "+++ b/f.txt"
assert_contains "$out" "@@ -1,3 +1,3 @@"
assert_contains "$out" "-two"
assert_contains "$out" "+TWO"
assert_eq "" "$(ch diff --cached)"
ch add f.txt
assert_eq "" "$(ch diff)"
assert_contains "$(ch diff --cached)" "+TWO"
ch commit -m second >/dev/null

out=$(ch diff "$first" HEAD)
assert_contains "$out" "+TWO"
assert_eq "" "$(ch diff "$first" HEAD -- d)"
assert_contains "$(ch diff "$first" HEAD -- f.txt)" "-two"

printf 'bin\0ary' > b.bin
ch add b.bin ; ch commit -m bin >/dev/null
printf 'bin\0ARY' > b.bin
assert_contains "$(ch diff)" "Binary files differ"
ch reset --hard >/dev/null

rm d/in.txt
out=$(ch diff)
assert_contains "$out" "deleted file mode 100644"
assert_contains "$out" "+++ /dev/null"
ch reset --hard >/dev/null

out=$(ch show HEAD~1)
assert_contains "$out" "commit "
assert_contains "$out" "Author: Test User <test@example.com>"
assert_contains "$out" "    second"
assert_contains "$out" "+TWO"
assert_contains "$(ch show "$first")" "new file mode 100644"

out=$(ch ls-tree HEAD)
assert_contains "$out" "040000 tree"
assert_contains "$out" "	d"
assert_contains "$(ch ls-tree -r HEAD)" "	d/in.txt"
assert_contains "$(ch ls-tree HEAD d)" "	d/in.txt"
blob=$(ch ls-tree HEAD f.txt | cut -d' ' -f3 | cut -f1)
assert_eq "TWO" "$(ch show "$blob" | sed -n 2p)"
expect_exit 1 ch ls-tree HEAD nope
finish
