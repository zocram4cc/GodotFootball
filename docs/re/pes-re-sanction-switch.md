

# PES 2021 sanction switch decode — final report (archived by Main from the agent's final message)

Read-only RE. Image `<local>/ghidra-projects/scratch/Unpacker/pes_full.bin` (offset = VA − 0x140000000). **Ghidra bridge DOWN the whole pass** → capstone on the image + `.pdata` bounds + existing rendezvous snapshots (no new captures, no writes to PES). Scratch: `<workdrive>/gf-SanctionDecode-scratch/` — `tree.asm` (full 2771 B disasm of 0x14047F170), `no_pdata.asm` (non-`.pdata` callees), `oracle.py`/`oracle.txt` (capture table, 20 rows), `consts.py`/`consts.txt` (script-read constants), `img.py`, `memlib.py`, `scan.py`, `funcs.pkl`. Labels: CONFIRMED = disassembly/`.pdata`/snapshot this pass; INF = inference, reason given; OPEN = blocker named.

## (a) Objective and scope
Unblock the sanction switch 0x14047F170 per FoulOffside4 §9.1: (1) partition its callee tree DECISION vs PRESENTATION with decision-dependency per presentation callee, (2) decode the A1 owner (vt 0x14259D590), 0x14042D5E0, 0x140A81230, (3) kind-4 record +0x20 and the `dl` flag in 0x140A3A690, (4) spec the scene id for the Godot hybrid layer, (5) oracle table from real foul-stop captures.

## (b) Findings, by item

### 1. Callee partition of 0x14047F170 (2771 B, `.pdata` CONFIRMED; full disasm `tree.asm`)

**DECISION (must run in GF):**

| VA | role | evidence |
|---|---|---|
| 0x140A3A500 (468 B) | 35 m assistant-involvement / Z-rule test | CONFIRMED (FoulOffside4 §1, boundary tests; already ported, pushed `b00f6a7e`) |
| 0x140A74D00 | period getter; reads G at 0x140A74D74 (OP-VA 0x1436F3DC8) | CONFIRMED; live periods 1 (seq096) and 3 (seq084/090) observed |
| 0x140A81230 | card-history / card-level lookup over A1's table (A1+0x6C, 40×12 B). Body: r8=1 path → 0x140A80EF0 → 0x140A80D10 (no-`.pdata` fragment); accessor 0x140A3E8A0 = `lea rax,[rcx+0x750d0]` (CONFIRMED, 0x140A3E8A0–0x140A3E8A7); tail-jmp 0x1408C9DF0 (fragment). No rip-relative data reads of its own — operates on the A1 argument | call structure CONFIRMED; "(kind, player)" key and 0/1/3 return mapping INF (matches sanction-caller §open-4) |
| 0x140A81040 | minute gate: 0x140A3C050→0x140A3C070 computes **FNV-1a64** (offset 0xcbf29ce484222325 at 0x140A3C111, prime 0x100000001b3 at 0x140A3C0E7) over a 4-byte clock entry and compares with the block's +0x10 field; 0x140A6BD80 (fragment) converts to minutes; called at tree site 0x14047F791 as `A81040(A1, period, 0, 0x1E)` | CONFIRMED (disasm); called only when period==3 (0x14047F77A–0x14047F78E gate) |
| 0x14042D5E0 | leaf integer-arithmetic helper; **no rip-relative reads at all** (pure argument math) | pure arithmetic CONFIRMED (disasm + consts scan); semantic label OPEN — blocker: typed bridge decompile of the 3-instruction tail; disasm in `no_pdata.asm` |
| 0x140A3A690 (468 B) | foul-kind classifier, `dl` parameter (see item 3) | Z-rule branch CONFIRMED; outer flow INF |

**PRESENTATION (GF stubs or Godot route), with decision-dependency:**

| VA | role | reads / writes the decision later depends on |
|---|---|---|
| 0x140480910 (707 B) | sanction builder. Entry (hooks): rcx=req, rdx=A1, r8=Flow+0x2790, r9=Flow. Writes req+0 = scene code, `[r15+0x210]` flag (setne of 0x140A3FE70), `inc [r14+0xd]` (0x140A3FD20 test); yellow-card case CONFIRMED at 0x140480A8B–0x140480AAC: `cmp [rbx], 0x20022` → card-display state 2 via 0x1403FB5B0, else generic path 0x140480AB1 | **Writes the A1 card table (+0x6C) and req block — the NEXT foul's 0x140A81230 reads that table, so the card-record write must be emulated in the GF core (or GF records the card itself).** A pure no-op stub WOULD change the next decision. Its reads (stoppage event code `[rbx]`) are not decision-dep. |
| 0x140473E20 | in-match HUD card (FoulOffside4 §4.2) | reads A1/req for display only; writes HUD state; decision never reads HUD state → **safe no-op stub** |
| 0x141794410 | in-match menu (FoulOffside4 §4.2) | menu state only → **safe no-op stub** |
| cutscene caller(s), FoulOffside4 §4.2 set (0x1414DAB…/0x140A89D0 family) | cutscene playback | consumes the scene id (item 4); writes playback state the decision never reads → **safe stub; scene id routed to the Godot hybrid layer** |

### 2. A1 owner, 0x14042D5E0, 0x140A81230

**A1 = the match's ANALYZE component object** — CONFIRMED. A1 lives at `owner+0x228` (owner = Flow+0x38). A1's first qword (its descriptor/vt) = **0x14259D590**, whose blob contains: slot 3 = string `"ANALYZE"` (0x14259D5A8, CONFIRMED read), slot 6 = **0x140458A10** (the ANALYZE message dispatcher, pes-re-offside §2.1 places it at 0x14259D5C0 — matches slot 6 of this blob), slots 7–23 = float easing constants (0x3FB33333… ramp array). Constructor 0x140457030 (62 B, `.pdata`): stores the descriptor (lea operand 0x14259D569), calls 0x140456900, and on `edx&1` allocates 0x28 via 0x140227930. So A1 is the same offside-ANALYZE object documented in `pes-re-offside.md`; the tree uses it as the match-rules context (card table +0x6C, kind-4 record +0x18, +0xE0 pointer). A1 layout from snapshots: +0x18 kind-4 record block, +0x6C 40×12-byte table, +0xE0, +0x24C fallback (dump for seq084 in `oracle.txt`).

**Owner O = Flow+0x38**: first qword 0x1425984C0 (descriptor blob among event-name strings CONCEPT_ARRANGE_RELEASE 0x142598300, GOAL_POST 0x1425983D8, VANISHING_SPRAY 0x1425984A8). Fields used in the sanction path: +0x18, +0x228 (A1), +0x38 (controller ptr, null-checked at 0x140423341), +0x6E0 (payload). Its event dispatcher **0x1404232E0** (107 B + fragments) switches on event id `[r8]`:
- 0x105001A → A1 build/publish chain: gate `*G+0x3B0` via 0x1423E7FD0 (0x140423321–0x140423341) → 0x140440D50(&out, &O+0x228, r8=&O+0x18) [0x14042335C] → 0x140440E60(&O+0x6E0) [0x140423368] → 0x140444000([O+0x38], &O+0x6E0) [0x14042337D] → 0x1404544E0([O+0x38], payload) [0x140423389] → 0x140445E70([O+0x38]) [0x140423392]
- 0x105001D → 0x1404455B0; 0x1050028 → 0x140452490; 0x105005C → 0x1404520F0 (same gates)
All CONFIRMED from disasm. Role: O is the **match-flow stoppage/restart event object** (provisional name `FlowStoppage`). Class name OPEN — blocker: no Itanium name string within ±512 B of 0x1425984C0; needs bridge or wider RTTI scan.

**0x14042D5E0**: leaf, pure integer arithmetic, zero memory reads (CONFIRMED); called in the tree's score-related branch. Inputs/outputs are the two score dwords vs an int result feeding the card-escalation branch (INF — reason: register flow only, no bridge typing).

**0x140A81230**: see partition table above; the open piece is the exact key/return semantics (INF; the 0/1/3 = card-level reading matches the observed A1+0x6C small-field values 0/1/2 in the seq084 dump).

### 3. Kind-4 record +0x20 and the `dl` flag

- **Kind-4 record +0x20: OPEN.** Not the fouler, not the victim slot (FoulOffside4 §3.3); values 0/1 across all captures read (seq084/090 period 3, seq096 period 1). Blocker (exact): perf write-watchpoint on `A1+0x18+0x20` across one card event, or a full bridge decompile of 0x140480910's remaining 600 B (the 0x20022 gate region is done; the rest is in `tree.asm`-adjacent scratch).
- **`dl` flag of 0x140A3A690:** the **Z-rule (35 m) branch is CONFIRMED** by the FoulOffside3/4 boundary tests. `dl` sourcing CONFIRMED from the call-site disasm: foul-stop caller loads `dl` = the foul-kind dword from the MI foul record; the in-play caller loads **MI+0x1544**; one additional call site's `dl` is dead (overwritten before use). Outer control flow (how the kind switch orders around the Z test) **INF** from the translated body — blocker: bridge decompile of the 468 B (mechanically trivial once the bridge is up) plus a `dl`-argument trace.

### 4. Scene id → Godot consumer spec

- **Scene id = the tree's return (rax).** CONFIRMED: RefReplay3 closure sweep replayed 0x14047F170 bit-exact on 7/10 of the captures (RefReplay3 §Table 2.9; `480910` 7/10 bit-exact, 3 aborts from import-think); observed value **0x20008** (live2_seq090, case 0). Builder 0x140480910 copies the same code to req+0 and special-cases **0x20022** = yellow-card event (CONFIRMED gate 0x140480A90). Format: `0x20000 | code` (INF on the full code enumeration; 0x08 and 0x22 observed).
- **Spec (what GF/Godot should consume):** after the GF referee core runs the ported decision tree, it emits `SanctionDecision { u32 scene_id; u32 card_level; u32 restart_kind; }`. The **Godot hybrid cutscene layer takes `scene_id` as its sole selection input** (owner ruling 26-10-26: cutscenes hybrid — PES selection/direction logic translated, Godot driven from PES scene data). The in-match HUD (PES 0x140473E20 role) and menu (0x141794410 role) become Godot presentation nodes fed by the same struct; the GF core keeps only the A1 card-table write (item 1) for the next-foul decision. The scene_id → cutscene-asset map itself is the CutsceneRoles pass's output — OPEN here, blocker: that table decode is not finished.

### 5. Oracle for the GF red test

`oracle.txt` (scratch) = 20 rows from the real foul-stop captures (stop=5): live (old launch) seq083–090, 095–098 with retained replay-read pages, live2 (new launch) seq088–095. Per row: inputs (foul kind, fouler id, `bVar26` injury/excite flag, G+0x24D0 per-foul record ptr, period, played frame MI+0x1198, A1 fields: +0x18 kind-4 block, +0x6C table, +0xE0) → outputs (scene id from the bit-exact replay rax; card level from the 480910 replay; restart from the captured post-stoppage state). Worked rows (full inputs): seq084 (fouler 17, bVar26 1, period 3), seq090 (fouler 4, bVar26 0, period 3, **scene id 0x20008**), seq096 (fouler 4, bVar26 0, period 1). Honest limits: (i) the 3 replay aborts (import-think, RefReplay3 §7.2: 0x144252Fxx OPEN, 0x144C880/0x140A9890 indirect dispatch unmodeled) are excluded until the import model lands; (ii) restart column is capture-observed, not replay-proven (44C880 0/11, 445E90 0/4 unmodeled); (iii) the EXCITE region 0x14047F77A–0x14047F7A9 (A81230==0 && minute path) was reached in **no** capture (fingerprint scan, all 20 snaps) → the red test does not need that branch yet.

## (c) Constants (script-read, `consts.txt`, 40 lines — instruction VA → operand VA → value)

- 0x14047F19F → OP-VA 0x14350B340 (global table ptr, u32 1239127280)
- 0x14047F1BD/313/3C4/4C7/5B8/628/67C/7D6/94C → OP-VA **0x1436F3DC8** = the G global (all tree/global reads resolve here)
- 0x14047F6C1 → OP-VA 0x142565D30 (all-ones SIMD mask)
- **0x14047F6CC → OP-VA 0x1425A2500 = 31.0f** — the tree's late-game minute threshold (`comiss`)
- 0x140A3B53B → OP-VA 0x14351AE30 = 0xFFFF
- 0x140A3E155/1A4 → OP-VA 0x1436F3A40 (u32 55); 0x140A3E1CB → OP-VA 0x1436F3D60
- 0x140A48A50, 0x140A74D74, 0x140A8104F/080 → OP-VA 0x1436F3DC8 (G)
- 0x140A81230 / 0x14042D5E0: **no rip-relative data constants** (CONFIRMED — argument-only functions)

## (d) Code changed
None — read-only task, per assignment.

## (e) Tests / verification run
All verification is RE-side, actually run this pass: `.pdata` size checks, full disasm of 47F170/480910/4232E0/81040/3C070/3E8A0/457030, builder-gate re-verification (0x140480A8B–0xAC), live snapshot reads (MI+0x1168 clock block, MI+0x1198 frames, A1 tables, owner layout for seq084/090/096), fingerprint scan of all 20 sweep snaps (0 reach into the EXCITE region), constants script-read. No GF code touched, no builds/tests applicable.

## (f) Differential fidelity
Inherited, not re-run: RefReplay3 §2.9 — 480910 7/10 bit-exact, 47F170 tree replay bit-exact on those 7 (148 snapshot pairs, 7 hooks PROVEN). That is the oracle's provenance.

## (g) Unresolved / blockers
1. Owner class name (0x1425984C0 blob) — no RTTI name within ±512 B; needs bridge or wider scan.
2. Kind-4 record +0x20 — write-watchpoint or full 480910 decode.
3. 0x14042D5E0 semantics and 0x140A81230 key/return mapping — bridge decompile.
4. `dl` outer control flow in 0x140A3A690 — bridge decompile (mechanically trivial).
5. Scene-code enumeration beyond 0x20008/0x20022 and the scene_id→cutscene-asset map — CutsceneRoles pass.
6. Ghidra bridge was down all pass; every `.pdata`-free fragment (80D10, 6BD80, 8C9DF0, 4232E0 tail) decoded by linear capstone only.

## (h) Recommended next step
Re-check the `consts.txt` operand-VA pairs and the `oracle.txt` rows (Main's stated acceptance), then the single highest-value unblock: **bridge decompile of 0x140480910 (707 B) + 0x140A3A690 (468 B)** — one session closes blockers 2, 3, 4 and finalizes the stub contract for the 480910 hybrid (which bytes the GF core must replicate into the A1 card table).

## Main's note (28-10-26)

- Re-checked: `0x1425A2500` = 31.0 (the late-game minute threshold).
- The Ghidra GUI bridge was down, but the headless decompiler works (`dec.sh`); Main decompiled `0x140480910`, `0x140A3A690`, `0x14042D5E0` and `0x140A81230` with it (scratch `<workdrive>/gf-SanctionDecode-scratch/dec/dec.txt`), which is the input for blockers 2-4.
