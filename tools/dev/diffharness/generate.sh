#!/usr/bin/env bash
# Generate one differential-test corpus at test time from the user's UNPACKED
# executable image (PES_EXE; file offset = VA - 0x140000000).
# Usage: generate.sh <spec-name> <out.jsonl>
# Skips cleanly (exit 77, like the install-gated tests) when PES_EXE is unset,
# when the image fails the spec's probe (e.g. a packed PES2021.exe, whose bytes
# differ from the unpacked image's), or when there is no unicorn-capable
# python. Corpora are PES-derived and live only in the build directory; they
# are never committed.
set -u
spec=$1
out=$2
repo=$(git rev-parse --show-toplevel)

exe=${PES_EXE:-}
if [ -z "$exe" ] || [ ! -f "$exe" ]; then
  echo "diffharness: no unpacked image (set PES_EXE); skipping $spec"
  exit 77
fi
py=${GF_DIFF_PYTHON:-python3}
if ! "$py" -c "import unicorn" 2>/dev/null; then
  echo "diffharness: $py has no unicorn (set GF_DIFF_PYTHON); skipping $spec"
  exit 77
fi
if ! "$py" "$repo/tools/dev/diffharness/run.py" --check-spec --exe "$exe" \
    --spec "$repo/tools/dev/diffharness/specs/$spec.json"; then
  echo "diffharness: spec $spec does not match $exe (packed exes fail the probe); skipping"
  exit 77
fi
mkdir -p "$(dirname "$out")"
exec "$py" "$repo/tools/dev/diffharness/run.py" --exe "$exe" \
  --spec "$repo/tools/dev/diffharness/specs/$spec.json" --out "$out"
