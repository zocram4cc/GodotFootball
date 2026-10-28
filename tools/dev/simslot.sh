#!/usr/bin/env bash
# Runs a command holding one of the shared sim slots, so at most GF_SIM_SLOTS sims run machine-wide (owner, 27-10-26: 4
# slots, raised from 2 so agents keep working while a gate runs). Slot 0 is the original lock file, so a plain
# `flock gf-sim.lock` still counts as slot 0. The gate holds slots 0 and 1 (tools/dev/gate.sh); slots 2 and 3 stay free
# for agents. Usage: tools/dev/simslot.sh <command> [args...]
set -euo pipefail
repo=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
base=${GF_SIM_LOCK:-$(dirname "$repo")/gf-sim.lock}  # worktrees and the main checkout share a parent
slots=${GF_SIM_SLOTS:-4}
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
