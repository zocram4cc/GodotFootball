# OffBallSelectors: state (2026-10-15, end of pass)

## Landed and pushed
- `834ab58` Chance stop head in PES order (pad, carrier, PASS_GET figure > 1, then the minimum). Doc `323a105` (off-ball 2.2 table, cited by VA). Pushed to `refs/heads/OffBallSelectors` at `323a105`. Gate on the rebased tree (base `origin/main` 6c77c00): 696 tests, 100% passed, standing skips VfsInstall.NestedOriginalsSurviveWithoutDownloads and EditSaveInstall.DecryptsAndMatchesTheRunningGame.
- Seed 7 (20 min). eb63cd9 before: chance runs 564-464, stop fires 490-425, mask walks 23250-26820, passes 56-47, determinism 95515192750cd51b. eb63cd9 after: chance runs 524-465, stop fires 471-414, mask walks 29330-30020, passes 46-49, determinism 09ebe29234c27d79. 6c77c00 after: identical determinism. 6c77c00 before: not re-run.
- Mutation checks (TDD): minimum-first killed by 4 tests; `>= 1` killed by 1; no PASS_GET gate killed by 6.

## Answers to Main's notes
1. Holder attribute `FUN_140a50250(team,k)` is NOT a team-instruction reader (attack-leftovers §1.5 is wrong; raw bytes at 0xa50250-0xa502e2). It returns the ball holder's in-match record dword `rec+0xd4+4k` (rec = FUN_141b59e20(FUN_140a3af90(roster,holder)) + 8; tail call 0x1408c75c0). GF imports named stats (`PlayerData::stats`) and position letters (src/sim/data/playerdata.hpp); the in-match record is not imported (src/pes/editsave.hpp has the save cipher only). No imported stat maps to those dwords. Their meaning is the O5 open item. Stand-in to name in code: holder dword = 0 (`ponytail:` citing the record and O5).
2. Team instruction table: `FUN_140a63950(team,id,val)` checks slot i in 0..3 of `G+0x25cf8` (ids) and `G+0x25d38` (values) at index `team*4+i`. Overlap asks for id 4, value 0xff. Stand-in: never set (`ponytail:` citing G+0x25cf8). Not in code yet (Overlap not ported).
3. Counter state, with PES writers:
   - `T+0xa508` (counter enable): single runtime writer `FUN_140581980` (call 0x1405819c0, `mov [rcx+0xa508],eax`), called only with `T+0x255 != 0` (pes-re-counter-trigger.md §1). GF deleted the counter roll: build the writer.
   - `T+0xc48` (defensive-shape S): line builder `FUN_1405fbcb0(ctx, S)`, reached from team tick `FUN_14051d9e0 -> FUN_1405ec600 -> ...` (pes-re-defensive-shape.md). GF has `src/sim/onthepitch/pesdefshape.*`: check whether it carries the S bands before building.
   - `T+0xb6e0` (team concept running, INF low): no writer documented. Needs an image search for `e0 b6 00 00` in store instructions. GF analogue `pesteamconcepts.*`.
   - `MA+0x3fb8` / `MA+0x4b54`: not new state. `0x3fb8+0xc48 = 0x4c00` (opponent's role class, GF `Position` — array B on the S base), `0x4b54` (opponent's slot-point z, GF `slotPoint`). _Corrected 19-10-26 from pes-re-role-writer.md §6: S+0x3FB8 = T+0x4C00 holds class 0..9, not "registered position"; old "registered position". Note: `P+0x3FB8` (set-piece object) is a different array that aliases S only at init._

## Decision 2 (PostPlay lane 0 xmm0): done as a model
`/pes-re-OffBallSelectors-postplay-xmm0.md`. Every early exit of `FUN_14061fa40` has xmm0 = 0 (explicit `xorps` at 0x61faf1, 0x61fb9b, 0x61fc31, 0x61fca8; `movaps %xmm7` with xmm7 = 0 from 0x61fc71 at 0x61fc93, 0x61fd1d, 0x61fe14, 0x620580). The only non-zero return is the success path, `call 0x61f8b0` at 0x6204f5, which returns K − d²(runner, receiver), K = 10000 (VA 0x14259d634). Lane 1 and lane 2 both need holder dwords >= 1, so under the stand-in PostPlay's score is 10000 − d² exactly when lane 0's gates pass, else 0.

## PostPlay: decoded, not ported
- Mask `FUN_140620590`: team possession, open play, carrier or fallback (`MA+0x2483c`), `FUN_140593290` team head, holder dword 0 (stand-in 0 switches off the holder branch), then per candidate `FUN_140a64030(id,3) || FUN_140a64030(id,2)`, where `FUN_140a64030(id,k)` = bit k of the player's PES skill mask AND the position table (GF: `PlayerSkills::Has` and `SuitsRole`). Fallback test `FUN_1404c8e60(MA,fb) > 42.25` (VA 0x1425c721c): squared ball distance table, GF `Dist2` from the ball.
- Stop `FUN_140620900`: lane byte at `T+0x288+0x40·slot` (written at take time from the executor's link byte), then conditions per pes-re-off-ball §2.2.
- Lane 1 `FUN_140620ca0` and lane 2 `FUN_1406210e0`: need holder dwords >= 1, so unreachable under the stand-in (name it in code if they are ported).
- Lane 0 gates: asm and decompile read (see the xmm0 report). Helpers still to read in full: `FUN_140a8ff10`, `FUN_140a62610`, `FUN_140a62690`, `FUN_140a62960` (state 0x3a scan). `FUN_140a4e120(G,id,k)`: timer ring of 4 entries; not found returns `fps*6000+0.5` (VA 0x14260b390), so the E3 gate passes for a player outside state k (stand-in: state k = current run state). `FUN_140a4e720`: cone helper, GF port needed (h1 #2). `FUN_140458740(record,t)`: bearing of the predicted displacement (GF `PBrg`).
- Target: the executor's vtable target is the zero stub `0x525150`; whether PES reads the record target for PostPlay is unverified (stand-in: current position). Must be decided before wiring.

## Counter and SecondLine: not ported
- Counter mask `0x60ab00` reads writers `0x60bdd0` (pins runner in sel+0x288) and `0x60b590` (pins in sel+0x28c), both read this pass (dec1.txt 16-494). They need `T+0xa508`, `T+0xb6e0`, `T+0xc48` (above), `FUN_1405a8290` (unported; off-ball chance-lanes §2.1), `FUN_1405c0f00` (`_good` selection, not in GF), `FUN_14124dad0`, `FUN_140596470`, `FUN_140a53f90`, `FUN_140a57800`.
- SecondLine: stop head read (pes-re-off-ball 2.2 corrected). Mask `0x60ebf0` (477 lines) calls about 18 unread helpers; its cooldown read is `b4a.c:1204` (`param_2[7][slot]`), i.e. RunCooldownBlocks belongs there.

## Scratch and reproduction
- Decompile batches: `<workdrive>/gf-OffBallSelectors-scratch/dec1..dec5.txt`, `dec_post.txt`, `dec_veto.txt`, `dec_cap.txt`, `dec_cap2.txt`, `dec_stops.txt`, `dec_pp_helpers.txt`, `lane0.asm`. Re-decompile any VA with `sh ~/ghidra-projects/scratch/Unpacker/dec.sh OUT 0xVA`. The old `gf-*-scratch` trees named in the earlier reports (b4a.c, run2.c copies, etc.) are gone from disk; the Unpacker `out/` copies remain.
