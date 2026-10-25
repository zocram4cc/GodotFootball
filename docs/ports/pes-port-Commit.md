# pes-port-Commit: translated layer committed (stage 0) and stage 1 handover

Heads: stage 0 = b0fa26b (on origin/main via Main's merge). Stage 1 = c4fc0a8 on branch Commit (origin/Commit, fast-forward from b0fa26b; contains the Presentation and main merges, no rewrites).

## Stage 0: the translated layer is committed and built unconditionally

- `src/pes/translated/`: one TU per function under `<subsystem>/pes_fn_<VA>.cpp`, plus `translated.hpp` (declarations), `pes_dispatch_gen.hpp` (indirect-call table), and the runtime headers (`pes_state.hpp`, `pes_cond.inl`, `pes_stubs.hpp`, `pes_dispatch.hpp`, moved from tools/dev/translate).
- `tools/dev/translate/gen_translated.py` regenerates the whole layer from the image. Each function is translated under the universe and stub map of its own diff run (driver.universe_of, driver.runnable_extra), and so is every direct callee that run compiled (BFS). Deterministic under two hash seeds.
- Closure members (`closure/`, 49 in the current layer): direct callees the recorded callers link. Each banner names its harness status. They are never counted bit-exact on their own (Main's ruling).
- Build: gfsim links `pestranslated` unconditionally. PES_EXE, PES_PILOT_AVAILABLE, gen_pilot_lib.py, the pespilot target, the `sheet_port` hand ports and the LaneZ fallback are deleted.
- Gate: `diff_translated_drift` (tests/pilot, skips 77 without PES_EXE). Mutation check: a changed byte fails with `differs`; a `hand-refined` marker skips the file; a clean copy passes.
- Team sheet test rewritten to check the readers against the record layout and PES's refusals (5/5). No image needed at test time.

## Stage 1: structure, share per step

Measured on the current layer: 1189 functions (1140 recorded, 49 closure). Bit-exact status checked after the last change.

| Step | Applied | Share |
|---|---|---|
| 5. Real signatures (PesState only where needed) | no | 168 of 1189 functions (167 of 1140 recorded) touch no memory, call no translated function and use no stack: the candidates. Not applied: the boundary change and the state adaptor for the harness are not written. |
| 6. Locals for non-escaping spills | no | 274 recorded functions use the stack; 101 of them make no calls (candidates). Not applied: needs definite-assignment for slots. |
| 7. Structured control flow | yes | Functions with gotos: 651 of 1189 before, 219 after. 432 functions (66% of those with gotos) now have no goto. Remaining 219 keep gotos: 135 loops, 83 unmodelled control lines, 1 over budget. |
| 8. Named field accessors from fieldmap | no | fieldmap maps 513 fields over 38 types; the 252 function bindings are marked provisional (A0/A1/A2 unnamed). Not applied: names would encode unverified types. |

Step 7 method (tools/dev/translate/flow.py, called from xlate.translate_one; flat body as fallback): each conditional is structured as if/else at its immediate postdominator; a block reached from both arms is copied into each (tail duplication). Loops and unmodelled control lines refuse. Every path runs the same statements and evaluates the same conditions in the same order. Self-check: 400 random CFGs, flat and structured traces equal.

Excerpt (function 0x140581CE0, ai; constant names only). Before, from the flat layer:

    if (s.cond("be")) goto L_140581CFE;
    s.set_x_f32(1, (float)((s.x_f32(1)) + (s.x_f32(0))));
    goto L_140581D07;
    L_140581CFE:;
    s.cmp_ss((float)(s.x_f32(1)), (float)(s.x_f32(0)));
    if (s.cond("c")) goto L_140581D0B;
    s.set_x_f32(1, (float)((s.x_f32(1)) - (s.x_f32(0))));
    L_140581D07:;
    s.wrf(((s.gpr[2])), s.x_f32(1));
    L_140581D0B:;

After, from the structured layer:

    if (s.cond("be")) {
      s.cmp_ss((float)(s.x_f32(1)), (float)(s.x_f32(0)));
      if (!s.cond("c")) {
        s.set_x_f32(1, (float)((s.x_f32(1)) - (s.x_f32(0))));
        s.wrf(((s.gpr[2])), s.x_f32(1));
      }
    } else {
      s.set_x_f32(1, (float)((s.x_f32(1)) + (s.x_f32(0))));
      s.wrf(((s.gpr[2])), s.x_f32(1));
    }

## Verification (stage 1 commit c4fc0a8)

- driver.py --check, bit-exact on every recorded set after the change: pilot 183/183, impure 8/8, cutscene 549/549, presentation 472/472.
- gen_translated --check: 1189 functions regenerate byte-identical.
- No-image build: clean; 0 errors.
- pes_team_sheet_test 5/5; pes_offball_test 71 passed, 2 install-dependent skips.
- Repro, seed 7, 5 minutes, --pes21-root <workdrive>/PES21: GF mode `7ca3d08f68567016`, `--pes-loop` `67d115b984bab251`. Both equal main's.
- PII scan: pass.
- Not run: the full gate (Main gates). The 52 closure members' bit-exact status is unchanged: unrecorded.

## What's left, in order

1. Step 7 remainder, 135 loop-shaped functions: a loop structurer (while/do-while with break) on the same CFG; the flow refuses any back edge today.
2. Step 7 remainder, 83 functions with unmodelled control lines: inspect the lines first (switch or computed-jump shapes) before choosing a form.
3. Step 7, 1 function over budget: raise the copy budget or split nodes.
4. Step 5: a boundary change in translate_one (real parameters and return, PesState kept as an internal image) plus a state adaptor for the harness and state-based callers. Needs entry-liveness with partial xmm writes handled. 168 candidates.
5. Step 6: definite-assignment for stack slots in the 101 call-free candidates; outgoing call arguments stay in memory.
6. Step 8: verify fieldmap bindings before naming any accessor.
