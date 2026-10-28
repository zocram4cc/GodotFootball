# pes-port-TrapClipPort: the kind-0 trap chain decoded; the clip port is NOT landed (28-10-26)

**Status.** The port is not done. `pes_stall_test` `ALiveBallNeverLiesStillLongEnoughForTheBreaker` is still RED (147 live firings, inherited from `pes-port-OwnerPickup2.md` §7 at the same code, `8eee7bfb`; not re-run). No code changed, so no `pes_*` binary was rebuilt or run and no mutation was made. Pushed: `StoppageFlow` at `c4a659f6` (docs only: `docs/re/pes-re-trap-kind0.md` new; `pes-re-trap-pick.md` and `pes-re-first-touch.md` corrected). `git ls-remote origin StoppageFlow` confirms `c4a659f6`.

**Why the port is not landed.** The kind-0 clip choice is not the row picker and not the frame race. It is a commit/search chain of about two dozen PES functions, most through vtables on the anime and action objects. GF has no PES layout for those records (`src/pes/movestep.hpp` and `derivation.hpp` cover only part of the anime interface), so a translate-and-prove or a hand port cannot be wired to GF state in this session. Details and numbered blockers are in §4.

## 1. What was decoded (VAs, dec.sh headless, image bytes)

Verdicts: CONFIRMED = decompile or image bytes; INF = inference (reason given); OPEN = not decoded.

- **Dispatch.** `0x1407999F0` (the Trap `+0x68` slot) is `E9 CB 67 00 00` = `jmp 0x1407A01C0` (CONFIRMED, image bytes). This reconciles `pes-re-trap-pick.md` (which named `0x1407999F0`) with OwnerPickup2 (which traced `FUN_14079A470`): both are right. `FUN_1407A01C0` reads the request kind (`request[2]`); kind ≤ 0 calls `FUN_14079A470` directly (CONFIRMED, `dec_trap68.txt:1-59`). `FUN_140799DB0` is not on this dispatch path (its caller OPEN).
- **Kind-0 control touch, `FUN_14079A470`** (CONFIRMED structure, `dec_kind0.txt`). Ordered steps: admission flag; `FUN_140798A70` refusal (OPEN body); reach test on ball samples (threshold `0x1425A9464` = 36.0, squared m²); height class `FUN_14084BCA0`; race refusal `FUN_140799440`; query `FUN_14084DB00`; bearing predicate `FUN_14085C070` (threshold `0x14259BE34` = 50.0°); commit `FUN_14079AFD0`; fallback `FUN_140857220` and `FUN_14079D310`.
- **The race is a refusal, not a clip.** `FUN_140799440` (CONFIRMED) returns 1 if `FUN_14079F920` (OPEN body) or `FUN_14079FBC0` (bool, CONFIRMED structure, lines 344–403 unread) passes at a frame in `[int(0.66·t0), t0)` (`0x1425CD060` = 0.66, script-read). A 1 makes `FUN_14079A470` return 0 (CONFIRMED control flow). The semantics "teammate wins the race" are INF.
- **The clip search** (CONFIRMED structure; per-candidate test OPEN). `FUN_14079AFD0` (commit, CONFIRMED structure) → query `FUN_14084DF20` → select `FUN_140857250`, with fallback `FUN_140857220 → FUN_140858B90` (three flag passes, `anime+0x19/+0x18/+0x1A`) → `FUN_1408586A0` (loop of `anime+0x70` states 0..5) → `FUN_14085B900` (loop of candidate index `anime+0xDC`, count via vt `+0x90` = `FUN_14084B440`) → `FUN_140858260` (one candidate; calls `FUN_14085B3E0`, UNREAD) → ranking `FUN_14085F150` (`DAT_1425F20C0`, `DAT_1425F2108` tables, UNREAD). No call to `FUN_14081FF70` appears among the decoded callees (CONFIRMED). Whether the candidates are motion-table clips is INF until `FUN_14085B3E0` is read.
- **Output.** `*param_6` = a refusal status (1, 3, 7, 8, 9), or on success `auStack_78[0]` of the selected candidate (CONFIRMED write; INF that this is the clip key). Return 1 = issued.
- **Touch frame for kind 0.** Not computed in this chain (OPEN: Trap `+0x98` slot `0x1407981F0`, `FUN_1406B3D60`). The entry-3 TRAP sets contact frame `FUN_1406B3D60(anime,7)` (`pes-re-first-touch.md` §0).

## 2. Constants (script-read from `pes_full.bin`; VA → value)

`0x1425A9464` = 36.0 · `0x14253EE30` = 100.0 · `0x14259BE34` = 50.0 · `0x1425CD060` = 0.66 · `0x1425CB30C` = 112.5 · `0x14259BE30` = 45.0 · `0x14259BE5C` = 90.0 · `0x142588200` = 0.1 · `0x14259BDDC` = 1.5 · `0x1425A3590` = 1.1 · `0x142565600` = 0.5 · `0x142598580` = 180.0 · `0x14259858C` = 360.0 · `0x1425F1217` = u8 20 · `0x1425F121E` = u8 10. Full table in `docs/re/pes-re-trap-kind0.md` §6.

## 3. Checks requested in the brief

- **G1 ball-distance limit.** Constant: correct. `0x14253EE30` = 100.0 m² = 10 m (script-read). GF reads the same VA (`pesreceive.cpp:119`) and gates `ballDistance² ≤ 100`. No change to the constant.
  - Quantity: different. PES `FUN_1409FA400` (CONFIRMED) reads the presser's own C1 entry `MA+0x24710 + (MA+0x247BE[id] + 11·team)·8` (f32 +0). GF computes a fresh 2-D distance from `Predict(0)`. The C1 writer is OPEN. Not changed in this ticket.
  - "GF passed the presser at 14.5 m": **not reproduced.** At this HEAD the code passes G1 only when `ballDistance² ≤ 100`. The distance column in the OwnerPickup2 probe logs was not re-read (OPEN).
- **G5 designation stand-in.** Not removed: the chain it would depend on is not landed.
- **`pes-re-first-touch.md` §7.6(b).** Corrected: the trap touch frame is already wired (`animcollection.cpp:603-608`, `humanoid.cpp:2714-2718`); "touchFrame = −1 for PES clips" is stale for traps (the hit frame is `HitRecord::frame`, `mbinfo.hpp:36`). Frame 18 for 0x547 remains the live reference, not re-verified here.
- **`pes-re-trap-pick.md` §1, §5 item 4.** Corrected: `FUN_14079FBC0` is a refusal race, not the clip search.

## 4. Unfinished (numbered; each names the blocker)

1. **Port the kind-0 clip choice.** Blocked on decode and struct layout: `FUN_14079AFD0`'s callees (`FUN_140857250`, `FUN_14079D310`, `FUN_14084DF20`, `FUN_140798D50`, `FUN_140848900`, `FUN_14084B810`, `FUN_14084B9C0`, `FUN_140846B20`); `FUN_14085B3E0` (the candidate test); `FUN_14084E910`; `FUN_1406E69F0`. GF has no PES layout for the anime `+0x70/+0xDC/+0xAB8/+0x3B18`, action `+0xC40`, or query records (`+0x6C`, `+0x20`). Settles: a full decode of the commit and candidate tests, then a struct map for those records.
2. **Harness proof.** Not started. Blocked on 1 (vtable stubs for the candidate test and motion reads; the pipeline's "stub model wrong" and "unmodellable callee" classes). Real-call capture banks are needed for live anime state.
3. **Wire `_TrapCommand` and delete `pes::PickTrapClip`.** Not done (depends on 1 and 2). `PickTrapClip` is still reached only from `_TrapCommand` (`playercontroller.cpp:515`); `FUN_140769FE0`'s row picker is not on the kind-0 path (CONFIRMED by callee check), so the delete is still expected to be clean.
4. **Red-first test and mutations.** None written: no code changed. The stall test is the oracle and is still red.
5. **`pes_*` sweep.** Not run (no code changed). Baseline inherited: 95 green, 1 red (`pes_stall_test`), `OwnerPickup2` §7 at `8eee7bfb`.
6. **Kind-0 touch frame.** OPEN: `0x1407981F0` (Trap `+0x98`) and `FUN_1406B3D60`.
7. **`FUN_14079FBC0` lines 344–403; `FUN_140A4D340`, `FUN_140439E50`, `FUN_140A8FF10`; `FUN_14079F920`; `FUN_140798A70` (280 lines).** Unread refusal and race helpers.
8. **`FUN_140799DB0`'s caller.** OPEN.
9. **C1 writer for G1 (`MA+0x24710`).** OPEN. Decides whether GF's `Predict(0)` distance is the same quantity PES gates on.
10. **Probe distance column behind "14.5 m".** Not re-read (OPEN).

## 5. Blockers needing a decision

None requiring user input. The next step is the scope call on item 1: a full kind-0 decode and struct map (multi-session), or a narrower target that the owner accepts as a stand-in for the clip search.

## 6. Verification run

None. No code changed; docs only. The only command output used: the image-byte reads for `0x1407999F0` and the constant table, and the `dec.sh` decompiles (`dec_chain1..6.txt`, `gf-OwnerPickup2-scratch/dec_{trap68,kind0}.txt`).
