# PressLane9: the press tick 0x140928FC0 per man (PES's player driver chain). Map, discrepancies, merge, no landing

Branch `PressLane4`, worktree `<workdrive>/gf-PressLane4`. Pushed `3d9ca8ba9` (Merge origin/Integrate `3ed0f35d9` into PressLane4; previous tip `f95743c16`; `git ls-remote origin PressLane4` confirms). Scenario claimed: **none**. Nothing lands. No behaviour change this round.

## 0. Verdicts

| # | Task | Verdict |
|---|---|---|
| 1 | Run the press tick per man where PES runs it; PES per-man object layout; delete the GF pieces it subsumes | **Not landed.** The chain is settled end to end (§2). The tick cannot run per man in GF until the driver `0x140925470` is translated and proven and the layer regenerates. Nothing deleted. |
| 2 | Measure ALoose, ADesignated, PesStallQuick, census; ALiveBall from Main; land M1/M2 | **Not measured** (no behaviour change). No M1/M2 claim. §5 gives the baseline plan. |
| 3 | ADesignated under M3 | **No decision.** The tick's output for the designated man is the evidence M3 needs; it needs §3 blocker 1 first. |

## 1. Brief discrepancies (CONFIRMED against the image; reported, not ported against)

- **A0 offset.** The brief says A0 sits at `X+0xFE70`. `docs/re/pes-re-press-owner.md` §7 gives `A0 = X+0x10E70` (= M+0xF518, M = X+0x958) and `A2 = X+0xACE8`. The WIP patch uses `0x10E70`. The tick's code uses `A2 = rsi+0xACE8` (`lea rax,[rsi+0xace8]` at 0x1409290D0). I use `0x10E70` and `0xACE8`.
- **Owner/ctx offsets.** The brief says the tick refills A2 from `X+0x17D8` (owner) and `X+0x17E0` (ctx). The decoded tick reads `X+0x7D8` and `X+0x7E0`, with `X = rsi`:
  - `1409291BA mov r9,[rsi+0x7E0]`, `1409291C1 mov [rsi+0xAD20],r9` (A2+0x38 = ctx);
  - `1409291C8 mov rax,[rsi+0x7D8]`, `1409291CF mov [rsi+0xAD28],rax` (A2+0x40 = owner);
  - `14092959A cmp qword [rsi+0x7D8],0` (owner gate);
  - `1409294F4 mov rax,[rsi+0x7D8]`, then `[rax+0xA08]` (frame builder gate, 0x140929505 / 0x140929516).
  The `0x1000` difference matches neither `docs/re/pes-re-press-owner.md` nor PressLane2's live reads (which saw `0x17D8/0x17E0` on seq3076). I trust the decoded displacements (the image's bytes). The live conflict is OPEN; a capture of `rsi` at the tick's entry settles it.

## 2. The per-man chain (settled; VA-cited)

Main's settlement (pes_full.bin + the `pt_140928FC0` captures pid2282538 seq3076/5518; return-address chain E+0x50 = 0x14092576E), checked against the image:

1. **Wrapper `0x140925750`** = vtable slot +0x10 of DATA `0x1426329B8` (= X's vtable `0x1426329A8` + 0x10; `inc [rcx+0x10]; call 0x1408BF090; call 0x140925470`). Neither `0x140925750` nor `0x140925470` is in the layer.
2. **Player driver `0x140925470`** (pdata `0x140925470..0x14092574A`). `this = rbx = X`, `rdx = rdi`. Arm: `0x1409256EB call 0x140926B00(rbx, r15, [rbp], rdi)`; `mov byte [rbx+0x7CA],1`; if `al != 0`: `0x14092570A call 0x140929930(rbx+0x1A4D8)`, `[rbx+0x19E88] = rax`, `0x140925719 call 0x14058F0F0`, then **the tick at `0x140925724 call 0x140928FC0` (rcx = rbx, rdx = rdi)**, clear `[rbx+0x1A4F8]` if set, `byte [rbx+0xC76D] = 0`. The tick runs only when the state query `0x140A60550(rsi, edx=[rbx+0x18])` returns something other than 0 or 1; state 2 also runs `0x140926890` first (`0x1409256C2..0x1409256D0`).
3. **The press tick `0x140928FC0..0x140929606`** (translated, `gameplay/pes_fn_140928FC0.cpp`):
   - zeroes the A2 frame words (`0x1409290D0..0x140929108`, straight-line, every tick);
   - copies ctx and owner into A2 (§1);
   - calls the AI **think** `0x1409257B0` (`0x1409294E4`; `rcx = X`, `rdx = A2`, `r8 = X+0xAD38`, `r9 = r15`);
   - calls the counter-B writer `0x14092ED30` (`0x1409294EF`, `rdx = r15`);
   - if `[[X+0x7D8]+0xA08] != 0`, calls the frame-builder wrapper `0x1409F20C0` (`0x140929516`, `r8 = X+0xAD38`, `rdx = A2`);
   - then the switch/press part (`0x140928710`, `0x140A3F560`, `0x140A6BF50`).
4. **Per-man action (ActionPress path).** The presser action A0 has vtable `0x142634948`; its slot +0x10 (`.rdata` qword `0x142634958`) is `0x140961FB0`, the per-man action tick **G**. G calls `0x1409634B0` (`0x140961FD3`) and then the driver `0x1405141F0` (`0x140961FE7`), which dispatches ActionPress through A0's vtable (`docs/re/pes-re-press-owner.md` §3). `0x140961FB0` is NOT in the layer.

Order (INF from the call graph): within a man, the driver runs the think and the tick (AI), then the action G (ActionPress). The team-tick position of the driver relative to `RunPesTrapTicks` / `RunPesDribbleTicks` (`match.cpp:898-903`) is OPEN.

GF already holds the driver's slot record: `pestrapactor.cpp:228` cites `FUN_140925470: slot record + 0x15C0` (`kSlotPPtrTargetOffset`). So the port point exists.

## 3. OPEN blockers (named, with the decode that settles each)

1. **Translate and prove the driver `0x140925470` and wrapper `0x140925750`.** Neither is in the layer, so GF cannot call them. The closure goes through the tick (in the layer); the driver's gate `0x140926B00`, `0x140A60550`, `0x140926890`, `0x140929930`, `0x14058F0F0` must be checked for translation. Proof: `prove_bounded` on the `pt_140928FC0` captures (seq3076, seq5518, seq8188 of pid2282538, per PressLane3). Main to confirm they are kept.
2. **Writers of X+0x7D8 (owner) and X+0x7E0 (ctx).** Not in X's ctor `0x14091E9F0`. The image's writes store one register to both fields (`0x1407D1F1D/F2B`, `0x1408BD9B6/BD9BD`, `0x1408BDE7F/BDED1`, `0x141181178/1817F`), so they are other objects' ctors or clears; `0x1411825E0` and `0x141180540` are constructors. The per-slot writer of owner = P and ctx = the team ctx is OPEN. PES's values (owner = P, `PVa(slot)`; ctx = the team ctx, `CtxVa(side)`) are confirmed by `docs/re/pes-re-press-owner.md` §4 and PressLane2's live reads.
3. **Frame-builder gate.** The tick calls `0x1409F20C0` only when `[[X+0x7D8]+0xA08] != 0`. Whether GF's trap P has `+0xA08` set by PES's ctor is unchecked.
4. **ActionPress anchor (AI+0x1414).** Unchanged from PressLane8 §2: its PES writer is not found. The think is the candidate (it writes the AI record); untested.
5. **Designated owner (ADesignated).** Unchanged: PES's designated owner is refused reaction 3 and admitted the DRIBBLE tick's TRAP 7 (`tasks/23-11-26.md`, PressLane6/7).

## 4. ADesignated (task 3)

No decision. M3 needs PES's designated-owner behaviour. The tick's output for that man is the evidence; it needs §3 blocker 1 (and 2) first. The existing ADesignated premise is GF's stand-in (`pressMan` in `elizacontroller.cpp`: designated man with no holder).

## 5. Measurement plan (not run)

Baseline on the merged tip (no code change): `pes_press_approach_test` (ALoose, ADesignated, ANoMan) and `pes_restart_delay_test --gtest_filter=PesStallQuick.*` (Quick), plus `PesStall.ALiveBall*` (~20 min, Main's to run). Last recorded on Integrate `c7fa9d01d`: ALoose and ADesignated red, Quick 53 (bound 53), ALiveBall 2229 (bound 2229).

## 6. Merge state

- Pushed `3d9ca8ba9`. Previous tip `f95743c16`.
- `results_trapaction.json` resolved by `json_union_merge` on both merge bases (c4404052, 0f6fafc5): outputs byte-identical.
- `gen_translated.py --image pes_full.bin`: wrote 11,191 functions (4,161 recorded bit-exact, 7,030 closure). `--check`: byte-identical, rc 0 (log `gf-PressLane4-scratch/p4/gen.log`).
- The merge carries Integrate's own changes only; no behaviour change in this lane. Ratchet not re-measured here.

## 7. Verification

- Tests run: none (no code changed by this lane).
- `gen_translated.py --check`: rc 0, byte-identical (11,191).
- PII: `grep` of every staged file (803) for `<workdrive>` and `<home>`: no hits. `tools/check_pii.sh` not run (the brief says run it from `<workdrive>/GodotFootball`, which this lane does not write to).

## 8. Artifacts (scratch only)

- `gf-PressLane4-scratch/p4/trace.py` / `trace.txt`: capstone linear sweep of `.text` (`va: mnemonic op_str`).
- `gf-PressLane4-scratch/p4/pdata.py` / `pd.s`: sorted `.pdata` ranges (file offset = RVA).
- `gf-PressLane4-scratch/p4/fn.py`: VA to containing function, and its call sites.
- `gf-PressLane4-scratch/p4/refs_slottbl.txt`: RIP-relative refs to `G = 0x1436F3DC8` and the slot table `0x1436F3FC0`.
- `gf-PressLane4-scratch/p4/gen.log`: regen and check log.

## 9. Rig requests

1. Hook `0x140925750` (wrapper, entry) FIRST 2 EVERY 1, cap 6, FULL: `rcx` (X), `rdx`, `[X+0x7D8]`, `[X+0x7E0]`, return address. Settles §3.2 and checks §1.
2. Hook `0x140961FB0` (A0 per-man G) FIRST 2 EVERY 20, cap 6, predicate `d(q(q(0x1436F3DC8)+0x270)+0x116c)==1` (in play). Settles the G order and the anchor writer order.

## 10. Next exact steps

1. Main: confirm the press tick captures (pid2282538 seq3076/5518/8188) are kept for the driver proof.
2. Translate the driver `0x140925470` (and wrapper `0x140925750`) with `closure.py`, prove on those captures, record the gate's translations (`0x140926B00`, `0x140A60550`, `0x140926890`).
3. Wire GF: replace `RunDecisionFrame`/`RunFrameBuilder` (`teamAIcontroller.cpp:3637`) with the driver call per slot at PES's position; delete the designated stand-in in the same change; red test first; then measure (M1/M2).
4. Find the owner/ctx writer (§3.2) and the `+0xA08` writer (§3.3) before step 3 can be faithful.
