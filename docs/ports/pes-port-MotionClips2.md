# MotionClips2: movestep reads clip ticks only through PES's translated readers; TurnStop's stand-ins replaced; AbilityByte and EntryAimFlag run PES's translated code on the world window (MotionClips2, 03-11-26)

Branch `MotionClips` (worktree `gf-MotionClips`), continuing `pes-port-MotionClips.md`. Continuation by MotionClips3 (04-11-26) after the 20:58 reboot: merged origin/main three times (`028f0feb`, batch BB `7636546b`, `0dd1c2b9`), proved the zero-record fallback arms, ablated the smoke move, re-staged the throw-in seeds, `GATE fast: GREEN`. Pushed heads (confirmed by `git ls-remote origin MotionClips`): `b8d61b91` (MotionClips2), merge `334ff524`, `3a909bbb`, merge `19331082` (batch BB), `67b5ca3d` (results, ledger), `ffd086a0` (seed re-stage), merge **`c7874a3c`** (final). Tags: CONFIRMED = decompile / asm / capture / run this session; INF = inference; OPEN = not settled.

## 0. Verdict

| Item | State |
|---|---|
| 1. merge origin/main, push | DONE twice: `334ff524` (028f0feb) and `19331082` (batch BB 7636546b: per-slot P conflicted in `pesworldrecord.*`; `RunAbilityByte` / `RunEntryAim` now bind `P(slot)` / `PVa(slot)`; `gen_translated.py` SETS keeps both `results_trapaction.json` and `results_motion.json`; layer regenerated, 5,003 functions). `extern/godot-cpp` restored as a symlink after each merge. |
| 2. cutover to the translated readers | DONE for the four tick readers (0x1406AF620, 0x1406AF8E0, 0x1406B1F00, 0x1406B1D00): `MoveHost::ClipTicks`; `RecordTicks`, `FrameTicks`, group A's `MotionTicks` deleted; every caller moved (group A x8, `ReadClipHits`, turn handlers x20). PARTLY for "clip records only through the readers": the RotData offset (0x1406B1C70), the CancelData / FootType counts (0x1406AFBC0) and the FootType pair (0x1406AFE20) are still read from `Mbinfo` through `MoveHost::Motion()`; their translated bodies have 0 proof cases (`results_core.json`), and no hook captures them. Named `ponytail:` on `Motion()`. |
| TurnStop stand-ins | DONE (section 2). |
| 0.85 divisor | DONE in TurnStop and DECELERATE (`movestep_run.cpp:1773`). |
| 3. AbilityByte, EntryAimFlag | DONE (section 3). |
| 4. `req[0x1F] < 3` | CONFIRMED against the asm (section 2). |
| 5. fallback records | CONFIRMED zero by live read AND proven bit-exact for cancel / clip length / tick length (section 4); blend / RotData end / RotData offset record-absent arms: no capture exists in any launch (named in section 4). |
| 6. tests, mutations, gate | Tests and mutations DONE (section 5; the six binaries re-run on the merged tree: 5 / 3 / 56 / 56 / 101 / 115 passed). Smoke ablation DONE (section 7). `GATE fast`: see the final line of section 8. |

## 1. ClipTicks

- `pes::ClipReader` moved to `pes/mbinfo.hpp`; `MoveHost::ClipTicks(reader, motion, rate, index)` (pure virtual) is implemented by the sim adapter through `Match::GetPesClipReaders()` (a `PesClipReaders` built once from `Mbinfo` in the Match constructor, tps from `TicksPerSecond()`). The four test fakes share `tests/clip_ticks_fake.hpp` (the reader contract computed from a decoded record: the readers themselves are proven bit-exact and tested on the install table in `pes_clip_readers_test`).
- Per-call cost: each `Read` builds a `PesState`; not measured (no smoke run).

## 2. TurnStop (0x1406CAB20) CONFIRMED by decompile + asm (`cab20.asm`)

- Divisor: `0x1406CABC1 COMISS XMM0,XMM6` with `XMM0 = [0x1425AB120]` (0.85), `JBE` skips `MOVAPS XMM6,XMM13` (`XMM13 = [0x14256AE1C]` = 1.0): `P+0x2468 < 0.85 ? 1.0 : P+0x2468`. GF used `max(.., 0.85)` in TurnStop and DECELERATE (0x1406BCD20 line 46); both fixed (ACCELERATE already had it).
- Toe-off window (0x1406CABB2..0x1406CACBB): `elapsed = vt+0x38`, `start = 0x1406B1F00(motion, 1.0, 0)`, `end = 0x1406B1D00(...)`; when the P+0x4D0 flag (`FUN_141EB3320` = `*(int*)(obj+8) == 2`) is clear and `start <= elapsed < (start + 2 end) / 3` (`IMUL 0x55555556`): `ticks = int((end - elapsed) / divisor + 0.5)`; `|FUN_14043A0A0(req[2], req[0x18])| / ticks < 45` (`[0x14259BE30]`) writes `req[0x3D] = req[0x18]` (0x1406CACA3), byte `req[0x3C] = 1` (0x1406CACAC), `req[0x3E] = ticks` (0x1406CACB4), byte `req[0x3F] = 0` (0x1406CACBB). Now in GF (`moveword::kTurnLock*`, new names for 0x3C..0x3F).
- The old gate was `AnimeState() != 0`, which in production is true for the MOVE action (the adapter answers 2): the window never ran. The real gate is the adjuster object's state; the object is not ported, so the flag is held clear (named `ponytail:`, as RUN's turn-lock block holds it).
- Part 5: `blend = 0x1406AF620(motion, 1.0)` (0x1406CB523), `cancel0 = 0x1406AF8E0(motion, 1.0, 0)` (0x1406CB541); replay dwell `blend / FUN_141E532D0` (0x1406CB55C); stop due when `req[9] > 1` (0x1406CB570 `CMP [R13+0x24],1; JG`), or `req[0x1F] < 3` (**0x1406CB576 `CMP dword ptr [R13+0x7C],3; JL`: CONFIRMED `< 3`; GF had `> 2`, inverted**), or `(cancel0 + blend) / 2 <= elapsed` (0x1406CB57D..0x1406CB588). The idle exit's `elapsed < blend` return is 0x1406CB8F3 (`CMP R14D,R15D; JL`); GF compared two zero stand-ins. The foot-count read (cab20 line 372) is the FootType count, not a cancel pair.
- IDLE_TURN (0x1406BFE70, found while sharing the shape): `FUN_14043A0A0(req[2], req[0x18])` (0x1406BFF72) takes the facing against the stamp; GF passed the clip-tick sum `start + 2 end` as the angle. Fixed.
- Stand-in sites found, NOT changed (state-changing without evidence to port): TurnStop's `distanceFactor` (GF lines 1451-1453) is not PES's `fVar24` (lines 91-99: `DAT_142565604`, `DAT_14259222C`, `_DAT_14258C264`), and the replay gate uses `k.s0p6` where PES multiplies by that `fVar24` (0x1406CB4FE `MULSS XMM1,XMM7`); the replay gate (0x1406CB4B0..0x1406CB4D6: `MI+0x116C == 2 && MI+0x1554 != 1 && MI+0x11D4 == 0`, or vt+0x28 == 0x4A) stays `MatchPhase() == 2 && IsReplay()` (the host's `MatchState1554/11D4` are zero stand-ins, so porting it would turn every stoppage into "replay"); `req.SetU(kDuration, 0.2f)` / `SetU(kDwellLimit2, 0.2f)` in the idle exit write 0 (a float into `SetU`; PES stores 0x3E4CCCCD at req[0x32] / req[0x2E], 0x1406CB9xx).

## 3. AbilityByte and EntryAimFlag

- `FUN_1406E6660(P, id)`: `rec = *(G + (slot + 0xF3) * 0x28)`, byte `rec + 0x70 + id`, `slot = (s8)P[0x2458]`. The `+0xAB8` interface is the vtable at **0x1425E3508** (found by scanning the image for the slot targets): slot 0 = `0x1406EAAF0` (`*(rcx + 0x3008)` = P+0x3AC0), slot 1 = `0x1406EA810` (`(s8)*(rcx + 0x19A0)` = P+0x2458: the "action id" `EntryAim` tests `< 0x16` is the slot). P+0x3AC0 is the side's context (its +0xE0 is the event store).
- `PesWorld::RunAbilityByte(slot, id)` and `RunEntryAim(slot, side, playState)` run `pes_fn_1406E6660` / `pes_fn_1406BA7D0` on the window with P's three fields bound (`P+0x2458`, `P+0xAB8` = the image vtable, `P+0x3AC0` = `CtxVa(side)`); the vtable (0x90 bytes) joins `PesImageTables`. MI+0x116C is not held in the window outside the PES loop, so `RunEntryAim` takes the adapter's `PlayStateFor` value and restores the period record after the call (named in the header). `MoveHost::AbilityByte(id)` / `EntryAimFlag()` replace the free stand-ins (`50`, `false`) in `movestep_turn.cpp`; the adapter passes `PesSlotOf(player)` and `GetTeamID()`.
- The ability bytes: ids 0x29 (Balance) is the only id the movestep reads through the reader. The window holds it only for a player whose roster row GF loaded (`LoadRosterRow`); `WriteSlotAbilities` (`elizacontroller.cpp:1046`) stores Speed/Jump/Height/foot from GF's stats otherwise, so for a synthetic player id 0x29 reads 0 (was 50). OPEN: GF's stat-to-byte for Balance (`physical_balance`?) for rowless players; `elizacontroller.cpp:1051` converts a stat as `stat * 100` over 40..99, consistent with the block's bytes.
- **Mis-mapped read fixed:** the `0x5b3 / 0x5b9 -> 0x5b6` override site called `AbilityByte(host, 0)` ("FUN_140a58550's ability byte"). PES's call there is `FUN_140A58550` (the stamina percentage; 0x1406D74FB, 0x1406D7811 `CALL 0x140A58550`, `CMP DIL,0x14` at 0x1406D783F), not `FUN_1406E6660` (callers in `FUN_1406D6780` are 0x1406D7BD5 and 0x1406D8249, both id 0x29). It now reads `host.StaminaPercent()`: a tank under 20 % takes the tired row, which the old stand-in (50) never did. OPEN: the PES flow at 0x1406D7843 `JNC 0x1406D7B72` jumps to the 0xBE5 setup for >= 20 %, so the GF ladder shape around the override may differ from PES; not decoded here.
- OPEN, not changed: PES `0x1406D7BE2` subtracts 0x14 from the 0x29 byte only when the byte `[RBP+0x98]` is set (a local written at 0x1406D68FC), GF subtracts 0x14 always and 0x14 again when `relax` is clear; the local is not the `EntryAim` result (that is stored elsewhere).

## 4. Fallback records (item 5)

CONFIRMED by live full captures of `live2/pid1037286` and `pid23358` (mem.bin read at the mapping offsets). The `.data` fallback objects each reader loads when the count byte is `<=` the index or the list pointer is null (the `LEA RDX,[rip+..]` operands, read by script from the image's decode): blend `0x1406AF656` -> `0x1436F2430`, cancel `0x1406AF924` -> `0x1436F2500`, RotData end `0x1406B1D44` and RotData offset `0x1406B1F44` -> `0x1436F23E8`, clip length `0x1406B1114` and tick length `0x1406B1224` -> `0x1436F23D8`. All four objects are 16 zero bytes in `pid1037286` seq798, seq861, seq4990 and `pid23358` seq024. (The MotionClips2 draft listed `0x1436F2430 / 0x1436F2500 / 0x1436F23E8` and missed `0x1436F23D8`, the kind-4 object; GF has no kind-4 consumer, so `PesClipReaders` is unaffected.)

**Proof.** `frame_trace.py recorded` over every capture, then `snapdiff.py --bounded` (scratch `tr2.sh`, `sd2.sh`, `more.sh`, `patch_results.py`; the six roots, the old `pid296049` captures plus every later launch's clip-reader captures: `pid1037286`, `pid23358`, `pid42407`, `pid358243`, `pid802552`; each linked under a unique `<pid>_<seq>` label because seq numbers repeat across launches; the five packed ones were unpacked and packed again): **`0x1406AF8E0` cancel 21/21 bit-exact (rax, xmm0, every written byte), `0x1406B10D0` clip length 17/17, `0x1406B11E0` tick length 17/17**, 0 mismatches, 0 excluded (the case lists are in each `results_motion.json` note). The new captures take the fallback block (`0x1406AF924` / `0x1406B1114` / `0x1406B1224`; cancel's own-block coverage 8/13 -> 9/13; clip/tick length were already 9/13 from the older seq022 whose count was `<=` the index). `results_motion.json` records refreshed (calls, coverage, note); closure files unchanged (same 2 members); `gen_translated.py --check` rc 0, byte-identical on the merged tree. Ledger regenerated (the merge had left `docs/re/LEDGER.md` stale: 2 motion closure members).

**Not proven (named missing captures):** the record-absent arms of **`0x1406AF620` blend, `0x1406B1D00` RotData end, `0x1406B1F00` RotData offset**: no `mot_blendrec_AF620`, `mot_rotend_6B1D00` or `mot_rotoff_6B1F00` capture exists in any launch after `pid296049` (`pid1037286`, `pid23358`, `pid42407`, `pid358243`, `pid790918`, `pid802552`). The record-absent predicates (`motion >= 0x1125 or count byte == 0 or list pointer == 0`, read from `run/watch_current.sh` as rvwatch52 had them) are armed for all six readers; cancel, clip length and tick length fired, these three did not. INF: live play may never reach these arms (the three readers' callers may only run on motions that carry the record), so no capture may ever arrive; OPEN until one does. Their fallback objects are zero (CONFIRMED above) and the arms are the same instructions as the proven cancel / length arms, but the arms of these three stay INF. Still uncovered in all six readers: the `motion >= 0x1125` arm (`table[1]`; the predicate includes it, no capture took it) and the `rate <= 0` / frame-rate `<= 0` arms (4 blocks of 13).

## 5. Tests and mutations (all run, rebuilt, reverted; `git diff` empty after)

Red first: `c66616b0` (cutover + 8 TurnStop tests failing on the stand-ins), then the rewrite turned them green; the Decelerate, IdleTurn, tired-tank and P-object tests are new.

- `pes_movestep_step_test` 56: `TurnStopToeOff` x5 (writes the words from the record window; start-inclusive / third-exclusive bounds; 45 deg/tick limit; divisor at 0.5 / 0.849 / 0.85 / 1 / 2; the MOVE action does not close the window), `TurnStopStop` x5 (midpoint, class `< 3`, `req[9] > 1`, replay dwell = blend/tps, idle exit waits for the blend tick).
- `pes_movestep_run_test` 101: Decelerate divisor. `pes_movestep_test` 115: IdleTurn angle is facing against stamp. `pes_movestep_turn_test` 56: tired tank (19 / 20 / 100 %). `pes_player_object_test` 5 (new): ability byte equals the window byte for slots 0, 5, 21 and ids 0, 0x14, 0x27, 0x29, 0x3B; two slots do not alias; the entry-aim flag follows a recent kind 0xA / 0xD event, clears out of play, with an old event, with a kind-1 event, and leaves the period record alone. `pes_clip_readers_test` 3.
- Mutations killed (each by the named test): divisor `max(.., 0.85)` (TurnStop divisor test); window `start < elapsed` (bounds); window `elapsed <= hi` (bounds); `adjusterBusy = AnimeState() != 0` (MOVE action); class `> 2` (StopsOnce, ClassWordUnderThree); replay dwell on cancel0 (replay dwell); idle exit guard removed (idle exit waits); `speedClass > 2` (ACommandClassAboveOne); midpoint `<` (StopsOnce); Decelerate `max(.., 0.85)` (Decelerate divisor); IdleTurn angle = tick sum (IdleTurn test); stamina `<= 0x14` (tired tank).
- NOT mutated: `RunAbilityByte` / `RunEntryAim` binding (slot, vtable, P+0x3AC0) and the adapter wiring.

## 6. Not run / left (numbered)

1. Smoke: DONE (section 7).
2. `snapdiff --bounded` over the new zero-record captures: DONE for cancel / clip length / tick length; blend, RotData end, RotData offset wait for captures (section 4).
3. RotData offset / counts / FootType pair through translated readers: need hooks on 0x1406B1C70, 0x1406AFBC0, 0x1406AFE20 (tell Main), then add them to `PesClipReaders`.
4. The P+0x4D0 adjuster flag, the replay gate's `MatchState1554/11D4`, TurnStop's `fVar24`, the two `SetU(.., 0.2f)` writes, the PES 0x14 subtraction of the 0x29 byte (section 2-3).
5. Balance stat for rowless players in `WriteSlotAbilities`.
6. Per-call cost of `PesClipReaders::Read` (a `PesState` per call) unmeasured.
7. Cleanup: `<workdrive>/gf-MotionClips-scratch`, `build/`, worktree `<workdrive>/gf-MotionClips`.

## 7. Smoke and ablation (CONFIRMED by runs; seed 7, 2 min, `gfsim_run`, every run through `simslot.sh`)

Baselines on main after batch BB (Main's note, `7636546b`): switch-off `399e180f39459c2b`, pes-loop `38549be3476430fd` (31,196 ticks to full time).

| Tree | switch-off | pes-loop | ticks |
|---|---|---|---|
| merged (`19331082`) | `399e180f39459c2b` (30,213 ticks): **unmoved** | `85c7d7c515cf92ae` | 30,080: **moved** |
| merged, TurnStop commit `6ff18eb9` reverted | - | `38549be3476430fd` | 31,196 = main |
| merged, all of `6ff18eb9` + `bd2f3c19` + `b8d61b91` reverted (cutover `c66616b0` only) | - | `38549be3476430fd` | 31,196 = main |
| merged, DECELERATE divisor `bd2f3c19` reverted | - | `85c7d7c515cf92ae` | 30,080 = merged |
| merged, AbilityByte / EntryAim / tired tank `b8d61b91` reverted | - | `85c7d7c515cf92ae` | 30,080 = merged |

Before batch BB (pre-merge ablation of the pushed commits, build of `f56a25d9` = main's pes-loop `0e480b158cf80bd4`, 43,824 ticks) the first smoke of this branch on the merged tree of that time was `bb5490362a061f02` (28,315 ticks), switch-off `3b73c968b4a377e0` unmoved; the batch BB re-run above supersedes it.

Attribution: the move is TurnStop (`6ff18eb9`) alone; the cutover to the translated readers (`c66616b0`), DECELERATE's divisor, AbilityByte and EntryAim are inert on this seed. Within `6ff18eb9`, one term turned back at a time to its old stand-in on the merged tree (a scratch `ABL` env switch in the ablation worktree, not committed):

| Term (PES site) | Old stand-in | pes-loop hash | ticks | Moves? |
|---|---|---|---|---|
| stop test class word `req[0x1F] < 3` (0x1406CB576) | `> 2` | `847a21b775c83354` | 35,428 | yes |
| stop test midpoint `(cancel0 + blend) / 2 <= elapsed` (0x1406CB57D..0x1406CB588) | always true (`0 <= int(divisor)` with zero frames) | `334ab24bd733cd87` | 50,611 | yes |
| idle exit `elapsed < blend` returns (0x1406CB8F3) | never returned (`0 < 0`) | `21211834bde0acbd` | 28,742 | yes |
| toe-off window writes `req[0x3C..0x3F]` (0x1406CABB2..0x1406CACBB) | window never ran | `85c7d7c515cf92ae` | 30,080 | no |
| replay dwell `blend / tps` (0x1406CB55C) | `0` | `85c7d7c515cf92ae` | 30,080 | no |
| 0.85 divisor floor -> 1.0 (0x1406CABC1) | `max(.., 0.85)` | `85c7d7c515cf92ae` | 30,080 | no |

So three terms each move the run (the PES stop test no longer fires at every tick, and the idle exit waits for the blend tick); each is a PES-cited fix, so the move is PES's behaviour entering GF, not a regression. Tripwire per the 17-10-26 smoke policy: no crash, no hang, deterministic (each hash re-printed by an identical rerun of the same binary is not claimed: one run per cell), non-zero passes and shots on both paths (merged pes-loop: passes 16-33, shots 3-0, goals 2-1; switch-off passes 12-16, shots 8-3). NOT claimed: any balance number, the three inert terms on other seeds, a 20-minute run.

## 8. Gate, census, re-staged seeds

**First `GATE fast` on the batch-BB merge (`19331082` + results commit `67b5ca3d`, `GATE_SLOTS="1 0"`, full gate env, Release `build`):** ctest 2342 tests, 4 failed, all `PesThrowInRelease.*` seed lists (`AThrowInIsReleasedFromInsideTheLine`, `ACarriedThrowInIsHeldAtItsRowsKeyOffsetUntilTheTouch`, `TheLiftTurnsACarriedThrowInTakerToFaceThePitch`: 1800 s timeouts searching four matches for a carried throw-in; `ACeremonyThrowInIsReleasedFromTheHandOverTheSpot`: `EXPECT_GT(ceremony, 0)` 0 vs 0 after three matches). `GATE fast: RED` on those four; every other test passed, the smokes and the ledger were green (`LEDGER.md current`, off-path ports 125 all allow-listed, no unrecorded status regression). Cause (CONFIRMED by the census): the seed lists are census results on the pre-TurnStop tree and TurnStop's PES stop test moved every two-minute pes-loop match (section 7).

**Census (CONFIRMED; probe test in the scratch ablation worktree only, never committed):** seeds 1..12, two-minute `--pes-loop` matches on the merged tree, one process at a time under `systemd-run --user --scope -p MemoryMax=6G nice -n 19` (Main's grant: all four sim slots were held by gates for 3+ h; I stopped the first process once when `free` showed under 4 GB available and resumed with seeds 7..12). Each throw-in whose release ends is a ceremony taker (`IsPieceTakerHeld`) or a carried one (`IsCarryingBall`), tick of the first:

| seeds | ceremony | carried (first tick) |
|---|---|---|
| 1, 2, 4, 9, 12 | 0 | 0 |
| 3 | 1 (32745) | 0 |
| 8 | 1 (26044) | 0 |
| 5 | 0 | 1 (22628) |
| 6 | 0 | 2 (4573) |
| 7 | 0 | 1 (25018) |
| 10 | 0 | 2 (14048) |
| 11 | 0 | 5 (10588) |

**Re-staged (assertions untouched):** `kThrowInSeeds = {6, 11, 10, 5}` (was `{3, 3, 13, 11}`, `kSeed` deleted), `kCeremonySeeds = {8, 3}` (was `{2, 15, 23}`) in `tests/sim/pes_throwin_release_test.cpp`, comment records the census. Direct runs of the four reds on the re-staged binary (merged tree incl. `c7874a3c`, each under the same memory-capped scope): all four PASSED (the ceremony test 1 match, the three carried tests 1 match each; 5-8 min each).
`PesRestartDelay.*` lists (`{13, 8, 4}`, `{SEED, 3, 5, 11, 13}`, `{SEED, SEAMLESS_SEED, 13, 11}`) passed in the first gate and were left alone.

**Merge 3 (origin/main `99bde2a2`+, 20 commits):** conflicts only in `gen_translated.py` SETS (kept `results_aiconcepts.json` and `results_motion.json`) and `docs/re/LEDGER.md` (regenerated, 38,116 rows); layer regenerated (5,267 functions), `c7874a3c` pushed.

**Final `GATE fast` (CONFIRMED): `GATE fast: GREEN`.** Tree `c7874a3c` (pushed, confirmed by `git ls-remote origin MotionClips`; origin/main `0dd1c2b9` merged), `GATE_TIER=fast GATE_SLOTS="1 0" tools/dev/gate.sh <workdrive>/gf-MotionClips/build` (Release, full gate env incl. `GF_IMPURE_BANKS=gf-Captures-scratch/banks_union9l`): ctest `100% tests passed out of 2342` (5,119 s; the four re-staged `PesThrowInRelease` tests Passed inside it); recorded re-check 50 / 23 / 14 / 389 / 28 selected per set, all pass (the gate's own result); no unrecorded status regression (18,635 rows); `LEDGER.md` current (38,116 rows); off-path ports 125 (all allow-listed). Smokes seed 7, 2 min: switch-off `399e180f39459c2b` (27 stoppages, longest 23.2 s of bound 32.5 s) = main's baseline, pes-loop `85c7d7c515cf92ae` (17 stoppages, longest 23.7 s) vs main's `38549be3476430fd`: moved, attributed in section 7 to TurnStop's class-word test, midpoint stop test and idle-exit blend guard (PES-cited). `gen_translated.py --check` byte-identical (also run standalone before the last merge).

Cleanup done: the ablation worktree (`gf-MotionClips-abl`, with its scratch `ABL` env switches and the census probe, never committed) removed, scratch binaries deleted; `<workdrive>/gf-MotionClips-scratch` (7 MB) and the worktree `<workdrive>/gf-MotionClips` left for Main to remove after landing.
