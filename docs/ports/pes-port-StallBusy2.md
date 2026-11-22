# pes-port-StallBusy2: the slide and trip re-fire premise is false at the decision level; PES's own-action slide gate is decoded but has no measurable effect on this stall; nothing landed, tree reverted (StallBusy, 21-11-26)

Status: REVERTED, NOTHING LANDED, NOTHING PUSHED. Branch `StallBusy` is back at StallRun a2439354 (the local branch never reached origin). Probe and dump diff saved to `gf-StallBusy-scratch/slide-probe-and-dump.patch` (77 lines, not for landing). Full-run baseline on this tree: **92** forced live-ball ticks (FAILED, as StallRun).

Correction to `pes-port-StallBusy.md` (the earlier report, the previous round): its §2 "Decision (cmdfn)" column is `clip->originatingCommand`, the command that started the current clip, not the per-tick decision. The decision distribution in §2 below replaces it.

## 1. What the forced firings do (decision logged per tick; `STALLDEC` line, env-gated, diagnostic only)

Full run, seed 7, 92 firings (`gf-StallBusy-scratch/census2-full.log`). Joined per firing: body function at the firing, and the decision (`PesDecision.command`, what MOVESTEP reads) on the same tick. Enum from `gamedefines.hpp`: 1 Movement, 12 Trip, 13 Sliding, 14 Special.

| Body fn | ac8 (PES action) | Decision | Count |
|---|---|---|---|
| 14 Special (locomotion/trap clip) | 2 MOVE | 1 Movement | 36 |
| 14 Special (trap clip) | 7 TRAP | 1 Movement | 18 |
| 13 Sliding | 2 MOVE | 1 Movement | 11 |
| 13 Sliding | 2 MOVE | 14 Special | 10 |
| 12 Trip | 2 / 7 | 1 Movement | 13 + 2 |
| 1 Movement | 2 MOVE | **13 Sliding** | 2 |
| 1 Movement | 2 | 1 Movement | 0 (see above) |

Reading (CONFIRMED for these 92 ticks):
- **Zero** firings have a Sliding decision while the body was already Sliding, and **zero** have a Trip decision while the body is in Trip. The "re-fire" premise is not in the forced ticks.
- The two Sliding decisions are new slide starts (body Movement, decision Sliding). The 21 slide-body firings are a committed slide clip (`sliding/walk/000.anim`, 60 frames, frames advancing one tick at a time) that plays out with a Movement or Special decision.
- The 15 trip-body firings: decision Movement. No Trip decision.

## 2. The PES own-action gate (CONFIRMED decode, headless `dec.sh` output `gf-StallBusy-scratch/dec_slide.txt`)

- `FUN_140a6e0d0` @0x140A6E0D0 (tackle/slide/keeper-save class): for action < 0x5e, returns `(word & 0x2c0) != 0`, i.e. class-table bits 6, 7 or 9 of `0x1426484f0 + 4*action`.
- `FUN_140a6e5e0` @0x140A6E5E0 (trap): bit 5.
- `FUN_140a6e3c0` @0x140A6E3C0 (bit 13, or the struct flag at +8 bit 13).
- Image check (class words, `pes_full.bin`): 11 SLIDING = 0x4080, 12 SLIDING_KEEP = 0x4080, 13 TACKLE = 0x4040, 7 TRAP = 0x4020, 2 MOVE = 0x1. Slide and tackle classes carry bits 6/7 (refused class), MOVE does not.
- The predicate list that uses them (`docs/re/pes-re-defender-decisions.md` §3, slide `FUN_140971b60`, gate "own anime not in 0x140a6e0d0 / 0x140a6e3c0 / 0x140a6e5e0"): doc-cited, the gate's position inside `FUN_140971ba0` is **INF** (not decompiled here).

GF's `_SlideCandidate` (`elizacontroller.cpp:1415-1463`) has no own-action refusal; it checks only `_AnyTeammateSliding` (which skips self) and the geometry. GF's `_TackleCandidate` (line 1354) does refuse on its own Sliding/Trip (GF's own rule). So the PES rule is genuinely missing from `_SlideCandidate`.

## 3. Measured effect of the missing gate: none, on this window

- Red-first probe `PesStallQuick.AManAlreadySlidingIsNotGivenASlideAgain` (per tick over the 240 s window: slide decision on a tick whose body was Sliding at the tick's start = re-issue). Run on HEAD: **PASSED**, `refires = 0` with `slideDecisions > 0` (the count was not printed; the ASSERT_GT guard passed). A test that passes on HEAD cannot be red-first, so no port was written against it.
- Trip re-fire: `Match::TripFromContact` (`match.cpp:1814`) already refuses a re-trip except stagger → fall (`match.cpp:1818-1821`), the PES veto 2 already in place. The forced trip bodies carry no Trip decision (§1), so there is no trip re-fire to gate.
- Probe crash on the first run (`GetAllPlayers()` holds null slots; a segfault at 45 s) fixed by `GetActivePlayers`. Recorded here so the next probe does not repeat it.

Verdict: the own-action slide gate is a real PES rule with no observable effect on these firings or on the quick window. A port would be a no-op for the stall count, so it is reverted (land-or-revert).

## 4. Not run (no change)

PesHang, PesPassTarget, MoveStepWire suite, `gen_translated --check`, the full ALiveBall as a landing check. None was needed: nothing landed. The full census2 run (92) is the ALiveBall count for this tree.

## 5. Open items (numbered)

1. **The slide cycle** (§1, the two new slide starts, body Movement → Sliding, and the 21 committed slide clips): the slide clip plays to its last frame (`humanoid.cpp:415`, the switch is held until the clip's last frame), and the next decision after it is a new slide when geometry still holds. Settle: does PES end a committed slide at the clip end and re-decide, or does a PES slide re-decide while it plays (PES's own-action gate says the second is refused)? Decode the PES slide's clip-end handling (`FUN_140a53690` / the anime end) before porting.
2. **Decision Special on 10 slide-body ticks** (one man, 19, about 20 ticks, `decv 5.13`): the Special command producers are `elizacontroller.cpp:1789`, `:1840`, `:3730` and `refereecontroller.cpp:102`. Not decoded. This is the next lead for why a Movement or Special decision cannot take a sliding body away from its clip.
3. **The trap bodies** (18 TRAP firings, decision Movement): PES's trap-touch and the GF trap clip (StallAnkle's and Main's area, `PlayIssuedClip` and trap touch), unchanged here.
4. The PES own-action gate (§2) should still be ported when item 1 is settled, since it is the decoded rule; it has no measured effect today.

Ownership: `Think()`, `MovementDecision` and the chooser are mine; nothing in the StallAnkle issue path was touched.
