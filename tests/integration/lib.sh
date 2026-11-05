set -u
export TZ=UTC
export LC_ALL=C
export CHRONICLE_AUTHOR_NAME="Test User"
export CHRONICLE_AUTHOR_EMAIL="test@example.com"
unset CHRONICLE_AUTHOR_DATE

T=$(mktemp -d "${TMPDIR:-/tmp}/chronicle-it.XXXXXX")
trap 'chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 1

FAILS=0
ch() { "$CHRONICLE" "$@"; }

fail() {
  echo "  FAIL: $*" >&2
  FAILS=$((FAILS + 1))
}

assert_eq() {
  if [ "$1" != "$2" ]; then fail "${3:-values differ}: expected [$1] got [$2]"; fi
}

assert_contains() {
  case "$1" in *"$2"*) ;; *) fail "${3:-missing text}: [$2] not in [$1]" ;; esac
}

assert_not_contains() {
  case "$1" in *"$2"*) fail "${3:-unexpected text}: [$2] in [$1]" ;; *) ;; esac
}

expect_exit() {
  local want=$1
  shift
  OUT=$("$@" 2>&1)
  local got=$?
  if [ "$got" != "$want" ]; then fail "exit $got (want $want) from: $* :: $OUT"; fi
}

commit_at() {
  CHRONICLE_AUTHOR_DATE="$1 +0000" ch commit -m "$2" >/dev/null || fail "commit '$2' failed"
}

finish() {
  if [ "$FAILS" -ne 0 ]; then
    echo "$FAILS failure(s)" >&2
    exit 1
  fi
  exit 0
}
