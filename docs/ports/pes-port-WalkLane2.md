# PES port: WalkLane2 (branch WalkLane, tip 7bac0dfd, nothing landed)

## 0. Verdict
Nothing landed. WalkLane stays at `7bac0dfd` (its tree is restored to that commit; no commit, no push). Neither the action ids nor the walker anchor reaches ALiveBall <= 2229 on the tree I measured. The ids regression is localised to two causes (sections 2 and 3), and neither has a PES port yet. Tasks 3 (strike root) and PesPassTarget.EveryPassGetEpisodeIsWithinPesLiveRange were not started (budget).

## 1. Measured ALiveBall (`PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker`, `pes_restart_delay_test`, seed 7, `GF_STALL_DUMP=1`)
The number is `census.liveFirings`: every breaker firing in play, including the series-freeze ones. Bound 2229.

| tree | liveFirings | note |
|---|---|---|
| 82ad8564 (Integrate base, dump run, passes) | 2,217 | counted from the dump lines |
| 2f8fcca3 ids, unchanged (Main's failing run, reproduced) | 4,965 | `alive_dump_ids.log` |
| walker patch alone over 82ad8564 (WalkLane1) | 2,375 | WalkLane1 section 7 |
| walker + ids (full, 2f8fcca3 rows) | 2,581 | `alive_dump_walker.log` |
| walker + ShortPass 9 only (variant B) | 2,375 | `alive_dump_vB.log` |
| walker + ShortPass 9, LongPass 8, Shot 0xA (variant C) | 2,375 | `alive_dump_vC.log` |
| walker + Sliding 0xB, Trip/Interfere 0xD, Catch/Deflect (variant D) | 2,581 | `alive_dump_vD.log` |
| walker + ids minus HighPass/Header/Trip/Interfere (MOVE stand-in, variant A) | 3,200 | `alive_dump_vA.log` |
| walker minus trap-clip anchor, full ids | 4,767 | `alive_dump_w2full.log`; the trap exemption is worse |

Variants B and C (walker with ShortPass, or ShortPass+LongPass+Shot) both give 2,375, the same as WalkLane1's walker-only number, so the walker alone sits at 2,375 and the ids add 206 on top of it.

## 2. Action ids: what the 4,965 is made of (CONFIRMED by the dump, 4,954 dump lines of the ids run)
- The largest term is ONE dribbler frozen 26 s: A4 (man 19, PES dribbler, `ac8 = 4`, holder), clip `walk_1_1` on its last frame (120/121), command speed 4.44 m/s, body at 0 m/s, MOVESTEP motion 3633 issued but never played. 2,588 firings at t=403203 ms on. The freeze is the GF series gap: PES's dribble series (3633, then 3005, 3011, ...) needs the next member issued when a member ends; GF plays only the first member (`PlayDribbleIssue`, teamAIcontroller.cpp:3532). INF on the mechanism; CONFIRMED on the freeze.
- With the walker in place this freeze shrinks to 304 firings, but the dribbler freezes (`ac8 = 4` owned) still rise from 186 (base) to 507 (walker + ids). Not diagnosed beyond the series gap.
- Sliding (0xB): ids-only gives 61 forced firings (`sliding/idle/000.anim`, `ac8 11`), walker+sliding 134-144 firings. The same clip under MOVE gave 224 (base). So the sliding id itself is not the regression, but the walker plus the sliding id held the idle clip for 609 ticks in the W2 run (4,767), i.e. the clip-start anchor and the sliding action interact. INF on the cause: the SLIDING action has no end in GF's clip (the idle loops), where PES's SLIDING action tick ends the slide (not translated).
- Trip/Interfere to TACKLE (0xD) is INF in TrapAdmit2. Making them MOVE is worse (variant A: 3,200, with 217 `trip_t1 fn=12 ac8=2` stalls). So the TACKLE mapping is the less wrong one; keep it until a PES site is found.
- The kick-id rows that were refused the trap gate (WalkLane1's guess) are NOT the cause: the forced men at the top of the ids run are MOVE, TRAP or dribblers. No forced man in ShortPass/LongPass/Shot action appears in the dump (variants B and C are neutral, 2,375 each).

## 3. Walker clip-start anchor (task 2): not landed
- The patch (`walker-clipstart.patch`, `PlayIssuedClip`) is GREEN on `pes_setpiece_stand_test` (12/12 with the patch, PES_EXE and PES21_ROOT set) and alone gives 2,375 over 82ad8564. It costs +158 ticks over the bound.
- PES rule for the composition (CONFIRMED in docs/re/pes-re-anim-blend.md section 7.1, from the decompile of `AnimMovePlugin::apply` 0x141AFD400): `translation += R(model)·(scale·rootDelta)` per frame, so PES adds per-frame root deltas to the body's own position. The patch's anchor (`position - positions[startFrame]`) is the delta form for a clip that starts at its start frame: the body stands still at the start and moves by the clip's later root. That part is the PES rule and I keep it.
- The same section (override `+0x81 -> lock translation to +0x90`) says PES locks the translation while the strike is armed. GF adds the clip's own track on top of the adjuster (WarpRoot, pesmovestep.cpp:727; PassArrival section 10 measured 0.8 m overshoot). That is the part GF is missing, and it is task 3's root, not the anchor.
- Trap-only exemption (keep the old anchor for `kTrap`): 4,767 with ids, so it is not the answer. Reverted.

## 4. Verified on the tree (for the record)
- `pes_receive_test` 60/60 with `PES21_ROOT` (on the ids build; 59 + 1 skip without the env).
- `pes_setpiece_stand_test` 12/12 with the walker patch.
- NOT run: `PesStallQuick`, `pes_throwin_release_test` ankle and carried tests, `PesAnimeAction` on the final tree, `gen_translated.py --check`. No landing, so no gate.

## 5. Open items (next steps, in order)
1. Decode PES's SLIDING (0xB) action end and the dribble series advance (the freeze). Without these, the ids cannot land, and the 2,000+ forced firings are not explained.
2. Port the strike translation lock (AnimMovePlugin `+0x81`) for trap clips (task 3). Then re-measure the walker anchor for trap and sliding families.
3. Task 3 probes (`A+0x14`, `GetPosition`, `positions[frame]` per tick) and FUN_14092ED30 (PesPassTarget.EveryPassGetEpisodeIsWithinPesLiveRange): not started.
4. Trip/Interfere (0xD) and Header/HighPass: find the PES site, or keep the stand-in and say so.

## 6. Artifacts
- Dumps and logs: `<workdrive>/gf-WalkLane-scratch/ids/alive_dump_*.log` (base, ids, walker, vA, vB, vC, vD, w2full). Binaries removed.
- Round notes: `<workdrive>/gf-WalkLane-scratch/report-round2.md` (WalkLane1 round 2).
