# PES team-tick Slice A port (TeamSliceA)

Status: DONE on `TeamSliceA`, pushed. Base `origin/RoleArrays` (`8cbf2a1`).
Commits: `e298441` (T-tick core), `62add76` (B8A3 gate), `24621ce` (PES-path
wiring). `origin/TeamSliceA` = `24621ce` (confirmed by `ls-remote`).

Summary: PES's per-tick team words — the attack level `T+0xB8A4` drift
(`FUN_1404F8350`), the full-attack/full-defence flags `T+0xB8A2/B8A3`
(inline at `0x1404F6E1C-58`), and the restart word `T+0x80 = 0xFF`
(`0x1404F6F91`) — now step every team tick on the PES path as a small
`pes::TeamTick` struct, and GF's level readers (defensive shape, set pieces,
concepts, run cap) read it there. The `attackLevel = 2` pin and the
`UpdateStrategy` in-play copy half are gone on the PES path. GF has the
`FUN_1405F9EB0` line-depth step, so the B8A3-gated `FUN_1405EF910` skip is
wired (`0x1405FA423`).

Spec: `pes-re-team-layer.md` §1 step table (steps 13/18/28), §2
V1/V2, §3 Slice A; `pes-re-team-layer2.md` item 4 (licence counts,
context only — no licence code in this slice); mentality chain
`docs/re/pes-re-community-qa.md` F7 (as corrected by team-layer §4),
`pes-re-tactics.md` §3.
Worktree `<workdrive>/gf-TeamSliceA`. Scratch `<workdrive>/gf-TeamSliceA-scratch`
(bridge dec/dis of `FUN_1404F6C30`, `FUN_1404F8350`, `FUN_1404FE9C0`,
`FUN_14051E910`).

## 1. What PES does (re-verified this pass, bridge dec + raw dis)

- Step 12 (`0x1404F6DBD`): `T+0x134 = brain+0xBCC` after `FUN_14051E910`
  (dec `fun_1404f6c30.c:199-201`; `e910` runs the decider only when
  `MI+0x116C ∈ {1,3}`). CONFIRMED.
- Step 13 (`0x1404F6DD8`): `FUN_1404F8350` drifts `T+0xB8A4` (dec
  `fun_1404f8350.c`, dis `dis_1404f8350.txt`). Non-pad branch: `want =
  {0:0,1:1,3:3,4:4}.get(T+0x134, 2)` (ladder `0x1404F8619-63E`);
  `t = T+0x210` replaces want when `t != 2`, `t != L` (`0x1404F8663`)
  and `(t<2 and t<=want<3)` or `(t>2 and 2<=want<=t)`
  (`0x1404F8640-6C8`); the crossing case past `0x1404F8675` (`t != L`,
  off in-play, helpers `FUN_140A606C0`, `FUN_140A3FE50`, `MI+0x116C==2`)
  is OPEN; `want != L and MI+0x116C != 1` freezes
  (`0x1404F86C8-6D4`, `CMOVNZ EBX,EBP` keeps OLD); store `min(want,4)`
  (`0x1404F86DE-E7`, no low clamp); stamp `T+0xB8C0 = MI+0x1198` on
  change (`0x1404F86ED-70C`). CONFIRMED raw dis.
- Step 18 (`0x1404F6E1C-58`): `B8A2 = (B8A4>3)`, `B8A3 = (B8A4<1)`
  (dec lines 215-221). CONFIRMED.
- Step 28 (`0x1404F6F91`): if `MI+0x116C != 1` or the latch return is
  nonzero, `T+0x80 = 0xFF` (dec lines 260-261); `T+0x6E6 = 0` only when
  `FUN_140A3E190(...) != 5` (dec 262-266, predicate OPEN, not ported).
  The latch `FUN_1404FE9C0` returns `old255 ^ new255`: a possession
  FLIP (dec tail `return cVar7 != '\0' ^ bVar19`). CONFIRMED dec.
- V2 reader `FUN_1405F9EB0` at `0x1405FA423`: `CMP [R15+0xB8A3],0; JNZ
  skip; CALL FUN_1405EF910` — level 0 skips one line adjust. CONFIRMED
  raw dis (team-layer §2 V2; R15=T INF(high)).

## 2. The GF defect

- `UpdateStrategy` copied the strategy map onto `instructions.mentality`
  in play, but nothing froze dead-ball, nothing applied the `T+0x210`
  override, nothing stored the flags.
- `CalculateDefensiveShape:392` pinned `attackLevel = 2`.
- `PrepareSetPiece:1072` read mentality via the identity `pes::AttackLevel`.
- `DefensiveLineHeight` applied the ef910 step-up unconditionally.

## 3. Port shape (PES path only, `match->PesLoop()`)

- `src/sim/onthepitch/pesteamtick.{hpp,cpp}` (new): `struct TeamTick
  { b8a4, b8a2, b8a3, t80, b8c0Frame }`, `StrategyWantLevel`,
  `ManualOverrideLevel`, `DriftAttackLevel`, `WriteAttackFlags`,
  `WriteRestartWord`.
- `TeamAIController` owns one `teamTick` (+ `lastTickPossession` for the
  flip test) and steps drift + flags + restart word at the head of
  `UpdateStrategy` on the PES path; the in-play mentality copy is
  off-path only. `GetTeamTick()` exposes it. T+0x210 stand-in: balanced
  (2), ponytail naming tactics §3 O4.
- Cutover on the PES path: `CalculateDefensiveShape` reads
  `teamTick.b8a4` (pin deleted) and passes `teamTick.b8a3` to
  `DefensiveLineHeight`; `PrepareSetPiece`, `DefenceInput`
  (`attackLevelZero`), the run-cap level read the tick. Off-path lines
  unchanged.
- `DefensiveLineHeight` takes `fullDefence` (default false): B8A3 skips
  the step-up. All 23 old call sites keep the default; no test touched
  except the new one.
- NOT wired (forbidden files): the carrier `fullAttack` feeds in
  `elizacontroller.cpp:2222,2234` still pin false with their ponytails;
  agent A4Lanes3 owns that file. `pes::AttackLevel` identity kept: the
  off-path setpiece line still uses it.

## 4. Tests (all red first on base)

- New `tests/sim/pes_team_tick_test.cpp` (8/8): `Level3SetsFullAttack`,
  `Level0SetsFullDefence`, `RestartWordSetWhileDeadBall`,
  `RestartWordSetOnPossessionFlip`, `RestartWordKeptInPlay`,
  `LevelFrozenWhileDeadBall`, `StrategyMapCoversTheRest`,
  `ManualOverridePullsLevel`. Red: missing header; the override case
  `ManualOverrideLevel(2,1,level=1) == 2` (the `0x1404F8663` t!=L check)
  was added while red after re-reading the asm.
- `DefensiveLine.FullDefenceFlagSkipsTheStepUp` (new in
  `pes_defensive_shape_test.cpp`): red (no parameter), green; pins the
  step-up 4 m bound vs the no-step-up line. Suite 26/26 (22 line +
  install/match, 3 match skipped without rig... actually 26 passed).
- `SimMatch.PesLoopStepsTheTeamTickFromTheStrategy` (new in
  `sim_match_test.cpp`): trailing 0-3 on pes_loop, the tick reaches 3+
  by minute 70 and B8A2 tracks level>=4 after a drift. Red (no
  `GetTeamTick`), green (77 s). Suite 11/11.
- Touched binaries green: pes_team_tick_test 8/8, pes_defensive_shape_test
  26/26, pes_setpiece_team_test 18/18, pes_team_concepts_test 31/31,
  pes_offball_test 65/65, pes_strategy_test 8/8, sim_match_test 11/11.
- Gate not run (Main gates).

## 5. Mutations (one at a time: edit, rebuild, run, revert, rebuild)

- M1: B8A2 `> 3` → `> 4`. `Level3SetsFullAttack` FAILED. Reverted: 8/8.
- M2: restart gate `||` → `&&`. `RestartWordSetWhileDeadBall` +
  `RestartWordSetOnPossessionFlip` FAILED. Reverted: 8/8.
- M3: freeze `!inPlay` → `inPlay`. `LevelFrozenWhileDeadBall` +
  `StrategyMapCoversTheRest` + `ManualOverridePullsLevel` FAILED. Reverted: 8/8.
- M4: step-up gate `!fullDefence` → `fullDefence`.
  `FullDefenceFlagSkipsTheStepUp` + `CushionHoldsTheLineBehindTheBall`
  FAILED. Reverted: 26/26.
- M5: wiring `match->PesLoop()` → `!match->PesLoop()`.
  `PesLoopStepsTheTeamTickFromTheStrategy` FAILED ("level never left 2",
  full 70-min run). Reverted: 11/11.

## 6. Smokes (`--seed 7 --minutes 2`, `assets/fixtures/teams.sql`, rc 0)

- Switch-off: `20f28a3df9b0ce12` — UNCHANGED from the RoleArrays baseline.
  32549 ticks, 20 stoppages, no crash/hang.
- `--pes-loop`: `7b3252ff790975c9` (was `fdb8788a96e87230`), rc 0, 41522
  ticks, 14 stoppages, shots on both sides (no gross breakage). Rerun
  reproduces `7b3252ff790975c9` (deterministic).
- Pes-loop move: expected. The level now drifts off 2 with the strategy
  (was: shape/setpiece pinned at 2-equivalent while the run cap followed
  mentality directly), so line/length/width/setpiece counts move. Not
  isolated by ablation; direction consistent with the cutover.

## 7. Open items

1. `T+0x210` writer not decoded: stand-in balanced (2). The manual arm is
   tested but unreachable until the writer lands.
2. `T+0x6E6 = 0` gate (`FUN_140A3E190(...) != 5`) not decoded, not ported.
3. Dead-ball crossing case past `0x1404F8675` OPEN (extra helpers).
4. Pad branch (`FUN_140A55CF0`, command 0x6c) not ported: gfsim sides are
   CPU-run; the wiring test covers the CPU branch only.
5. Carrier `fullAttack` feeds (elizacontroller.cpp) still pinned false —
   A4Lanes3's file; wire to `GetTeamTick().b8a2` when touching that file.
6. Licence counts (team-layer2 item 4) are context for a later slice; no
   licence code here.
7. Stock-vs-mod ruling (Main, dt18): this slice reads no dt18 constants;
   the two `constexpr int kManualBalanced = 2` / `0xFF` are immediates at
   the cited VAs, unaffected.
