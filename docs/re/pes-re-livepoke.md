# LivePoke: live reads of the press hook, the stop-leaf records, and Chance run lifetimes

Method: read-only `/proc/<pid>/mem` through `sudo` (no writes, no input). Scripts and raw
samples are in `the RE scratch dir/LivePoke/` (`common.py`, `probe.py`, `sample.py`,
`watch.py`, `table.py`; `out/samples30.jsonl` 267 samples; `out/chance_runs.jsonl` 40 completed runs).
Match live at 10:07-10:25 (owner's match). The match context G = `*(u64*)0x1436F3DC8` read
`0x2c31e230` throughout. `G+0x270` (MI) and `G+0x18` (MA) read 0 for ~85% of reads and are valid
otherwise: every reader must retry (MI+0x1198 frame counter advances normally when valid).

## 1. Press hook `_DAT_1425301f0` (item 1): resolved, decoded, one table location still open

- Live pointer: `*(u64*)0x1425301f0 = 0x6ffffec86620` (constant over ~2 min of reads).
- Mapping: inside a Wine module, `ucrtbase.dll` loaded at `0x6ffffec60000` (the `MZ` header page
  is file-backed at `0x6ffffec60000`; its code `0x6ffffec61000-0x6ffffed0c000` is anonymous).
  Target RVA `0x26620` = `ucrtbase.dll` export `sqrtf` (export table: `sqrtf` at RVA 0x26620, ord 0x947).
- Static check: the first 48 live bytes at the target equal the on-disk `ucrtbase.dll` bytes at
  RVA 0x26620; disassembly is `movss [rsp+8],xmm0; ... sqrtss xmm1,[rsp+0x60]`, with a NaN/Inf tail
  jump to `0x1800258b0` and a domain-error path (`call 0x180025754`, code 0x21 = EDOM) for negative
  input. Confirmed: `_DAT_1425301f0` is the exe's `api-ms-win-crt-math` IAT slot for `sqrtf` (exe
  import table: `sqrtf` hint 0x29 in `api-ms-win-crt-math-l1-1-0.dll`).
- So `FUN_14044e0d0(team, man)` = `sqrtf(float at MA+0x24710 + (MA+0x247be[man] + (man>=11 ? 11 : 0))*8)`.
  `FUN_140639830` sets `flagA` when that distance `< 4.0` (`0x1425656c0`). The table holds squared
  distances (per the defensive and receiving reports, `MA+0x24710` is a per-side sorted `{float d^2, id}`
  list), so the flag means "within 4 m".
- Live verification of the table values: NOT done. `G+0x18` is a stable pointer (0xa3e6a44) whose
  contents read zero (ball, rank bytes and table all 0) while the game plays, so it is not the
  analysis object FUN_140639830 reads. The ball is at `*(G+0x40)+0x88` (x, y, z), moving (checked).
  A pointer scan from G (+0..0x3000, depth 2 via +0x18) found no 22-byte rank pattern.
- Code change (commit `c6afcb5`, branch `LivePoke`): `PressFillMan::hookValue` renamed to
  `tableDistSq` (squared metres); `PressCandidateFill` now tests `sqrt(tableDistSq) < 4.0`. New test
  `FlagAIsDistanceNotSquaredDistance` (15.9 -> 3.987 m passes, 16.1 -> 4.012 m fails). Mutation
  (drop the sqrt) fails `CountsPassersAndSetsFlags` and the new test. `pes_press_test` 40/40 with
  `PES21_ROOT`. Not run: the full gate (only that target was built), and no smoke.
- Writer `FUN_140536fc0` itself is still not ported; its table input is still not wired.

## 2. Stop-leaf records (item 2): fields identified; GF has no equivalent state

Accessors (decompiled, dereferenced; `*(G+(k+N)*0x28)`):
- `PI(k) = *(G+(k+0xf3)*0x28)` (`FUN_140a489f0`, k<0x16): the leaves' record `A`.
- `PM(k) = *(G+(k+0xbf)*0x28)` (`FUN_140a48a10`): move object, pos `+0x55c` x / `+0x564` z.
- `PA(k) = *(G+(k+0xa5)*0x28)` (`FUN_140a48910`): action object, `+1` action byte, `+8` flags,
  `+0x11c` float facing, `+0x1a`/`+0x1c` u16 clip counters.
- `T(s) = *(G+(s+0xa1)*0x28)` (team): `+0xa2a0[slot]` = team action state (`T.st` below).

Live counts: 267 snapshot samples over 30 s (all 22 players), 31 Chance-run stops (`watch.py`).

| Field | Read by | Live evidence | GF counterpart |
|---|---|---|---|
| `PI+0x14` (u32) | 5a7be0 `==8`; 5a7fe0 tail | 0 at all 31 stops; in samples, `(14,1c,20,48)=(9,id,22|24,0)` 7 times | none: GF `PlayerFacts.state` is the team action state, not a per-player pass aim |
| `PI+0x1c` (u32) | 5a7be0 / 5a7fe0 `==id` | 255 at all stops | none |
| `PI+0x1490` (u32, receiver id) | 5a7fe0 (`1494==3`), 5a7d10 | 255 at all stops; in samples 4/9/10/18/19/20 occur | partial: `RunWorld.receiver` is per team (FUN_1405969b0), `BallEvent.target` is the last kick only |
| `PI+0x1494` (u32 state) | 5a7fe0 (`{2,3}`), 5a7be0 (`==2`) | 0 (26), 3 (5) at stops; samples 0/1/3/4 | none |
| `PI+0x20` (u32) | 5a7fe0 `==0x18` | 0 at stops; 22/24 only with an active aim (above) | none |
| `PI+0x48` (u32) | 5a7fe0 keeper case `==0x18` | 0 for all 31 stops and all samples (incl. GK k=0, 11) | `goalie` (role GK) is not this flag; the trigger value never appeared |
| `PI+0x45` (byte) | 5a7fe0 `{2,3}` | 0/1/3 | none |
| `PI+0x38` (float, deg) | 5a7fe0 heading vs `MA+0x247d4` | 0..360 | partial: `PlayerFacts.aim` is a point, not a bearing |
| `PI+0x13ac`, `+0x13c4` | 5a7fe0 `==0x18` / `==id` | 13ac 0 except k=10/20 (3, 1); 13c4 255 | none |
| `PI+0x14a8` (frame stamp) | 5a7fe0 timing vs `MI+0x1198` | 0 at all stops | partial: `PlayerFacts.t` (state age), not a stamp |
| `PA+1` (action byte) | 5a7fe0/5a7be0 mask, 5a7d10 mask `{9,0x23,0x29}` | samples 2, 3, 74, 79, 9, ...; at the 40 stops: 2 (22), 3 (7), 74 (2), none (9) | none: NOT the team action state (agreement with `T.st` 0 of 5553 pairs) |
| `PA+0x11c` (float) | 5a7be0 / 5a7fe0 facing | 0..360 | `PlayerFacts.facing` is `+0x554` (move object) |
| `PA+0x1a`/`+0x1c` (u16) | 5a7fe0 timing | `+1a < +1c` never true during Chance | none (clip counters) |

Conclusions:
- While a runner is in Chance (`T.st == 0x1e`), his `PI` pass record is idle (`+0x14 = 0`,
  `+0x1c = 255`, `+0x1490 = 255`). So the runner-side keeps `5a7fe0`/`5a7be0` return 0 for runners,
  live. Their remaining inputs are not in GF, so their port would invent state; not ported.
- `5a7d10` (the carrier's pass aim) reads the carrier's `PI`/`PA`. The carrier object is `MA+0x2601c`,
  and MA was not located (section 1), so 5a7d10 was not sampled. Not ported.
- Missing per GF to port the three leaves faithfully: a per-player pass-aim record (action id,
  target id, receive state, route bearing, stamp) and the move-action byte `PA+1`. GF has the
  pass event (`BallEvent.target`) but none of the other fields.

## 3. Chance run lifetime (item 3): measured, and it contradicts the 39 s inference

`watch.py`: state `T.st[slot] == 0x1e` (Chance) start and end, timed by `MI+0x1198`. Window 597 s,
one match, both teams.
- Completed runs: 40 (one still open at the end). Starts: team 0 = 13, team 1 = 28.
- Duration (frames; 54 Hz `=` 1.58 s median): min 8, q1 39 (0.72 s), median 85.5 (1.58 s), q3 152
  (2.81 s), max 372 (6.89 s), mean 98.5 (1.82 s).
- Per 20 min extrapolated: 82 starts for both teams (team 0 26, team 1 56). Doc §10 used 93 per team.
  The windows differ (10 min mid-match vs a full match), and the doc counts admissions by another
  path, so this comparison is inconclusive.
- Concurrency: at most 2 Chance slots per team at once (cap 3); the cap is not saturated.
- So the doc's 39 s (`3 * 1200 / 93`) assumes all three slots are always occupied. Live data does
  not support that. PES's run occupancy is low, and a run lasts about 1.6 s. GF's 8-9 s runs are
  about 5 times longer than this measurement. This points at the release side, not the admission side.
- Ends: 29 of 40 end at state 0, the rest move to other action states (54, 38, 42, 58, 61, 1). A
  run can be reassigned rather than released.
- Stop-instant leaf inputs are in the table in section 2.

## Not done / next

1. Locate the analysis object (MA) that FUN_140639830 reads: derive it from the team brain
   (`brain+0x6d48 = ctx`, `ctx+0x18 = MA`) or from `FUN_140a627e0` (ball holder at `MA+0x2601c`). Then
   check `MA+0x24710` d^2 against the live ball and player positions. Until then the table values are
   doc-based.
2. Measure the carrier's `PI`/`PA` at 5a7d10 (sample the ball holder during a Chance run).
3. Re-run `watch.py` over a full match (20 min) and reconcile run counts with the doc's 93 per team
   (confirm whether PES counts admissions or state changes).
4. Wire the press writer (`FUN_140536fc0`) only after step 1.
