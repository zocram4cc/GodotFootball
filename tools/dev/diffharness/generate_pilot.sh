#!/usr/bin/env bash
# Generate one pilot differential-test corpus at test time from the user's
# UNPACKED executable image (PES_EXE; file offset = VA - 0x140000000).
# Usage: generate_pilot.sh <va-hex> <out.jsonl>
# (va-hex is the bare hex digits, e.g. 140439F50; the spec is
# tools/dev/diffharness/specs/pilot/<va-hex>.json.)
# Skips cleanly (exit 77) exactly like generate.sh when PES_EXE is unset,
# when the image fails the spec's probe, or when there is no unicorn python.
set -u
va=$1
out=$2
repo=$(git rev-parse --show-toplevel)

exe=${PES_EXE:-}
if [ -z "$exe" ] || [ ! -f "$exe" ]; then
  echo "diffharness: no unpacked image (set PES_EXE); skipping pilot $va"
  exit 77
fi
py=${GF_DIFF_PYTHON:-python3}
if ! "$py" -c "import unicorn" 2>/dev/null; then
  echo "diffharness: $py has no unicorn (set GF_DIFF_PYTHON); skipping pilot $va"
  exit 77
fi
if ! "$py" "$repo/tools/dev/diffharness/run.py" --check-spec --exe "$exe" \
    --spec "$repo/tools/dev/diffharness/specs/pilot/$va.json"; then
  echo "diffharness: spec pilot/$va does not match $exe (packed exes fail the probe); skipping"
  exit 77
fi
mkdir -p "$(dirname "$out")"
exec "$py" "$repo/tools/dev/diffharness/run.py" --exe "$exe" \
  --spec "$repo/tools/dev/diffharness/specs/pilot/$va.json" --out "$out"
