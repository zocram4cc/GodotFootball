# PES port: KickTick: the chain-alone smoke move explained (it is not the chain), the Kick action's per-tick driver traced and GF's pair driven per tick; GF's window answers none on every try, so the kick start stays on a none pick (named stand-in) (05-11-26)

Branch `KickMotion` (worktree `gf-KickMotion`, scratch `gf-KickMotion-scratch`), pushed (see section 6). Image `pes_full.bin`, base `0x140000000`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Predecessors: `pes-port-KickBranch2.md`, `pes-port-KickBranch.md`, `pes-port-KickRoot.md`, `pes-port-KickWindow.md`, `pes-port-KickChooser.md`, `pes-port-KickHeader.md`.

## 0. Status against the ticket

| item | state |
|---|---|
| 1. explain the chain-alone move | **DONE, the premise was wrong (section 1).** Ablation at HEAD: the anime chain unbound, and unbound and not even built, give `a2f78ca1b76a66e4`, the same hash as with it. The chain is inert. The 85c7 -> a2f78 move is two unlanded behaviours the branch carries: the kick motion pick (Q's descriptor and in-flight branch) and the chooser's none kind. Nothing incidental to fix. |
| 2. the per-tick Kick driver | **TRACED and GF drives the pair per tick (section 2); "kicks with clips" NOT MET (section 3).** PES's start `FUN_140724CC0` is tried every tick; GF's window makes the pair answer none on 16,300 of 16,300 tries of a 2-minute match, so a refusing gate would end every open-play kick (16 kicks, all restarts). The start therefore proceeds on a none pick, a named `ponytail:` stand-in with its removal condition. The `[kickmotion]` line now prints the tries. |
| 3. smoke move, census, GATE fast | smoke explained by ablation (section 4); census run for `SEAMLESS_SEED` (section 5); gate line in section 5. |

## 1. The chain-alone move (CONFIRMED by ablation, seed 7, 2 min, `--pes-loop`, `simslot.sh`)

KickRoot's report said the pushed tree is not main's: it carries unlanded KickKind2 / KickMotion / KickChooser commits (`git diff c45fbd57 HEAD -- src` is 165 files). The chain was never the only difference. Scratch switches (reverted; not in any commit) at the commit that wires the pair:

| run | what is switched | `[determinism]` | ticks |
|---|---|---|---|
| HEAD | nothing | `a2f78ca1b76a66e4` | 46,254 |
| nobind | `BindAnimeChain` keeps only the two old pointer stores (chain loaded, not linked to P/A/B) | `a2f78ca1b76a66e4` | 46,254 |
| noload | also `BuildAnimeChain` skipped (no 35 MB pack segment, no key table) | `a2f78ca1b76a66e4` | 46,254 |
| standin | the pes-loop uses the default engine's stand-in kinds (no chooser), the motion pick kept | `51f3bb6e247f35de` | 35,572 |
| standin + nomotion | stand-in kinds and the motion forced to "no clip" (main's `PesKickSituation`) | **`85c7d7c515cf92ae`** | 30,080 |

So main's `85c7...` is reproduced exactly by turning off the two branch behaviours, and the chain alone changes nothing: no reader of the chain runs (the only reader, the kick geometry test's `0x141AFDF10`, is reached only when the chooser gets a real geometry request, which it never does, section 3), and the load has no side effect (clip packs, RNG and allocation order are not in the hash). Decomposition: main `85c7` --(motion pick feeding Q: descriptor 1, in-flight branch)--> `51f3` --(chooser answers none instead of the stand-in kind 0x69/0x21/0x49)--> `a2f78`. Both are real branch behaviours, so there is nothing to fix at a root; they land with the branch (new smoke baseline).

## 2. The Kick action's per-tick driver (CONFIRMED by decompile, `.pdata` unwinds of captured stacks, and 8 strike captures)

Unwinder: `.pdata` / UNWIND_INFO walk (scratch `unwind.py`, `unw_cap.py`) from each capture's `rsp`.

Per tick, for every man: `FUN_1406E5FC0` (player update) calls, in order,
1. `FUN_1406F9B70(P, cmd)` at `0x1406E609B` = the pending-command commit. It calls `FUN_1406EEA90(P, cmd[1])` (cmd[1] = the commanded action id): when the current motion is past its first ticks it asks the new action's vtable slot 2 (can-enter) and then the **action switch `FUN_1406EEBB0`** (return `0x1406EEB7D` in `FUN_1406EEA90`). Then it asks the *current* action's slot 15 (`FUN_1407253D0`, the re-pick for a new command, call `0x1406F9C12`, return `0x1406F9C17`) and, when that answers, stores the order record (`FUN_1406E1D60(P+0x2BC8, cmd+0x5EC)`).
2. the current action's slot 4 (call return `0x1406E60CA`): for the Kick (vtable `0x1425E2358`) `FUN_140719F20`, which switches on the playing motion's state byte (P+0xAB8 vtable+0x20): 0 -> `FUN_14071AB70`, 0xF -> `FUN_14071A080`, **0x10 -> `FUN_14071A250`** (the kick clip is playing), default -> `FUN_14071B350` (the approach; it writes kind 0x96 and starts the prep clip 0x8CC/0x8CD).

`FUN_140724CC0` = Kick vtable slot 13 ("start"; call `[r10+0x68]` at `0x14071AB2C` inside `FUN_14071A250`, return `0x14071AB30`, order record copied from P+0x2BC8; and from the AI side `FUN_1409316E0` (return `0x140931927` through the thunk `FUN_1406B4580` = `vt[13]`), whose callers are `0x140933A9D` (`FUN_1409336BF`) <- `0x140930DF5` (`FUN_1409307C0`) <- `0x1409F512C`). Its decompile (asm checked for the return):
- runs once per tick per Kick object: it works only when `Kick+0x44 != MI+0x1198` (the tick), and sets `Kick+0x44 = tick` only on success;
- first `vtable slot 2` (`FUN_1407266B0`, the can-enter gate: pitch margin and carried-ball tests);
- `FUN_140833CD0` (builder, call `0x140724DEF`, return `0x140724DF4`) -> `FUN_1408364D0` (chooser) -> the kind, or the alternate kind (descriptor +0x28) when the kind is none and the alternate is not (dropped when +0xDE is set) -> `FUN_14071E6F0` (clip row) -> `FUN_1406ABDD0` (does the Animetable cell exist);
- **returns 1 (`mov al,1` at `0x140725387`) only when the cell exists, else 0 (`xor al,al` at `0x1407253AA`)**; on success it writes P+0x2752 (kind), +0x2754 (band), +0x2755 (bucket), +0x2756 (side), +0x2758 and the Kick object's fields. On none it writes only the out word (`*param_5`) and returns 0: the AI asks again next tick.

Callers of the builder in the 15 `kick_header_833CD0` captures (unwound: seq4358, 4363 = shots through the AI side `FUN_1409316E0`; seq4331, 4772 = the Kick tick `FUN_14071A250` re-pick (`0x14071AB30 <- 0x140719F8C <- 0x1406E60CA <- 0x1409F23E7`); seq4342 = slot 15 `FUN_1407253D0` (`0x140725679 <- 0x1406F9C17`)). Three call paths, all `FUN_140724CC0` or its re-pick sibling, so KickBranch2's inference ("the pair runs every tick of the run-up and PES answers none until the strike") is right in structure.

**What happens at the strike (8 `action_switch_6EEBB0` captures whose new action is 8 or 9, from action 7 = Trap; pid2122415 seq55536, 56436, 55933, 55280; pid3439759 seq10216; pid3741448 seq6247, 4812, 23140):** the kicker's P already holds a real kind: P+0x2752 = 28, 31, 99, 75, 73, 57, 63, 33 (none 0x97 in 0 of 8), band 0, bucket 9, 11, 6, 9, 15, 11, 9, 9, side 0, 0, 0, 1, 1, 0, 0, 0, and `Kick+0x44` holds a tick stamp (44723, 45515, 45082, 44536, 4503, 2781, 2137, 10202). So the answer is written by the start (the successful try) and the action switch follows; INF: what changes between the none tries and the strike try is the chooser's own geometry (`FUN_140829E80`'s feasibility tests over the root-motion of each candidate clip and the ball record), not a counter or a distance threshold read by the driver (no tick counter gates the pair except the once-per-tick stamp).

## 3. What GF does now, and why it cannot refuse yet (CONFIRMED by runs)

Commit on `KickMotion`: `SelectAnim` (humanoid.cpp) runs `PesKickStart` (builder -> chooser -> alternate kind -> `KickRow` -> `KickMotion`, a clean-room port of the glue above) every tick it would take a pass or shot command in open play (`PesLoop() && IsInPlay() && !IsInSetPiece()`); the pick rides `Anim::pesKick` to the contact frame, where `PesKickSituation` reads it. The touch-time chooser call is deleted (clean cutover); restarts keep the none kind (PES's restart actions leave -1).

Experiment (refuse on none, scratch run): 2-minute seed-7 pes-loop match: **16,300 tries, 16,300 none**, 16 kicks (all restarts), 5 stalemate idles, hash `a5cc37024c4186ca`: a refusing gate ends every open-play kick. A scratch print of the first tries shows a ground pass (request 9) to a receiver, the kicker 1.1 m from the ball and almost still, the ball at rest on the grass, answering kind 0x97 / alternate 0x97 on 40 consecutive ticks. INF (high): that is a window defect, not PES's geometry: the three inputs the translated closure reads that GF's window lacks are
- the MA arrival list `MA+0x2470C..0x247D0` (zero in GF, 29-43 non-zero bytes per block in every capture; its writer is still undecoded, KickBranch2 section 3.3);
- the anime player's pose slot (`FUN_141AF7A90` keeps it; GF's holds zero, so the geometry test's root-motion reader returns identity);
- the Kick action object (`Kick+0x18`, `+0x40`, `+0x70`, `+0x75`, the can-enter gate `FUN_1407266B0`), named in `RunKickRequest`'s `ponytail:`.

So the start proceeds on a none pick (`pes::PesKickPick{none, band 0xC, bucket 0x12, side 0, motion 0}`), a `ponytail:` at the site that names these three and says what removes it (`selectedAnimID = -1` on none). With the stand-in the match is unchanged: `[kickmotion] 79 kicks, no clip 79, ..., Kick start tries 57 none 57`, hash `a2f78ca1b76a66e4` (= the pre-change tree; the default engine, `399e180f39459c2b`, unmoved, 34 kicks). **"pes-loop kicks with clips" is NOT met**; it needs the three inputs above.

Test added (compiled into the restart-delay family, sharing its seed-7 match): `tests/sim/pes_kick_start_test.cpp`, `PesKickStart.TheKickStartRunsForEveryPassOrShotAskedInOpenPlay` and `ANoneAnswerIsATryAndAKickWithAClipWasAnAnswer` (tries > 0; no open-play kick before a try; none answers are a subset of the tries; a kick with a clip was an answered try). Mutation check not run (budget); the first test fails if the per-tick call is removed (tries == 0).

## 4. Hook request for Main (strike-tick proof)

Hooks asked of Main (both FULL, caps as listed) and **armed** in `live2/pid4154480` (action_switch with the strike predicate at EVERY 20 instead of EVERY 1, global cap 51; kick_try_140724CC0 cap 24; kick_gate_1407266B0, kick_commit_1406F9B70 (`d(rdx+4) in (8,9,10)`), kick_tick_140719F20, kick_ai_exec_1409316E0 cap 12 each):
1. `action_switch_6EEBB0` with `--when '(rdx&0xff) in (8,9,10) and b(rcx+0xac8)==7'`: the strike tick (8 slim ones exist, but their P/ball/MA pages beyond the call's read set are missing). The pending command `cmd` of `FUN_1406F9B70` (the order record at `cmd+0x5EC`) is in the frame above the capture.
2. `kick_try_140724CC0` at the entry of `0x140724CC0` with `--when 'd(rcx+0x44) != d(q(q(0x1436F3DC8)+0x270)+0x1198)'` (a first try of the tick): a series of one kicker's tries around a strike (the try after which `Kick+0x44 == tick` is the strike). Add `b(rdx+0x2458)==<slot>` to pin one man.
The untranslated callees of the per-tick driver (static): `FUN_140724CC0`, `FUN_1407253D0`, `FUN_1407266B0`, `FUN_140719F20`, `FUN_14071A250`, `FUN_14071B350`, `FUN_14071AB70`, `FUN_14071A080`, `FUN_1406F9B70`, `FUN_1406EEA90`, `FUN_1409316E0`, `FUN_1406E5FC0`, `FUN_140727230`; already in the layer: `FUN_1406EEBB0`, `FUN_14071E6F0`, `FUN_14071E570`, `FUN_1406ABDD0`, `FUN_140723DE0`, `FUN_1406B0250`, `FUN_14071EC10`. Captures land in `live2/pid4154480` (not yet read by this run).

## 5. Census, smoke and gate

- Smoke seed 7, 2 min: switch-off `399e180f39459c2b` (main's baseline, 30,213 ticks); pes-loop `a2f78ca1b76a66e4` (46,254 ticks), main's is `85c7d7c515cf92ae` (section 1 explains the move).
- Census (`gf-KickMotion-scratch/census`, a scratch copy of `pes_restart_delay_test` with `SEAMLESS_SEED` read from the environment, four 20-minute seeds in parallel through `simslot.sh`): the six path-A tests (`AThrowInIsReleasedFromItsSpotAndTaken`, `...CarriedToItsSpotAndReleasedThere`, `...TakerPlaysABallActionWhenTheBallLiesOverTheLine`, `...TakerWalksToHisSpotAndIsNotPlacedOnIt`, `ACarriedThrowInReachesItsSpot`, `...CarrierStandsAtThePickupClipsContactOffset`) PASS on seeds 5 (284 s), 9 (291 s), 13 (982 s) and 4 (1,563 s); `SEAMLESS_SEED` is now 9 (seed 3 gave the taker root 1.398 m from the spot). With the re-staged seeds those two throw-in tests and `pes_throwin_release_test` are GREEN in the gate.
- Census 2 (`pes_throwin_release_test`: `ACeremonyThrowInIsReleasedFromTheHandOverTheSpot` was the second red, seeds 3 and 8 no longer place a ceremony taker; the other five tests of that binary passed): scratch copy with the seeds read from the environment, three seeds per process, two-minute matches: lists `{1,2,4}` (834 s), `{9,10,11}` (1,101 s) and `{12,13,14}` (3,104 s) PASS, `{5,6,7}` fails; `kCeremonySeeds` is now `{1u, 2u, 4u}`. Per brief rule 6 the merge-lane census of both families on the merged batch is the final word.
- **Brief rule 8 (stand-in justification).** The refusing path is translated and run (builder, chooser, alternate kind, row, cell) and answered none on 16,300 of 16,300 tries, so it cannot be proven on GF's window now. Missing inputs, each named: (a) the MA arrival list `MA+0x2470C..0x247D0` (writer undecoded: no direct-displacement store in the image, KickBranch2 section 3.3); (b) the anime player's pose slot (`FUN_141AF7A90`; the reader's identity answer holds only for the captured clips); (c) the Kick action object and its can-enter gate `FUN_1407266B0` (untranslated; no capture before the hooks above). Remove the stand-in when (a)-(c) land.
- **`GATE fast: RED pes_restart_delay_test`** (2341 of 2342 ctest green; recorded re-check "no unrecorded status regression vs HEAD (18760 rows compared, 160 scope moves recorded)", ledger current, off-path ports 118 all allow-listed; `pes_throwin_release_test` and every other family GREEN; 11,584 s wall, log `gate_tick2.log`). Two cases failed inside the binary: (1) **`PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker`: the stalemate breaker forced a live ball for 20 ticks** (two 5.0 s firings in the seed-7 match, a ball at rest at 0.08 m/s at the feet of the designated man: the resting-ball stall RestingBall2 owns). The main-equivalent run (`85c7d7c515cf92ae`, stand-in + nomotion ablation) has `[idle] 0 episodes`; every tree with the kick motion pick has the stalls (`51f3...` 1 episode, `a2f78...` 2), so this branch's moved seed-7 match reaches the stall that main's does not. Per the brief it must NOT be censused away; it waits on RestingBall2's translation of `0x140856370`. (2) my own `PesKickStart.TheKickStartRunsForEveryPassOrShotAskedInOpenPlay`: a test bug (it compared cumulative tries with kicks including restart kicks); fixed after the gate (`tries >= open-play kicks`, commit `a6b2f60e`), re-run alone on the fixed binary: both `PesKickStart` tests PASS (963 s). Not re-gated (the remaining red is the stall, which no seed edit may fix).

## 6. Left (numbered)

1. The window inputs of section 3 (MA arrival list writer, anime pose slot, Kick action object with its can-enter gate `FUN_1407266B0`), then delete the none-pick stand-in and let a none answer refuse the animation: that is "kicks with clips".
2. The strike-tick captures of section 4, then prove `FUN_140724CC0`'s glue (once-per-tick stamp, alternate kind, clip cell) against them.
3. Mutation checks for `pes_kick_start_test.cpp` (not run); RestingBall2's fix turns `PesStall` green on this branch.
4. `gf-KickMotion-scratch`: Main deleted the ~46 GB of synthetic states (owner-approved, regenerable); logs and scripts kept.
