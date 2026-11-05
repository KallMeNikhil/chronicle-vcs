. "$(dirname "$0")/lib.sh"
ch init r >/dev/null
cd r
for i in $(seq 1 30); do
  seq 1 $((200 + i)) | sed "s/^/row /" > data.txt
  echo "v$i" > small.txt
  ch add .; commit_at $((1700000000 + i)) "c$i"
done
ch log --oneline > ../log_before
ch ls-tree -r HEAD > ../tree_before
loose_before=$(find .chronicle/objects -type f -path '*/[0-9a-f][0-9a-f]/*' | wc -l)

expect_exit 0 ch pack
assert_contains "$OUT" "as deltas"
assert_eq 0 "$(find .chronicle/objects -type f -path '*/[0-9a-f][0-9a-f]/*' | wc -l)"
ls .chronicle/objects/pack/*.pack >/dev/null || fail "no pack written"
ls .chronicle/objects/pack/*.idx >/dev/null || fail "no idx written"
assert_eq "$(cat ../log_before)" "$(ch log --oneline)"
assert_eq "$(cat ../tree_before)" "$(ch ls-tree -r HEAD)"
assert_eq "row 1" "$(ch at HEAD~29 data.txt | head -1)"
assert_eq "v1" "$(ch at HEAD~29 small.txt)"
expect_exit 0 ch verify
assert_contains "$OUT" "0 corrupt, 0 missing"
out=$(ch stats)
assert_contains "$out" "stored as deltas"
assert_contains "$out" "loose objects:  0"

for id in $(ch ls-tree -r HEAD~10 | awk '{print $3}'); do
  ch cat-file -p "$id" >/dev/null || fail "cannot read packed $id"
done

echo "garbage $RANDOM" > junk.txt
junk=$(ch hash-object -w junk.txt); rm junk.txt
ch checkout -b doomed >/dev/null
echo doomed > d.txt; ch add d.txt; commit_at 1700000100 "doomed"
ch checkout main >/dev/null
ch branch -d doomed >/dev/null
expect_exit 0 ch gc --dry-run
assert_contains "$OUT" "[dry run]"
assert_eq "blob" "$(ch cat-file -t "$junk")"
expect_exit 0 ch gc --grace-seconds 3600
assert_contains "$OUT" "kept by grace period: 4"
assert_eq "blob" "$(ch cat-file -t "$junk")"
expect_exit 0 ch gc
assert_contains "$OUT" "unreachable loose objects deleted: 4"
expect_exit 4 ch cat-file -t "$junk"
assert_eq "$(cat ../log_before)" "$(ch log --oneline)"
assert_eq "$(cat ../tree_before)" "$(ch ls-tree -r HEAD)"
assert_eq 1 "$(ls .chronicle/objects/pack/*.pack | wc -l)"
expect_exit 0 ch verify

echo new > n.txt; ch add n.txt; commit_at 1700000200 "n"
nid=$(ch hash-object n.txt)
f=.chronicle/objects/${nid:0:2}/${nid:2}
chmod u+w "$f"; printf 'blob 4\0nEw\n' > "$f"
expect_exit 1 ch verify
assert_contains "$OUT" "corrupt"
assert_contains "$OUT" "$nid"
expect_exit 4 ch cat-file -p "$nid"
assert_contains "$OUT" "corrupt object $nid"

p=$(ls .chronicle/objects/pack/*.pack)
chmod u+w "$p"
printf 'X' | dd of="$p" bs=1 seek=200 conv=notrunc 2>/dev/null
expect_exit 1 ch verify
assert_contains "$OUT" "trailer checksum mismatch"
finish
