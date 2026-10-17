# A3States: MOVESTEP on the PES path, the restart freezes (update)

Branch `A3States` on origin, head `5477448` (rebased onto origin/main `81bc0b1`).

## Ported this round (tests red first, one mutation each)

| Commit | Change | Red | Mutation |
|---|---|---|---|
| `c2f1b9e` | AI command fill (req[1]/[3]/[6]/[7]/[0x11]/[0x12]/buckets), foot band, AnimeState, GOTO rows | STOP and ACCELERATE not reached (`TheFsmReachesStopAndAccelerate`) | look distance zeroed: ACCELERATE lost |
| `41955fd` | foot flag `req+0x26` promoted to shared contract (`kFootFlag`), GOTO foot writes | foot flag unit assertion | foot bit flipped: caught |
| `c917dfb` | style table is 64 words, 16 styles x 4 members (`DAT_1425ddcf0`) | `RunTablesInstall` style 4 band 2 | 16-word load: caught |
| `53aeb9b` / `9d57ba2` | EnterTurnStop playback = `TurnPlayback(G, p)` (vt+0x50), not a km/h band speed | standing taker playback 0 | playback zeroed: caught |
| `88792b9` / `df8fac9` | EnterAccelerate / EnterDecelerate issue `req[0x35]` (vt+0x50) with the start ladder, in FUN_1406ccd00 / FUN_1406cdaf0 order | `IssuesThePlaybackWordWithTheStartLadder` (both) | reads `req[0x4b]` again: caught; start ladder wrong: caught |
| `5477448` | one PES bearing `PesBearingDeg` (atan2(x, -z)) for every heading the adapter gives PES, incl. FillCommandWords | `ABearingToAGroundPointIsTakenOnPesZ` (0 vs 180) | sign dropped: caught |

## Root causes found (with evidence)

1. **Kick-off freeze (fixed in `df8fac9`).** EnterAccelerate issued `req[0x4b]` as playback. The adapter never fills `req[0x4b]`, so it read 0. `PlayIssuedClip(motion, 0)` froze the clip at frame 0 forever. Trace: taker in ACCELERATE, mot 2243, frame 0, `rate 0.000 phase 0.000`. Decompile (`FUN_1406ccd00:138-140, 417-418`): PES writes `req[0x35] = vt+0x50(vt+0x30(...))` and calls `vt+0x10(req, motion, mirror, 0, 0.1667, req[0x35], startLadder)`. The 13.5/8/11 rungs are ground-speed thresholds for the mirror flip, not playback or start time. The same playback/order fault is in EnterDecelerate (`FUN_1406cdaf0:69, 337-339`).
2. **Mirrored bearing in FillCommandWords (fixed in `5477448`).** The z-sign fix (`254edf6`) covered `HeadingDeg` only. The fill's own `atan2(dx, dz)` and `atan2(lx, lz)` were unsigned, so a live command walked the player away from the target. Trace: taker at 80 to 98 m from the ball, `mot 4152` advancing, ACCELERATE, while his command pointed at the restart spot. Red: `PesBearingDeg(0, 10)` gave 0; PES gives 180.

## Still open (not fixed)

- **2-min `--pes-loop` smoke does not complete at `5477448`.** It stalls at a restart (piece 1, taker A11, dead ball at (-7.8,-9.6) in play, stalemate forcing). That is a different restart than before the bearing fix. Not diagnosed.
- **Throw-in (A5Tail's case).** Their trace and mine agree on the taker standing at the ball with `touch -1` for about 3 s. Mine then reaches `touch 39` with `func 4` (ShortPass), so the throw is taken. The ball then sits dead inside play with no retainer; the stalemate counter climbs. The stall after that is a post-throw dead-ball problem, in A5Tail's area (picker/retainer), not in MOVESTEP.
- **20-min `--pes-loop` smoke: not run.**
- **Off-path hash.** `a3d5e43a3b7b03af` at `df8fac9` (run after the EnterAccelerate/Decelerate fix). The bearing commit is inside `ProcessMoveStep`, which returns early off-path, so it should be unchanged. Not re-run at `5477448`.

## Ponytails left in code

- `StoreFootBand` takes the foot from the turn's sign (PES vt+0x78 is undecoded).
- `req[0x4b]` (P+0x3750) is still never filled by the adapter. The EnterAccelerate/Decelerate issues no longer read it, but RunLoop arm 5 still does. Undecoded writer; PES default 1.0.
- The TurnPlayback `p` is answered through `MirrorChoice` (ponytail).
