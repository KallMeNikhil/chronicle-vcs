. "$(dirname "$0")/lib.sh"

expect_exit 0 ch --version
assert_contains "$OUT" "chronicle"
expect_exit 0 ch --help
assert_contains "$OUT" "usage: chronicle"
expect_exit 2 ch no-such-command
assert_contains "$OUT" "chronicle: unknown command"
expect_exit 2 ch

finish
