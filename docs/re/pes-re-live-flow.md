# PES 2021 live match-flow — ground-truth reference for GF comparison

**Purpose.** GodotFootball (`<workdrive>/GodotFootball`) is becoming a reproduction
of PES 2021. As translated PES code replaces GF's, every behaviour change must be
judged against PES itself. This doc is the **ground-truth reference of PES's match
flow, measured from the running game**, that GF's `--pes-loop` runs can be compared
with. It carries (a) the exact fields the poller reads, with the RE doc that names
each one; (b) the statistical definitions; (c) the frame-rate check against GF's
54 fps `kPesFps`; and (d) a comparison table whose GF column is filled from
GF's `--pes-loop` logs.

Tags: **CONFIRMED** (source, verified xref/asm, or live read whose checksum
verifies), **INF(conf)** (inference), **OPEN** (not determined; states what would
settle it). All PES claims cite a `docs/re` report or a live read.

**State at writing (27-10-27, ~03:30 local):** the live game (pid <pid>) is
**hard-frozen for the entire poll** (~100 min, ~700k rows): every row reads
`period 2, play 2, frame 21729, kind 7, reason 1, step 0x16, ball (0,0,-1),
score 0-0` — the frame counter never advanced once, so **no in-play frames were
captured**. `period 2` + `kind 7`/`reason 1` is the **half-time break** (end of
period 1); the process is alive at ~367% CPU but its logic loop is stuck in the
half-time substitution/UI window (step `0x16` = the substitution/UI-window +
think gate, `pes-re-restart-step.md`) and is **not producing new matches**,
contrary to the "bot churning full matches" assumption. The bot driver
(`rigbot.core.padd`, pid <pid>) is **idle at 0.0% CPU** — it is not driving the
game. **The poller is hardened and restart-proof** (see §7): it re-acquires the
pid on a game restart and tolerates a transient bad read, so the §5 PES column
fills automatically the moment a fresh full match starts. The §5 **GF column is
fully populated** from one completed reference run (`gfsim_run --minutes 10
--pes-loop --parity-log --seed 7`, `gf-LiveFlow-scratch/gf_pesloop_M10_seed7.log`).
Everything else — method, fields, definitions, frame-base math, GF mapping — is
final and does not depend on the live data.

---

## 1. The poller

`gf-LiveFlow-scratch/poller.py` — a read-only fast loop (~100 Hz, wall) that
`pread`s `/proc/<pid>/mem` (never writes, signals, or pins). One row per read,
appended to `gf-LiveFlow-scratch/liveflow.jsonl` (space-separated, 19 columns).
Match boundaries are detected in the analyzer (§2.1) from the `G` pointer and the
frame-counter reset, not in the poller.

### 1.1 Fields, and the doc that names each

| # | col | value read | meaning | doc (citation) |
|---|-----|-----------|---------|----------------|
| 0 | `t`   | `time.monotonic()` − start | wall time since poll start, s | (measurement) |
| 1 | `G`   | `*(0x1436F3DC8)` | the match object pointer; changes per match | brief anchor; `pes-re-A6Writers.md` §0 naming |
| 2 | `period` | `MI+0x1168` | period: 1 H1 play, 2 H1-break, 3 H2 play, 4 pre-ET break, 5/7 ET, 6 ET-break, 8 pre-shootout break, 9 shoot-out, 10 full time | `pes-re-A6Writers.md` §5 (period writer `FUN_1404494E0`/`1404493D0`/`1404499D0`); `pes-re-A6Period.md` §1 |
| 3 | `play` | `MI+0x116C` | play state: 1 in-play, 2 stopped (whistle/stoppage), 3 set-play ready (armed) | `pes-re-A6Period.md` §3; `pes-re-A6Writers.md` §1.2/§4 |
| 4 | `frame` | `MI+0x1198` | the PES logic-tick counter (PES "frame") | `pes-re-clock-record.md` §5; `pes-re-A6Writers.md` §8 |
| 5 | `step` | `MI+0x11B0` | restart-step byte (the sequencer's current step) | `docs/re/pes-re-restart-step.md` |
| 6 | `kind` | `MI+0x1554` | restart kind: 1 kickoff, 2 throw-in, 3 goal-kick, 4 corner, 5 free-kick, 6 penalty, 7 period-end | `docs/re/pes-re-A5Judge.md`; `pes-re-restart-step.md` |
| 7 | `reason` | `MI+0x1574` | stop reason: 1 period-end, 3 goal/kickoff, 4 ball-out, 5 foul/offside, 9 shoot-out, 10 abandon | `pes-re-A5Judge.md`; `pes-re-restart-step.md` |
| 8 | `foulkind` | `MI+0x1588` | foul sub-kind | `pes-re-A5Judge.md` |
| 9 | `stamp` | FNV-decoded ring slot `MI+0x1188+4·(idx&3)`, key `MI+0x1180`, idx `MI+0x1184` | the period-record clock stamp (f32), units 1/60 game-second | `pes-re-A6Period.md` §1 (decode `FUN_140A3C070`); `pes-re-A6Writers.md` §1–2 |
| 10 | `F`   | `MI+0x1174` (f32) | stoppage accrual: +0.5 per frame in stoppage, constant in play | `pes-re-A6Writers.md` §1 (live +0.5/frame); `pes-re-A6Period.md` §3 |
| 11 | `cs`  | FNV-1a 64 over decoded bytes vs `MI+0x1178` | 1 if the stamp checksum verifies | `pes-re-A6Period.md` §1 |
| 12 | `M`   | `E+0x58` (f32), `E = *(G+0x2c0)` | the match-time setting in minutes (10.0 in the live bot) | `pes-re-A6Writers.md` §6; `pes-re-clock-record.md` §3 |
| 13 | `s0`  | `E+0x6a` (byte) | score, team 0 | `pes-re-A6Writers.md` §5 (team tallies `0x6a/0x6b`) |
| 14 | `s1`  | `E+0x6b` (byte) | score, team 1 | same |
| 15-17 | `bx by bz` | `B+0x64/0x68/0x6C`, `B = *(G+0x40)` | ball position (x, y, z) | `pes-re-officials-ball.md`; `pes-re-A6Writers.md` §3 (`B = G+0x40`) |
| 18 | `holder` | `MA+0x2601C` (0xff = none) | ball holder id | `pes-re-holder.md`; poller chain `M0=*(0x1436F3F78); W=*(M0+0x118); MA=*(W+0x10)+8` |

Anchors: `MI = *(G+0x270)` (null outside the match step — the poller skips those
rows), `E = *(G+0x2c0)`, `B = *(G+0x40)`. The frame counter `MI+0x1198` is the
PES logic tick (Milestone A1 in GF). The stamp is the period clock minus the
stoppage accrual: the full period clock is `stamp + F` (`FUN_140A3C050`,
`pes-re-A6Period.md` §1, CONFIRMED).

> **Note on `stamp` during the current stuck state:** with the endless-match patch
> active (gate constants raised to 1e12, `pes-re-clock-live.md` §8.2), the clock
> keeps running; `stamp + F` reads 0.0 at the half-time break because the stamp is
> held in the break period (`pes-re-A6Writers.md` §1 step 5: F and the stamp do
> not move in periods 2/4/6/8). That is expected, not a decode failure (`cs`=1).

## 2. Statistics — definitions (applied by `gf-LiveFlow-scratch/analyze.py`)

### 2.1 Match boundary

A **full match** = a maximal run of rows sharing one `G`. A fresh match that
reuses the same `G` (allocator reuse) is split when the frame counter drops by
more than half the current span and by >500 frames (a kickoff restarts the frame
near 0). Rows with `period`/`play`/`frame` outside their sane ranges are the
transient `MI` mid-reallocation and are dropped (`analyze.py` `load`).

### 2.2 Per-match / per-half

- **frames** = `max(frame) − min(frame) + 1` over the distinct frames present.
  The frame counter advances in play *and* in stoppage (CONFIRMED: F rises
  0.5/frame in stoppage, `pes-re-A6Writers.md` §1), so this spans the stoppages
  too; it **excludes** the half-time break (the frame is held there, so the break
  wall-time is not in the frame count).
- **H1 frames** / **H2 frames** = the same over `period==1` / `period==3` frames.
- **in-play frames** = distinct frames whose state is `play==1`.
- **in-play share** = in-play frames / frames. (Frames, not wall: the break is
  not in the frame base. A wall-time share is reported separately where it
  differs.)

### 2.3 Stoppages, by kind

A **stoppage** = a maximal run of distinct frames with `play != 1`, bracketed by
in-play frames (the whistle is the last in-play frame before; the end is the
first in-play frame after — the restart taken). The **kind** is the most-common
`MI+0x1554` value inside the run (stable for a restart).

**Duration** = number of frames in the run (whistle→restart, exclusive on both
ends). Split where the docs allow:
- **`play==2` (whistle→taker set / stoppage+placement)**: frames with `play==2`.
- **`play==3` (set-piece hold / armed)**: frames with `play==3`.
- **execution→in-play**: the single `3→1` (or `2→1`) transition, 1 frame; it is
  the restart actually being taken, folded into the run's last frame.

Within the `play==2` window the analyzer further splits by the `step` byte
(`MI+0x11B0`, `pes-re-restart-step.md`): **`step < 0x13`** = the pure wait
before placement begins ("wait before the taker is set"), **`step ≥ 0x13`** =
placement underway (0x13 formation plan, 0x14 teleport, 0x15 finish). This
sub-window is what the brief's "wait before the taker is set / set-piece hold /
taken-to-in-play" split resolves to, using only the polled fields.

Reported per kind: **n**, and for the total, the play2/play3 split, and the
wait/placement sub-split: **median, p10, p90** (linear-interpolated
percentiles). The clean, citable splits are play2/play3 plus the step-based
wait/placement within play2.

### 2.4 Goals and clock rate

- **goals** = a change in `(s0, s1)` across consecutive distinct frames.
- **match-seconds per frame** = median of `(stamp_next − stamp_prev)/60` over
  consecutive in-play frames (the clock is frozen in stoppage, so only in-play
  deltas are used; 60 stamp units = 1 game-second, `pes-re-A6Period.md` §2).
- **frames per match-second** = its reciprocal.

## 3. Frame-base check (item 3) — the math, settled by the docs

The logic tick (PES "frame", `MI+0x1198`) is the **wall-clock** rate:
`GetGameSpeed() = 54.0` (`@0x14351ae54`, int 54 `@0x14351ae50`), CONFIRMED live
(`pes-re-A6Writers.md` §3, `rate = FUN_141E532E0() = 54`). So:

- **frames per real (wall) second ≈ 54.** This is exactly GF's `kPesFps`
  (`src/sim/onthepitch/pesoffball.hpp:82`, `kPesFps = 54.0f`;
  `Match::kFramesPerSecond = pes::kPesTickHz`, `match.hpp:404`). **CONFIRMED the
  two agree by construction** — the poller measures it to confirm the live game
  is at the install rate.

The match clock (stamp) counts **logic frames**, in units of 1/60 game-second
(60 units = 1 game-second; 162000 = 45 game-minutes, `pes-re-A6Period.md` §2).
In play the stamp advances `100·E60·E5C/E58 = 100/M` units per frame
(`pes-re-A6Writers.md` §6, CONFIRMED formula + live point 10.0 at M=10). Hence:

- **frames per match (game) second = `60 / (100/M) = 0.6·M`.** At M=10 that is
  **6 frames per game-second**, i.e. the clock runs 9 game-seconds per wall
  second (54 frames/wall-s ÷ 6 frames/game-s), matching the live HUD rate of
  9.4–9.5 displayed-s per wall-s (`pes-re-clock-live.md` §4,
  `pes-re-A6Writers.md` §6).

GF encodes the identical mapping. At `--minutes M`, each tick is `kTick_ms =
1000/54` ms of wall time, and game time per frame is
`gameSecondsPerFrame = 90/(54·M)` (`pesclock.cpp:309`) — 1/6 game-second per
frame at M=10, i.e. 6 frames per game-second. And `MatchDurationGameTimeFromRealMilliseconds`
(`matchduration.hpp:37`) advances the match clock at `90/M`× real time, so a
90-game-minute match takes `M` wall minutes — the *same* meaning as PES `E58=M`
(`pes-re-A6Writers.md` §6: `3240·M` ticks at 54 Hz = `M` wall minutes).

> **So the live bot (E58=10) and `gfsim_run --minutes 10` are directly
> comparable:** both are "a full match in 10 wall minutes," both should measure
> 54 frames/wall-s and 6 frames/game-s. The live measurement is the confirmation
> that the running game is at the install rate GF models.

## 4. GF comparison table (item 4)

GF's `gfsim_run --pes-loop --parity-log` prints, after the match, the report
block (`src/sim/onthepitch/matchreport.cpp`) then the sim lines
(`apps/gfsim_run.cpp:387-396`) and the parity lines. Each LiveFlow statistic maps
to a named GF log line:

| LiveFlow stat (this doc) | GF `--pes-loop` log line (file:line) | how it is read |
|---|---|---|
| frames per match | `[ticks] N sim ticks to full time` (`gfsim_run.cpp:394`) | `N` = total logic ticks; ÷ 2 ≈ per half (H1/H2 not split in the log) |
| frames per half | *(not printed)* — GF has no H1/H2 frame split; derive from a per-half tick observer if needed | OPEN in GF |
| in-play share | `[parity-state] ticks N live L \| ...` (`gfsim_run.cpp:79`) | `L / N`; also `[profile-split] in-play T1 (s) \| hold T2 (s)` (`:392`) |
| stoppages per kind | `[restarts] N stoppages, longest …` (`:389`) for the total; per-kind from `[parity-restart] <name> whistle= w release= r frames= d` (`:86`) | count the `[parity-restart]` lines by `<name>` (kickoff/throwin/goalkick/corner/freekick); M8 excludes the opening kickoff (`pes-parity-metrics.md`) |
| stoppage duration by kind (median, p10/p90) | `[parity-restart] … frames=d` (`:86`) | `d = release − whistle` per restart; group by name, take med/p10/p90 |
| stoppage split (play2 vs play3) | GF's `[parity-restart]` gives one `frames` (whistle→release); the play2/play3 split is not logged. The `hold` portion is `[profile-split] … hold T2` (`:392`) wall time | partial: play2+play3 total = `frames`; the 2/3 split is OPEN in GF's log |
| goals | `[gfsim] Full match complete: … a - b` (`matchreport.cpp:59`); also `[balance] … goals a - b` (`:179`) | score `a-b` |
| match-seconds per frame / frames per match-s | not printed by GF (it *assumes* 54 fps and `90/(54·M)` game-s/frame); GF's own clock is `pesclock.cpp` | GF value is modelled, not measured — the live table is the reference GF is checked against |
| frame rate (frames/wall-s) | not printed (headless sim, not wall-timed); GF's tick is `kTick_ms = 1000/54` (`match.hpp:56`) | GF assumes 54; the live 54 is the reference |
| score | `[gfsim] Full match complete` / `[balance] goals` | as above |
| score line | `[balance] shots … | on target … | goals a-b | …` (`matchreport.cpp:179-214`) | richer than the live score; for the score itself use `[gfsim]` |

GF lines that exist but have **no live-PES counterpart in this poll** (for the
reverse direction, GF→PES): `[balance]`, `[balance-passing]`, `[offball]`,
`[dodge]`, `[tackle-kind]`, `[pass-*]`, `[challenges]`, `[balance-keeper*]`,
`[determinism]`, `[idle]`. These are GF-internal; the live poll does not read
them. They are listed so a GF run is never mistaken for a full match-flow match.

**To fill the GF column for a run:**
```
./build/apps/gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 \
   --seed 7 --minutes 10 --pes-loop --parity-log --pes21-root <workdrive>/PES21
```
(`--minutes 10` = the live bot's E58=10, §3.) Extract `[ticks]`,
`[parity-state]`, `[profile-split]`, `[restarts]`, the `[parity-restart]` lines,
and `[gfsim] Full match complete`.

## 5. Measured statistics

> **PES column pending — live game hard-frozen.** The running game (pid <pid>)
> is frozen at the half-time break for the entire ~100-min poll (frame 21729 in
> all ~700k rows, `period 2, play 2, kind 7, step 0x16, ball (0,0,-1), score
> 0-0`); the frame counter never advanced, so **no in-play frames were
> captured**. The **GF column is fully populated** from one completed reference
> run: `gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7
> --minutes 10 --pes-loop --parity-log --pes21-root <workdrive>/PES21`
> (`gf-LiveFlow-scratch/gf_pesloop_M10_seed7.log`). Each PES row below is added
> per completed live match, with a pooled row over ≥3 matches; sample counts
> are the distinct-frame counts, not the ~100 Hz row count.

### 5.1 Per match

| match | G | M | frames | H1 | H2 | in-play | in-play share | wall s | fps/wall-s | fps/game-s | goals |
|-------|---|---|--------|----|----|---------|--------------|--------|-----------|-----------|-------|
| **PES live m1** | `0x1c821d60` | 10 | _(frozen 21729, 0 in-play)_ | – | – | 0 | – | 100+ (frozen) | – | – | 0-0 (at HT) |
| **GF run** (seed 7, `--minutes 10`) | – | 10 | **71168** | – | – | **48705** | **68.4%** | 1317.9 (model `71168/54`; CPU ~600 s headless) | 54 (model) | 6.0 (model) | 0-0 |

GF "wall s" is the *model* wall time (`frames/54`); GF runs headless (CPU as
fast as possible, ~600 s CPU for this run), so its CPU time is not a game-wall
figure. GF's per-half frame split is not logged (H1/H2 = –), see §7.

### 5.2 Stoppages per kind (median / p10 / p90, frames)

**PES live:** _(pending — no in-play captured; game frozen.)_

**GF run** (whistle→release, from the 21 `[parity-restart]` lines; `[restarts]`
reports 22, the 22nd being the closing period-end whistle with no release):

| kind | n | total med | p10 | p90 | play2 med | play3 med |
|------|---|-----------|-----|-----|-----------|-----------|
| freekick | 7 | 971 | 954 | 984 | (not logged) | (not logged) |
| throwin | 6 | 523 | 519 | 606 | (not logged) | (not logged) |
| goalkick | 5 | 553 | 551 | 557 | (not logged) | (not logged) |
| kickoff | 3 | 721 | 719 | 723 | (not logged) | (not logged) |
| **ALL** | **21** | **688** | **523** | **971** | – | – |

GF's per-restart `frames = release − whistle` is the *whole* stoppage; the
play2 (whistle→taker set) / play3 (armed hold) split is **not logged** per
restart in GF's log, so those columns are OPEN on the GF side. GF's total
stoppage frames `[profile-split] … hold 22463` is the aggregate of all of them.
GF longest stoppage 18.6 s (1002 ticks); install bound 32.5 s (1757 ticks).

### 5.3 Frame-base (pooled)

| measure | PES live (measured) | GF model (kPesFps=54, M=10) | agree |
|---------|--------------------|----------------------------|-------|
| frames / wall-second | _(pending — frozen)_ | 54 | – |
| frames / game-second | _(pending — frozen)_ | 6.0 | – |
| game-s / wall-second | _(pending — frozen)_ | 9.0 | – |

The GF column is the **model** (`kPesFps=54`, `gameSecondsPerFrame = 90/(54·10)
= 1/6`, `matchduration.hpp:42` at 9× real time). The live measurement is what
confirms the running game sits on these numbers; it is blocked by the freeze.

### 5.4 GF vs PES (per matched run)

| stat | PES live (measured) | GF `--pes-loop --minutes 10` (measured) | Δ |
|------|--------------------|------------------------------------------|---|
| frames / match | _(pending)_ | 71168 | – |
| in-play share | _(pending)_ | 68.4% (48705/71168) | – |
| stoppages / kind | _(pending)_ | FK 7, TI 6, GK 5, KO 3 (21 logged) | – |
| stoppage duration med | _(pending)_ | 688 f (12.7 s) | – |
| goals | 0-0 (frozen at HT) | 0-0 | – |
| fps / wall-s | _(pending)_ | 54 (model) | – |

## 6. Reproduction

```
# live poll (read-only, needs sudo for /proc/<pid>/mem; runs forever)
sudo -n python3 gf-LiveFlow-scratch/poller.py <PES2021.exe pid> gf-LiveFlow-scratch/liveflow.jsonl inf
# watch for the first live move (fires on frame advance / play==1)
sudo -n python3 gf-LiveFlow-scratch/watch.py <pid> <timeout_s>
# analyze (per-match + per-kind stats, as in §5)
python3 gf-LiveFlow-scratch/analyze.py gf-LiveFlow-scratch/liveflow.jsonl
# analyzer self-check (synthetic match with known ground truth)
python3 gf-LiveFlow-scratch/selftest.py      # -> "selftest: ALL PASS"
# analyzer print-path check (synthetic frozen prefix + in-play + stoppage + restart)
python3 gf-LiveFlow-scratch/smoke_printpath.py   # -> "PASS"

# GF reference run (the one that populated the §5 GF column)
cd <workdrive>/GodotFootball
./build/apps/gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 \
   --seed 7 --minutes 10 --pes-loop --parity-log --pes21-root <workdrive>/PES21
# -> gf-LiveFlow-scratch/gf_pesloop_M10_seed7.log  (det b3df4982249ca273)
```

## 7. Open

1. **PES in-play column empty — live game hard-frozen (blocker).** The running
   game (pid <pid>) is frozen at the half-time break for the entire ~100-min
   poll: frame 21729 in all ~700k rows, `period 2, play 2, kind 7, reason 1,
   step 0x16`, ball (0,0,-1) (reset sentinel), score 0-0, `stamp + F = 0`. The
   process is alive at ~367% CPU but its logic loop is stuck in the
   substitution/UI-window + think gate (step `0x16`,
   `pes-re-restart-step.md` §phase-table) and is **not producing new matches** —
   contrary to the "bot churning full matches" assumption (bot driver
   `rigbot.core.padd` idle at 0.0% CPU). It was already in this state when the
   poll began. It cannot be resumed from a read-only RE process. **To populate
   the PES column the owner must restart PES2021.exe (or fix/restart the bot)
   so it churns ≥3 full matches.** The poller is hardened and restart-proof
   (§7.1 below): it re-acquires the pid on a restart and tolerates a transient
   bad read, so the §5 rows fill automatically from `liveflow.jsonl` via
   `analyze.py` when in-play frames land.
2. **Poller hardening (done this session).** The first poller instance died at
   ~696k rows on a transient bad ball read (`TypeError: must be real number,
   not NoneType` — `f32(B+0x64)` returned `None` mid reallocation). Every
   None-able field read is now guarded (score bytes, ball xyz) and the poller +
   watcher re-acquire the pid when the process is restarted (the old fd dies).
   `analyze.py` `main()` also had a missing `tot` assignment in its stoppage
   print block; fixed and covered by `smoke_printpath.py` (a synthetic
   frozen-prefix + in-play + stoppage + restart file, PASS).
3. **play2/play3 split of the stoppage** is the clean citable split; the
   "wait before the taker is set" sub-window is inside play2. The analyzer now
   also splits play2 by the `step` byte (`MI+0x11B0`): `step < 0x13` = pure
   wait before placement, `step ≥ 0x13` = placement underway (0x13 formation,
   0x14 teleport, 0x15 finish; `pes-re-restart-step.md`). OPEN on both sides
   until in-play data lands; GF's log does not record the step byte.
4. **H1/H2 frame split** is measured per match here; GF's log has no per-half
   tick split, so the per-half comparison is one-sided until GF adds one.
