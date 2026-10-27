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

**State at writing (27-10-27, ~09:45 local):** **one complete live match is
captured end-to-end (1 635 071 rows, G `0x1c821d60`, frames 21729–48708): H2
fully observed, the HT break's tail and the FT whistle both in the log, final
score 0-0.** The poller started on pid <pid> and re-acquired across 4 game
restarts (53052 → 2777338 → 2780589 → 2789823 → 2802803, `poller.err`); the
complete match is the G `0x1c821d60` segment on pid <pid>. H1 was not
observed (the poller attached at the HT whistle, frame 21729; H1 = 21729
frames inferred, §5.1) — so this report carries **1 of the 3 required full
matches**: every table is populated, every number has a sample count, and
the §5 rows repeat verbatim per future match. The poller is hardened and
restart-proof (§6/§7): it keeps appending to `liveflow.jsonl`, so matches
2–3 land in the same file and the same commands below produce their rows
with no new tooling.

Sample-count convention: frame statistics use **distinct observed frames**
(1 635 071 rows collapse to 26 424 distinct frames here); distributions use the
named n. Every statistic names its n.

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

A **full match** = a maximal run of rows sharing one `G` (the match object
pointer). A fresh match that reuses the same `G` (allocator reuse) is split
when the frame counter drops by more than half the current span and by >500
frames (a kickoff restarts the frame near 0). Rows with `period`/`play`/`frame`
outside their sane ranges are the transient `MI` mid-reallocation and are
dropped. **This match (G `0x1c821d60`) was selected by widest frame span.**

- **Frames that stay inside one logic tick are a poll re-read, never a second
  tick.** The poller polls several ~100 Hz offsets per logic tick (median
  first-poll-of-frame delta 17.541 ms, i.e. each 17.5 ms tick is sampled
  ~1.75×), so one frame yields 4–12 near-identical rows (e.g. frame 27000:
  12 polls, all `play 3, step 0x16`; first-polled frame 25714 mixes valid 22
  with the top-byte-`0x10` spill `16843030` = 22-in-pass-through). **A
  distinct frame is the atomic sample** — all counts below use
  last-poll-of-frame after step-rule propagation (§2.3). CONFIRMED live
  (§5.3: 17344 distinct in-play frames, n=17331 consecutive gaps, median
  17.541 ms).
- **One-sample non-in-play frames are torn reads, not events**
  (`[INFERENCE]`, high confidence, mechanism-verified). When the game's tick
  boundary lands inside one poll pass, in-place fields tear against each
  other: `367M+0x19`-class step values (`16842774`=0x1010016,
  `16843030`=0x1010116 — step 22/278 with high byte `0x10`,
  `pes-re-restart-step.md` "in pass-through" convention) occur **only** in
  single-poll-stopped rows (315 of them; 0 in multi-poll stopped frames).
  `flowstats_match.py` propagates the previous valid step across them; after
  that H2 shows 11 non-in-play runs. **The 20 remaining single-sample
  `play!=1` frames inside H2's runs are the same poll-phase class and are
  excluded from the nominal 9-stoppage pass** (see §5.2); no 1-frame play-1
  islands and no single-sample whistle-adjacent frames survive in the
  nominal segmentation.
- **Missed whole ticks are the only loss.** Before consolidation the raw pass
  shows 455 gaps totalling 556 frames (max 5 in H2) — the loop skipping whole
  17.5 ms ticks under `sudo pread` load. Gaps are reported per stoppage
  (`[+G]` flags §5.2); **none of the 9 nominal stoppages crosses a gap
  interior** (whistle..release fully observed), so their durations are
  **exact frame counts**, not estimates.

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

A **stoppage** = a maximal run of consecutive observed non-in-play frames
(play 2/3) bracketed by in-play frames (play 1). The **whistle** is the last
in-play frame before the run; the **release** is the first in-play frame after
it. **Duration = release − whistle − 1** (the whistle→restart-taken interval in
logic frames; both ends exclusive). The nominal per-stoppage pass consolidates
multi-poll frames to last-poll-of-frame after propagating the valid step
across the single torn class (§2.1); torn-class frames inside a stoppage are
resolved, not dropped — the pass counts stops by kind over the fused runs and
uses 9 separate whistle/release pairs (all fully observed).

The **kind** is the mid-hold (step-`0x16`) mode of `MI+0x1554` (identical at
every hold poll for all 9 H2 stoppages); the **reason** is likewise the mode
of `MI+0x1574`. Split where the docs allow:
- **`play==2` (stoppage / whistle→armed)**: frames with `play==2`.
- **`play==3` (set-piece hold / armed)**: frames with `play==3`.
- **taken→in-play**: the `3→1`/`2→1` release transition, 1 frame, folded into
  the run's last frame (release is the first `play==1` frame).

Within `play==2` the analyzer further splits by the `step` byte (`MI+0x11B0`,
`pes-re-restart-step.md`): **`step < 0x13` = wait before placement**, and
placement is the rest (`0x16`-class) — (**`[INFERENCE]`**, medium, from this
match's joint step×play table: in the stoppage holds, `0x16` is play-2 in
86–95% of samples and never play-1 while
`0x10100xx` is play-3-only; `0x13/0x14/0x18–0x1e` never appear in 9080 hold
frames, so the restart-step doc's 0x13–0x15 ladder is **not confirmed as the
operative placement code on this path**). Reported per kind: **n**, and for
the total, the play2/play3 split, and the wait/placement sub-split: **median,
p10, p90** (linear-interpolated percentiles). The operative, confirmed
stoppage split in this match is play2/play3 plus the small step-`<0x13` wait
prefix (a few ×10 frames per stoppage).

### 2.4 Goals and clock rate

- **goals** = a change in the score field across consecutive distinct fused
  frames, where the score field reads clean at **every** fused frame of this
  match (0-0 throughout, ±0 changes over 26 424 frames — CONFIRMED). The
  earlier poll-level 118-change count on `E+0x6a/0x6b` is withdrawn: those
  bytes tear under poll load and are not the goal signal here (§5.5).

> **PES column: one complete live match (H2 fully observed, §5).** The old
> claim that the game was frozen at HT is withdrawn — the frame counter ran
> H2 to full time 1.5 h into the poll (the "freeze" was the 05:34
> Rendezvous-driven SIGSTOP pauses plus the HT break).

The live in-play logic tick runs at **57.01 frames/wall-s** (median
first-poll-of-frame delta 17.541 ms, n=17331 consecutive in-play gaps, p10
17.0/p90 19.1 ms), not the install rate 54: GF's `kPesFps = 54.0f`
(`pesoffball.hpp:82`) and `Match::kFramesPerSecond` (`match.hpp:404`) model a
tick **5.4% slower than the running game**. **CONFIRMED live, n=17331.**
Wall-clock cause is OPEN (docs note 54 as the install rate;
`pes-re-A6Writers.md` §3); a 56.89–57.01 Hz engine tick with the live bot's
settings is what the poller sees. GF-side consequence (§5.4): every GF
frame-count prediction of a wall-time interval is ~5.4% high relative to this
live match.

The match clock (stamp) counts **logic frames**, in units of 1/60 game-second
(60 units = 1 game-second; 162000 = 45 game-minutes, `pes-re-A6Period.md`
§2). In play the stamp advances `100·E60·E5C/E58 = 100/M` units per frame
(`pes-re-A6Writers.md` §6, CONFIRMED formula + live point 10.0 at M=10),
directly measured at **10.00000 units/in-play-frame** (median, n=17332).
Hence:

- **frames per match (game) second = `60 / (100/M) = 0.6·M`.** At M=10 that
  is **6.00 frames per game-second** (median 0.166667 game-s/frame, n=17332),
  i.e. the clock runs 9.5 game-seconds per wall second (57.01
  frames/wall-s ÷ 6.00 frames/game-s), matching the live HUD rate of 9.4–9.5
  displayed-s per wall-s (`pes-re-clock-live.md` §4, `pes-re-A6Writers.md` §6).

GF encodes the identical mapping with its 54 Hz assumption: at `--minutes M`,
each tick is `kTick_ms = 1000/54` ms of wall time, and game time per frame is
`gameSecondsPerFrame = 90/(54·M)` (`pesclock.cpp:309`) — 1/6 game-second per
frame at M=10, i.e. **6.00 frames per game-second**. And
`MatchDurationGameTimeFromRealMilliseconds` (`matchduration.hpp:37`) advances
the match clock at `90/M`× real time, so a 90-game-minute match takes `M`
wall minutes — the *same* meaning as PES `E58=M` (`pes-re-A6Writers.md` §6:
`3240·M` ticks at 54 Hz = `M` wall minutes).

> **So the live bot (E58=10) and `gfsim_run --minutes 10` are directly
> comparable:** both are "a full match in 10 wall minutes," both count 6
> frames/game-s. The live measurement confirms the 6.00 exactly; the tick
> itself runs 5.4% fast (57.01 vs 54).

| measure | PES live (measured, H2 n in §5) | GF model (kPesFps=54, M=10) | agree |
|---------|---------------------------------|-----------------------------|-------|
| frames / wall-second | **57.01** (med gap 17.541 ms, n=17331) | 54 | **no: PES +5.4%** |
| frames / game-second | **6.00** (0.166667 game-s/frame, n=17332) | 6.0 | yes |
| game-s / wall-second | **9.50** (57.01/6.00) | 9.0 | **no: PES +5.4%** |

## 3. Frame-base check (item 3) — locked by measurement

## 4. GF comparison table (item 4)

GF's `gfsim_run --pes-loop --parity-log` prints, after the match, the report
block (`src/sim/onthepitch/matchreport.cpp`) then the sim lines
(`apps/gfsim_run.cpp:387-396`, emitters below) and the parity lines. Each
LiveFlow statistic maps to a named GF log line:

| LiveFlow stat (this doc) | GF `--pes-loop` log line (file:line) | how it is read |
|---|---|---|
| frames per match | `[ticks] N sim ticks to full time` (`apps/gfsim_run.cpp:394`) | `N` = total logic ticks; ÷ 2 ≈ per half (H1/H2 not split in the log) |
| frames per half | *(not printed)* — GF has no H1/H2 frame split; derive from a per-half tick observer if needed | OPEN in GF |
| in-play share | `[parity-state] ticks N live L \| ...` (`apps/gfsim_run.cpp:79`) | `L / N`; also `[profile-split] in-play T1 (s) \| hold T2 (s)` (`:392`) |
| stoppages per kind | `[restarts] N stoppages, longest …` (`:389`) for the total; per-kind from `[parity-restart] <name> whistle= w release= r frames= d` (`:86`) | count the `[parity-restart]` lines by `<name>` (kickoff/throwin/goalkick/corner/freekick); M8 excludes the opening kickoff (`tools/dev/parity.py:87`) |
| stoppage duration by kind (median, p10/p90) | `[parity-restart] … frames=d` (`:86`) | `d = release − whistle` per restart; group by name, take med/p10/p90 |
| stoppage split (play2 vs play3) | GF's `[parity-restart]` gives one `frames` (whistle→release); the play2/play3 split is not logged. The `hold` portion is `[profile-split] … hold T2` (`:392`) wall time | partial: play2+play3 total = `frames`; the 2/3 split is OPEN in GF's log |
| goals | `[gfsim] Full match complete: … a - b` (`matchreport.cpp:59`); also `[balance] … goals a - b` (same file `:179`) | score `a-b` |
| match-seconds per frame / frames per match-s | not printed by GF (it *assumes* 54 fps and `90/(54·M)` game-s/frame); GF's own clock is `pesclock.cpp` | GF value is modelled, not measured — the live table is the reference GF is checked against |
| frame rate (frames/wall-s) | not printed (headless sim, not wall-timed); GF's tick is `kTick_ms = 1000/54` (`match.hpp:56,392-395`) — 54 is the model, the live 57.01 the reference | GF assumes 54 |
| score | `[gfsim] Full match complete` / `[balance] goals` | as above |
| score line | `[balance] shots … | on target … | goals a-b | …` (`matchreport.cpp:175-214`) | richer than the live score; for the score itself use `[gfsim]` |

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

> **PES column: one complete live match.** G `0x1c821d60` on pid <pid>,
> M=10.0 throughout, first observed frame 21729 (HT whistle — H1
> unobserved), last frame 48708 (FT whistle), period path
> 2 → 3 → 10 (HT break → H2 play → full time), no ET (`period 4/5/7` never
> appears), final score 0-0. Sample counts are distinct-frame counts (§2.1).

## 5. Measured statistics

### 5.1 Per match

| match | G (pid) | M | frames | H1 | H2 | in-play (H2) | in-play share (H2) | goals |
|-------|---------|---|--------|----|----|--------------|--------------------|-------|
| **PES live m1** | `0x1c821d60` (53052) | 10.0 | **48708** (0–48708; H1 = 21729 inferred 0–21728) | 21729 (inferred) | 26980 (21729–48708) | **17344** | **64.3%** (17344/26980) | **0-0** |
| **GF run** (seed 7, `--minutes 10`) | – | 10 | **71168** | – | – | **48705** | **68.4%** | **0-0** |

- PES m1 H1 = 21729 is the HT whistle frame (first observed frame, period 2,
  `kind 7/reason 1`): CONFIRMED as the H1/H2 boundary; its 0–21728 span is
  INF (the only consistent kickoff-to-whistle span — kickoff restarts the
  frame `pes-re-kickoff-trigger.md` — and no second whistle precedes it).
- PES m1 FT = frame 48708 (`period 10`); goal count from a clean,
  always-`(0,0)` score field (±0 torn reads in 26 424 fused frames — the
  earlier 118-change count was poll-level `E+0x6a/0x6b` tear, §2.4).
- In H2, 556 frames are missing to 455 ≤5-frame poller gaps (max 5; §2.1);
  none of the 9 stoppages crosses a gap interior (§5.2); the 17344 in-play
  and 64.3% share are over the nominal span (26 980 = 21729–48708).
- GF "wall s" is the *model* wall time (`frames/54`); GF runs headless (CPU as
  fast as possible, ~600 s CPU for this run), so its CPU time is not a
  game-wall figure. GF's per-half frame split is not logged (H1/H2 = –), §7.

### 5.2 Stoppages — H2, 9 exact stoppages (n=9)

Kind = mid-hold mode of `MI+0x1554`; reason = mode of `MI+0x1574`;
duration = release − whistle − 1 (logic frames); split = last-poll-of-frame
`play`/`step` counts inside. Ball/field spot-checks confirm every assignment
(§5.2 table notes).

| # | whistle | release | kind | reason | dur | split (wait `<0x13` / place / armed) | field note |
|---|---------|---------|------|--------|-----|-------------------------------------|------------|
| 1 | 23484 | 25084 | goal-kick TK (kind 1, INF) | ball-out (4) | **1599** | 27 / 1516 / 129 | ball frozen (−5.6, 76.7) at whistle; becomes the placed TK at (−5.6, 3.2)-class spot; F +0→783 |
| 2 | 25714 | 27078 | goal-kick TK (kind 1, INF) | ball-out (4) | **1363** | 23 / 1239 / 129 | whistle ball (−2.3, −72.3); release ball (0, −3.2); F +783.5→1449 |
| 3 | 28863 | 30709 | goal-kick TK (kind 1, INF) | ball-out (4) | **1845** | 22 / 1533 / 119 | whistle ball (−8.5, 66.4); release (0, 4.2); F +1449→2355.5 |
| 4 | 33815 | 34424 | corner CK (kind 4) | ball-out (4) | **608** | 28 / 570 / 375 | whistle ball (3.1, −65.8) = corner-arc class at y≈−66; F +2355.5→2642.5 |
| 5 | 37451 | 38120 | corner CK (kind 4) | ball-out (4) | **668** | 23 / 636 / 489 | whistle (−22.3, 55.3) corner-arc class; release (−33.1, 49.0) taken; F +2642.5→2961 |
| 6 | 38609 | 39138 | corner CK (kind 4) | ball-out (4) | **528** | 21 / 495 / 349 | whistle (5.1, 67.9) corner-arc class; release (−15.9, 39.8) taken; F +2961→3210 |
| 7 | 41869 | 42396 | corner CK (kind 4) | ball-out (4) | **526** | 22 / 491 / 346 | whistle (−27.9, 54.3) corner-arc class; release (−25.1, 34.3) taken; F +3210→3458 |
| 8 | 43924 | 44591 | free-kick FK (kind 5) | foul/offside (5) | **666** | 26 / 632 / 352 | whistle (7.8, 21.1) midfield; F +3458.5→3775.5; foulkind folded into reason 5 |
| 9 | 44641 | 45458 | goal-kick TK (kind 1, INF) | ball-out (4) | **816** | 23 / 777 / 134 | whistle (5.7, 74.4) dead behind goal; F +391.5→783-era accrual |
+ the **HT tail** (f21729–21925, 196 frames, kind 7/reason 1, split 3/80/105)
  and the **FT head** (f47899–48708, 788 frames, kind 7/reason 1, split
  0/788/0) — period-end programme frames, not restart stoppages.

Per-kind distributions, H2 (frames):

| kind | n | dur med | p10 | p90 | wait med | place med | arm med |
|------|---|---------|-----|-----|----------|-----------|---------|
| goal-kick TK (1, INF) | 4 | 1481 | 980 | 1771 | 23 | 1378 | 129 |
| corner CK (4) | 4 | 568 | 527 | 650 | 22 | 532 | 362 |
| free-kick FK (5) | 1 | 666 | 666 | 666 | 26 | 632 | 352 |
| **ALL H2** | **9** | **668** | **528** | **1648** | **23** | **636** | **275** |

The live stoppage split actually shows: a small **`step<0x13` wait prefix of
~20–28 frames (~1.2–1.6 wall-s at 17.5 ms)**, a long **placement `play==2,
step 0x16`-class hold**, and a **terminal `play==3` armed hold** whose length
is kind-typical (TK ~120–135 frames, CK ~350–490, FK 352). `step 0x13–0x15`
never appears; the operative placement state's mid-hold code is
`0x1010016`-class (86–95% of placement polls).

**GF run** (whistle→release, from the 21 `[parity-restart]` lines;
`[restarts]` reports 22, the 22nd being the closing period-end whistle with
no release):

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

### 5.3 In-play tick rate and clock rate (pooled over this match)

| measure | PES live m1 (H2) | GF model (kPesFps=54, M=10) | agree |
|---------|------------------|-----------------------------|-------|
| frames / wall-second | **57.01** (gap med 17.541 ms, n=17331; p10 17.0/p90 19.1) | 54 | **no (+5.4%)** |
| frames / game-second | **6.00** (0.166667 game-s/frame, n=17332) | 6.0 | yes |
| game-s / wall-second | **9.50** | 9.0 | **no (+5.4%)** |

The GF column is the **model** (`kPesFps=54`, `gameSecondsPerFrame =
90/(54·10) = 1/6`, `matchduration.hpp:42` at 9× real time). The live
measurement confirms the 6.00 frames/game-s exactly and timestamps the tick
5.4% fast of the model.

### 5.4 GF vs PES (per matched run)

| stat | PES live m1 (M=10) | GF `--pes-loop --minutes 10` seed 7 | Δ (GF − PES) |
|------|--------------------|--------------------------------------|--------------|
| frames / match | 48708 | 71168 | +22460 (+46.1%) |
| in-play share | 64.3% (17344/26980, H2) | 68.4% (48705/71168) | +4.1 pp |
| stoppages / kind | TK 4, CK 4, FK 1 (H2; HT/FT excluded) | FK 7, TI 6, GK 5, KO 3 (21 logged + FT) | kind mix differs (no TI/PK in PES H2; mapping OPEN) |
| stoppage duration med | 668 f = 11.7 wall-s @57 (ALL, n=9) | 688 f = 12.7 wall-s @54 (ALL, n=21) | same frame med by coincidence; per-kind PES≫GF (TK ~3×, CK ~1.1×, FK ~0.7×) |
| goals | 0-0 | 0-0 (+AWA shootout 9-10, out of scope) | none in play |
| tick / wall-s | 57.01 (measured) | 54 (model) | GF −5.4% |

> Reading the headline deltas: GF's +46.1% match frames are accounted to
> first order by its set-piece programme lengths (GF med whistle→release:
> FK 971 / TI 523 / GK 553 / KO 721 ticks ≈ the Cutover4 5 s and +17%
> findings' family), not by the tick rate (−5.4% would shorten, not
> lengthen). Per-kind, PES H2's TK holds (1363–1845 f) and CK holds
> (526–668 f) are the comparison rows GF's `GK 553 / CK n/a / FK 954–1003`
> have to meet once the kind mapping (kind-1 TK vs KO) is settled in §7.

### 5.5 Score and ball/holder

- Score: 0-0 at every fused frame (H2 + HT tail + FT head); **no goals in
  this match** (CONFIRMED, ±0 changes over 26 424 frames; the `E+0x6a/0x6b`
  bytes tear under poll load and are not the goal signal, §2.4).
- Ball (B = `*(G+0x40)`, `B+0x64/68/6C`): reset sentinel `(0,0,−1)` in the
  HT tail (CONFIRMED: all 188 HT-tail obs frames); whistle-anchored II/SS
  spot-checks in §5.2 (dead-ball corner arc `(±, ~±55–68)`, TK dead-behind
  `(5.7, 74.4)`, FK midfield `(7.8, 21.1)`, taken positions at the arc/spot
  or the release point) — fine for mapping GF's restart coordinates.
- Holder (`MA+0x2601C`): 0xff (=none) at every fused frame incl. all 9
  whistle frames (CONFIRMED; n=26 424): no dribble-carry was in progress at
  any whistle — ball free or dead at all 9. [INFERENCE] the chain reads a
  live zero-carry field, but "no carry at a whistle" is all this match can
  confirm.

## 6. Reproduction

```
# live poll (read-only, needs sudo for /proc/<pid>/mem; runs forever)
sudo -n python3 gf-LiveFlow-scratch/poller.py <PES2021.exe pid> gf-LiveFlow-scratch/liveflow.jsonl inf
# watch for the first live move (fires on frame advance / play==1)
sudo -n python3 gf-LiveFlow-scratch/watch.py <pid> <timeout_s>
# per-match flow stats (the §5 tables; last-poll-of-frame fusion, torn-class rule, whistle/release framing)
python3 gf-LiveFlow-scratch/flowstats_match.py gf-LiveFlow-scratch/liveflow.jsonl [G]
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

1. **Two more live matches (sample-count gate for item 2).** This report
   carries 1 complete match (H2 exact; H1 inferred 21729). The poller keeps
   appending to `liveflow.jsonl` across game restarts; matches 2–3 add rows
   under their own `G`, and the §5 tables repeat per match with pooled
   medians once n≥3. The ticket's "≥3 full matches" is a row-add, not a
   rewrite.
2. **Restart-kind-1 mapping (TK vs KO).** Four H2 stoppages carry kind 1 with
   reason ball-out — dead-behind-goal II/SS at both ends, which reads as a
   goal-kick taker-release sequence, against the A5Judge "kick-off" name.
   Needs one planned live probe to settle: hold `MI+0x1558` (awarded side),
   `MI+0x155C/60` (spot) and JR `+0x04/0x08` across a kind-1 stoppage —
   side+spot disambiguate TK from KO without touching the kind label, which
   stays cited to `pes-re-A5Judge.md` until the probe lands.
3. **Step-ladder docs (0x13–0x15) don't fire on this path.** `pes-re-restart-step.md`'s
   0x13/0x14/0x15 placement ladder never appears in 9080 hold frames; the
   live placement code is `0x1010016`-class (`step 0x16` + high byte `0x10`,
   "in pass-through"), with `0x16` = 86–95% of mid-hold polls. Either the
   ladder is a different restart path (period-end/choreography) or the doc's
   ladder needs a same-path revision — flagged for the restart-step owner,
   not renamed here.
4. **Poller hardening (done).** The first poller died at ~696k rows on a
   transient bad ball read (`TypeError` on `f32(B+0x64)` mid-reallocation);
   every None-able field is now guarded and the poller re-acquires the pid on
   a restart (4 REACQUIREs this session).
5. **H1/H2 frame split** is measured per half here (H1 = 21729 inferred);
   GF's log has no per-half tick split, so the per-half comparison is
   one-sided until GF adds one. GF's 71168-vs-48708 delta (§5.4) should be
   re-split per half once logged.
