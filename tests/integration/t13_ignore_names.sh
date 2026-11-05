. "$(dirname "$0")/lib.sh"
ch init r >/dev/null
cd r
cat > .chronicleignore <<'EOF'
# comment
*.log
build/
secret?.txt
EOF
mkdir -p build sub/build logs
echo x > a.log; echo x > logs/b.log; echo x > build/out; echo x > sub/build/o
echo x > secret1.txt; echo x > secret12.txt; echo keep > keep.txt
ch add .
out=$(ch status)
assert_contains "$out" "new file:   keep.txt"
assert_contains "$out" "new file:   secret12.txt"
assert_contains "$out" "new file:   .chronicleignore"
assert_not_contains "$out" "a.log"
assert_not_contains "$out" "b.log"
assert_not_contains "$out" "build/out"
assert_not_contains "$out" "sub/build"
assert_not_contains "$out" "secret1.txt"
ch commit -m ignore >/dev/null

printf 'sp' > "with space.txt"
printf 'nl' > "$(printf 'new\nline')"
printf 'tb' > "$(printf 'tab\there')"
printf 'pc' > "100%.txt"
printf 'u' > "café.txt"
ch add . || fail "add weird names"
ch commit -m weird >/dev/null || fail "commit weird"
assert_contains "$(ch status)" "nothing to commit"
rm -f "with space.txt" "$(printf 'new\nline')" "$(printf 'tab\there')" "100%.txt" "café.txt"
ch reset --hard HEAD >/dev/null
assert_eq "nl" "$(cat "$(printf 'new\nline')")"
assert_eq "tb" "$(cat "$(printf 'tab\there')")"
assert_eq "pc" "$(cat "100%.txt")"
assert_eq "u" "$(cat "café.txt")"

ln -s keep.txt link
mkdir realdir; echo inner > realdir/inner
ln -s realdir dirlink
ch add link dirlink realdir
ch commit -m links >/dev/null
tree=$(ch cat-file -p HEAD | head -1 | cut -d' ' -f2)
listing=$(ch cat-file -p "$tree")
assert_contains "$listing" "120000 blob"
assert_not_contains "$listing" "dirlink/inner"
rm link dirlink
ch reset --hard HEAD >/dev/null
[ -L link ] || fail "symlink not restored"
assert_eq "keep.txt" "$(readlink link)"

echo "core.maxfilebytes=10" >> .chronicle/config
printf '0123456789abcdef' > big.bin
expect_exit 1 ch add big.bin
assert_contains "$OUT" "file too large"
assert_contains "$OUT" "big.bin"

if [ "$(id -u)" != "0" ]; then
  echo s > noperm; chmod 000 noperm
  expect_exit 1 ch add noperm
  assert_contains "$OUT" "noperm"
  chmod 644 noperm
fi

for i in $(seq 1 300); do echo "blob $i" > "f$i"; done
ch add . 2>/dev/null
found=""
for p in $(ls .chronicle/objects | grep -E '^[0-9a-f]{2}$'); do
  n=$(ls ".chronicle/objects/$p" | cut -c1-2 | sort | uniq -c | awk '$1>1{print $2; exit}')
  if [ -n "$n" ]; then found="$p$n"; break; fi
done
if [ -n "$found" ]; then
  expect_exit 1 ch find "$found"
  assert_contains "$OUT" "ambiguous prefix '$found'"
  expect_exit 1 ch cat-file -t "$found"
  assert_contains "$OUT" "ambiguous id prefix"
else
  fail "could not construct an ambiguous 4-char prefix"
fi
expect_exit 1 ch find ffffffffffffffff
finish
