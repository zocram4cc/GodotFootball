#!/bin/bash
# passget_oracle_batch.sh SEQ...: the window oracle (tests/pes_passget_oracle_test.cpp) over captured ActionPassGet ticks, a few at a time
# (each capture is ~4 GB unpacked). Unpacks scratch copies of live2/<PID>/SEQ, proves them (prove_bounded.py keeps the work dir the oracle
# directories are made from), builds the oracle directories, runs the test, deletes the copies.
# Env: PES_EXE, PES21_ROOT, LIVE (the launch folder, e.g. .../live2/pid480109), RV_HOOKS (a hooks cfg that still holds the passget hook),
#      WORK (scratch dir), PY (harness Python), REPO (default: the checkout this script is in), TMPDIR.
set -e
REPO=${REPO:-$(cd "$(dirname "$0")/../../.." && pwd)}
cd "$REPO"
export PES_EXE PES21_ROOT RV_HOOKS TMPDIR=${TMPDIR:-$WORK/tmp}
mkdir -p "$TMPDIR"
args=""
for s in "$@"; do
  d=$WORK/real/$s
  if [ ! -d "$d" ]; then
    mkdir -p "$d"; cp "$LIVE/$s/index.json" "$LIVE/$s/call.bin" "$d/"
    if [ -f "$LIVE/$s/mem.bin.zst" ]; then zstd -q -f -d --no-sparse "$LIVE/$s/mem.bin.zst" -o "$d/mem.bin"; else cp "$LIVE/$s/mem.bin" "$d/mem.bin"; fi
  fi
  args="$args --real $d"
done
rm -rf "$WORK/wo" "$WORK/oracle"; mkdir -p "$WORK/oracle"
$PY tools/dev/translate/prove_bounded.py 0x14094E0E0 "$WORK/wo" --tag passget_o $args > "$WORK/prove.log" 2>&1
for s in "$@"; do
  $PY tools/dev/translate/passget_oracle.py "$WORK/real/$s" "$WORK/wo/build/gen/14094E0E0/snap_$s" "$WORK/wo/tr_$s" "$RV_HOOKS" "$WORK/oracle/$s" > "$WORK/oracle/$s.log" 2>&1
done
GF_PES_PASSGET_ORACLE=$WORK/oracle timeout 900 build/tests/pes_passget_oracle_test
for s in "$@"; do rm -rf "$WORK/real/$s"; done
