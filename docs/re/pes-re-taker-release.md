# PES restart taker-release: who releases the taker, the timing rule, and what T+0x78 does while the ball rests (TakerRelease, 18-11-26)

Scope: why `PesPassTarget.IsSetInFlightAndNeverStaleWhileTheBallRests` is red on
`origin/OneWorld-series-wip` (set target stale 9–15 s with the ball at rest, in
set-piece restart windows), and why `TheThrowInTakersAnklesStayBehindTheLine`
broke on the same line. Static decode only (image + Ghidra bridge); no builds,
no live reads. Branch file:lines are `origin/OneWorld-series-wip`.

## 0. Verification (script output, pasted)

Script: `<workdrive>/gf-TakerRelease-scratch/verify_vas.py` (flat image mapping,
`.pdata`-anchored decode, one planted wrong VA). Ghidra bridge decompiles saved
beside it (`dec_*.txt`).

```
control OK: [0x14256AE1C] = 1.0f (flat offset 0x256ae1c)
.pdata: 158648 functions parsed
OK selector dispatch FUN_1404FB2A0: 0x1404fb2a0 (func 0x1404fb2a0-0x1404fb380)
OK in-play arm FUN_1404FB380: 0x1404fb380 (func 0x1404fb380-0x1404fb510)
OK stopped arm FUN_1404FB780: 0x1404fb780 (func 0x1404fb780-0x1404fb8ee)
OK holder FUN_140A627E0: 0x140a627e0 (func 0x140a627e0-0x140a62848)
OK release notify FUN_1406EC260: 0x1406ec260 (func 0x1406ec260-0x1406ec29b)
OK flow notify FUN_140445750: 0x140445750 (func 0x140445750-0x140445760)
OK throw-in AI FUN_142135700: 0x142135700 (func 0x142135700-0x142135afe)
  0x1404fb2c3: cmp byte ptr [rcx + 0x1775], 0xc
  0x1404fb2de: mov ecx, dword ptr [rcx + 0x116c]
  0x1404fb2ee: cmp ecx, 1 / 0x1404fb2e7: je 0x1404fb31f / 0x1404fb2ec: je 0x1404fb315 / 0x1404fb2f1: je 0x1404fb365
  0x1404fb2fa / 0x1404fb34a: mov dword ptr [rax + 0x78], 0xff
  0x1404fb7ae: cmp dword ptr [rbx + 0x11d4], 2
  0x1404fb7ca / 0x1404fb8b1: cmp dword ptr [rbx + 0x1558], eax
  0x1404fb8a5: cmp dword ptr [rbx + 0x11d4], 3
  0x1404fb8c6: mov eax, dword ptr [rbx + 0x11cc]
  0x140a627ec: mov eax, dword ptr [rax + 0x2601c]
plant OK: 0x1404fb2a1 correctly rejected (not an instruction start)
ALL CHECKS PASSED
```

Status: every VA below marked CONFIRMED is covered by the script (instruction
start in its `.pdata` function) plus the bridge decompile quoted. Doc-cited
values (counter gates, clip numbers, live timings) are marked DOC and keep
their original evidence; they were not re-decoded here.

## 1. PES restart sequence, whistle to taker's kick (CONFIRMED where marked)

### 1.1 Selector dispatch (the field the staleness reads)

`FUN_1404FB2A0` (CONFIRMED, decompile + script): reads `MI+0x1775`
(`0x1404FB2C3`, `cmp [rcx+0x1775],0x0C`) then `MI+0x116C` (`0x1404FB2DE`):

- `0x1775 == 0x0C` → return masked (target kept, no write).
- `116C == 1` → two extra gates, else `T+0x78 = 0xFF` (stores `0x1404FB2FA`).
- `116C == 2` → `FUN_1404FB780` (the stopped arm).
- otherwise (including `116C == 3`) → `FUN_1404FB380` (the in-play arm).

### 1.2 Stopped arm: PES keeps a set target through the stoppage on purpose

`FUN_1404FB780` (CONFIRMED, decompile + script):

- `MI+0x11D4 == 2` (`0x1404FB7AE`) AND `MI+0x1558 == side`
  (`0x1404FB7CA`) AND holder `> 0x15` → if `T+0x78` is set (`> 0x15`),
  **return 1 with it untouched**. The pre-stoppage target survives the
  stoppage. This is the keep the test's history cites (TeamTick4 §1).
- `MI+0x11D4 == 3` (`0x1404FB8A5`) and side matches → `T+0x78 = MI+0x11CC`
  (`0x1404FB8C6`).
- Else the bearing gates, or `T+0x78 = 0xFF`.

### 1.3 In-play arm: the same arm runs at 116C 1 AND 3, and it clears/renames

`FUN_1404FB380` (CONFIRMED, decompile + script). Clear arms
(`T+0x78 = 0xFF`): holder `uVar7 < 0x16` or the ball-record flag clear
(`FUN_140A57940` false); frames-since (`FUN_140A51320`) `< 1`; the latch arm
that drops a target that is not the holder. Otherwise rename on the 5/7-frame
cadence via `FUN_1404F0E50`, stored to `T+0x78`.

- Holder = `FUN_140A627E0(t, 0)` = `MA+0x2601C`, the ball owner
  (CONFIRMED: `0x140A627EC mov eax,[rax+0x2601C]`; captured `0xFF` from the
  kick until the receiver's touch). So a **ball-held take clears the target**:
  owner = taker `< 0x16` fires the clear arm.
- Consequence: during a take (`116C == 3`) the target is cleared or renamed
  on cadence. It cannot sit stale 9–15 s in PES. The staleness the test reads
  is `T+0x78` (and the receiver derived from it via `FUN_140931C60`, the
  side's-own-man check), and its bound is enforced by this arm keyed on
  `MI+0x116C ∈ {1,3}`.

### 1.4 Throw-in: who releases the taker, and when

- Whistle path (DOC, `pes-re-throwin-choreography.md` §§1–3): ball wholly
  over the line → judge event 10 → `judge+0xCC = 3` (`0x14050E4A0`); side =
  team that did not touch last (`0x14050E320`); restart record
  `R = (ball.x clamped to |x| ≤ F0−1, 0, ±(F1+0.25))` (`0x14050D280` /
  `0x14050E770`); `MODE = 2`, stop kind 4. Taker published (`MI+0x173C`) one
  frame after ball-out.
- Path A seamless vs path B ceremony (DOC §3.2): taker runs to the ball and
  carries it (ball held from pickup to release), or is teleported to the
  stand point (1.02 m outside) at ball-out +162, ready (`116C` 2→3) +178.
- Release (DOC §6.4, EV-TI-0015): the sequencer reaches step `0x1A`, the
  set-play counter `MI+0x11AC` runs (path A: ready+18; path B: ready+68);
  the AI throw command fires at **counter ≥ 81** (+2 frames latency; live 83
  in 10/11) when a receiver passes the course test (2–25 m) —
  `0x142135700` (CONFIRMED: real `.pdata`-anchored function,
  `0x142135700–0x142135AFE`; the ≥81 compare itself is DOC-cited, not
  re-verified). `THROWIN(_LONG)` starts 2 frames later.
- Release clip by target distance (DOC §4.1–4.2): ≤13 short, ≤25 mid, else
  long (run variants out of the carry). Release at the clip's release event
  (short 18 @rate 1 / 24 @0.75, mid 27, long 99, run-mid 36) → the ball is
  launched **and `MI+0x116C` goes 3→1 (in play) at that touch**.
- Ball held in the taker's hand until the release (`FUN_140792E10`, DOC
  §4.2); nearer ankle must be at least `F1 + 0.1` (`DAT_142588200`) from the
  centre line. Ball-out → in play: 185–288 frames path A (3.4–5.3 s),
  +351–357 path B (~6.5 s) (DOC live).

### 1.5 Free kick: who releases the taker, and when

- Taker chain `FUN_1404A8AD0` (DOC, `pes-re-setpiece-kicker.md` §1.2):
  designated kind 1/0 by 32/60 m → ready kicker → keeper near own goal →
  CB/SB own half → nearest non-forward → rating fallback. Wall in step
  `0x17`; placement/teleport in the `0x14` 20-tick window; `0x15` finish;
  fade; `0x1A` armed.
- Think `FUN_142135c50` (DOC §2.4): shoot/pass decided from
  `ballplayerSetplay.o`, evaluated once **elapsed ≥ fps·3 (162 frames)**;
  live `FREEKICK_LONGPASS` at counter 164 (×2). Corner gate fps·3.5 (189).
- Strike window = run-up frame ≥ evt − ⌊fps·20/60+0.5⌋ = **−18 frames**;
  live strike phase entered 19 frames before contact (×6), run-ups 73–77
  frames. Attackers' runs start inside the strike window (~0.24 s before
  contact); contact → in play.

### 1.6 The in-play edge (take start vs touch)

GF's release-event chain per `pes-re-throwin-choreography.md` §4.2 phase 1
and `pes-fix-RestartRef.md` §5.3: `FUN_1406EC260 → FUN_1406EC2A0 → … →
FUN_140445750` (flow notify), raising `116C` 3→1 at the release event, i.e.
at the **touch**, not at take start. CONFIRMED endpoints: `0x1406EC260`
(decompile: calls `FUN_1406EC2A0`) and `0x140445750` are `.pdata`-anchored
instruction starts (script); the middle of the chain is DOC-cited. GF's
counterpart raises in-play at take start: 25 ticks before the touch for the
throw-in (RestartRef §2.2 trace: take 3320, touch 3345), ~90 frames for a
free kick/corner (run-up + strike lead).

## 2. GF vs PES: the stand-ins that stall (branch file:line)

1. `src/sim/onthepitch/referee.cpp:722` (`buffer.started = true`),
   `:739` (`match->StartPlay()`), `:740` (`match->StartSetPiece()`) —
   **in-play is raised at take start**. PES holds `116C == 3` through the
   take and writes 1 at the kick (§1.6). The test's
   `resting = IsInPlay() && ball slow` is therefore true through GF's whole
   take window while the ball rests and the (faithfully kept, §1.2) target
   stays set. This is the stand-in that manufactures the 9–15 s episodes.
2. `src/pes/movestep.hpp:35-40` (`PlayStateFor`): `if (inPlay) return 1`.
   Its own ponytail names the PES rule it violates: "An awarded restart
   that is prepared or started is 3 (PES holds 3 through the TAKE and writes
   1 at the kick)". Same selector arm either way (§1.1), but every
   in-play-only consumer — including the test — sees live play during the
   take. PES rule replacing it: stage `116C = 3` from take start to the
   touch, `1` only at the kick (chain §1.6; PES's per-restart writer is
   OPEN per `pes-re-A6Writers.md` §4, so key it on the touch event).
3. `src/sim/onthepitch/pessequencer.cpp:22-23` (`kCornerReleaseFrames = 365`,
   `kFreeKickReleaseFrames = 355`) + `:18` ponytail ("DOC 189 not
   reproduced") + `:94-96` (`MayRelease`: throw-in = `takerArrived` only).
   Fixed 6.6–6.8 s gates stand in for PES's taker reach + kick action (A5
   §§9.2/9.4). They delay arming (pre-release, in-play false — no stale by
   themselves) but they age the kept pre-stoppage target before the take
   even opens.
4. `src/sim/onthepitch/teamAIcontroller.cpp:1408-1411`
   (`PieceTakerArrived() == pieceLifted`) + `AdvancePieceWalk` (`:1319+`):
   the throw-in take is arrival-gated and locomotion-driven (MOVE arrival,
   lift warp, loop, ShortPass command). Any walker/carrier stall (creep,
   refused issue, far taker) stretches the **in-play** take window with the
   ball at rest and the kept target set: the 9–15 s shape.
5. `teamAIcontroller.cpp` `ThrowInTargetMate` (≈`:1413-1432`) ponytail:
   nearest-mate-inside-the-line stands in for `FUN_142135700` + the course
   test — the receiver the take throws to (or waits on).
6. `src/sim/onthepitch/team.cpp:445`
   (`world.RunPassTarget(id, PlayStateFor(IsInPlay(), buffer.active, …))`)
   + staging ponytails (`:392-444`: owner = GF retainer vs PES W1 holder,
   PASS_GET-only action words, zeroed SR/MA/T/B fields, time-to-ball
   counter-B stand-in). Feeds `playState = 1` through GF's take.
7. Ankle test, same take window: `PushPieceTakerBehindLine`
   (`teamAIcontroller.cpp:1453-1490`, gate `:1459-1460`: `IsInSetPiece &&
   started`, carrying, arrived) is the GF stand-in for `FUN_140792E10`'s
   guards (`PushGateOpen`); the nearer-ankle rule (joints `P+0x538+12b`,
   threshold `F1 + DAT_142588200` = 0.1 m) is PES-faithful. It gates on
   `started`, so any take stall/re-timing on this line breaks it too. Its
   exact red mode (which release, which ankle margin) is OPEN: it needs the
   test log, i.e. a build+run, which is outside this static-only ticket —
   ask Main for `pes_throwin_release_test` output on the line.

## 3. Verdict: the test's scope is correct — the engine is wrong

- The bound **applies in restart windows in PES** (CONFIRMED §§1.1–1.3):
  the take runs at `116C == 3` through the **same** `0x1404FB380` arm,
  which clears (ball-held take: owner = taker `< 0x16`) or renames on the
  5/7-frame cadence. A set target cannot survive 9–15 s of rest in PES,
  take included. The `0x1404FB780` keep is stoppage-only (`116C == 2`) and
  the test's `resting` already requires in-play, so the keep does not leak
  into the measurement **in PES**.
- Do **not** re-scope the test: `58350844`'s `active||prepared||started`
  exclusion was removed by `a936c452` for a verified reason —
  `prepared`/`started` are never cleared at the take (only the next award
  resets them; TeamTick4 §1), so that scope excludes all live play after
  the first restart. Re-adding it (or any `prepared/started` keying) hides
  the stall instead of fixing it.
- Fix the engine, in this order: (i) move `StartPlay()` from take start
  (`referee.cpp:739`) to the touch — the release-event chain of §1.6
  (this is RestartRef §5.3's open item, with its red test named there);
  (ii) stage `116C = 3` through the take so the translated chain takes the
  take-state path (`movestep.hpp:35-40` ponytail already names the site);
  (iii) shrink the take itself (arrival/issue stalls, item 4) — the ankle
  test in item 7 is its tripwire.
- Temporary scoping, if Main needs the tree green before (i): key on the
  **touch** (`buffer.active`, i.e. take window only), never on
  `prepared/started`. Note this blesses exactly the stall above, so it must
  carry the take-stall ticket number and aExpiry; it is not a verdict on
  PES behavior.

## 4. Open items

1. The `≥81` compare site in `0x142135700` and the `116C` 3→1 writer
   between `FUN_1406EC2A0` and `FUN_140445750`: DOC-cited, endpoints
   verified, middle untraced.
2. Which GF-staged record defeats the `0x1404FB380` clear during GF's take
   (owner/latch/frames-since/cadence): needs the per-tick probe the
   PassArrival line already owns; not re-traced here.
3. Ankle-test exact red mode: needs Main's `pes_throwin_release_test` log
   on the line (no build in this ticket).
4. `MI+0x11CC`/`0x11C8` writers and `FUN_140A51320`'s event input on GF's
   staging: OPEN, same probe as item 2.
