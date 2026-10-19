#!/usr/bin/env bash
# Loose-episode metric, the acceptance number on the PES path. Per seed: the loose episodes longer
# than the stall bound (over) and the sum of their excess ticks. Then the totals over the seeds.
# Usage: PES21_ROOT=<PES 2021 install root> tools/dev/loose_episodes.sh [seed ...]
#   Default seeds: 7 11 23 42. Build the target first: cmake --build build --target pes_restart_delay_test.
#   GF_TEST_BIN overrides the test binary. Logs go under build/loose-episodes.*.
set -euo pipefail
: "${PES21_ROOT:?set PES21_ROOT to the PES 2021 install root}"
repo=$(git rev-parse --show-toplevel)
bin=${GF_TEST_BIN:-$repo/build/tests/sim/pes_restart_delay_test}
[ $# -gt 0 ] || set -- 7 11 23 42
logs=$(mktemp -d "$repo/build/loose-episodes.XXXXXX")
for seed in "$@"; do
  GF_SEED=$seed "$bin" --gtest_also_run_disabled_tests \
    --gtest_filter='PesRestartDelay.DISABLED_LooseEpisodeMetric' > "$logs/$seed.log" 2>&1 &
done
wait || true  # a stalled match fails its own assertions but still prints its line
status=0
for seed in "$@"; do
  if line=$(grep -h '^\[loose-episodes\]' "$logs/$seed.log"); then
    echo "$line"
  else
    echo "seed $seed: no result (log: $logs/$seed.log)"
    status=1
  fi
done
grep -h '^\[loose-episodes\]' "$logs"/*.log \
  | awk '{ split($4, o, "="); split($5, x, "="); over += o[2]; excess += x[2]; seeds++ }
         END { printf "total over=%d excess=%d seeds=%d\n", over, excess, seeds }' || true
exit $status
