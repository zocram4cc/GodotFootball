# PES port: RestingBall5: the failing tests surveyed, contact-search leaves and the action-4 start proven, GATE branch RED (05-11-26)

The agent ran out of budget before writing its report file; Main archived its final message here. Branch `RestingBall3` is pushed through `79ba4894` (merges main `b267af11`). The worktree `gf-RestingBall` holds uncommitted work (section 6).

## 1. Survey (`GF_PES_SURVEY`, one run per failing test)

| Test | Untranslated target | Hits | First frame | Caller |
|---|---|---|---|---|
| `PesForfeitMatch.ARedCard…` | `0x1407A38E0` | 16 | 8732 | `pes_fn_14079E5C0` (trap end handler) |
| `GoalSceneHold…/PesLoop` | `0x140862AE0` | 2 | 4370 | `pes_fn_14085E3C0` (contact search) |
| `pes_throwin_release_test` | `0x140862CE0` | 8 | 3477 | `pes_fn_14085E3C0` |
| `pes_restart_delay_test` | `0x1407A38E0` (51, f7506), `0x140862CE0` (1, f6970); a second batch adds `0x1407A38E0` (59, f28284), `0x140862AE0` (1, f222) | | | |

- The restart survey is incomplete: one batch timed out at 3600 s inside `TheSeedTwentyThreeKickOffRestarts`, and the second was cut off before finishing.
- The only static caller of `0x140862CE0`, `0x140862AE0` and `0x140861170` is `0x14085E3C0`. `0x14085E1C0` is a callee of the first two, so the closure holds it.

## 2. Is `0x1407A38E0` on PES's path?

**Yes.** RestingBall4's "PES never calls it live" was wrong: a real PES call exists, `live2/pid615595/seq7805` (`act4_start_7A38E0`, return address `0x14079E6C5`).

- The man in the Trap action (`P+0xAC8 = 0x02020707`) is the holder: `P+0x2458 = 15`, `MA+0x2601C = 15`.
- So `FUN_1406EC5F0` is true, and the trap end handler (`param_6 != 0` branch) starts the Dribble action through vtable +0x58.
- GF's first hit has the same shape: slot 17 holds the ball, `hasPoss = 1`, the ball is at rest, and he is in Trap.

**Open:** GF re-issues `_TrapCommand` for one man every 2–3 ticks while the ball lies still (`speed 0.00–0.19`; player 8 from f416, player 26 from f8699). It is not shown whether PES issues those repeats. The action-switch call from the dispatcher (`pes_fn_1406EEBB0`, ret `0x14079E68D`) was not examined.

## 3. Translation and proof

- **`0x140862CE0` and `0x140862AE0`** (closure `trapcontact`, 35 members), SYNTHETIC only:
  - Each state is a real trap tick (`pid1572516/seq398`, No-Mod) walked with PES's bytes to the contact search, then a motion word poked so the leaf's gate opens. Specs: `synth_specs/trap_contact_leaf*.json`.
  - `0x140862CE0` is 9/9 bit-exact (own-block 0.9787); `0x140862AE0` is 2/2 (0.5926).
  - No real call has been captured yet; the `rb5_*` hooks are live.
- **`0x1407A38E0`:** a capture-bounded proof over 337 members is 2/2 (the real `seq7805` call plus one synthetic case with the holder poked away), own-block 0.7143.
  - Six more synthetic cases were excluded by snapdiff (`page not captured at 0x2b984680`).
  - The bounded layer aborted in the gate at `0x1407ABBF0` and `0x1407C18B0`. The widening to the 661-member static closure did not finish regenerating.
- **Tooling:**
  - `tools/dev/translate/prove_bounded.py` (new): trace, bounded snapdiff, record.
  - `synth_trapkind.py` and `synth_trajleaf.py` now use `synth.py` (only `py_compile`d).
  - `0x1407AD150` was added to `UNHANDLED_DEAD_NOISE`.

## 4. PesStall

- **Keeper-stand mutation:** with the fix reverted, `ANoManIsEverHandedNoCommandAtAClipEnd` printed the NO APPLICABLE ANIM warning, then aborted at `0x1407ABBF0` before reaching its EXPECT. Not a clean red. A red run on the unfixed tree has not been done.
- **PressAI tree** (`48275157` + this branch, local scratch `1d404bfc`): no abort. `ATrapClip…`, `TheBreaker…` and `ANoMan…` pass. `ALiveBall…` fails with 394 live firings: men on `movestep 12<-3`, `fn 1`, idle/walk clips, forced by the breaker. TeamTick4 reports the same shape.
- **`diff_impure_recorded`:** not reproduced. It passes alone in 49 s with `GF_IMPURE_BANKS` set, and in the branch gate.

## 5. Gate (`79ba4894`, bounded layer)

`GATE branch: RED`:
- `PesForfeitMatch.ARedCard…` aborts at `0x1407ABBF0`.
- `pes_restart_delay_test` aborts at `0x1407C18B0`.
- `pes_throwin_release_test`: no ceremony throw-in released in any searched match, so `kCeremonySeeds = {8,3}` drifted.

The other 2,298 selected tests passed, and GoalSceneHold passes again.

Smokes: switch-off `a0fbe2d5cd445a52`, pes-loop `cae52312062a4e50` (the pass-target stand-in plus the goal-kick and wait changes).

## 6. Uncommitted in `gf-RestingBall`

- `xlate.py` and `pes_state.hpp`: a `CVTPS2DQ` lowering and a `cvt_i32` helper (round to nearest even, MXCSR RC=0), not tested against hardware.
- `ro_tables.json`: `0x1403A7FA0`, `0x14258A3B0`, `0x1425EB7F8`.
- `closure_trapdribble.json` (852-member union), `closure-trapdribble.txt` (661), `universe.json`, `synth_specs/trap_dribble_start_sw.json`.
- The last two `gen_translated.py` runs failed before writing, so the tree still holds the bounded layer.

## Resume, in order

1. Declare five read-only tables in `0x1403A7290`:
   - byte tables `0x1403A7E14`, `0x1403A7EE4`, `0x1403A7FB0` (0xBC bytes each; sites `0x1403A7650`, `0x1403A77A6`, `0x1403A7900`);
   - dword tables `0x1403A7E00` and `0x1403A7ED0` (five entries each).
2. Re-run `gen_translated.py`, then `--check` and `ledger.py`.
3. Rebuild; run the four failing tests, then PesStall on both trees, then `GATE_TIER=branch`.
4. Redo the keeper-stand mutation as a clean red.
5. Census the ceremony seeds (merge-lane rule: report them).
6. Settle whether GF's every-2–3-tick trap re-issue for a resting holder matches PES.
7. Replace the synthetic leaf proofs with real `rb5_*` captures.
8. The ALiveBall walk-vs-run stall on the PressAI tree belongs to TeamTick4.
