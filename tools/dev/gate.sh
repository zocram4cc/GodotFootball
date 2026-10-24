#!/usr/bin/env bash
# Landing gate: build, the full ctest suite and both 20-minute seed-7 smokes (switch-off and
# --pes-loop), with the smokes running in parallel with the suite. One summary at the end.
# Usage: PES21_ROOT=<PES 2021 install root> tools/dev/gate.sh [build-dir]
#   GATE_JOBS (default 4) sets ctest -j; the suite's floor is its longest test (the 20-min restart
#   test, ~14 min), so more jobs only help while cores are free. Logs go under <build-dir>/gate.*.
#   The suite and the smokes run under the shared sim lock, so concurrent gates queue instead of
#   oversubscribing the machine.
set -euo pipefail
: "${PES21_ROOT:?set PES21_ROOT to the PES 2021 install root}"
repo=$(git rev-parse --show-toplevel)
build=${1:-$repo/build}
jobs=${GATE_JOBS:-4}
lock=${GF_SIM_LOCK:-$(dirname "$repo")/gf-sim.lock}  # worktrees and the main checkout share a parent
save=$(find ~/Documents/KONAMI/'eFootball PES 2021 SEASON UPDATE' -name EDIT00000000 2>/dev/null | head -1)

nice -n 19 cmake --build "$build" -j3 > "$build/gate.build.log" 2>&1 || {
  echo "BUILD FAILED"; grep -m5 -E "error" "$build/gate.build.log"; exit 1; }

smoke() {  # $1 = log suffix, rest = extra gfsim_run flags
  local tag=$1; shift
  nice -n 10 "$build/apps/gfsim_run" --db-sql "$repo/assets/fixtures/teams.sql" --home 1 --away 2 \
    --seed 7 --minutes 20 --pes21-root "$PES21_ROOT" "$@" > "$build/gate.smoke-$tag.log" 2>&1
  echo "smoke-$tag rc=$?"
}

# Prefilter: both smokes first, short (GATE_PRE_MINUTES, default 5 match minutes: the 5-min seed-7 run is where
# the throw-in stalls showed), in parallel. A stall or crash fails here instead of after the full 20-minute
# run; GATE_SKIP_PREFILTER=1 skips it.
if [ "${GATE_SKIP_PREFILTER:-0}" != 1 ]; then
  for mode in off pesloop; do
    flags=(); [ "$mode" = pesloop ] && flags=(--pes-loop)
    nice -n 10 "$build/apps/gfsim_run" --db-sql "$repo/assets/fixtures/teams.sql" --home 1 --away 2 \
      --seed 7 --minutes "${GATE_PRE_MINUTES:-5}" --pes21-root "$PES21_ROOT" "${flags[@]}" > "$build/gate.pre-$mode.log" 2>&1 &
  done
  wait
  for mode in off pesloop; do
    grep -q "sim ticks to full time" "$build/gate.pre-$mode.log" || {
      echo "GATE RED (prefilter: ${GATE_PRE_MINUTES:-5}-min $mode smoke did not reach full time)"
      grep -hE "\[restart\] stalled|\[stalemate\]" "$build/gate.pre-$mode.log" | head -3; exit 1; }
    echo "prefilter $mode: $(grep -h '\[determinism\]' "$build/gate.pre-$mode.log")"
  done
fi

# The gate runs ctest and both smokes at once, so it holds every sim slot (tools/dev/simslot.sh): slot 0 is the lock file
# itself, slot 1 is "$lock.1".
flock "$lock.1" flock "$lock" bash -c "
  $(declare -f smoke); repo='$repo'; build='$build'; PES21_ROOT='$PES21_ROOT'
  smoke off & smoke pesloop --pes-loop &
  env PES21_ROOT='$PES21_ROOT' PES21_EDIT_SAVE='$save' GF_DIFF_PYTHON='${GF_DIFF_PYTHON:-}' PES_EXE='${PES_EXE:-}' nice -n 19 \
    ctest --test-dir '$build' -j $jobs --timeout 2400 > '$build/gate.ctest.log' 2>&1
  echo \"ctest rc=\$?\"
  wait"

for tag in off pesloop; do
  echo "== smoke $tag"; grep -hE "\[determinism\]|\[restarts\]|\[ticks\]" "$build/gate.smoke-$tag.log" | tail -3
done
echo "== ctest"; grep -E "tests passed|Total Test time" "$build/gate.ctest.log"
sed -n '/The following tests FAILED/,$p' "$build/gate.ctest.log" | grep -E "^\s+[0-9]+ - " || true

# The verdict comes from the logs, not the background jobs' exit codes: green means every test passed
# and both smokes reached full time.
green=1
grep -q "100% tests passed" "$build/gate.ctest.log" || green=0
for tag in off pesloop; do grep -q "sim ticks to full time" "$build/gate.smoke-$tag.log" || green=0; done
if [ "$green" = 1 ]; then echo "GATE GREEN"; else echo "GATE RED"; exit 1; fi
