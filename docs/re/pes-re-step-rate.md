# PES step-rate and anime-foot decode (StepRateRE)

Branch: none (read-only RE). Image `pes_full.bin` (VA − 0x140000000 = file offset).
Bridge `http://127.0.0.1:8080` (one request at a time); `dec.sh` for VAs the bridge has no function for.
Checker + all evidence kept in `<workdrive>/gf-StepRateRE-scratch/` (ALL PASS at delivery).
GF paths below are `origin/main` (`<workdrive>/GodotFootball`); the ClipRoot branch cited is `origin/ClipRoot`.

## §0 Constants (script-read from the image, pasted)

| VA | value |
|---|---|
| `0x14256ae1c` | 1.0 |
| `0x14259a764` | 60.0 |
| `0x14256ae34` | 1000.0 |
| `0x14259ce38` | 3.5999999046325684 |
| `0x1425ab124` | 0.949999988079071 |
| `0x14258c260` | 0.8999999761581421 |
| `0x14258c264` | 3.0 |
| `0x14259222c` | 5.0 |
| `0x14257a190` | 0.6000000238418579 |
| `0x142565600` | 0.5 |
| `0x1425ddf4c` | 0.13333334028720856 |
| `0x14259be30` | 45.0 |
| `0x142588210` | 10.0 |
| `0x1425a1ffc` | 13.5 |
| `0x14259bdf4` | 8.0 |
| `0x1425c48c0` | 22.0 |
| `0x14259be10` | 16.0 |
| `0x1425ddf60` | 0.3500000238418579 |
| `0x14259d618` | 22.5 |
| `0x1425ddf94` | 28.125 |
| `0x1425ddf8c` | 16.875 |
| `0x1425cca34` | 67.5 |
| `0x1425dbdb8` | -67.5 |
| `0x1425dbdbc` | -112.5 |
| `0x1425dd660` | 0.0 |
| `0x1425ddf84` | 5.625 |
| `0x1425ddfd0` | 174.375 |
| `0x1425ddfd4` | 1139.0625 |
| `0x1425cb30c` | 112.5 |
| `0x14259858c` | 360.0 |
| `0x142598598` | -360.0 |
| `0x142598580` | 180.0 |
| `0x142588218` | -1.0 |
| `0x14259be5c` | 90.0 |
| `0x14259be60` | 135.0 |
| `0x1425ddfcc` | 168.75 |
| `0x1425ddffc` | -180.0 |
| `0x1425ddf54` | 0.22500000894069672 |
| `0x14259b89c` | 0.4000000059604645 |
| `0x14257a194` | 0.699999988079071 |
| `0x142592228` | 0.25 |
| `0x1425a8a28` | 0.05000000074505806 |
| `0x1425c48a0` | 0.15000000596046448 |
| Rate ladder `0x1425dde20` | 0.29, 0.59, 0.7, 1.0 |
| Rate answers `0x1425dde00` | 0, 1, 2, 3 |
| Band knots `0x1425dde38` | 0.5, 1.0, 0.15 |
| Band denominators `0x1425dde40` | 0.15, 0.19999998807907104, 0.5 |
| Band bases `0x1425dde50` | 1.1534849405288696, 3.055999994277954, 4.040719985961914 |
| Band slopes `0x1425dde60` | 1.9025150537490845, 0.98471999168396, 4.253344535827637 |
| `DAT_1425dd9b0` (u16 x32 motion rows) | 1325, 0, 1325, 0, 1325, 0, 1326, 0, 1331, 0, 1332, 0, 1333, 0, 1351, 0, 1337 ×2, 0, 1337 ×2, 0, 1338, 0, 1340, 0, 1341, 0, 1342, 0, 1351, 0 (even slots; odd slots 0) |
| `DAT_1425dd988` (bucket cols) | 9, 0, 11, 0, 13, 0, 15, 0, 17, 0 (even slots; odd slots 0) |

## 1. `vt+0x68(rate)` — CONFIRMED: the rate ladder, not a speed

Vtable of the `+0xac0` interface (`0x1425e35b0`, verified against `docs/re/pes-re-fox-registry.md`):

| slot | VA | implementation |
|---|---|---|
| `+0x00` | `0x1406d2030` | run entry |
| `+0x18` | `0x1406eadf0` | rate ceiling (P+0x377c) |
| `+0x48` | `0x1406b9aa0` | band speed from rate and G |
| `+0x68` | `0x1406ba030` | rate → class 0..3 |
| `+0x78` | `0x1406ea230` | branchless `arg ? P+0x27A8 : P+0x2794` (NOT the foot; §3) |
| `+0x98` | `0x1406ea280` | `P+0x2780` address |

`FUN_1406ba030` (`dec.sh`, body `0x1406ba030..0x1406ba067`):

```
class = 3;
for (i = 0..3) if (rate < ladder[i]) return answers[i];   // ladder 0x1425dde20, answers 0x1425dde00
return 3;
```

Semantics: first ladder entry the rate is **strictly below** (checker mutation: `rate == ladder[0]` skips entry 0), else the last class; saturates at 3.
Already ported as `ClassForRateFor` (`src/pes/movestep_run.cpp:323`) with the same tables and VA comments.

### 1a. TURN_STOP's call site — CONFIRMED (ClipRoot's decode stands)

`FUN_1406cab20`, asm `0x1406caf20..0x1406caf2d`: `LEA RBX,[RDI+0xac0]; RCX=RBX; XMM1=req[0x4a]; CALL [RAX+0x68]` → `param_2[0x1f]`.
Inputs: the `+0xac0` object (param_1+0xac0), one float = the request's rate word `req[0x4a]`.
Output: int class 0..3 into `req[0x1f]` (the class word). No G, no speed, no band.
ClipRoot's fix (`f393fe7`: `std::clamp(host.CommandSpeedClass(), 0, 3)`) reads the right quantity from the wrong source — see §1c.

### 1b. STEP_LOOP_CHANGE's call site — CONFIRMED, same implementation

`FUN_1406c6480`, asm `0x1406c66e6..0x1406c6799`: `LEA RBX,[RSI+0xac0]; RCX=RBX; XMM1=req[0x4a]; CALL [RAX+0x68]` → `param_2[0x1f]`,
then `vt+0x48(rate, G)` → `req[0x1d] = speed × 3.6` (`0x14259ce38`).
Same object, same input word, same implementation as TURN_STOP. **Yes, it equals TURN_STOP's: same slot, same function, same argument.**

### 1c. STEP_LOOP proper has NO `vt+0x68` call — CONFIRMED

`FUN_1406c4c10`'s only indirect `+0x68` forms are `RDI+0x68` / `RSP+0x68` (request-word traffic, bridge asm `step_loop2.asm`).
The `~1250` site named in the brief is `host.BandSpeedKmh(int(req.F(kRate)), host.PhysiqueG())` (`src/pes/movestep_step.cpp:1250`,
STEP_LOOP_CHANGE's ceiling arm — the same one-line misread shape as ClipRoot's TURN_STOP site at `:1488`).
GF's `BandSpeedKmh(band, g)` signature already says the truth: it stands for **`vt+0x48(rate, G)` = `FUN_1406b9aa0`** (the band speed),
never for `vt+0x68`. No new state is needed for the class word; the port spec is:

- At both step sites (`movestep_step.cpp:1250` and `:1488`): replace
  `host.BandSpeedKmh(int(req.F(kRate)), host.PhysiqueG())` with `ClassForRateFor(req.F(kRate), host.RunTablesView())`
  (already ported, `movestep_run.cpp:323`; `RunTablesView` already on the `MoveHost` contract). Units: int class 0..3, no scale.
- `BandSpeedKmh` stays for its real readers (`movestep_run.cpp:302,808,1803,2022`: `BandSpeedKmh(cls, G) × speed3600`
  into `kSpeedBand`/`kDesiredSpeedKmh`), which match `vt+0x48`'s call shape (rate, G) → km/h. Its adapter body
  (ground-speed stand-in, `pesmovestep.cpp:351`) is still undecoded — that decode is `FUN_1406b9aa0`'s 3-segment curve (§0 knots),
  OPEN, with the curve's constants above. It is a follow-up decode, not new GF state.
- `CommandSpeedClass` (the AI command's class `cmd[0x17]` → `CommandClassFor`, GF `CommandSpeedClassOf`, `pesmovestep.cpp:46`)
  is a DIFFERENT quantity from the `vt+0x68` class (command class vs rate class): conflating them reintroduces ClipRoot's
  alternation through the back door. The `f393fe7` clamp should be re-pointed at `ClassForRateFor(req rate)` per above.

### GF state for §1

1:1 carry exists: `MoveReq::kCommandSpeedClass` is the class word; `RunTables::{rateLadder,rateClass}` are loaded from the exe
(`movestep.hpp:368-369`). No new state. The exact port is the one-line swap above at each of the two sites.

## 2. `vt+0x78` — CONFIRMED: the anime foot, read off the wrong interface in the brief

The two MOVESTEP call sites (`FUN_1406cab20` `0x1406cae7f`/`0x1406cb6ea`, `FUN_1406c4c10` `0x1406c60f4`) both run on
**`param_1+0xab8`** (`LEA RSI/RBX,[R14/RDI+0xab8]` immediately upstream; checker asserts both), NOT `+0xac0`.
`+0xab8`'s vtable (`0x1425e3508`) slot `+0x78` is **`FUN_1406e80b0`** (bridge decompile + raw asm; the `0x1406f9b70`
function the registry lists at action-class slot 15 is a different vtable — the per-action class's, reached through
`GetAction(P+0xc40, id)` — and is not what MOVESTEP calls).

`FUN_1406e80b0` (`0x1406e80b0..0x1406e81e1`), inputs and rule:

```
live   = vt+0x38()                                   // +0xab8 slot 0x38 = FUN_1406e6960: the current clip's live frame
cur    = *vt+0x18()                                  // the current motion id
n      = FUN_1406afbc0(tbl, cur, 9)                  // FootType count of the CURRENT clip
pick   = n                                          // default: past-the-end
for (i = 0; i < n; i++)
  if (live <= FUN_1406afe70(tbl, cur, 1.0, i)) { pick = FUN_1406afe20(tbl, cur, i); break; }
  else if (i == n-1) pick = FUN_1406afe20(tbl, cur, i) + 1;
foot   = min(pick, 3)
if (vt+0x30()) foot = (foot==0)?2:(foot==1)?3:(foot==2)?0:1;   // +0xab8 slot 0x30 = FUN_1406ec700: P+0x19ba flag byte
return foot
```

- `FUN_1406afe70(tbl, motion, 1.0, i)` = the i-th FootType event's frame: `int(2·((w>>13)&0x1ff) / 1.0 · (54/60) + 0.5)`
  (dec.sh; 54 = `FUN_141e532d0`, 60 = `0x14259a764`, 0.5 = `0x142565600`; rate arg is the constant `1.0` = `0x14256ae1c`).
- `FUN_1406afe20(tbl, motion, i)` = the i-th FootType pair: `(w>>22)&3` (`Mbinfo` `FootRecord::pair`, already loaded).
- The `<=` is load-bearing (checker mutation): `live == event` still picks that event.
- The past-the-end `+1` is the "next foot" (the swing foot after the last recorded event), then clamped to 3.
- The mirror map under `P+0x19ba != 0` is the 4-cycle {0→2, 1→3, 2→0, 3→1} (an involution with no fixed point).
- Values are only 0..3 (callers' `(foot-1) < 2` tests the {1,2} half against the {0,3} half).

So: **the clip's current foot from the motion data** (live frame vs the current clip's FootType event frames),
with a mirror-side swap. Inputs: the clip's live frame (not a phase test — a frame-vs-event comparison),
the current motion's FootType list (frames + pairs), the mirror flag byte `P+0x19ba`.
`FUN_1406eebb0` (dec.sh, `0x1406eebb0+48..53`) stores exactly this into **`P+0x245b`** at action entry;
MOVESTEP's mid-tick calls re-read the live value.

### 2a. What GF carries 1:1, and what is missing

- `pes::MotionRecord::foot` (`mbinfo.hpp:77`) + `FootCount()`/`FootPair(i)` already carry the FootType pairs. **Missing: the FootType event frames** —
  `Mbinfo` keeps no per-event frame (the `w` the `afe70` formula reads). New state: one `int frames` (or the raw `w`) per `FootRecord`,
  loaded from the same `footType` words (`mbinfo.cpp:208`), plus a reader for the `afe70` formula
  (`int(2·((w>>13)&0x1ff)/1.0·(54/60)+0.5)`; the 54/60/0.5/1.0 are the §0 constants).
- The live frame: GF's `CurrentClipFrames()` is the clip's **length**; PES's `vt+0x38` is the **live frame number**.
  New state: the playing clip's current frame (GF's `Anim` has `frameNum`; `pesmovestep.cpp:545` already reads `clip->frameNum`).
- The mirror flag `P+0x19ba` (`vt+0x30`): GF's `ClipFlag()` stands `false` (`pesmovestep.cpp:396`, writer undecoded).
  Its writer is still OPEN (this task did not decode it); until then the port keeps the `false` stand-in with its ponytail.
- `P+0x245b` itself already exists 1:1 as `MoveStepRecord::footBand` (`movestep.hpp:79`), stored at action entry
  (`pesmovestep.cpp:559-563`).

### 2b. Exact port spec (no new GF plumbing beyond the two reads)

1. `Mbinfo`: store each foot event's frame alongside its pair (same word `w`: pair `(w>>22)&3` exists; add `event = afe70(w)`)
   — `footType` words already in hand at `mbinfo.cpp:208`.
2. `MoveHost`: add `int ClipLiveFrame() const` (GF: the playing clip's `frameNum`) and reuse `Motion()` + `ClipFlag()`.
   Implement `FootBand()`'s PES rule as a shared helper (e.g. `movestep::AnimeFoot(host)`): the `live <= event` scan,
   the past-the-end `+1`, the clamp, the `ClipFlag()` 4-cycle. Both mid-tick call sites (`movestep_run.cpp:284,847,1788,2000,2568`,
   `movestep_step.cpp:1473`, tail-issue at `pesmovestep.cpp:585`) then read it live instead of the stored band.
3. Keep storing `P+0x245b`/`footBand` at action entry from the same helper (matches `FUN_1406eebb0`); the travel-angle
   `StoreFootBand`/`DisplacementTurnDeg` (`pesmovestep.cpp:175-186`) is deleted with it (clean cutover, no shim).
   `footBand` values {1,2}-vs-{0,3} keep every `(foot-1) < 2` / `mirror` use site unchanged.

## 3. Status and open items

| claim | status | evidence |
|---|---|---|
| `vt+0x68` = `FUN_1406ba030`, first-ladder-below, saturate 3 | CONFIRMED | dec.sh body `0x1406ba030..67`; §0 ladder/answers; checker (incl. strict-`<` mutation) |
| TURN_STOP's site reads `+0xac0`/`req[0x4a]` | CONFIRMED | asm `0x1406caf20..2d` + decompile lines 142-143 |
| STEP_LOOP_CHANGE's site = same implementation and input | CONFIRMED | asm `0x1406c66e6..99` + decompile lines 102-124 |
| STEP_LOOP proper has no `vt+0x68`; `:1250` is the `vt+0x48` shape | CONFIRMED | full indirect-call scan of `FUN_1406c4c10` (only `vt+0x10/0x18/0x28/0x30/0x60/0x78`) |
| `vt+0x48` = `FUN_1406b9aa0` (rate,G)→speed, band floor 0.15 | CONFIRMED | dec.sh body; §0 knots/denominators/bases/slopes (curve port OPEN) |
| `vt+0x78` (MOVESTEP sites) = `FUN_1406e80b0` on `+0xab8` | CONFIRMED | asm iface bases at all three sites; `0x1425e3508+0x78` = `0x1406e80b0`; checker |
| foot rule: live frame vs FootType events, `<=`, past-end `+1`, clamp 3, mirror 4-cycle | CONFIRMED | bridge decompile + raw asm `0x1406e80b0..e1`; `afe20/afe70/afbc0` dec.sh bodies |
| `P+0x245b` stored from `vt+0x78` at action entry | CONFIRMED | `FUN_1406eebb0` dec.sh lines 48-53 |
| `0x1406ea230` is the `+0xac0` slot `+0x78` (branchless P+0x2794/0x27A8 select), not MOVESTEP's foot | INF | raw bytes decode (`TEST EDX,EDX; CMOVNZ`); its reader/writer not traced |
| `P+0x19ba` writer (mirror flag behind `vt+0x30`) | OPEN | blocks the mirror arm; `ClipFlag()` stays `false` until decoded |
| `FUN_1406b9aa0` curve port (real `BandSpeedKmh`) | OPEN | constants in §0; adapter ground-speed stand-in stays with its ponytail until ported |

Checker: `<workdrive>/gf-StepRateRE-scratch/checker_steprate.py` — 21 checks, ALL PASS
(thresholds, answers, boundaries, strict-`<`/`<=` mutations, mirror involution, iface bases at both sites,
raw-byte shape of the `+0xac0` slot). Re-run: `python3 <workdrive>/gf-StepRateRE-scratch/checker_steprate.py [pes_full.bin]`.
