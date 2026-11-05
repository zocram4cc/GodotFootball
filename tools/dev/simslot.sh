#!/usr/bin/env bash
# Runs a command holding sim slots, so at most GF_SIM_SLOTS sims run machine-wide (owner, 27-10-26: 4 slots, raised from 2
# so agents keep working while a gate runs; 05-11-26: 6, after FlatKeys cut a match from 3.7 to 2.3 GiB VmRSS).
#   Usage: tools/dev/simslot.sh <command> [args...]
#   GF_SIM_PRIORITY (default 5): queue priority, lower runs first. Gates use 0 (gate.sh), so they jump ahead of agents.
#   GF_SIM_HOLD (default 1): slots the command holds (gate.sh holds 2, one per smoke).
#   GF_SIM_PIN (default: any slot): the slot numbers to hold from, e.g. "3 2".
#   GF_SIM_SLOTS (default 6), GF_SIM_MIN_AVAILABLE_MIB (default 5800), GF_SIM_LOCK: as before. The command sees the slots it
#   holds in GF_SIM_HELD.
# A slot is an flock on the lock file (slot 0) or on $lock.N (slot N), the files old scripts lock directly, so a plain
# `flock gf-sim.lock` still counts as slot 0. Waiters queue in $lock.queue: one ticket per waiter, named
# <priority>-<arrival ns>-<pid>. Only the lowest live ticket probes the slots, so the order is (priority, arrival) and a
# later ticket never takes a slot ahead of an earlier one. A waiter that is not the head holds no slot, so a head that
# holds part of its set can always be overtaken without deadlock. A slot is taken only while MemAvailable leaves room for
# one more sim (binaries built before FlatKeys still hold 3.7 GiB). Slots are released when the command exits. Old scripts
# that poll the flock directly bypass the queue and can still win a free slot. ponytail: the head re-checks every
# GF_SIM_POLL_S (0.5 s) rather than waking on the release; a kernel wait would need a flock per ticket, not worth it yet.
set -euo pipefail
export LC_ALL=C  # ticket names sort in byte order, which is what the queue needs
repo=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
base=${GF_SIM_LOCK:-$(dirname "$repo")/gf-sim.lock}  # worktrees and the main checkout share a parent
queue=$base.queue
slots=${GF_SIM_SLOTS:-6}
priority=${GF_SIM_PRIORITY:-5}
hold=${GF_SIM_HOLD:-1}
poll_s=${GF_SIM_POLL_S:-0.5}
# One pre-FlatKeys sim (3.7 GiB VmRSS, docs/ports/pes-merge-BF.md) plus 2 GiB for builds and the rig.
SIM_MIN_AVAILABLE_MIB=${GF_SIM_MIN_AVAILABLE_MIB:-5800}
for n in "$slots" "$priority" "$hold"; do
  [[ $n =~ ^[0-9]+$ ]] || { echo "simslot: GF_SIM_SLOTS, GF_SIM_PRIORITY and GF_SIM_HOLD must be integers" >&2; exit 2; }
done
candidates=()
if [ -n "${GF_SIM_PIN:-}" ]; then read -ra candidates <<< "$GF_SIM_PIN"
else for ((i = 0; i < slots; i++)); do candidates+=("$i"); done; fi
if [ "$hold" -lt 1 ] || [ "$hold" -gt "${#candidates[@]}" ]; then
  echo "simslot: cannot hold $hold of ${#candidates[@]} candidate slots" >&2; exit 2
fi

lockfile() { if [ "$1" -eq 0 ]; then echo "$base"; else echo "$base.$1"; fi; }
mem_available_mib() { awk '/^MemAvailable:/ {print int($2 / 1024)}' /proc/meminfo; }

mkdir -p "$queue"
ticket=$queue/$(printf '%03d-%020d-%07d' "$priority" "$(date +%s%N)" "$$")
: > "$ticket"
trap 'rm -f -- "$ticket"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP

# The lowest ticket whose waiter is alive; tickets of dead waiters are swept.
head_ticket() {
  local t
  for t in "$queue"/*; do
    [ -e "$t" ] || continue
    if kill -0 "${t##*-}" 2>/dev/null; then echo "$t"; return 0; fi
    rm -f -- "$t"
  done
  return 1
}

held=()
fds=()
probe() {  # takes the first free candidate slot this command does not hold yet
  local slot fd h skip
  for slot in "${candidates[@]}"; do
    skip=0
    for h in "${held[@]+"${held[@]}"}"; do [ "$h" = "$slot" ] && skip=1; done
    [ "$skip" = 0 ] || continue
    exec {fd}>"$(lockfile "$slot")"
    if flock -n "$fd"; then held+=("$slot"); fds+=("$fd"); return 0; fi
    exec {fd}>&-
  done
  return 1
}
release() {  # gives back a partial hold
  local fd
  for fd in "${fds[@]+"${fds[@]}"}"; do eval "exec ${fd}>&-"; done
  held=(); fds=()
}

while :; do
  if [ "$(head_ticket || true)" = "$ticket" ] && [ "$(mem_available_mib)" -ge "$SIM_MIN_AVAILABLE_MIB" ]; then
    while [ "${#held[@]}" -lt "$hold" ] && probe; do :; done
    [ "${#held[@]}" -eq "$hold" ] && break
  else
    release
  fi
  sleep "$poll_s"
done
rm -f -- "$ticket"
export GF_SIM_HELD="${held[*]}"
rc=0
"$@" || rc=$?
exit "$rc"
