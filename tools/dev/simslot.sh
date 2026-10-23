#!/usr/bin/env bash
# Runs a command holding one of the shared sim slots, so at most GF_SIM_SLOTS sims run machine-wide (owner ruling 22-10-26:
# 2 slots). Slot 0 is the original lock file, so a plain `flock gf-sim.lock` still counts as slot 0. The gate holds every
# slot (tools/dev/gate.sh). Usage: tools/dev/simslot.sh <command> [args...]
set -euo pipefail
repo=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
base=${GF_SIM_LOCK:-$(dirname "$repo")/gf-sim.lock}  # worktrees and the main checkout share a parent
slots=${GF_SIM_SLOTS:-2}
poll_s=1  # how often a waiting run retries the slots
lockfile() { if [ "$1" -eq 0 ]; then echo "$base"; else echo "$base.$1"; fi; }
while :; do
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
