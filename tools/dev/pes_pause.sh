#!/usr/bin/env bash
# Free CPU for sims by pausing the running PES 2021 (SIGSTOP) and resuming it afterwards (SIGCONT).
# Usage: tools/dev/pes_pause.sh stop|cont|status|slow
#   stop    pause PES2021.exe (the live match freezes; live RE must not run meanwhile). Verified safe
#           for a 2 s pause on 20-10-26 (resumed at full CPU, all threads intact); LONG pauses are
#           untested against GPU/audio timeouts, so prefer `slow`, and check the PES window after `cont`.
#   cont    resume it
#   slow    keep it running but pin it to two cores at nice 19 (the default for shared use)
#   status  show its state, nice and CPU use
# Always pair stop with cont: `tools/dev/pes_pause.sh stop; <your run>; tools/dev/pes_pause.sh cont`.
# The user's own python processes can be niced the same way: PES_PAUSE_PYTHON=1 makes slow
# renice them to 19 too (pausing them is not offered: agents' tools run in python).
set -euo pipefail
pids=$(pgrep -x PES2021.exe || true)
[ -n "$pids" ] || { echo "PES2021.exe is not running"; exit 0; }
case "${1:-status}" in
  stop) kill -STOP $pids; echo "paused PES2021 ($pids)";;
  cont) kill -CONT $pids; echo "resumed PES2021 ($pids)";;
  slow)
    for p in $pids; do taskset -a -cp 14,15 "$p" >/dev/null; renice -n 19 -p "$p" >/dev/null; done
    echo "PES2021 pinned to cores 14-15, nice 19"
    if [ "${PES_PAUSE_PYTHON:-0}" = 1 ]; then
      for p in $(pgrep -u "$(id -u)" -x python) $(pgrep -u "$(id -u)" -x python3); do
        renice -n 19 -p "$p" >/dev/null 2>&1 || true; done
      echo "own python processes niced to 19"
    fi;;
  status) ps -o pid,stat,ni,pcpu,comm -p $(echo $pids | tr ' ' ,);;
  *) echo "usage: $0 stop|cont|slow|status"; exit 2;;
esac
