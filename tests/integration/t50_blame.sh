. "$(dirname "$0")/lib.sh"
ch init r >/dev/null
cd r
export CHRONICLE_AUTHOR_NAME="Alice"
printf 'one\ntwo\nthree\n' > f; ch add f; commit_at 1700000000 "c1"
c1=$(cat .chronicle/refs/heads/main)
ch checkout -b side >/dev/null
export CHRONICLE_AUTHOR_NAME="Bob"
printf 'one\ntwo\nthree\nfour\n' > f; ch add f; commit_at 1700086400 "c3 side"
c3=$(cat .chronicle/refs/heads/side)
ch checkout main >/dev/null
export CHRONICLE_AUTHOR_NAME="Carol"
printf 'one\nTWO\nthree\n' > f; ch add f; commit_at 1700172800 "c2 main"
c2=$(cat .chronicle/refs/heads/main)
ch merge side >/dev/null || fail "merge"

out=$(ch blame f)
assert_eq 4 "$(echo "$out" | wc -l)"
assert_eq "${c1:0:8} (Alice 2023-11-14) 1) one" "$(echo "$out" | sed -n 1p)"
assert_eq "${c2:0:8} (Carol 2023-11-16) 2) TWO" "$(echo "$out" | sed -n 2p)"
assert_eq "${c1:0:8} (Alice 2023-11-14) 3) three" "$(echo "$out" | sed -n 3p)"
assert_eq "${c3:0:8} (Bob 2023-11-15) 4) four" "$(echo "$out" | sed -n 4p)"
assert_contains "$(ch blame f "$c3")" "${c3:0:8} (Bob 2023-11-15) 4) four"
expect_exit 1 ch blame missing.txt

assert_eq "CHRONICLE-HISTORY 1" "$(head -1 .chronicle/history.idx)"
lines_before=$(wc -l < .chronicle/history.idx)
rm .chronicle/history.idx
expect_exit 0 ch reindex
assert_contains "$OUT" "history.idx rebuilt"
assert_eq "$lines_before" "$(wc -l < .chronicle/history.idx)"
printf 'deadbeef\tM' >> .chronicle/history.idx
assert_eq 4 "$(ch blame f | wc -l)"

printf 'bin\0x' > b.bin; ch add b.bin; commit_at 1700200000 "bin"
expect_exit 1 ch blame b.bin
assert_contains "$OUT" "binary file"
finish
