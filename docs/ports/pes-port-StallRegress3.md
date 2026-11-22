# PES approach, round 3: ActionPress and its target translated and proven; not yet wired (StallRegress3)

Branch `StallApproach` pushed at `d4919cc4af` (`git ls-remote` confirmed). Parents: `632c88b7` (merge of origin/StallTrap `8e782040`, CounterFeed's cmove fix) on `68872384` (StallLiveRun, the 151 fix).

**Result: the two functions are translated and proven bit-exact on real captures and are in the layer. The designated man is still NOT commanded by them: the wiring is not built. The census is still 151, not <=118, and no behaviour changed.**

## 1. Merge

- `origin/StallTrap` at `8e782040` merged into StallApproach (merge, not rebase). No conflicts.
- `gen_translated.py --check` on the merged tree (before the new functions): 10211 functions byte-identical, exit 0.

## 2. Proofs (`prove_bounded.py`, real captures, `RV_HOOKS=<workdrive>/PES21/rendezvous/hooks.cfg`)

| root | captures | calls | replayed | mismatches | own-block | tag |
|---|---|---|---|---|---|---|
| `0x140639100` press target | pid1458145, hook `press_140639100` | 10 (merged); 19 earlier, unmerged | 10 / 19 | 0 / 0 | 53/70 (0.76); 0.80 on 19 | `presstarget` |
| `0x140962440` ActionPress | pid1502356 + pid1458145 seq1961, hook `press_140962440` (EVERY 300 / 1000) | 10 (merged); 4 earlier, unmerged | 10 / 4 | 0 / 0 | 42/73 (0.58) | `pressaction` |

Own-block coverage is the fraction of the root's own blocks the replays reached. For ActionPress 31 of 73 blocks are unreached, so the carrier and stop arms are unproven. A GF caller that enters those arms would run unproven code. That is the main limit on this proof.

Captures copied into scratch, unpacked with `capz.py unpack`, and deleted after each proof (the captures were packed by the rig's auto-slim). The disk stayed above the 300 GiB floor.

## 3. Layer

- `gen_translated.py` regenerated the layer: 10217 functions (was 10211). New: `ai/pes_fn_140962440.cpp`, `ai/pes_fn_140639100.cpp`, and the callees the replays entered: `closure/pes_fn_1406378F0.cpp`, `closure/pes_fn_1405C6560.cpp`, `closure/pes_fn_1406E7910.cpp`, `closure/pes_fn_1406EA260.cpp`.
- `gen_translated.py --check`: 10217 functions regenerate byte-identical, exit 0.
- Untracked-file check: `git status --untracked-files=all` shows no leftovers beyond the commit's own paths.
- Specs: `tools/dev/diffharness/specs/trapaction/140962440.json` (401 lines), `.../140639100.json` (279). Closure sets: `closure-pressaction.txt`, `closure-presstarget.txt`.

## 4. Tests on the merged tree (`d4919cc4af` with the regenerated layer; build from a fresh configure)

- `pes_throwin_release_test` (the ankle suite): **9/9 PASSED** (`ankle3.log`).
- `pes_hang_test`: **1/1 PASSED** (`hang3.log`).
- ALiveBall census (`pes_restart_delay_test --gtest_filter=PesStall.ALiveBall…`, `GF_STALL_DUMP=1`): **151 live firings**, FAILED (`census3.log`). Target <=118, not met. Unchanged, because the sim does not call the new functions yet.
- `gen_translated.py --check`: exit 0 (above).

## 5. Wiring: not done, and what it needs (the honest blocker)

ActionPress is `void pes_fn_140962440(PesState&)`, args by register: `rcx = A0` (the presser's action record: `+0x30` u8, `+0x48`, `+0x58` u32, `+0x5C` u8, `+0x68` u64, `+0x70/+0x78/+0x7C` f32, `+0x74` u8), `rdx = A1` (output float[3]), `r8 = A2` (the context: `+0x4` u32, `+0x40` the planner owner, and the chain `A2+0x38` -> `A2.38+0x18`). The spec also reads the image region `IMG.1436F3DC8` (+0x270).

GF's `PesWorldImage` (`src/sim/onthepitch/pesworldrecord.hpp`) already holds the team-AI MA object per side (`kMaOffset`, `MaVa`), and `PesFoxWorld::Call(fn, args)` is the call path. Missing:
1. a presser action record in the world window for the designated man, filled from GF's press state each tick (GF has `pespress.hpp` state, not this record);
2. the context chain `A2 -> +0x38 -> +0x18` and the planner owner (`+0x40`), which the world must carry for the planner solver (`FUN_1406B9270`, already in the layer);
3. the `IMG.1436F3DC8` global object's `+0x270` field.

Until these are built, calling the function would read zeroed or stale memory. That is not a PES approach. It is the ponytail that Main asked to remove, so I did not wire it.

## 6. What's left (named)

1. Build the presser action record and context chain in the PES world window for a designated man without possession, synced each tick from GF state. Confirm each field's source against a real capture (the spec's A0/A2 inputs).
2. Prove the carrier and stop arms of ActionPress (31 blocks unreached). That needs captures where the presser has a carrier, or a synthetic state (`synth.py`) over a real capture.
3. Call ActionPress for the designated man in `_BallControlCommand` (or the movement path), and remove the `AI_GetBallControlMovement` walk in the same change (clean cutover).
4. Then re-run the census. Target <=118 and ankle 9/9.

## 7. Hygiene

- Worktree `<workdrive>/gf-StallLive` on `StallApproach`, clean. godot-cpp symlink restored.
- Scratch worktree `wt-trap` (detached, origin/StallTrap 5b67bc56) removed at the end of the round.
- Build directory removed at the end of the round (20-11-26 rule).
- Captures: none kept in scratch. Source captures stay in live2 (rig's).
