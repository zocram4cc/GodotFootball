# WalkLane10: a strike-owned man and PES's contact push (ATrapsRoot, handoff + exemption). Scenario M1.

Branch WalkLane, worktree <workdrive>/gf-WalkLane. Base: origin/Integrate 3ed0f35d9, merged into WalkLane (merge 5f2c5e771, clean).
Commit: 66f2930d7 (local; push pending gen check, see section 6). Static evidence: headless decompiles via `<local>/dec.sh` (the Ghidra bridge on :8080 returned 404 on every path this session). Outputs: `<workdrive>/gf-WalkLane-scratch/dec/contact1.txt`, `contact2.txt`.

## 0. Rig requests
None. No live PES reads, no hooks, no captures.

## 1. What PES does to a man whose root the strike adjuster owns (B+0x81 set) when players overlap

| # | Claim | VA | Verdict |
|---|---|---|---|
| P1 | The contact push is computed per man, not per pair, and is NOT gated on B+0x81. `FUN_1408C48C0(P)` gathers P's contacts (`FUN_1408C31C0`), adds the separation to P's record `P+0x624` (x) and `P+0x62C` (z), and adds it to the anime root offset `ap+0x248C/+0x2490/+0x2494` (`ap = *(P+0x7D8)`). The root offset is zeroed when its squared length is under `_DAT_142577F6C`. | `1408C48C0` (body `1408c48c0..1408c4953`) | CONFIRMED (decompile) |
| P2 | The layer root composer `FUN_141AFD400` writes the root translation ABSOLUTELY from B when B+0x81 is set: `matrix+0x30/0x34/0x38 = B+0x90/0x94/0x98`, the last store of the function, after the clip-track accumulation into `pfVar1[0xc]/[0xe]`. While the strike is valid the clip root track and any root offset are overwritten. | `141AFD400`, block `if (*(char*)(param_1+0x81) != 0)` | CONFIRMED (decompile) |
| P3 | The composer never reads `ap+0x248C..0x2494`. The only translated `+0x248C` reader, `140946D70`, reads a different object (`param_1+0x248C`, the team/AI record). No reader of the anime root offset was found in the translated tree. | `141AFD400`, `140946D70` | CONFIRMED for the composer; INF that no other reader moves the root |
| P4 | The wrapper `FUN_141AFD3C0` calls the composer only when `B+0x80` and `B+0x78 != 0`. | `141AFD3C0` | CONFIRMED |
| P5 | Consequence: for a strike-owned man PES does not move his root by separation. The push is written to his record and to the root offset; the composer discards the root offset while B+0x81 is set. The opponent is pushed by his OWN call of `FUN_1408C48C0`, so the rule is "skip the trapper, the other man keeps his push", not "skip the pair". | P1 + P2 | CONFIRMED for the trapper's root; INF for the other man's root path (O2) |
| P6 | `P+0x624/+0x62C` is the man's 2D position record: `FUN_14091E9F0` stores there (`wrf(P+0x624, x)`, `wrf(P+0x62C, z)`, translated `src/pes/translated/ai/pes_fn_14091E9F0.cpp:218/228`). The contact push therefore moves the record. The composed root (B+0x90) is what the body draws. | `14091E9F0` | CONFIRMED (writer); INF (that the record is the body's world position) |

**Rule ported (exempt him exactly as PES does):** while the strike root is valid, GF's pair pushes (the bounce at `match.cpp` ~1433 and the bump at ~1586/1587) are not applied to that man. The other man keeps GF's push (named `ponytail:`, open item O1).

## 2. Change (commit 66f2930d7; WL9's handoff ported without its probes)

- `pesworldrecord.{hpp,cpp}` `ReadAdjusterFrame`: the root point is read from B+0x90/B+0x98 directly. Fields `strikeMoved, x, z` become `strikeValid, rootX, rootZ`. The O2 read is removed from this function; `kO2PointOffset` stays (writers and `pes_adjuster_tick_test` use it).
- `pesmovestep.cpp` `StepAdjusters`: `WarpRoot(frame.x, frame.z)` is replaced by `body->SetStrikeRoot(frame.strikeValid, root point)` each PES tick.
- `humanoidbase.{hpp,cpp}`: `SetStrikeRoot`, `StrikeRootValid`. `CalculateSpatialState` absorbs the clip root track and the smuggles into `startPos` while valid (position == root point), and shifts `nextStartPos` and `previousPosition2D` as `WarpRoot` does.
- `match.cpp`: the two pair-push sites skip a strike-owned man.
- `playerbase.hpp`: `StrikeRootValid()` accessor.
- `tests/sim/pes_stall_test.cpp`: ratchet constants lowered to the measured values (section 4).

## 3. Measurements (seed 7, PES loop, pes_pass_target_test and pes_restart_delay_test)

Red-first: the unchanged merged base was run first (log `wl10_base` in scratch, `base_passtarget.log`).

| test | base 3ed0f35d9 (merged, unchanged) | WalkLane10 (66f2930d7) |
|---|---|---|
| PesPassTarget.ATrapsRootMovesByExactlyTheStrikeAdjusterStep | RED, worst gap 0.156218 m (222 ticks) | GREEN (worst gap < 1 mm, 320 ticks; value not printed on pass) |
| PesPassTarget.EveryPassGetEpisodeIsWithinPesLiveRange | RED, 10 of 30 episodes > 2.94 s, median 1.630 s, longest 11.72 s | still RED: 6 of 34 > 2.94 s, median 1.148 s, longest 11.61 s |
| PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver | green | green |
| PesPassTarget.IsSetInFlightAndNeverStaleWhileTheBallRests | skipped (named gap, unchanged) | skipped (unchanged) |
| PesStallQuick (quick census, GF_STALL_DUMP=1) | bound 53 (Integrate) | forced_live 49, PASSED |
| PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker | bound 2229 (Integrate) | liveFirings 479, episodes 47, PASSED |

Census per bucket: the quick summary prints only `armed=0 live=49 unarmed=0`; no per-bucket breakdown is available in the output, so the delta is reported in total only (53 -> 49). The base quick census of 53 is the bound recorded on Integrate, not re-run here.

EveryPassGet: the median did not rise. Attribution is not needed (the median fell, 1.63 -> 1.15 s); the remaining 6 over-bound episodes are PassLane2's (not tuned here).

Mutations (each rebuilt, run, reverted; the tree was rebuilt after the last revert):
- M1, exemption removed at both push sites (`match.cpp` 1436, 1590-1591 unguarded): ATrapsRoot RED, worst gap 0.008635 m (320 ticks). Exemption is load-bearing.
- M2, absorption disabled (`humanoidbase.cpp` `if (false && strikeRootValid)`): ATrapsRoot RED, worst gap 0.045897 m (331 ticks). Absorption is load-bearing.
- No mutation was run on the ratchet constants (they are measurement bounds; the measured counts are reported above).

## 4. Ratchet (M1: bounds lowered in the merge commit)

- `kBestMeasuredLiveFirings`: 2229 -> 479 (ALiveBall, full match).
- `kBestMeasuredQuickLiveFirings`: 53 -> 49 (PesStallQuick).
- Both are lower than the bound, so the ratchet holds. A later landing must be at or under these.

## 5. Gate

- Builds: `pes_pass_target_test`, `pes_restart_delay_test` (Release, ccache).
- Tests run: PesPassTarget.* (base and change), PesStallQuick.*, PesStall.ALiveBall*, PesPassTarget.ATrapsRoot* (M1, M2).
- `GATE fast`: not run (the assignment ran the named tests only; Main gates the batch).
- `gen_translated.py --image pes_full.bin --check`: running (log `gen_check.log`); the translated layer is unchanged by this commit (only `src/sim` and `tests/sim` files changed), so the check depends on the merged base.
- PII: no `<workdrive>`, `<home>`, `/tmp` or user name in the added lines (grep over the commit).

## 6. Push
Pushed: `git ls-remote origin WalkLane` = 66f2930d700e30db8ac0d83dfdf5884934064f83 (fast-forward from 08d6a3ad8). `gen_translated.py --check`: 11191 functions regenerate byte-identical (GEN_RC=0, 1515 s). Build dir removed after the run.

## 7. Open items

- O1: the other man's push. PES's per-man share (`FUN_1408C48C0` with `FUN_140A6D670`) is not ported; GF's bounce stays the push for the other man (`ponytail:` in match.cpp).
- O2: whether a non-striking man's anime root offset (`ap+0x248C`) moves his root is not decoded (no reader found in the translated tree; a full-image scan is needed).
- O3: P6 (the record `P+0x624/62C` as the body's world position) is INF.
- O4: EveryPassGetEpisodeIsWithinPesLiveRange still red (6 of 34 over the bound; longest 11.61 s). PassLane2 owns its causes.
- O5: IsSetInFlightAndNeverStaleWhileTheBallRests is skipped (named gap, unchanged).
- O6: the Ghidra bridge (port 8080) returned 404 on every path this session; decodes used `dec.sh`.
