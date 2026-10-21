# PES-vs-GF parity metrics

The gate says a tree is healthy; this says whether it plays like PES. A fixed metric set, measured once live
in PES 2021 and on GF after every landing, recorded in one table so "closer to PES" is a trend line.
It sits beside `tools/dev/gate.sh`, never inside it: a parity move is information, not a pass/fail.

## The metric set (fixed; change it only by adding a new id)

All rates are per 20 in-play match minutes; frames are 54 Hz logic frames (GF's sim tick is the same rate).

|id|metric|unit|PES source|GF source|
|---|---|---|---|---|
|M1|MOVESTEP state histogram, per state (share of player-ticks)|% per state|+0x245C state byte, all 22 players|`pes::movestep` request state, all 22|
|M2|STOP entries by predecessor state|count per state|the state before each 8 (STOP)|same, from the GF state stream|
|M3|whistle-to-release, kickoff|frames, median (n)|restart record|`[restarts]` / sequencer|
|M4|whistle-to-release, throw-in|frames, median (n)|〃|〃|
|M5|whistle-to-release, goal kick|frames, median (n)|〃|〃|
|M6|whistle-to-release, corner|frames, median (n)|〃|〃|
|M7|whistle-to-release, free kick|frames, median (n)|〃|〃|
|M8|stoppages|count|referee state|`[restarts]` line|
|M9|longest stoppage|s|〃|〃|
|M10|passes|count|action id (anime+0xAC9) or the decision layer, as S1Watch names it|the matching GF action|
|M11|crosses|count|〃|〃|
|M12|shots|count|〃|〃|
|M13|tackles (standing)|count|〃|〃|
|M14|slides|count|〃|〃|

GF runs: `gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --minutes 20 --pes-loop --pes21-root <PES21>`
at seeds 7, 11 and 23; report the mean and the min..max over the three seeds.

Run: `GF_SIM_LOCK=<shared lock> tools/dev/parity.py --pes21-root <PES21> [--record <SHA>]`, after `build/apps/gfsim_run`
is built. Each seed runs with `--parity-log`, a read-only observer that prints the state, STOP, restart and action
lines; it writes nothing back, so the determinism hash is the same with and without it. Counts are per 20 in-play
minutes (the clock's own ticks). Both teams are summed.

## PES baseline

S1Watch round 36, live: /biz/ vs /xs/, Akihabara denki-gai, endless-match patch (`tools/dev/pes_endless_match.py`).
One match, so treat it as one sample, not a distribution.

Fill one row per metric, `|M3|142 (n=9)|notes|` (the first number is the value). M1 and M2 take one row per state, `|M1:8|4.2|notes|`.

|id|PES value|notes|
|---|---|---|
|M1-M14|pending (round 36)||

## Per-landing table

One row per main landing (the code SHA), filled by the GF parity script. Distance = |GF − PES| / PES per metric;
the summary column is the mean distance over the metrics PES has a value for.

|landing (main SHA)|date|M1 dist|M2 dist|M3-M7 dist|M8|M9|M10-M14 dist|mean dist|
|---|---|---|---|---|---|---|---|---|
