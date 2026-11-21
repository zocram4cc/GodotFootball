# PES penalty save and the shoot-out keeper's body G (KeeperDive)

Branch `PassArrival` at `f8aa8e41` (red test only; height fix still unlanded, patch `<workdrive>/gf-PassArrival-scratch/r5/height_fix_NOT_LANDED.patch`). Read-only on code; scratch in `<workdrive>/gf-KeeperDive-scratch/` (`consts.py`, `g_table.py`). Image: `pes_full.bin` (flat, offset = VA − 0x140000000). Ghidra bridge answered (decompile of `0x1406BA070`, `0x141AF3180`, `0x1406B9E80`). No build, no sim, no live read.

Verdict in one line: a 180 cm keeper's PES body scale is G = 1.0 exactly; the save count cannot yet be made PES-sourced (PES has no shoot-out save rate in the tree, and the dive reach at the kick's arrival frame is not decoded). The three GF G readers compute PES's formulas at the same G. The fault is upstream, at G's height input.

## 1. The G formula and its constants (CONFIRMED, script output)

PES body scale G = clamp(BlockScale(h, L), lo, hi), with BlockScale = (1 + legGain·L)·(1 + tallGain·dev(h)). Reconstructed in `src/pes/physique.cpp` (sites at lines 21–30 check the opcode at each load VA). Script output (`consts.py`, every value read from the image):

```
one 0x14256AE1C                  1.0                      bytes=0000803f
heightFloorCm 0x142A83540        132.0                    bytes=00000443
heightCeilCm 0x1425CA310         220.0                    bytes=00005c43
heightAnchorCm 0x142598580       180.0                    bytes=00003443
lowerSpanCm 0x142588214          48.0                     bytes=00004042
upperSpanCm 0x14259BE2C          40.0                     bytes=00002042
tallGain[0] 0x142A83538          0.22300004959106445      bytes=205a643e
tallGain[1] 0x142A8353C          0.2770000100135803       bytes=f2d28d3e
legGain[0] 0x142A83530           0.018000006675720215     bytes=c074933c
legGain[1] 0x142A83534           0.018999993801116943     bytes=e0a59b3c
scaleLo 0x1425DDF78              0.9165999889373779       bytes=4ca66a3f
scaleHi 0x1425DDF80              1.083299994468689        bytes=93a98a3f
gSlope 0x1425DDFE0               -0.16670000553131104     bytes=68b32abe
stepSlope 0x1425C48A0            0.15000000596046448      bytes=9a99193e
stepBase 0x1425AB124             0.949999988079071        bytes=3333733f
turnP2 0x142588200               0.10000000149011612      bytes=cdcccc3d
turnT 0x1425A8A28                0.05000000074505806      bytes=cdcc4c3d
centi 0x14256AE18                0.009999999776482582     bytes=0ad7233c
```

Consequences (CONFIRMED by the same numbers):
- The scale clamp `[0x1425DDF78, 0x1425DDF80]` is **the same pair** as the playback ramp's gLow/gHigh (0.9166 / 1.0833).
- At the anchor, dev(180) = 0, so tall = 1 and (for a neutral leg slider, L = 0) leg = 1: **G(180 cm) = 1.0 exactly**, whatever the gains.
- Any height up to about 165.6 cm gives BlockScale below 0.9166 and clamps to the floor. The **pre-fix GF input (stat 0.6 → 60 cm) is on the floor**: G = 0.9166 for every man under about 166 cm.

Table (`g_table.py`, float32, mirrors `pes::PhysiqueG` and the step playback; L = 0):

```
lo=0.9165999889373779 hi=1.083299994468689 one=1.0 anchor=180.0
  h_cm          G  stepPlayback  hit-dist x vs G=1
   125   0.916600      1.100000           0.916600
   150   0.916600      1.100000           0.916600
   165   0.916600      1.100000           0.916600
   170   0.942292      1.076882           0.942292
   180   1.000000      1.024955           1.000000
   190   1.055750      0.974790           1.055750
   195   1.083300      0.950000           1.083300
   205   1.083300      0.950000           1.083300
GF pre-fix input: physical_height stat 0.6 -> cm 60 -> 0.9165999889373779 (floor)
turn playback p=0.5 at G=1.0 / floor: 0.9999849796295166 1.024999976158142
t(G=1.0) = 0.49970000982284546  step(1.0) = 1.0249550342559814  step(floor) = 1.100000023841858
f32 @0x142588210 = 10.0  (movestep.hpp:356 cites this VA for turnP2 = 0.1)
```

Heights 125–205 cm are the save range (`docs/re/pes-re-physique.md` §1.1, CONFIRMED there). Inside it, G runs 0.9166 to 1.0833.

Stale citation: `src/pes/movestep.hpp:356` says turnP2 = 0.1 is at `0x142588210`. The image there reads 10.0. The live value is at `0x142588200` (0.1), which is what translated `FUN_1406E9F10` reads. Fix the comment.

## 2. Which PES code reads G, and the three GF readers (CONFIRMED)

PES readers take the physique chain G: `FUN_141AF3180(P+0xA08) = *(*(P+0xA08)+0xF0)+0xC0` (Ghidra decompile this session). `FUN_141EB5810` writes it from the physique block.

| PES function (VA) | what it computes | GF reader (file:line) | verdict |
|---|---|---|---|
| `FUN_1406BA070` step/stop playback: `0.95 + 0.15·t(G)`, `t` = ramp, G from `FUN_141AF3180` (decompile this session) | `step(G)` | `movestep.cpp:64-66` `StepPlayback`, ramp `movestep.cpp:39-45`; callers `movestep_group_a.cpp:296`, `movestep_step.cpp:950/1732`, `movestep_run.cpp:1289` | **MATCH.** Same constants (`0x1425DDF78/80/FE0`, `0x1425C48A0`, `0x1425AB124`), same order `ramp·slope + base`. Edge: NaN G gives t = 1 in PES, 0 in GF. Not reachable for finite G. |
| `FUN_1406E9F10` turn playback: `0.95 + 0.10·p² + 0.05·t(G)` (translated, `src/pes/translated/restart/pes_fn_1406E9F10.cpp`) | `turn(G,p)` | `movestep.cpp:68-70` `TurnPlayback`; caller `movestep_issue.cpp:36` `EntryPlayback` | **MATCH.** Constants `0x142588200` (0.1), `0x1425A8A28` (0.05), `0x1425AB124` (0.95), same ramp VAs; arithmetic order identical. Stale VA comment at `movestep.hpp:356` (section 1). |
| `FUN_1406E7730` per-motion hit distance: `sqrt(x²+z²)·g`, root × `0x14256AE18` (0.01), cancel record at `min(index, count−1)`, motion ≥ `0x1125` → 0 (translated, `src/pes/translated/closure/pes_fn_1406E7730.cpp`; g = xmm2 saved to xmm9 at entry; the final `mulss` by xmm9) | `hit(G)` | `movestep_issue.cpp:55-68` `ClipHitDistance`; caller `movestep_issue.cpp:75` `WriteIssue` | **MATCH.** Same scale, same clamp, same `0x1125` motion cut (`movestep_issue.cpp:13`, `kMotionLimit`). |

G consumers the brief did not name (checked in passing):
- `SpeedToRate`, `movestep_common.cpp:375`: `v = (speed/3.6…) / PhysiqueG()`. PES `FUN_1406B9E80` (decompile this session) divides by `FUN_141AF3180(P+0xA08)`. **Same placement.** The scale constant (`DAT_1425c48a8` in PES vs GF's `k.speed3600`) is not checked.
- `RateToSpeedFor`, `movestep_run.cpp:341`: multiplies by G. PES `FUN_1406B9AA0` body is NOT decoded (the bridge does not resolve `0x1406B9AA0`; my grep of `closure/pes_fn_1406B9AA0.cpp` shows only the 0.15 floor, the body was not read in full). **OPEN.**
- `pestrapactor.cpp:236` writes B+0xC0 from `pes::PhysiqueG(physique, {f.heightCm, f.legSlider, false})`. The same G, through `TrapFactsOf` (`teamAIcontroller.cpp:3525`). Same input defects as section 5.

## 3. The shoot-out keeper: what PES decides, and what GF decides

PES (from `docs/re/pes-re-penalty-keeper.md`; not re-decompiled this session):
- Gate `FUN_1409D5860` (penalty: set-piece phase 3 with kind 6, or ball kind 6); state machine `FUN_1409E5600`; dive target `FUN_1409E5E10` (first ball path record within 3.5 m, only along the goal axis; else stay). Dive timing gate (state 0) uses the shot record `FUN_140A3E940(state,0x11)[2]` and the taker's animation fields: **OPEN** (doc §7).
- Claim/reach test `FUN_1405912D0` (`docs/re/pes-re-goalkeeper.md` §11.3, OBS, reduced): arms in gs frames and 10/5/2/4/1/0.5/3/1.5 m. Its penalty/shoot-out force-0 rule is ambiguous in the doc (the return convention is stated 1 = do not claim); **OPEN**.

GF (`f8aa8e41`):
- The save **attempt** is gated by `Player::KeeperAttemptsSave` (`player.cpp:128-180`), called from `playercontroller.cpp:240`. Its reach and speed are `GameplayTuning::GetKeeperDiveReach_m` = `0.9 + 1.5·coverage` and `GetKeeperDiveSpeed_ms` = `3.5 + 2.0·coverage` (`gameplaytuning.hpp:584-595`). These are GF stand-ins, calibrated to "shots a viewer expects", not PES. **So GF's save-attempt gate is not PES-sourced, and it does not depend on G.** Whether the shoot-out path reaches `playercontroller.cpp:240` is OPEN.
- The shoot-out's G dependence comes through PES movestep: playback (`StepPlayback`, `TurnPlayback`) and the hit distance (`ClipHitDistance`) on every issued clip, and `SpeedToRate`/`RateToSpeedFor` on speed. The test flips when only G changes (round 6, pes-port-PassArrival §13.1), which is measured.

Mechanism at PES's G (INF, not measured):
- At G = 1.0 (180 cm) against the pre-fix floor 0.9166: the step playback is 1.02496 against 1.10000, so the clip plays about 7.3% slower and its hit time is about 7.3% later. The hit distance is about 9.1% longer (×1/0.9166).
- Which of the two flips the save (0 saves against 1) is not decoded. A single-variable run would settle it (section 8).

## 4. PES facts for the test (CONFIRMED where marked)

1. G(180 cm, neutral leg slider) = **1.0** exactly (anchor `0x142598580` = 180.0; the clamp `0x1425DDF78..80` contains 1.0). CONFIRMED.
2. Step playback at that G: `0.95 + 0.15·t`, `t = (1.0833 − 1.0)/0.1667` = 0.49970 ⇒ **1.0249550** (`consts.py`/`g_table.py`). CONFIRMED (decompile `0x1406BA070`).
3. The pre-fix floor: G = `0x1425DDF78` = 0.9166 for any height ≤ 165.6 cm; step playback 1.1000. So the old `> 0 saves` run had **every keeper on the clamp floor**. CONFIRMED.
4. Hit distance scales by G exactly: `hit(G) = G · |root(index)|` (`FUN_1406E7730`). CONFIRMED.
5. The dive's **reach at the kick's arrival frame** for a given placement: NOT computable from the tree. It needs the keeper's dive motion id and its root and hit records (the motion table), the kick's arrival tick (the shot record `FUN_140A3E940`, the ball path), and the keeper's speed at G (`RateToSpeedFor`, body open). OPEN.

## 5. Verdict on the three G readers, and the real mismatch

- `StepPlayback`, `TurnPlayback`, `ClipHitDistance` compute PES's formulas at the same G: **no mismatch at their file:line** (section 2).
- The mismatch is at G's **input** (height), not at the readers:
  - `pesmovestep.cpp:781` (`PhysiqueHeightCmOf`, `f8aa8e41`): `round(GetStat("physical_height") * 100)`. GF's `physical_height` is a development-curve rating (1.8 m reads about 0.6; `src/sim/utils.cpp:525`), so a 180 cm man gives 60 cm, and G = 0.9166 (the floor). PES's height is cm (`docs/re/pes-re-physique.md` §1.1; the roster byte 0x31 = the EDIT Height, `pes-port-DribbleProposal7` §1).
  - Same wrong input at `pestrapactor.cpp:233/236` (`P+0x3B18` scale and B+0xC0), `elizacontroller.cpp:2610/2905/3151`, and the StepHost at `pesmovestep.cpp:303-305`.
  - Fixed in the unlanded patch (`PhysiqueHeightCmOf(match, player)`: PES record height, else `GetHeight()·100`).
- Stand-ins still in the chain (not the test's failure, but G-relevant):
  - `legSlider = 0` (`pesmovestep.cpp:300-302`, `teamAIcontroller.cpp:3525`, ponytail). PES's slider is nibble 0x11:0 → `2n/14 − 1`, so 0 is exact only for nibble 7 (the neutral value; most saved men are all-7, `pes-re-physique.md` §1.2).
  - `unscaled = false` (the block's disable byte +0x40, `physique.hpp`). No GF source.

## 6. The assertion to write (replaces `EXPECT_GT(saves, 0)`)

The save count has no PES source, so **no save-count assertion can be PES-sourced yet**. The PES-sourced assertion is the keeper's body G and the playback it issues at that G. Both come from install constants, not from a number table:

```cpp
// PES: a 180 cm man with a neutral leg slider has body scale 1.0 (FUN_141EB5810; the anchor 0x142598580 = 180 is the deviation's
// zero; 1.0 lies in the clamp 0x1425DDF78..0x1425DDF80). The keeper's step playback is FUN_1406BA070(G) with the install's ramp.
auto exe = pes::PeImage::Open(pesRoot);  // ILLUSTRATIVE: check src/pes/exe.hpp for the real open call; the test already has PES21_ROOT / PES_EXE
const auto phys = pes::LoadPhysiqueConstants(exe, &err);
const auto play = pes::LoadPlaybackConstants(exe, &err);
Player* keeper = /* the shoot-out keeper (the test's GK of the penalty side) */;
EXPECT_NEAR(pesmovestep::PhysiqueHeightCmOf(*match, *keeper), 180.0f, 0.5f);     // his height, cm (fixture is 1.8 m)
EXPECT_FLOAT_EQ(pes::PhysiqueG(*phys, {180.0f, 0.0f, false}), 1.0f);            // the PES scale of that height
const pes::MoveReq req = pesmovestep::TickRequest(*keeper, *match);              // only if his issued kind is a step (VERIFY)
EXPECT_NEAR(req.F(pes::moveword::kPlayback), pes::StepPlayback(1.0f, *play), 1e-5f);
```

Caveats for the writer (not verified here):
- The last line holds only if the keeper's issued kind is a step. That is an assumption (the keeper's clip kind is not decoded). Check the request kind before keeping it.
- The `Player* keeper` lookup must be the fixture's GK, not a field man.
- The current height fix (r5 patch) must land in the same change. Without it, line 1 fails by design.

Do NOT land this with the save-count removed silently. Options for Main (owner call): (a) keep the test and mark it `DISABLED_` with the reason "no PES save rate; dive reach at arrival not decoded" (section 8), adding the G assertions as a new non-disabled test; (b) delete the save expectation. The rule against narrowing still applies: the owner must approve.

## 7. PES's real penalty save rate

None in the tree. Grep over `docs/re` and `docs/ports` for save rate, penalty save, shootout save finds no PES figure. The only save-rate number is GF's own: `elizacontroller.cpp:~748-753` comment "save rate 25%" (a GF measurement against GF's catch rules, not PES). `docs/re/pes-re-penalty-keeper.md` §6 gives the verdict rule, not a rate. No shoot-out keeper capture exists in the rendezvous docs I read.

## 8. Open items

1. **Mechanism of the 0-vs-1 flip.** One sim, single-variable: (a) fix only; (b) fix with the step playback forced to floor; (c) fix with the hit distance forced to floor. Needs a build and a sim slot (not run here). Tells which of playback and hit distance flips the save.
2. **Dive reach at arrival.** Needs: the keeper's dive motion id, its root/hit records (`MotionRecord`, install), the arrival tick (`FUN_140A3E940(state,0x11)`), and the speed at G (`FUN_1406B9AA0`, body not decoded). One hook suggestion for Main, not made here: `0x1409E5600` (penalty state machine) with `--when 'd(q(q(0x1436F3DC8)+0x270)+0x1554)==6'`, every call, cap 12, to capture p1[8] and the shot record. Not added to `hooks.cfg`.
3. **`RateToSpeedFor` body** (`FUN_1406B9AA0`): not decoded; the bridge does not resolve the address. Use `dec.sh` or the closure translation.
4. **`KeeperAttemptsSave` on the shoot-out path.** Does the penalty keeper reach `playercontroller.cpp:240`? If yes, GF's stand-in reach (`gameplaytuning.hpp:584-595`) decides the attempt, not PES. One probe.
5. **Leg slider and disable flag** stand-ins (section 5).
6. **`movestep.hpp:356`** stale VA (0x142588210 → 0x142588200).
7. **Penalty keeper timing gate** (`pes-re-penalty-keeper.md` §3, state 0): OPEN, unchanged.

Finding outside this ticket (for Main): `playercontroller.cpp:240` and `Player::KeeperAttemptsSave` are a GF stat model that gates the whole keeper save attempt in play, not PES. The PES claim test is `FUN_1405912D0`. That is a larger gap than the G input.
