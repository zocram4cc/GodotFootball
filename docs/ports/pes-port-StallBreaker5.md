# pes-port-StallBreaker5: the three slide-action handlers are now in the layer; NOT committed, NOT proven, no SHA; the regeneration changes 10 other layer files

Status: PARTIAL, no commit. Branch `StallBreaker` is still `09a5f7e2` (pushed). The worktree holds an uncommitted regeneration. Main asked for one self-contained commit; I did not make it because the regeneration is not self-contained (section 3).

## 1. What the three handlers do (CONFIRMED, headless decompile `dec3.txt` in scratch)
- **0x14073DD50** (SLIDING-adjacent slot at 0x1425DF8B0 per survey): `param_2+0x39 = param_1+0x1C` (one byte copy). No calls.
- **0x140791DD0** (slot 0x1425E1AD8): copies a word triple from `param_1+0x10..0x18` to `param_2+0x64..0x6C` when the address differs; sets/clears bits 0x40 and 0x200 of `param_2+8` from `param_1+0x20` bits 1 and 3; copies byte `param_1+0x68` to `param_2+0x44`. No calls.
- **0x140789D00** (SLIDING +0x90, slot 0x1425E1908): copies `param_1+8/0xC/0x10` to `param_2+0x64/0x68/0x6C`; bit 0x200 of `param_2+8` from `param_1+0x18` bit 1; reads an object `param_3+0xAB8` through its vtable (+0x20, then `[P]` byte `== 4` test, then `func_0x141E532D0` (translated, in the layer), then vtable +0x38 `< (int)(fps*k+0.5)`, which sets or clears bit 0x8000 of `param_2+8`); byte `param_2+0x45 = (param_1+0x14 == 1)`. The two vtable calls are dynamic: they cannot be closed statically; the proof needs the object's real vtable.

## 2. What exists now (uncommitted)
- `tools/dev/translate/gen_translated.py`: (a) `INDIRECT_EDGES["0x1406F6CE0"] = (0x14073DD50, 0x140789D00, 0x140791DD0)` with the survey citation; (b) the closure walk admits an indirect edge's target even when the set's universe lacks it (before, the three were dropped: the kickjoints set's universe does not hold them). Without (b) the edge was silently ignored (verified: gen2 wrote no file for them).
- `tools/dev/diffharness/specs/trapaction/{14073DD50,140789D00,140791DD0}.json` and `specgen/sets/closure-slide.txt`: written by `closure.py --spec-set trapaction --tag slide` (pilot specs; no proof inputs yet).
- Layer (gen3, `gen_translated.py --image`, ~15 min): the three functions are **closure members**: `src/pes/translated/closure/pes_fn_{14073DD50,140789D00,140791DD0}.cpp` (new), in `pes_dispatch_gen.cpp` and `translated.hpp`. `untranslated_callees.py --image` on root 0x1406F6CE0 now walks 67 functions, 0 untranslated (this tool cannot see the dynamic vtable calls, which is why the survey found them).
- **Not proven.** These three are closure members with no record: "proven only through their diffs" applies, and no diff reaches them. The brief asks for a synthetic proof; I did not write the synthetic spec generator (the object at `param_3+0xAB8` needs a real vtable from a real capture, and I found no live capture of these handlers in `live2/`).

## 3. Why I did not commit (the gen run changed 10 other files)
gen3 also changed files outside the three handlers:
- **Deleted (6)**: `closure/pes_fn_{1406E5F50,140724820,140782570,140789E60,14079A310,141AF6E40}.cpp`. Each has a recorded copy elsewhere (e.g. `trapadmit/pes_fn_140789E60.cpp`), and the dispatch table points to the recorded copy, so the deletion looks like de-duplication. But the committed layer (merge commit `55ff8f64`) passed `gen --check` earlier, so gen's output moved between that check and now. I have not found the cause.
- **Modified (4)**: `closure/pes_fn_{1404121D0,1406E47A0,1415A174A}.cpp`, `objinit/pes_fn_140440FC0.cpp`. Three of them replace `pes_dispatch_missing(0x…)` (a runtime abort if reached) with a direct call to `pes_fn_1403FB730 / 1406E7A60 / 1403FB5B0`. That is a behaviour change on the sim path (a call that aborted now runs), not a neutral regeneration. The fourth (`pes_fn_1415A174A`) drops image-constant declarations.
- Cause of the change set: unknown; candidates are my universe edit (reverted, so no) and the closure walk change in (b). Settling it needs a gen run on HEAD's tree (~15 min) to compare.

Putting the regeneration into one commit would therefore ship those behaviour changes under a "three handlers" message. I left it uncommitted for Main's call.

## 4. Hook lines
`untranslated_callees.py --hooks --image pes_full.bin --first 1 --every 1 --prefix slide 0x140789D00 0x14073DD50 0x140791DD0` now resolves the three as translated, but printed **no hook rows** (the prologue check did not produce a `hooks.cfg` line; the tool exits 0 silently). Not resolved; a hook predicate still needs writing by hand: an in-play predicate on the SLIDING action (`P+0xAC8 == 11`), EVERY 1, cap 6, per Main's ruling, using the vtable slot at 0x1425E1908 for 0x140789D00.

## 5. Not done
- Synthetic proof of the three (needs an object with a real vtable and a capture of a slide-adjacent state; none in `live2/`).
- One self-contained commit, push, messages to MergeLane2 and StallLive13 (not sent: nothing committed).
- Walker census and fix (step 2) and the acceptance set (step 3): not started. The abort still stands on the pushed branch `09a5f7e2`.

## 6. Verification
| Check | Result |
|---|---|
| `gen_translated.py --image` (gen3) | ran, rc 0, 10,471 recorded+closure set; the three in closure; 10 other files changed (section 3) |
| `untranslated_callees.py` on 0x1406F6CE0 | 67 translated, 0 untranslated (static only) |
| `gen --check`, `ledger --check`, builds, tests, smoke | not run on the regenerated tree |
