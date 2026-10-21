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
|M8|stoppages (excluding the opening kickoff, which is not a stoppage in PES's count)|count|referee state|`[restarts]` line|
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
One match, so treat it as one sample, not a distribution. Comparable now: M1, M2 (shares and per-20-min counts), M12-M14. Not yet: M3-M9 (no PES whistle signal; wall-clock gaps include replays) and M10 (action entries vs completed passes).

Fill one row per metric, `|M3|142 (n=9)|notes|` (the first number is the value). M1 and M2 take one row per state, `|M1:8|4.2|notes|`.

|id|PES value|notes|
|---|---|---|
|M1|0:8.45, 1:3.05, 2:0.84, 3:33.32, 4:5.19, 5:2.70, 6:2.31, 7:10.98, 8:0.53, 9:13.27, 10:1.15, 12:6.41, 13:0.64, 14:0.45, 15:6.06, 16:3.79, 17:0.05, 19:0.76, 22:0.05 (% of reads)|+0x245C on 22 AnimePlayer primaries, 708334 reads at 26.8 Hz|
|M2|3:207, 5:176, 4:140, 15:97, 16:84, 12:76, 7:71, 14:54, 13:41, 9:33, 19:20, 10:10, 1:9, 0:4, 6:3, 2:2 (STOP entries per 20 min, by predecessor)|consecutive samples at 26.8 Hz: transitions shorter than one sample are missed|
|M3-M7|not measured|the ball never stays beyond the lines for a detectable time; a whistle signal is needed|
|M8|50|gaps >= 2 s in the in-play state byte, wall clock: includes replays/cutscenes, so NOT yet comparable with GF's referee stoppages|
|M9|63.1 s|same caveat (wall clock incl. replays)|
|M10|916 (short 809 + long 107)|anime+0xAC9 transitions into 9/8: counts action entries, which may exceed completed passes; definition to reconcile with GF before computing a distance|
|M11|not measured|no action id for a cross|
|M12|64|action 10 entries|
|M13|31|action 13 entries|
|M14|2|actions 11 + 12 entries|

## Per-landing table

One row per main landing (the code SHA), filled by the GF parity script. Distance = |GF − PES| / PES per metric;
the summary column is the mean distance over the metrics PES has a value for.

|landing (main SHA)|date|M1 dist|M2 dist|M3-M7 dist|M8 dist|M9 dist|M10-M14 dist|mean dist|
|---|---|---|---|---|---|---|---|---|
|860e2f1|2026-10-21|pending|pending|pending|pending|pending|pending|pending|

## Notes
- M3 includes the period-start kickoffs (the referee sets the phase at the period-end whistle); seeds ending 0-0 have only those.
- Open: on 860e2f1, seed 23 shows two period starts where seeds 7 and 11 show three (its third produced no measured stoppage); untraced.
- `--minutes 20` runs about 28-29 in-play minutes (the match-length factor is clamp(minutes, 5, 90)/90 of the clock); counts are normalised per 20 in-play minutes.
