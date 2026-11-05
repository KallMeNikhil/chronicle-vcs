. "$(dirname "$0")/lib.sh"
ch init r >/dev/null
cd r
d1=1700096400; d2=$((d1 + 86400)); d3=$((d2 + 86400)); d4=$((d3 + 86400))
mkdir src
echo v1 > src/app.txt; echo readme > README; ch add .; commit_at $d1 "day1 add app"
c1=$(cat .chronicle/refs/heads/main)
echo v2 > src/app.txt; ch add .; commit_at $d2 "day2 change app"
c2=$(cat .chronicle/refs/heads/main)
echo other > other.txt; ch add .; commit_at $d3 "day3 other"
ch rm src/app.txt >/dev/null; commit_at $d4 "day4 delete app"
c4=$(cat .chronicle/refs/heads/main)

out=$(ch history src/app.txt)
assert_eq 3 "$(echo "$out" | wc -l)"
assert_contains "$(echo "$out" | sed -n 1p)" "D ${c4:0:8}"
assert_contains "$(echo "$out" | sed -n 3p)" "A ${c1:0:8}"
assert_contains "$(ch history src)" "src/app.txt"
out=$(ch history --first src/app.txt)
assert_contains "$out" "$c1"
blob1=$(ch hash-object /dev/stdin <<< "v1")
out=$(ch history --blob "$blob1")
assert_contains "$out" "${c1:0:8}"
assert_contains "$out" "replaced in ${c2:0:8}"
blob2=$(ch hash-object /dev/stdin <<< "v2")
assert_contains "$(ch history --blob "${blob2:0:12}")" "deleted in ${c4:0:8}"
expect_exit 1 ch history nothing-here

assert_eq "day3 other day2 change app" "$(ch log --oneline --since 2023-11-17 --until 2023-11-18T12:00 | cut -d' ' -f2- | xargs)"
assert_eq 2 "$(ch log --oneline --until $d2 | wc -l)"
assert_eq 1 "$(ch log --oneline --since $d4 | wc -l)"
assert_eq 3 "$(ch log --oneline -- src/app.txt | wc -l)"
assert_eq 1 "$(ch log --oneline -- other.txt | wc -l)"

assert_eq "v1" "$(ch at "$c1" src/app.txt)"
assert_eq "v2" "$(ch at 2023-11-17T12:00 src/app.txt)"
assert_eq "v1" "$(ch at $((d2 - 1)) src/app.txt)"
assert_eq "v2" "$(ch at $d2 src/app.txt)"
expect_exit 1 ch at HEAD src/app.txt
expect_exit 1 ch at 2000-01-01 README
assert_contains "$(ch at HEAD~3 src)" "app.txt"
assert_eq "$c4" "$(cat .chronicle/refs/heads/main)"
[ -e src/app.txt ] && fail "at modified the working tree"

expect_exit 0 ch export 2023-11-17T12:00 "$T/snap"
assert_contains "$OUT" "exported ${c2:0:8}"
assert_eq "v2" "$(cat "$T/snap/src/app.txt")"
assert_eq "readme" "$(cat "$T/snap/README")"
[ -w "$T/snap/README" ] && [ "$(id -u)" != 0 ] && fail "export should be read-only"
expect_exit 1 ch export HEAD "$T/snap"
assert_contains "$OUT" "must not exist or be an empty directory"
assert_eq "$c4" "$(cat .chronicle/refs/heads/main)"

out=$(ch stats --sharing)
assert_contains "$out" "structural sharing over the last 4 commits"
assert_contains "$out" "sharing ratio:"
finish
