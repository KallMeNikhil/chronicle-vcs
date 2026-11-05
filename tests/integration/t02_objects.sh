. "$(dirname "$0")/lib.sh"

expect_exit 0 ch init repo
assert_contains "$OUT" "Initialized empty Chronicle repository"
[ -f repo/.chronicle/HEAD ] || fail "HEAD missing"
assert_eq "ref: refs/heads/main" "$(cat repo/.chronicle/HEAD)"
expect_exit 1 ch init repo
assert_contains "$OUT" "already exists"

cd repo
printf 'hello\n' > hello.txt
id=$(ch hash-object hello.txt)
assert_eq 64 "${#id}" "id length"
[ -e ".chronicle/objects/${id:0:2}/${id:2}" ] && fail "hash-object without -w stored the object"
id2=$(ch hash-object -w hello.txt)
assert_eq "$id" "$id2"
[ -e ".chronicle/objects/${id:0:2}/${id:2}" ] || fail "hash-object -w did not store"
assert_eq "blob" "$(ch cat-file -t "$id")"
assert_eq "6" "$(ch cat-file -s "$id")"
assert_eq "hello" "$(ch cat-file -p "$id")"
assert_eq "$id" "$(sha256sum ".chronicle/objects/${id:0:2}/${id:2}" | cut -c1-64)"

expect_exit 2 ch cat-file "$id"
expect_exit 4 ch cat-file -t "$(printf '%064d' 0)"

cd "$T"
expect_exit 5 ch -C "$T" cat-file -t "$id"
assert_contains "$OUT" "not a chronicle repository"

finish
