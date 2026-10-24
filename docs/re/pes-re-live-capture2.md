# PES live capture 2: stoppage settlement (T8b / restart step / T6A / camera+PA+meter / kick-off action)

Status: read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(VA = fileoff + 0x140000000), live PES pid 2510857 (read-only `sudo -n` pread only).
Log: `<workdrive>/gf-LiveSessionRE-scratch/live_session_capture2.log`
(25,959 lines, ~1 h, `live_session.py` in the same dir; format §0).
Prior session: `docs/re/pes-re-live-session.md` (capture 1: endless match, no stoppages).
Samplers for the remaining gaps: `<workdrive>/gf-LiveSession2RE-scratch/`
(`cap_restart_step.py`, `cap_kickoff_action.py`, `cap_camera_pa.py`).
No live match is running at write time (`MI=0` on re-check 2026-10-24); the samplers below are written, syntax-checked, NOT yet run.

Verdicts: **CONFIRMED** = log lines or live reads this session; **INF** = inference;
**OPEN** = blocked, blocker named. Line numbers below are 1-based log lines.

## §0 Log format (read once, cited everywhere below)

- `MI per=.. play=.. restart=.. side=.. reason=.. frame=.. step11B0=.. mode44=.. T8b=..`:
  `play` = `MI+0x1168`, `restart` = `MI+0x1554`, `side` = `MI+0x1558`,
  `reason` = `MI+0x1574`, `frame` = `MI+0x1198`, `step11B0` = `MI+0x11B0`,
  `mode44` = `MI+0x44`. Logged only when the tuple changes (~20 Hz poll).
- `T8b=[(hex11B, word), (hex11B, word)]`: `T_s+0x8C1F` (11 bytes) + `T_s+0x8CE8` (u16)
  for `s` in {0,1}, `T_s = *(G+(s+0xA1)*0x28)`. `None` = unreadable that tick
  (mid-tick publish race, same cause as the `MI null flicker` lines).
- `T6A count=.. n=..`: `MA+0x24D50` (8B) + `MA+0x24D54` (u16), 1 Hz.
- `T12B nz=../48 ...`: 48 qwords at `H+0x62B8+off` (**wrong base**, see §5;
  true rows are `[H+0x450]+0x62B8`, `pes-re-t12b.md` §2).
- Restart-kind map (`pes-re-A5Judge.md` §2 table): 1 kick-off, 2 throw-in,
  3 goal kick, 4 corner, 5 FK (+6 PK, 7 period end, absent here).
  Reason map: 3 goal/kick-off, 4 ball out, 5 foul/offside.

Coverage: 18,858 MI lines: `play=1` ×15,769, `play=2` ×1,847, `play=3` ×1,242.
54 stoppage episodes (`play=2/3` runs): kind 1 ×15, 2 ×8, 3 ×14, 4 ×5, 5 ×12
(one log-tail kind-3 still `play=2` at line 25951; durations §2.2).
`mode44=0x00` on all 18,858 MI lines. `step11B0` values seen:
`0x16` ×18,719 + `0x01` ×45, `0x15` ×55, `0x14` ×5, `0x0e` ×4, `0x11` ×4,
`0x0d` ×8, `0x0c` ×2, `0x02` ×1, `0x06` ×1, `0x0a` ×1, `0x04` ×2, `0x0f` ×3,
`0x13` ×1, `0x0b` ×2, `0x05` ×2, `0x09` ×1, `0x08` ×1, `0x12` ×1.

## §1 T8b (blocked-slices §1 recipe) — SETTLED except the writer

Doc OPEN (blocked-slices §1, survey T8b row, `pes-re-live-session.md` §2/§6):
readers CLOSED, zero static writers image-wide; live recipe = per-tick
`T+0x8C1F` (11B) + `T+0x8CE8` (u16); first nonzero names the set tick.

### 1.1 What the log shows (CONFIRMED)

- 2,179 of 18,858 MI lines carry a nonzero `8C1F` payload byte (script §0 recipe;
  counting rule: any of the two 11-hex strings not all-zero).
  215 contiguous nonzero runs (`analyse_t8b_runs.py` in the scratch dir; full table
  `transitions.txt` sibling; run list regenerated on demand).
- Byte coding (CONFIRMED, whole-log count): the 11-byte hex string is
  `bytes.fromhex`, index 0 = first hex pair. Only byte values `0x01` ever set
  (no other value in any of the 2,179 lines). Set-count by index:
  8 ×821, 9 ×392(+62 double with 3/6/7), 7 ×419, 6 ×336(+77/60/27 doubles),
  3 ×129(+128 with 6, +62 with 8), 4 ×123, 5 ×10.
  The `u16` word on nonzero rows: 17 ×2,077, 0 ×65, 1 ×22, 14 ×10, 2 ×5.
- By game state (CONFIRMED): open play (`1r0`) 1,924; stoppage lines with
  nonzero flags: `2r1` 77, `3r1` 9, `2r3` 37, `3r3` 58, `2r4` 17, `3r4` 52;
  `3r2`/`2r5`/`3r5` zero nonzero lines (throw-in-armed and FK-armed never set flags
  in this log; only 2 FK-armed episodes exist at all, both `0x16`-only).
- Team side (CONFIRMED): entry 0 = `s=0`, entry 1 = `s=1`; all 215 runs touch
  exactly one entry (zero MI lines have both entries nonzero). No run ever sets
  the same byte index on both teams at once.

- Flags are **open-play marking state, not stoppage state**: all but 4 of the 215
  runs live and die entirely inside `play=1 restart=0` (typical life 0.2–2 s, e.g.
  lines 35–63 slots {3,8} team 1; lines 429–443 slots {6,9} team 0).
  Only 4 runs overlap a stoppage, and all 4 are stoppage-PERSISTENT bits,
  set in open play BEFORE the stoppage and cleared at/just after the return
  to open play — never set or cleared mid-stoppage:
  - lines 1212–1432 slots {3,6} team 1: set ≤164.2 s (open), persists through
    the kind-1 stoppage (lines 1215–1431) and one more open-play line,
    clear line 1433. NOT set at +85 s; the ticket's "+85 s" line 429 is a
    different, ordinary 1.6 s open-play run (slots {6,9} team 0,
    lines 429–443, set 85.3 s clear 87.1 s, no stoppage within ±75 s).
  - lines 3173–3235 slot 9 team 0: set 414.0 s (open, line 3173), stoppage
    starts 414.7 s (line 3180), ends 422.1 s (line 3236, clear same line).
  - lines 12595–12721 slot 3 team 0: set exactly on the stoppage's first line
    (line 12595 = play 2 start), clear exactly on the first open-play line
    (line 12722). Single-line resolution; direction ambiguous, but the
    matching clear-at-open-end in the other three makes set-at-stop-start INF.
  - lines 17568–17695 slot 3 team 0: set 2546.0 s (open, line 17568),
    stoppage 2546.7–2575.2 s (lines 17574–17695), clear line 17696.
- Therefore: **no T8b bit is set or cleared by any stoppage transition in this
  log** (no run starts/ends on a `play`/`restart`/`step` change line except the
  ambiguous 12595 case above, which still clears at the open-play return).
  The "which slot/bit means what" mapping stays what the static docs say:
  marking-flag inputs to the 40f cost (`pes-re-team-tickets.md` §5.2,
  `// ponytail:` naming `FUN_1405A4830`/`FUN_140A638C0`) — the log adds
  liveness (single-team, value-always-1, open-play duty cycle ~11.5% of MI
  lines) but no per-slot semantic. Slot↔player mapping still OPEN.
- Correction to the ticket's premise: the "+85 s" event is open-play churn,
  not a stoppage edge (lines 429–444 all `play=1 restart=0`).

- CLOSED by this log: flags fire live (both teams), value domain ({0,1}),
  team-exclusivity, open-play duty cycle, persistence through stoppages,
  clear-at-open-return (3/4 exact, 1/4 one line after).
- Still OPEN (unchanged, narrowed): the per-slot meaning and the writer
  (`FUN_1405A4830`/`FUN_140A638C0` still undecoded; static scan already
  exhausted — full-image owner scan: 0 writers, blocked-slices §1).
  Next: live watch on a `T+0x8C1F` store during an open-play run
  (debugger-class tooling, not pread — same class as the PA breakpoint).

## §2 Restart step `MI+0x11B0` (restart-step §4 recipe) — VALUE SEQUENCE CLOSED

Doc OPEN (restart-step §§4/6–7): sparse-poll INF order
(`0x16 → 0x00/0x01` at kick-off pending; `0x16` at ready; take = `play 3→1` +
`restart 1→2` on ONE 0.3 s sample); frame-level programme across 2–3 restarts
still OPEN; per-value `O+0x728` writer OPEN.

### 2.1 Observed sequences (CONFIRMED; line ranges = transition lines)

Poll is change-gated (a step value is logged iff the whole MI tuple changed),
so each list below is the COMPLETE value sequence per episode, order CONFIRMED,
dwell times NOT measured (a value held across many frames logs once):

- Kind 1 kick-off (15 eps): `0x16`-only ×3 (lines 3795–3928, 17239–17390,
  24087–24316 — goal + kick-off with NO step excursion at this poll density);
  `0x16,0x01,0x16` ×3; `0x16,0x12,0x16` ×1 (lines 110–339: `0x12` line 319,
  then `play 3` line 322 at `0x16`); `0x16,0x08,0x16` ×1 (lines 1215–1431:
  `0x08` line 1415, `play 3` line 1418 at `0x16`); `0x16,0x0d,0x16` ×1;
  `0x16,0x01,0x0a,0x14,0x16` ×1 (lines 8012–8232: `0x01` line 8211,
  `0x0a` line 8213, `0x14` line 8214, `play 3` line 8215 at `0x16`);
  `0x16,0x0e,0x16` ×2 (one with `0x01` prefix); `0x16,0x0b,0x16` ×1;
  `0x16,0x01,0x0d,0x16` ×1; `0x16,0x14,0x16` ×1 (lines 23676–23859:
  `0x14` line 23833, `play 3` line 23836 at `0x16`).
- Kind 2 throw-in (8 eps): `0x16`-only ×5 (e.g. lines 10344–10372,
  20479–20527, 21114–21152, 23536–23579, 25602–25638 — stop→armed→in-play
  with no excursion); full `0x16,0x01,0x14,0x15,0x16` ×1 (lines 2149–2200:
  `0x01` line 2169 frame 174117, `0x14` line 2171 frame 174145,
  `play 3` + `0x15` line 2174 frame 174156, `0x16` line 2176 frame 174168);
  `0x16,0x15,0x16` ×1; `0x16,0x01,0x0f,0x15,0x16` ×1 (lines 19740–19790).
- Kind 3 goal kick (13 eps; 12 complete + tail cut at log end line 25951):
  `0x16,0x01,0x15,0x16` ×3; `0x16,0x01,0x04,0x15,0x16` ×2 (lines 9389–9472,
  12186–12267); `0x16,0x01,0x0d,0x15,0x16` ×1 (lines 3180–3235:
  `0x01` line 3198, `0x0d` line 3202, `play 3` + `0x15` line 3203,
  `0x16` line 3207); `0x16,0x01,0x0c,0x15,0x16` ×1 (lines 3575–3653);
  `0x16,0x01,0x02,0x11,0x15,0x16` ×1 (lines 6504–6577);
  `0x16,0x01,0x06,0x14,0x15,0x16` ×1 (lines 6698–6771: `0x06` line 6738,
  `0x14` line 6741, `play 3` + `0x15` line 6743);
  `0x16,0x0f,0x15,0x16` ×1; `0x16,0x15,0x16` ×1; `0x16,0x11,0x15,0x16` ×1;
  `0x16,0x01,0x05,0x15,0x16` ×1.
- Kind 4 corner (5 eps): `0x16,0x01,0x15,0x16` ×1 (lines 9891–9995);
  `0x16,0x01,0x0d,0x15,0x16` ×1 (lines 10146–10271);
  `0x16,0x01,0x13,0x16` ×1 (lines 14158–14280 — `play 3` arrives AT `0x16`,
  no `0x15`); `0x16,0x01,0x0d,0x0f,0x15,0x16` ×1 (lines 14699–14787);
  `0x16,0x01,0x09,0x15,0x16` ×1 (lines 17574–17695).

### 2.2 Rules (CONFIRMED, refine restart-step §4)

- `play 2→3` (take) coincides with the FIRST `0x15` sample in every episode
  that reaches `0x15` (32/32 kinds-2–5 episodes incl. the log tail; 7 reach no
  `0x15` — five `0x16`-only throw-ins, one kind-4 `…0x01,0x13,0x16` at line 14235,
  and the log-tail kind-3 cut at line 25951) — and with the first `0x16` AFTER the
  excursion in every episode that never shows `0x15` (kick-off and
  `0x16`-only throw-ins; e.g. lines 322, 1418, 3920, 6244).
  So at this poll density the take edge = first sample of the post-excursion
  value (e.g. kind-2 line 2174, kind-3 line 3203, kind-5 line 2455);
  kinds 2–5 end `…0x15 → 0x16`; kick-off ends `… → 0x16` directly.
- Durations (CONFIRMED, wall-clock from log stamps, poll-density caveat):
  kind 1 total 41.7–64.3 s (p2→p3 39.4–62.3 s, p3→end ~2 s);
  kind 2 total 3.6–6.3 s; kind 3 total 3.1–30.7 s (median 18.8);
  kind 4 total 12.8–31.8 s; kind 5 total 16.1–34.2 s (median 17.9).
- The §4 "take = `play 3→1` + `restart 1→2`" single-sample claim is SUPERSEDED:
  no `play 3→1` edge in this log carries a `restart` change (restart returns to
  0 only on the later `play→1` line, e.g. line 2201 after kind-2; line 3929
  after kick-off). The take edge is `play 2→3`, not `3→1`.
- New values beyond §3's table: `0x01,0x02,0x04,0x05,0x06,0x09,0x0a,0x0b,0x0c,0x0f,0x11`
  all appear as live pre-take excursions (kind-indexed above).
  `0x01` is the common post-`0x16` entry for kinds 2–5 (30/39 episodes
  including the log tail; the 9 others enter via `0x0f/0x11/0x13/0x15` or stay
  `0x16`-only); kick-off entries vary (`0x16`-only or via
  `0x01/0x08/0x0d/0x0e/0x0b/0x14/0x12` — no `0x15` in any of the 15 kick-offs).

### 2.3 Status change

- CLOSED: the per-kind value sequences (order) above; the take-edge rule
  (`2→3` = first post-excursion sample); kick-off never visits `0x15`.
- Still OPEN (unchanged): per-frame dwell/advance (needs the armed-tick
  sampler, §6 — poll here is change-gated, not frame-level);
  the per-value `O+0x728` writer (static, untouched by this log).

## §3 Camera writer / PA identity / metering rect — what the log carries

### 3.1 Camera (CONFIRMED negative)

- The log carries exactly one camera byte: `mode44 = MI+0x44`, and it reads
  `0x00` on all 18,858 MI lines (all plays, all restarts, all 54 stoppages).
  No mode switch occurred, so the `[matchstruct+0x44]` writer
  (camera-settings §4 OPEN, one hop) is untouched by this log. No `InplayCamera`
  pose scan is in the log (recipe: camera-live §1 vtable-scan for `0x142609C28`;
  per-session addresses, must run live).
- Correction to `pes-re-live-session.md` §6: the "camera writer" item is NOT
  settled by capture 2 — capture 2 only proves the byte never leaves 0 in this
  match. Targeted sampler `cap_camera_pa.py` (§6) does the heap scan + slider
  dump during a stoppage; NOT yet run (no live match at write time).

### 3.2 PA identity (CONFIRMED gap)

- The log carries NO PA sample (grep `PA|camera|CAM|METER|InplayCamera` = 0 hits;
  only `0x44` hit is the ticket's own `mode44` column). Publisher-link §1
  (`dst` vs `FUN_140A48910(G,id)` at `FUN_140430500`) still needs a breakpoint
  (not pread-reachable) — the script named the gap and took no fake sample,
  same here. Still OPEN, unchanged.

### 3.3 Metering rect (CONFIRMED gap)
- Same: no exposure/grading sample in the log (0 hits). Survey §5.1 /
  ComposePath §4 inputs stay OPEN. `cap_camera_pa.py` logs the gap the same
  honest way; no fake sample. Still OPEN, unchanged.

  (Full tables: T8b run list + 269-line transition list in
  `<workdrive>/gf-LiveSession2RE-scratch/` — `analyse_t8b_runs.py`, `transitions.txt`.)

## §4 T6A — INERT through stoppages too (CONFIRMED)

- All 3,143 `T6A` lines read `count=0000000000000000 n=0` — including the 1 Hz
  samples inside all 54 stoppage episodes (spot-checked episodes: kind-2
  lines 2167–2200, kind-3 lines 3172–3236, kind-5 lines 2416–2511, kick-off
  lines 110–339: every enclosed `T6A` line is zero).
- Closes blocked-slices open item 3 ("one live second-half read through a
  restart; if zero across a full match, lock in as inert"): the full-match
  zero (open + all 5 restart kinds + 15 kick-offs) LOCKS IN the §1 verdict —
  no pusher ran in this match. Port action stays: gate only, push parked.

## §5 T12B — nothing new (wrong base, CONFIRMED again)

- Capture 2 uses the same wrong base as capture 1 (`H+0x62B8+off`,
  missing the `[H+0x450]` deref; `pes-re-t12b.md` §2). Its 1,141 `T12B` lines
  (117 inside stoppages, 1,024 in open play) are the same unrelated region:
  `0xbf800000bf800000` (−1.0,−1.0) flats + one drifting f64 column, same as
  `pes-re-live-session.md` §5 / `pes-re-t12b.md` §2. No row payloads, no
  vtable reads. The true 46-row table (`[H+0x450]+0x62B8`, `rows_live.txt`)
  is unaffected. No doc change needed beyond this note.

## §6 Samplers for the remaining gaps (written, NOT run)

All in `<workdrive>/gf-LiveSession2RE-scratch/`, syntax-checked (`ast.parse`
GREEN 2026-10-24), read-only pread, one at a time, each exits by itself:

1. `cap_restart_step.py --pid PID --dur 900`: 100 Hz MI poll from the first
   `play=2/3` line to the `play→1` release; logs every
   `(play,restart,side,reason,frame,step)` change. Closes the §2 dwell
   remainder (which frame advances each step value) in ONE stoppage.
2. `cap_kickoff_action.py --pid PID --dur 1800`: waits for a kick-off
   stoppage (`play 2/3 & restart=1`, reason 3), dumps the `G+0x180` gate
   bytes (`0x18` entries, stride `0x74`) each tick while armed; resolves the
   `R12` player array (`**(Flow+0x17860)+0x68`, stride `0xb4`) when the Flow
   chain is reachable and dumps `*P` action bytes with the same cadence.
   Closes kickoff-trigger §4 (§7) if the match reaches a kick-off.
   Caveat: Flow resolution needs the tick's Flow object (the transition-race
   in kickoff-trigger §2 applies); gates-only output still constrains the
   scan (`{0,2}` gate set per entry).
3. `cap_camera_pa.py --pid PID --dur 1800`: one heap pass for `0x142609C28`
   (InplayCamera: `+0x08` mode, `+0x0C/+0x10` sliders) + `0x142A003E0`
   (fox Camera: `+0x60` pos, `+0x100` FOV), then `MI+0x44` + PA/METER gap
   notes. Closes the §3.1 writer-adjacent values in ONE stoppage; PA/METER
   stay breakpoint/open-walk by construction.

Run order when a match is up: (1) during any stoppage, (3) during the same
stoppage if time allows, (2) only when a kick-off (goal) happens. Total < 1 h.

## §7 Kick-off action byte — NOT in this log (sampler ready)

- Capture 2 has 15 kick-off episodes (kind 1, reason 3, §2.1) but the logger
  never sampled `G+0x180` gates or the `R12` `*P` bytes (format §0 has no
  such column; kickoff-trigger §2's `kickoff_actions.log` is a different
  script's product). Kickoff-trigger §4 OPEN stands: the kick's `*P` value /
  writer is still unknown. `cap_kickoff_action.py` (§6.2) is the one
  armed-tick sampler for it. No live match at write time, so NOT run.

## §8 Doc corrections to apply

1. `pes-re-live-session.md` §6: "T8b ... unsettled ... Rerun during a normal
   exhibition match" — SUPERSEDED by §§1/4 here: flags fire in open play on
   both teams (value domain {0,1}, team-exclusive), persist through stoppages,
   clear at the open-play return; T6A inert through stoppages too (locked in).
2. `pes-re-restart-step.md` §4: the take rule "play `3→1` + `restart 1→2`"
   (one 0.3 s sample) is SUPERSEDED by §2.2: take = `play 2→3` = first
   post-excursion sample (30/30 kinds-2–5 at `0x15`; kick-off at post-excursion
   `0x16`, never `0x15`); add the per-kind sequences (§2.1) and the new
   excursion values (`0x01/0x02/0x04/0x05/0x06/0x09/0x0a/0x0b/0x0c/0x0f/0x11`).
3. `pes-re-blocked-slices.md` §4 item 3: "if zero across a full match, lock in
   as inert" — DONE (§4): zero across open + all restart kinds + kick-offs.
   Item 1 (T8b writers) narrows to: live watch during an OPEN-PLAY run
   (not a stoppage), since stoppages never set/clear bits.
4. `pes-re-t12b.md` §2: capture 2 repeats the wrong-base read (1,141 lines,
   §5) — no new row data; no correction to the true-row table needed.
5. `pes-re-kickoff-trigger.md` §4: unchanged (OPEN stands); the sampler is
   `cap_kickoff_action.py` in `<workdrive>/gf-LiveSession2RE-scratch/`.
6. Camera/PA/meter docs (`pes-re-camera-settings.md` §4, `pes-re-publisher-link.md`
   §1, Survey §5.1): unchanged (OPEN stands); capture 2 contributes the
   `mode44≡0x00` negative (§3.1) and the sampler (`cap_camera_pa.py`).
