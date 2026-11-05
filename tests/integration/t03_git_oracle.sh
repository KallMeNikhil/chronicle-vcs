. "$(dirname "$0")/lib.sh"

if ! command -v git >/dev/null 2>&1; then
  echo "git not installed: oracle skipped"
  finish
fi
git init -q --object-format=sha256 g 2>/dev/null || { echo "git lacks sha256 support: skipped"; finish; }
cd g
sizes="0 1 2 55 56 63 64 65 100 1000 4095 4096 4097 65535 65536 65537 1048576"
i=0
for s in $sizes; do
  head -c "$s" /dev/urandom > "f$i"
  i=$((i + 1))
done
while [ $i -lt 100 ]; do
  head -c $(( (i * 7919) % 1048577 )) /dev/urandom > "f$i"
  i=$((i + 1))
done
mismatch=0
for f in f*; do
  a=$(git hash-object "$f")
  b=$(ch hash-object "$f")
  if [ "$a" != "$b" ]; then
    mismatch=$((mismatch + 1))
    fail "$f ($(wc -c < "$f") bytes): git=$a chronicle=$b"
  fi
done
echo "compared $i files, $mismatch mismatches"
finish
