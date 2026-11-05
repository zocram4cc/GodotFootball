#!/usr/bin/env bash
# Runs a command holding one of the shared sim slots, so at most GF_SIM_SLOTS sims run machine-wide (owner, 27-10-26: 4
# slots, raised from 2 so agents keep working while a gate runs; 05-11-26: 6, after FlatKeys cut a match from 3.7 to 2.3 GiB
# VmRSS). Slot 0 is the original lock file, so a plain `flock gf-sim.lock` still counts as slot 0. Gates hold a pair
# (tools/dev/gate.sh). A slot is taken only while MemAvailable leaves room for one more sim: binaries built before FlatKeys
# still hold 3.7 GiB, and 40 unbounded sims once OOM'd the machine. Usage: tools/dev/simslot.sh <command> [args...]
set -euo pipefail
repo=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
base=${GF_SIM_LOCK:-$(dirname "$repo")/gf-sim.lock}  # worktrees and the main checkout share a parent
slots=${GF_SIM_SLOTS:-6}
# One pre-FlatKeys sim (3.7 GiB VmRSS, docs/ports/pes-merge-BF.md) plus 2 GiB for builds and the rig.
SIM_MIN_AVAILABLE_MIB=${GF_SIM_MIN_AVAILABLE_MIB:-5800}
poll_s=1  # how often a waiting run retries the slots
lockfile() { if [ "$1" -eq 0 ]; then echo "$base"; else echo "$base.$1"; fi; }
mem_available_mib() { awk '/^MemAvailable:/ {print int($2 / 1024)}' /proc/meminfo; }
while :; do
  if [ "$(mem_available_mib)" -lt "$SIM_MIN_AVAILABLE_MIB" ]; then sleep "$poll_s"; continue; fi
  for ((i = 0; i < slots; i++)); do
    exec {fd}>"$(lockfile "$i")"
    if flock -n "$fd"; then
      "$@"
      exit $?
    fi
    exec {fd}>&-
  done
  sleep "$poll_s"
done
