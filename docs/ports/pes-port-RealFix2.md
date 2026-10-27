# RealFix2: import-call codegen, gs segment base, and the 0x141F55030 result (27-10-26)

Branch `RealFix` pushed (`git ls-remote origin RealFix` = c052fbe3). Base: origin/NoFloor 7136fbb1. Scratch under <workdrive>/gf-RealFix-scratch/ (not committed).

## 1. Result

- **SceneBirth 25 functions (the import-reason set, 6,641 import-model exclusions before):** 9,326 traced calls, 8,962 replayed, **0 mismatch records. PROVEN (every replay bit-exact, at least one replay): 24 of 25.** Before this change those 25 had 0 replayable calls, so 0 proven. Coverage is reported, not gated (Main's ruling); the 25 medians are 1.0 and the lowest is 141F5DE00 at 0.2.
- **Not proven: 0x141F5F330** (1 call, 0 replayable). Its one call fetches code from a DLL address that is not an export entry (the harness refuses it, emu error at 0x6FFFFA67146C; S4 in the SceneBirth plan). Named exclusion, not a mismatch.
- **Note on the count:** the import-reason set is 25 functions with 6,641 import calls (Main's "19"). Several of the 25 also have calls excluded for other reasons: 14155E260 (99 "did not return", budget) and 141F53E80 (264 "did not return"). Those are excluded and named; the PROVEN counts are over replayed calls only.
- **0x141F55030: 50/50 replays bit-exact, floor true, coverage 0.8.** Before (pre-change commit 7136fbb1 with the harness `_icount` fix only) the function does not replay: the translated run aborts with PES_DISPATCH_UNTRANSLATED at 0x14159EFF8. I did not reproduce Main's byte-49 record; on this commit the symptom was that dispatch abort. Attribution: the absolute `JMP [slot]` to an image address (1415A01DC) returned silently, and its successor was never translated. The fix is in section 2 (dispatch on the slot's runtime value, plus the closure fixpoint).
- **Recorded sets, `--check` on the final state (commit c052fbe3):** pilot 183/183 still diff bit-exact (rc 0), cutscene 549/549 (rc 0), presentation 472/472 (rc 0). Verdicts gained: none (`--check` re-runs the recorded-diffed set only). Verdicts lost: none. Impure has 0 recorded diffs.
- **gs segment item (FrameRoot 0x1403FB710, 0x1403FB730): translator fix done and verified in the generated code; replay BLOCKED, see section 3.**

## 2. What changed (all committed in c052fbe3)

1. **Named import-call codegen (CONFIRMED).** `import_slots.py` (new) names each absolute `CALL/JMP qword ptr [slot]` the universe reaches from a snapshot: the slot value is attributed to the PE image holding it (base + SizeOfImage), and the export at that exact RVA is the name. Committed `import_slots.json`: 401 slots: 166 exports, 42 image code, 4 null, 189 unnamed (their module's section is anonymous in the snapshot, so no name; they keep the dispatch path and abort loudly if reached). `xlate.py` `import_model_lines`: a call or jump through a named slot runs its model from `import_stubs.json` (`return`, `memcpy`), with the harness's exact stack writes. The harness (`snapcase.py`) no longer excludes modelled imports; an unmodelled export is still refused. No model for a named slot is a build error.
2. **JMP through an absolute slot (CONFIRMED).** A named export runs its model then returns. An unnamed slot dispatches on its runtime value through the table, like an indirect CALL, aborting loudly if the target is not translated. (This was the 141F55030 fix; the old code returned silently.)
3. **BSF / BSR (CONFIRMED).** Previously fell through to `UNHANDLED (void)0`, leaving the destination unchanged (140401EE0 case 10: r14 differed). Now: ZF = source zero; a zero source leaves the destination; otherwise the bit index, with the destination's width.
4. **INT3** is `__builtin_trap()` (a loud abort, not silent).
5. **gs segment (CONFIRMED by generated code).** `split_ops` moves the prefix inside the brackets (`gs:[0x58]` -> `[gs:0x58]`), `parse_mem` returns `seg`, `addr_expr` adds `s.gs_base`, `bake_ro` refuses segment reads. `PesState::gs_base` (default 0). `diffmain` sets `gs_base` from `frame_trace.TEB_BASE` (the harness's TEB). Before: `mov rax, qword ptr gs:[0x58]` -> `s.rd(0x58ULL, 8)`. After: `s.rd((s.gs_base + (0x58ULL)), 8)`.
6. **Build closure (CONFIRMED by replay).** `snapdiff.build_closure` now (a) roots every image address the replay executed as an indirect site or target, and (b) translates to a fixpoint: a tail jump or direct call to plain image code with no stub entry joins the build, as the harness runs its bytes. Before, those were missing from the dispatch table (abort).
7. **Harness:** `Replayer` and `frame_trace` initialise `uc._icount` (run.py's shared hook reads it; this was an AttributeError on any snapshot run).
8. **Committed layer:** MUL/IMUL, MOVSD m64, BSF/BSR, the absolute JMP and gs changes change generated code for committed functions with those forms. Main regenerates `src/pes/translated` (gate drift check). `pes_state.hpp` gained `gs_base` (default 0).

Verification: lockstep (scratch) on 140401EE0 case 10 (217 steps) and case 20 (319 steps), 141F55030 case 0 (290 steps), 1404405D0 case 0 (8,993 steps): no GPR/XMM divergence after the fixes.

## 3. FrameRoot gs item: blocked (CONFIRMED)

- The translated code for 1403FB710 and 1403FB730 is now gs-relative (section 2.5). Replay is blocked by the traces:
  - `<workdrive>/gf-FrameRoot-scratch/reccut/cut_snap1-213436_s9` has the calls (7 each) but its deltas predate call-time TEB recording: 0 of 1,922 deltas contain the TEB page (0x7FF00000). The harness then fails: "unmapped read at 0x7ff00058" (7 of 7 cases, both functions).
  - A fresh cutscene trace on the same snapshot (`frame_trace.py`, current tree, `--spec-dir specs/cutscene`, 50 frames) records the TEB in all 90,917 deltas, but reaches neither accessor (0-byte call files). A default trace does not trace them either.
- Needed to finish: traces of these two functions that record the TEB (a frame_trace of the reccut mode on the current tree, or a 50-frame cutscene run that reaches them). I did not guess the TEB contents.

## 4. Open items

1. **0x141F5F330**: one DLL-code fetch not at an export entry (S4). Needs a model of that fetch path or the CREATE service reimplementation.
2. **Unnamed slots (189):** their modules are anonymous in the snapshot. Named exports for them would need the module's header in the snapshot; until then they abort loudly if reached.
3. **FrameRoot accessors**: section 3.
4. **Lockstep-by-hand:** the reccut / fresh-trace discrepancy means I have not replayed a gs function. The gs path is verified by the generated code and the harness's TEB, not by a replay.
5. **--check after every change:** I ran the three recorded-set checks on the commit before the gs/closure changes (state checks4) and again on the final state. Not after each individual change (BSF, INT3, fixpoint); the final state is the one reported.
6. **Main's byte-49 record** for 141F55030 not reproduced (dispatch abort on the base; bit-exact now).
