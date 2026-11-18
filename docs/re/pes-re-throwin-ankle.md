# PES throw-in taker placement vs the line, and the ankle regression on OneWorld-series-wip (ThrowInAnkle, 18-11-26)

Scope: why `PesThrowInRelease.TheThrowInTakersAnklesStayBehindTheLine` fails on
`origin/OneWorld-series-wip` (head `8a65de72`): 29 of 168 taking ticks keep the
taker's nearer ankle short of the line (worst shortfall 1.06 m), green on `4791bbbe`.
Static RE (image + Ghidra bridge) + read-only `git diff` on the branch. No builds,
no live reads. The queued bisect (`4791bbbe..8a65de72`) has NOT landed:
`bisect_throwin_run.log` holds only its launch line, `throwin_steps.log` does not
exist. All VAs below are `.pdata`-anchored (script) or bridge-confirmed.

## 0. Verification (script output, pasted)

Script: `<workdrive>/gf-ThrowInAnkle-scratch/verify_vas.py` (flat image mapping,
`.pdata` at file offset `0x3AC5000`, capstone decode from containing function begin,
one planted wrong VA). Bridge `xrefs_to`/`decompile_function` saved beside it
(`dec_140792E10.txt`).

```
control OK: [0x14256AE1C] = 1.0f (flat offset 0x256ae1c)
.pdata: 158648 functions parsed
OK fn: 0x140792e10 (func 0x140792e10-0x140792eae) [mov rax, rsp]
OPEN fn: 0x141eb51a0 has no .pdata entry (leaf; call target => instruction start by definition, bytes decode cleanly, 20+ bridge xref callers)
OK fn: 0x141eb5bb0 (func 0x141eb5bb0-0x141eb5bbb) [sub rsp, 0x48]
OK fn: 0x141eb5e70 / 0x141eb3af0 / 0x1406eedf0 / 0x1406ec260 / 0x1406af620 / 0x1406b08a0 / 0x1406e68c0 / 0x1406b0ed0 (all .pdata-anchored instruction starts)
OK push call site: 0x14079355c [call 0x141eb5bb0] in .pdata func 0x140793368-0x14079358b (bridge: "From 14079355c in FUN_140792e10")
OK writer 0x1406EEDF0: 0x1406eeec3 movaps [rcx+0x90], xmm3 (B+0x90 step, 16 B) + 0x1406eee8a mov [rcx+0x81],dil (strike valid) + 0x1406eef40/84/5f (yaw rate B+0xB8/B+0xBC, yaw valid B+0x82)
OK leaf 141EB51A0: xor eax,eax; mov [rdx],rax; mov [rdx+8],eax; cmp [rcx+0xa04],al (the P+0xA04 ready check GF mirrors)
OK const 0x142588200 = 0.1 / 0x14257A194 = 0.7 / 0x14259D5D0 = 0.65 / 0x142565604 = 2.0 / 0x14259222C = 5.0 / 0x142588218 = -1.0 / 0x1425C5948 = 13.0 / 0x14259BE20 = 25.0
plant 0x14079355d correctly rejected (not an instruction start)
```

Bridge decompile of `FUN_140792E10` (CONFIRMED, quoted): taking-state gate
(`*vtable+0x20 == 1`, i.e. `P+0x245C`), test A (`E68C0 < AF620`), bookkeeping
(`EC260` true => `P+0x2460 |= 0x80`, `0x1404511E0/0x140A605E0/0x144232210`,
`FUN_140445750`), test B (`vtable+0x38` getter), then joints 3 and 6 via
`FUN_141EB51A0`, fabs-compare through the `DAT_142565D30` abs mask (reads NaN as
float: it is the bit mask), `F[1] + DAT_142588200`, and the shove
`FUN_141EB5BB0(P, x, y, (threshold-|z|)*sign + z, 1)` with `sign = -1.0`
(`DAT_142588218`) for negative z else `1.0` (`DAT_14256AE1C`, `fVar19`).

## 1. Where PES puts the taker, and what keeps the feet behind the line

(choreography refs are `docs/re/pes-re-throwin-choreography.md` on the branch.)

- **Approach target.** Path A: throw spot `S = (approach x clamped to R.x +/- 2 m
  (`DAT_142565604`), 0, sign*(F[1]+0.7) (`DAT_14257A194`))** (`Update 0x140791EC0`,
  phase 4; carry arrival `< 0.65 m` (`DAT_14259D5D0`) and `|x| < F[1]-0.5`).
  OBS live: taker root z = 34.74-34.76 with F[1] = 34.06. Path B: stand point from
  `ThrowinLoop::SetKick` (`0x140794280`), ball-release key over R: OBS live 4/4
  `|z| = R.z + 0.773`, `x = R.x - 0.022`, yaw 0/180 deg into the pitch.
- **Throw-in clip's root track + adjusters.** The per-tick driver is
  `FUN_141EB5E70` (CONFIRMED pdata): strike adjuster `P+0x428`, height `P+0x498`,
  yaw `P+0x4D0`, each stepped by `FUN_141EB3AF0` (CONFIRMED pdata), then the chain's
  phases on the BodyAnimControl at `P+0xA08`, outputs into B by `FUN_1406EEDF0`
  (CONFIRMED pdata; `[rcx+0x90]` movaps = the root step, `[rcx+0x81]` = strike
  valid). Release rows (§4.2): from loop d<=13 short (release frame 18), <=25 mid
  (27), else long (99); from carry run-short/run-mid (`DAT_1425C5948 = 13`,
  `DAT_14259BE20 = 25`, both verified above).
- **What keeps the feet behind the line: the per-tick shove, nothing static.**
  Every tick of phase 1 before the release frame, `FUN_140792E10` reads the two
  ankles (`FUN_141EB51A0`, joints 3 and 6 — `left_ankle`/`right_ankle`, KickJoints
  1), takes the nearer by |z|, and if it is inside `F[1] + 0.1 m`
  (`DAT_142588200`, verified) moves the root outward by the shortfall
  (`FUN_141EB5BB0`, call site `0x14079355C` verified). There is no placement
  constraint and no ball-position rule (the joint map overturned the old
  ball-position assertions; the test's comment already records this). A short
  reading in one tick is corrected the same tick; a short reading surviving into
  the next tick is a push that did not run or did not stick.

## 2. Which changes in `4791bbbe..8a65de72` can move the ankle ~1 m

Only three commits in the range touch the ankle path (`teamAIcontroller.cpp`,
`pesworldrecord.cpp/.hpp`, `pestrapworld.hpp`, `pesmovestep.cpp`, `match.cpp`);
`pessequencer.cpp`/`referee.cpp` (release timing) are untouched, so **release
timing is excluded as the delta** (identical at both ends; `MayRelease` gates and
the `buffer.started` take window did not move).

1. **`4d6eff7d` (one-chain cut) — strongest candidate: engine-chain root
   composition.** `StepAdjusters` (step GF's window-P adjusters with
   `FUN_141EB3AF0` on the stand-in chain, displace = accumulator `A+0x14` minus
   root point `O2+0xD0`) became `ReadAdjusterFrame` (displace = PES's step
   `B+0x90/0x98` minus engine chain's `O2+0xD0`, §0 writer verified), and the
   pose feeding the joint writer changed from GF's fox-engine pose (copied into
   the stand-in chain's arrays) to whatever PES's driver leaves in the engine
   chain's arrays (no copy). Two ~meter-scale mechanisms: (a) the engine chain's
   clip/action state is not GF's body state, so PES-side root outputs (loop
   side-steps row `0x9c2`, turn-in-place root warp, spot snap) get `WarpRoot`ed
   onto a GF body that never played those clips; (b) the same commit misread the
   yaw (see 2), rotating every absolute facing. It also put trap calls on the
   window's 8 KB stack, whose 0x8010-byte runner frame zeroed slot 21's `P`
   (found 17-11) — a zeroed `P` reads taking-state 0, closing `PushGateOpen`, so
   no push at all for that slot.
2. **`3066f71c` (half-angle yaw fix) — moves facing, narrows the suspect window.**
   `4d6eff7d` read `yawRad = atan2(B+0xA4, B+0xAC)`; B+0xA4/A+0xAC are
   sin/cos of **half** the yaw, so every absolute facing was halved (up to ~90
   deg off) and `TurnRoot(-remainder(...))` turned the GF body wrong every
   yaw-stepped tick — meters of root error over a take. `3066f71c` corrected to
   `2.0*atan2`. If the bisect lands on `4d6eff7d`, this half-angle error is the
   mechanism; if it lands after `3066f71c`, the yaw is exonerated and the root
   composition (item 1a) is left standing. `3066f71c`'s other half (memory stack)
   changes no value (slot-21 zeroing gone, strictly a fix).
3. **Cut4-side `721d39e6`/`4791bbbe` (DemoInfoRef writer + `BuildDemoInfoRef` call)
   — excluded.** The bookkeeping block runs at both ends (test `DemoInfoProbe`
   green at base, `BookkeepingSkips` path unchanged in range); it gates the push
   identically before/after.
4. **`4ac3c389` (bulk memset) — excluded** (write-record byte-identical proof in
   the cut report). **`16234a42` (merge) — no independent change** beyond (3).

Bisect status: not landed (see top). Prediction: first-bad = `4d6eff7d`, with the
yaw half-angle as the largest single contributor and the engine-chain root
composition as the remainder the yaw fix does not repair.

## 3. Verdict on the test, and fix direction

- **The test asserts a PES fact.** PES keeps the nearer ankle behind
  `F[1] + 0.1 m` by an active per-tick shove inside the throw-in update ( §0,
  CONFIRMED), not by placement: a taker may read short for one tick, but the
  same tick's update corrects it, so `persistentShort` (short in two consecutive
  taking ticks) is a genuine PES-rule violation. Do not re-scope or weaken it;
  the old ball-position assertions were already correctly replaced (joint map).
- **Fix direction.** (i) Per-taking-tick divergence trace on the failing seed
  (base vs head): taker root, engine `O2+0xD0`, `B+0x90`, `yawRad`, joint z —
  separates composition (1a) from yaw (2). (ii) Most likely repair: while
  `PieceTakerOwnsBody` holds (lift through release), suppress the driver's
  `WarpRoot`/`TurnRoot` for the taker — PES's throw-in action owns his root
  (lift warp, spot warp, loop side-steps are its own clip motion), so GF must
  not add the engine chain's step outputs on top of the GF-side placement; keep
  the joint writer + push (they are the assertion path). Only if the trace shows
  the engine chain's action state matching GF's body should the warps stay.

## 4. Open items

1. Bisect result (queued `4791bbbe..8a65de72`) — confirm `4d6eff7d` vs `3066f71c`.
2. `B+0xA4/A+0xAC` half-angle writers not statically located (rate path
   `B+0xB8/B+0xBC` and valid bytes in `0x1406EEDF0` verified; the absolute-yaw
   sin/cos stores sit in the curves writer, DOC-cited via the cut report).
3. `FUN_140792E10`'s release-frame gate above the ankle block (phase dispatch) —
   bridge decompile lines 1-28 not quoted here; the per-tick-before-release
   framing is choreography §4.2.
