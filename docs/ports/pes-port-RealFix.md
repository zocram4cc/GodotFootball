# RealFix: the two real-call translator mismatches, and the team tick (26-10-26)

Branch `RealFix` (pushed; `git ls-remote origin RealFix` = 070ae61e). Based on `origin/CallDelta` f8f16a18, merged with `origin/main` (355e5ef3). Commits: 0e4f32ed (translator fixes), e4c08b8b (EXTRA_CAP), 070ae61e (seven-snapshot record). Scratch and traces stay outside the repo.

## 1. Result

- **1404405D0 fixed (CONFIRMED).** Real calls: 350 of 350 replay bit-exact over seven snapshots (was 200 mismatches). Own-block coverage stays 0.2883 (reported, not a gate).
- **1405CE930: the first divergence is found and fixed (CONFIRMED), but the function cannot be replayed.** All 300 real calls now enter the out-of-set callee 140A79640, which the closure cannot build (section 4). They are excluded with that named reason. Before, the translation silently returned rax=0 for that call; that placeholder was the source of the 300 mismatches after the MUL fix.
- **Floor before/after (seven snapshots, 50 functions):** coverage-floor count 9 of 50 before and after (same set). Bit-exact on every replayed call: 19 before, 20 after (1404405D0 joins). Mismatch records 500 before, 0 after. Replayed calls 27,948 before, 27,648 after (300 fewer: the 1405CE930 exclusions, named).
- **Team tick 1404F6C30: still 0 of 700 replayed.** Named blocker: 140A79640 and 140537AC0 (section 5).
- **Regressions:** no verdict changed except the two functions above (merged-tree snapdiff vs CallDelta baseline, exact). `--check` on the merged tree: pilot rc=0 (results.json, 183 diffed), cutscene rc=0 (549), presentation rc=0 (472). Impure record has 0 diffed, so nothing to check.

## 2. Instruction classes fixed in `tools/dev/translate/xlate.py` (CONFIRMED)

1. **One-operand MUL / IMUL (F6/F7 /4, /5), all widths.** Found at 1405CE930 by lockstep: `mul ebx` at 140A644B6; the next instruction (140A644B8) differed in RDX. The 32-bit form kept the upper 32 bits of RAX and RDX; SDM zero-extends both. The 8- and 16-bit forms multiplied the whole EAX and wrote DX into EAX. IMUL left CF/OF clear. Now: 8-bit AL*r8->AX, 16-bit AX*r16->DX:AX (only AX/DX low halves written), 32-bit EDX:EAX with zero-extension, 64-bit unchanged; IMUL CF=OF = product does not fit the low part.
2. **MOVSD xmm, m64 (F2 0F 10, memory source).** Found at 1404405D0 by lockstep: `movsd xmm0, qword ptr [rax+0x55c]` at 14043FDDB; PES zeroes bits 64-127, the translation kept them. Now zeroes them. The register form keeps bits 64-127 (SDM), unchanged.

The earlier MOVSS m32 and MOVD fixes (CallDelta) are kept.

Lockstep method (scratch, not committed): the translator emits a `g_pes_trace(va, s)` before each PES instruction, and the harness logs pre-instruction state (16 GPR, 16 XMM lanes, flags) over the same replayed call. Both were compared. For 1404405D0 case 0 (snapshot 1), all 8,993 instructions match on GPR and XMM state. For 1405CE930 case 0, the first mismatch is at step 351 (item 1); after the fix the path matches through step 2030 and then reaches the out-of-set call (section 4). Flags are not compared by this tool.

## 3. Silent gaps made loud (CONFIRMED, general)

- `driver.runnable_extra` dropped unmodellable out-of-set seeds that fail `_why_dispatchable`: neither built nor blocked, so the harness ran the real callee while the translation did something else. They are now recorded as blocked with their why-chain, so run.py excludes a case that enters one.
- `xlate.py` emitted `OUT-OF-SET CALL` as `s.gpr[0] = 0;`. It now calls `pes_dispatch_missing(VA)` (loud abort). Two committed closure files carry the old placeholder: `src/pes/translated/closure/pes_fn_140A41CC0.cpp` and `pes_fn_140A7FA10.cpp`. They need regeneration (Main).
- `driver.EXTRA_CAP` 1000 -> 1100. Measured: the 1404F6C30 out-of-set walk (cap 100000) is 1,042 functions; 1404F6720's own direct-call closure is 29. At 1000 the single budget over all seeds ran out and blocked 1404F6720 by budget, not by its size. Blocked set at 1100: 140537AC0, 14053C6E0, 140A79640 (1415A1780 thunk and 14159F3E0 engine-noop are stub-table entries, not replay blockers).

## 4. 1405CE930: why it stays blocked (CONFIRMED)

The call at 1405CEB82 targets 140A79640 (out-of-set). Its stubs.json chain (`why`) contains: 14159F8B0 with an indirect call (14159F8C0) through import slot 14252F6E8 (no name in the image), global stores at 1403FB8A9, 1403FB622, 1403FB67B (and 140537C4E on the 140537AC0 chain), and call depth > 10 at 1403FB2E0. Owner policy (`_blocker_dispatchable`): only abort-trap import slots and indirect calls are dispatchable; unnamed import slots and global stores are unmodellable. So the chain cannot be built. Every replayed call of 1405CE930 enters it (300 of 300), and the exclusion is named.

## 5. Team tick 1404F6C30 (CONFIRMED: 0 of 700 replayed)

- Blocked set at cap 1100: 140A79640, 140537AC0, 14053C6E0. Snapshot 1: 100 of 100 calls enter 140A79640. With 140A79640 unblocked (measurement only), 100 of 100 enter 140537AC0 next (`tick_unblock`), so the two chains are the blockers.
- Both chains reach the same unnamed import slot 14252F6E8 through 14159F8B0. 14053C6E0 calls import slot 14252FD98 (unnamed). Blocker: calls through unnamed DLL import slots (outside the abort-trap policy) plus global stores.
- 1404F6720's cap blocker is removed (section 3). It is not the blocker.
- Replays on the tick: none can start until the import-slot policy changes. This is an owner policy decision, not done here. Main's queued import-call codegen (below) is the path to it.

## 6. Open items

1. **MOVQ xmm forms (INF, not replay-measured; not fixed).** `emit_sse` routes MOVQ (non-GPR) to `do_mov128v`: `MOVQ xmm, m64` loads 16 bytes; `MOVQ xmm, xmm` copies the upper lane; `MOVQ m64, xmm` writes 16 bytes. SDM: the xmm destination's bits 64-127 are zeroed, and the store writes 8 bytes. Needs a red real-call case before the fix.
2. **Committed layer drift (Main regenerates).** MUL/IMUL, MOVSD m64 and the placeholder change generated code for every committed function with those forms. The gate's drift check will flag them. Two closure files carry the old placeholder (section 3).
3. **Gate not run (Main gates).** Translator changes have no unit test; the evidence is lockstep plus the replays. No mutation check was run.
4. **Main's queued items (not started; beyond this brief).** (a) import-call codegen through named models per slot (stub table; unknown slot = build error), then replay the 19 cutscene functions from SceneBirth's traces; (b) 0x141F55030 (50/50 mismatch at written byte 49). Budget ended first.
5. **driver.py merge note.** Main's NoFloor branch edits driver.py's floor gating. My driver edits are in `runnable_extra` (blocked seeds) and `EXTRA_CAP`. They should not conflict with the floor change.

## 7. Verification run

- Full seven-snapshot snapdiff, pre-merge and merged (identical per-function results), recorded in `tools/dev/translate/results_snapdiff.json` (mismatch values omitted).
- `--check`: pilot (results.json), cutscene (results_cutscene.json), presentation (results_presentation.json): rc=0 on the merged tree. Pre-merge, the cutscene check lost 0x14008CE00 (77 cases, rax 0 vs 0x208000AB); the same failure occurred on the unmodified CallDelta tree, and it passes on the merged tree (77 of 77). The cause is the base's older cutscene specs and banks, not these changes.
- Lockstep and replay runs: as above. Test binaries: none (translator-only change).
