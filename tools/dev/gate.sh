#!/usr/bin/env bash
# Landing gate, two tiers (tools/dev/README.md, "Gate tiers"). One summary at the end, last line `GATE <tier>: GREEN|RED ...`.
#   GATE_TIER=fast (default): build; every ctest test except the 20-minute-class whole-match tests (SimMatch.*,
#     gfsim_run_test); the five diff_*_recorded checks narrowed to the recorded functions the change can reach
#     (tools/dev/translate/select_recorded.py, relative to GATE_BASE, default the merge base with origin/main); the
#     translated-layer drift check (a ctest test); the ledger check; one 2-minute seed-7 smoke per path (switch-off and
#     --pes-loop) with their determinism lines. Target ~30 minutes.
#   GATE_TIER=full: the whole ctest suite with the complete core/impure/cutscene/presentation/pilot re-checks, a 5-minute
#     prefilter, and both 20-minute seed-7 smokes.
# Usage: PES21_ROOT=<PES 2021 install root> [GATE_TIER=fast|full] tools/dev/gate.sh [build-dir]
#   GATE_JOBS (default: free cores, nproc minus the 1-minute load average, at least 2) sets ctest -j. The full suite's floor is
#     its longest test, so more jobs only help while cores are free. Logs go under <build-dir>/gate.*.
#   GATE_SLOTS: the two sim slots held for the run (tools/dev/simslot.sh: slot 0 is the lock file itself, slot n is
#     "$lock.n"). The tests and the two smokes run under them. Unset (the default): whichever pair (1 0) or (3 2) is
#     free first, waiting until one is. Set (e.g. "3 2"): exactly that pair, blocking.
#   GATE_BASE=<rev> (fast only) is what "changed" is measured against.
#   GF_IMPURE_BANKS (the capture bank) is required with GF_DIFF_PYTHON: impure records whose corpus is the bank refuse to run
#   without it (diff_impure_recorded).
set -euo pipefail
: "${PES21_ROOT:?set PES21_ROOT to the PES 2021 install root}"
tier=${GATE_TIER:-fast}
case "$tier" in fast | full) ;; *) echo "GATE_TIER must be fast or full, not '$tier'"; exit 2 ;; esac
repo=$(git rev-parse --show-toplevel)
build=${1:-$repo/build}
if [ -n "${GATE_JOBS:-}" ]; then jobs=$GATE_JOBS
else jobs=$(awk -v n="$(nproc)" '{j = int(n - $1); print (j < 2 ? 2 : j)}' /proc/loadavg); fi
# Diff jobs: the driver's --jobs in each diff_*_recorded check (one VA is a g++ build and a unicorn run, about 1 GB at its
# peak). Sized from free memory and cores, keeping GATE_RIG_MEM_GB and GATE_RIG_CORES for the live rig; 2 to 6 jobs.
if [ -n "${GF_DIFF_JOBS:-}" ]; then diff_jobs=$GF_DIFF_JOBS
else
  by_mem=$(awk -v r="${GATE_RIG_MEM_GB:-10}" -v j="${GATE_DIFF_JOB_MEM_GB:-2}" '/^MemAvailable:/ {print int(($2 / 1048576 - r) / j)}' /proc/meminfo)
  by_cpu=$(awk -v n="$(nproc)" -v r="${GATE_RIG_CORES:-4}" '{print int(n - $1 - r)}' /proc/loadavg)
  diff_jobs=$(( by_mem < by_cpu ? by_mem : by_cpu ))
  [ "$diff_jobs" -lt 2 ] && diff_jobs=2
  [ "$diff_jobs" -gt 6 ] && diff_jobs=6
fi
export GF_DIFF_JOBS=$diff_jobs
echo "GATE diff jobs: $diff_jobs (cores $(nproc), load $(cut -d' ' -f1 /proc/loadavg))"
lock=${GF_SIM_LOCK:-$(dirname "$repo")/gf-sim.lock}  # worktrees and the main checkout share a parent
slotfile() { if [ "$1" -eq 0 ]; then echo "$lock"; else echo "$lock.$1"; fi; }
# GATE_SLOTS set: hold exactly that pair (blocking). Unset: hold whichever pair (1 0) or (3 2) is free first, so
# concurrent gates do not queue on one pair while the other sits idle (03-11-26: three gates queued up to 112 min on 3/2).
slots_any=${GATE_SLOTS:+0}; slots_any=${slots_any:-1}
read -ra slots <<< "${GATE_SLOTS:-any}"
save=$(find ~/Documents/KONAMI/'eFootball PES 2021 SEASON UPDATE' -name EDIT00000000 2>/dev/null | head -1)
echo "GATE tier: $tier (ctest -j $jobs, sim slots ${slots[*]})"

nice -n 19 cmake --build "$build" -j3 > "$build/gate.build.log" 2>&1 || {
  echo "BUILD FAILED"; grep -m5 -E "error" "$build/gate.build.log"; exit 1; }

if [ "$tier" = fast ]; then
  smoke_minutes=2
  # ctest_exclude: the whole-match tests (the smokes below cover a match end to end).
  ctest_exclude='^(SimMatch\.|gfsim_run_test$)'
  base=${GATE_BASE:-$(git -C "$repo" merge-base HEAD origin/main)}
  python3 "$repo/tools/dev/translate/select_recorded.py" --base "$base" --out "$build/gate.select.json" | tee "$build/gate.select.log"
  recorded_select=$build/gate.select.json
else
  smoke_minutes=20
  ctest_exclude=
  recorded_select=
fi

smoke() {  # $1 = log suffix, rest = extra gfsim_run flags
  local tag=$1; shift
  nice -n 10 "$build/apps/gfsim_run" --db-sql "$repo/assets/fixtures/teams.sql" --home 1 --away 2 \
    --seed 7 --minutes "$smoke_minutes" --pes21-root "$PES21_ROOT" "$@" > "$build/gate.smoke-$tag.log" 2>&1
  echo "smoke-$tag rc=$?"
}

# Full tier prefilter: both smokes first, short (GATE_PRE_MINUTES, default 5 match minutes: the 5-min seed-7 run is where
# the throw-in stalls showed), in parallel. A stall or crash fails here instead of after the full 20-minute
# run; GATE_SKIP_PREFILTER=1 skips it. The fast tier's smoke is already short.
if [ "$tier" = full ] && [ "${GATE_SKIP_PREFILTER:-0}" != 1 ]; then
  for mode in off pesloop; do
    flags=(); [ "$mode" = pesloop ] && flags=(--pes-loop)
    nice -n 10 "$build/apps/gfsim_run" --db-sql "$repo/assets/fixtures/teams.sql" --home 1 --away 2 \
      --seed 7 --minutes "${GATE_PRE_MINUTES:-5}" --pes21-root "$PES21_ROOT" "${flags[@]}" > "$build/gate.pre-$mode.log" 2>&1 &
  done
  wait
  for mode in off pesloop; do
    grep -q "sim ticks to full time" "$build/gate.pre-$mode.log" || {
      echo "GATE full: RED (prefilter: ${GATE_PRE_MINUTES:-5}-min $mode smoke did not reach full time)"
      grep -hE "\[restart\] stalled|\[stalemate\]" "$build/gate.pre-$mode.log" | head -3; exit 1; }
    echo "prefilter $mode: $(grep -h '\[determinism\]' "$build/gate.pre-$mode.log")"
  done
fi

# The gate runs ctest and both smokes at once, so it holds the sim slots in GATE_SLOTS (two: one per smoke).
export repo build PES21_ROOT jobs smoke_minutes ctest_exclude
export PES21_EDIT_SAVE=$save GF_DIFF_PYTHON=${GF_DIFF_PYTHON:-} PES_EXE=${PES_EXE:-} GF_IMPURE_BANKS=${GF_IMPURE_BANKS:-}
export GF_RECORDED_SELECT=$recorded_select  # empty: diff_*_recorded re-check every recorded function
inner="$(declare -f smoke)"'
  smoke off & smoke pesloop --pes-loop &
  nice -n 19 ctest --test-dir "$build" -j "$jobs" --timeout 2400 ${ctest_exclude:+-E "$ctest_exclude"} > "$build/gate.ctest.log" 2>&1
  echo "ctest rc=$?"
  wait'
cmd=(bash -c "$inner")
if [ "$slots_any" = 1 ]; then
  while :; do
    for pair in "1 0" "3 2"; do
      read -r a b <<< "$pair"
      exec 8>"$(slotfile "$a")" 9>"$(slotfile "$b")"
      if flock -n 8 && flock -n 9; then slots=($a $b); break 2; fi
      exec 8>&- 9>&-
    done
    sleep 5
  done
  echo "GATE sim slots held: ${slots[*]}"
  "${cmd[@]}"
  exec 8>&- 9>&-
else
  for ((i = ${#slots[@]} - 1; i >= 0; i--)); do
    slot=${slots[i]}; cmd=(flock "$(slotfile "$slot")" "${cmd[@]}")
  done
  "${cmd[@]}"
fi

for tag in off pesloop; do
  echo "== smoke $tag ($smoke_minutes min)"; grep -hE "\[determinism\]|\[restarts\]|\[ticks\]" "$build/gate.smoke-$tag.log" | tail -3
done
echo "== ctest"; grep -E "tests passed|Total Test time" "$build/gate.ctest.log"
failed=$(sed -n '/The following tests FAILED/,$p' "$build/gate.ctest.log" | grep -E "^\s+[0-9]+ - " || true)
[ -z "$failed" ] || echo "$failed"
if [ "$tier" = fast ]; then
  echo "== recorded re-check, selected per set (base ${base:0:12})"; grep -E "recorded selected" "$build/gate.select.log"
fi
# Ledger: regenerate from evidence; fail on regression, off-path ports, or a hand edit (stamp mismatch).
if [ -f "$repo/tools/dev/ledger.py" ]; then
  python3 "$repo/tools/dev/ledger.py" --check > "$build/gate.ledger.log" 2>&1 || {
    echo "GATE $tier: RED (ledger)"; cat "$build/gate.ledger.log"; exit 1; }
  sed -n '1,12p' "$build/gate.ledger.log"
fi

# The verdict comes from the logs, not the background jobs' exit codes: green means every test passed
# and both smokes reached full time.
green=1
grep -q "100% tests passed" "$build/gate.ctest.log" || green=0
for tag in off pesloop; do grep -q "sim ticks to full time" "$build/gate.smoke-$tag.log" || green=0; done
# The PES differential suites (tests/diff, tests/pilot) skip (rc 77) without the harness Python or the unpacked image, and a
# build configured without PES_EXE compiles every pilot test to a skip stub: green while nothing was compared. With both
# set, a skipped diff test is a red gate.
if [ -n "${GF_DIFF_PYTHON:-}" ] && [ -n "${PES_EXE:-}" ]; then
  skipped=$(grep -cE "Test +#[0-9]+: (diff_|Diff).*Skipped" "$build/gate.ctest.log" || true)  # grep -c exits 1 on 0 matches
  if [ "$skipped" != 0 ]; then
    echo "GATE $tier: $skipped PES diff tests skipped (reconfigure with PES_EXE set: cmake -S . -B build)"; green=0
  fi
else
  echo "gate: GF_DIFF_PYTHON or PES_EXE unset, so the PES diff suites did not run"
fi
if [ "$green" = 1 ]; then echo "GATE $tier: GREEN"
else echo "GATE $tier: RED $(sed -E 's/^\s+[0-9]+ - ([^ ]+).*/\1/' <<< "$failed" | tr '\n' ' ')"; exit 1; fi
