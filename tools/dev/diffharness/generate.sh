#!/usr/bin/env bash
# Generate one differential-test corpus at test time from the user's executable.
# Usage: generate.sh <spec-name> <out.jsonl>
# Skips cleanly (exit 77, like the install-gated tests) when there is no
# executable or no unicorn-capable python. Corpora are PES-derived and live
# only in the build directory; they are never committed.
set -u
spec=$1
out=$2
repo=$(git rev-parse --show-toplevel)

exe=${PES_EXE:-}
if [ -z "$exe" ] && [ -n "${PES21_ROOT:-}" ]; then exe=$PES21_ROOT/PES2021.exe; fi
if [ -z "$exe" ] || [ ! -f "$exe" ]; then
  echo "diffharness: no executable (PES_EXE or PES21_ROOT/PES2021.exe); skipping $spec"
  exit 77
fi
py=${GF_DIFF_PYTHON:-python3}
if ! "$py" -c "import unicorn" 2>/dev/null; then
  echo "diffharness: $py has no unicorn (set GF_DIFF_PYTHON); skipping $spec"
  exit 77
fi
if ! "$py" "$repo/tools/dev/diffharness/run.py" --check-spec --exe "$exe" \
    --spec "$repo/tools/dev/diffharness/specs/$spec.json"; then
  echo "diffharness: spec $spec does not match $exe; skipping"
  exit 77
fi
mkdir -p "$(dirname "$out")"
exec "$py" "$repo/tools/dev/diffharness/run.py" --exe "$exe" \
  --spec "$repo/tools/dev/diffharness/specs/$spec.json" --out "$out" --count 300
