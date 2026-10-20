#!/usr/bin/env bash
# Landing gate: build, the full ctest suite and both 20-minute seed-7 smokes (switch-off and
# --pes-loop), with the smokes running in parallel with the suite. One summary at the end.
# Usage: PES21_ROOT=<PES 2021 install root> tools/dev/gate.sh [build-dir]
#   GATE_JOBS (default 8) sets ctest -j; the suite's floor is its longest test (the 20-min restart
#   test, ~14 min), so more jobs only help while cores are free. Logs go under <build-dir>/gate.*.
#   The suite and the smokes run under the shared sim lock, so concurrent gates queue instead of
#   oversubscribing the machine.
set -euo pipefail
: "${PES21_ROOT:?set PES21_ROOT to the PES 2021 install root}"
repo=$(git rev-parse --show-toplevel)
build=${1:-$repo/build}
jobs=${GATE_JOBS:-8}
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

flock "$lock" bash -c "
  $(declare -f smoke); repo='$repo'; build='$build'; PES21_ROOT='$PES21_ROOT'
  smoke off & smoke pesloop --pes-loop &
  env PES21_ROOT='$PES21_ROOT' PES21_EDIT_SAVE='$save' nice -n 19 \
    ctest --test-dir '$build' -j $jobs --timeout 2400 > '$build/gate.ctest.log' 2>&1
  echo \"ctest rc=\$?\"
  wait"

for tag in off pesloop; do
  echo "== smoke $tag"; grep -hE "\[determinism\]|\[restarts\]|\[ticks\]" "$build/gate.smoke-$tag.log" | tail -3
done
echo "== ctest"; grep -E "tests passed|Total Test time" "$build/gate.ctest.log"
sed -n '/The following tests FAILED/,$p' "$build/gate.ctest.log" | grep -E "^\s+[0-9]+ - " || true
