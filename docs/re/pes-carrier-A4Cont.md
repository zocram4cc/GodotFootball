# A4Cont: carrier units after Long and Cross (BackSpace, Safety, GoalFront, OneTwo, rest): decode status and blockers

Branch `A4Cont`, pushed as `0f14239` (`git ls-remote origin A4Cont` confirmed). Base `origin/main` `c43fb22`, which already contains the A4Units Long and Cross ports (`4cca875` is an ancestor). Worktree `<workdrive>/gf-A4Cont`.

**Verdict: no unit was ported.** Each remaining unit's gate or picker sits on a closure of undecoded PES helpers, and the closure is larger than the session budget allowed. Porting against stand-ins for those helpers would have been a fake port, so I did not do it. The only code change is a comment-only cleanup that Hygiene asked for (below).

## 1. What changed

- Comment-only, commit `0f14239`: removed the `playingstyles.cpp` citations, which dangled after Hygiene deleted `GetPassTypeBias`. Touched `pescarrier.hpp:749-750`, `pescarrier.cpp:1134`, `elizacontroller.cpp:2402-2403`. The mapping is unchanged (through ball = LongPass, lofted/cross = HighPass). No behaviour change, so the off-path hash cannot move.
- No tests added and no mutations run. The brief's TDD rule needs a behaviour change to red-test, and there is none.

## 2. Decode results (C-now = decompiled this session, from the Ghidra bridge on 8080 or the saved decompiles in `gf-A4Carrier-scratch` and `CarrierAIRE`)

| VA | unit (kind) | status | what is decoded |
|---|---|---|---|
| `0x142129110` | BackSpace (3) | C-now, 147 lines | Gate block 1: IsMid, lane `0x14063de20` free, nearest opp 9-frame prediction MORE than 6 m from ball (`d2 > 36.0` squared) → no answer (A4Lanes §2.4 correction CONFIRMED). Gate block 2: ctx+0x6F == 0 and (ctx+0x6C or ball height ≤ 1 m). Plan 4 → two plan-record bytes tested first, then slot-10 target `0x14211cb00(plan,10)` validated by `0x1421226c0` (decoded: carrier-helpers §1.1), answer type 2 b=3. Otherwise cut-back `0x142125140` (decoded: carrier-helpers §1.2), b=3, downgraded to b=0 when `0x14063e270` holds and the mate's pressure entries are > 1. _Corrected 19-10-26 from pes-re-carrier-helpers.md §6 item 5: old "within 36 m → no answer" (already corrected by A4Lanes §2.4); plan-record bytes added._ |
| `0x142125140` | cut-back picker | C-now, 12 KB, 11-slot loop | Loop body read (lines 100–291). Needs `0x1421226c0` (18.8 KB), `0x14063e960`, `0x140a4f720` (1 KB), `0x1421225e0` (885 B), `0x14063e010`, `0x14061e7c0`, `0x140a8f860/890`, `0x140439f90`, `0x1404c8e00`, `0x14043a0a0`, `0x140458740`, `0x140699820`, `0x14069a3e0`, and the b6c accessors. |
| `0x14212b520` | Safety (6) | C-now, gate fully read (lines 64-137 decoded: carrier-helpers §2.1) | Gate: k from press counts (+0x30 → 2, +0x34 → 1) + 135° facing-vs-plan test; lane `0x14063DE20` (carrier pos, wrap(angleTable+180), len = sqrt(nearestOppD2) runtime, cone 80°, rad 3.0) — FREE ⇒ no answer; danger ⇒ k=3 skip. Answers: k==0 → short branch; else through `0x142128490` else short `0x142126B70`. _Corrected 19-10-26 from pes-re-carrier-helpers.md §6 item 6: raw 0x14212B71D-7D8; old "lines 41-110/114-137 NOT read" + "free lane 80 m / 3.0"._ |
| `0x14212aba0` | GoalFront (2) | C-now, 57 lines | Gate: not IsCentral; per-player angle from `0x140a71260` (side +0x22C64 / +0x22A7C, float at [id]) +180 wrapped; lane `0x14063de20` (20 m, 80°) must be blocked; receiver `0x1421265c0(…, *(p1+0x3d))` ≠ 0xFF → type 2, b=0. |
| `0x1421265c0` | GoalFront receiver picker | C-now, 5 KB | Loop of 11 mates; b6c status `0x142118d20 == 0xb`; record `0x142118c90`; `0x14063c560`; zone `0x140698cc0/d10` (not read); `0x14069a3e0`. |
| `0x14212aeb0` | OneTwo (10) | C-now, 35 lines | Gate: (\|wrap(BP+0x34 − ctx+0x10)\| ≤ 90° or closing `0x140a4e720`) and one-touch `0x142128c10` and target `0x142126a80` ≠ 0xFF → type 2, b=0, f10 = 1. |
| `0x142126a80` | OneTwo target | C-now, 45 lines | Side from carrier index (11..21). List `0x140a714e0(MA+0x18, ctx+0x14 == 1 ? +0x22b9c : +0x229b4)`; count = byte at list+0xB0+side (`0x142124f90`). First id where predicate `0x142124950` holds, else 0xFF. |
| `0x142124950` | OneTwo predicate | C-now, 3.4 KB, read in full | b6c record dwords [0x39] (+0xE4), [0x3a] (+0xE8), [0x3b] (+0xEC), [0x3c] (+0xF0), [0x3d] (+0xF4). Windows use constants `0x14259be0c`, `0x14259d628`, `0x14256ae1c`, `0x142565604`, `0x1425a9464`, `0x14259a764`, `0x1425a9bf0`, `0x1425c4900`, `0x142588210`, `0x14259222c`, `0x14256ae30`, `0x14259be1c`, `0x14253ee30`. Helpers: `0x14063c560` (→ `0x14063c9f0`, 3.6 KB, OPEN), `0x140439fe0` (MA+0x23F7C table, semantics OPEN), `0x1404c8ea0` (= NearestList read, MA+0x22E4C, 200 B stride, rank +0xB2: same layout as `NearestEntry`), `0x140481b00` (angle diff, 826 B), `0x140477130` (speed, 529 B), `0x141052e30` (team mode, GF ponytail). |
| `0x140a4e720` | closing query (OneTwo gate, DHB-adjacent) | C-now wrapper, 749 B | Angle normalise, then `0x140a4e3f0` (2.8 KB, read in full): annulus [param_8, param_5] count of opponents whose bearing is below `0x140a4c4e0`'s threshold. **OPEN:** Ghidra drops the carrier index in this call chain (`0x140a48a10(G)` is called with one argument), so the origin of the sweep is not established. |
| `0x142128c10` | one-touch (OneTwo, DHB gate) | C-now, 181 lines, read in full | Helpers: `0x140a40bd0` (237), `0x140a60360` (87), `0x140587e70` (456), `0x140699820` (IsClose, in GF), `0x140a3e190` (482), `0x140a62ea0` (380), `0x140a602e0` (88), `0x140a58320` (141), `0x140458740` (384), `0x1421b7d40` (72), `0x142139650` (694), `0x14063cd90` (lane core, below), `0x1421085b0` (577). |
| `0x14212b9e0` | PassTarget (13) | C-now, 1.2 KB | Only admitted with plan 9, which GF never admits. Blocked by `0x14063c560`. |
| `0x142129be0` | DirectHighBall (11) | C-now, 251 lines, read in full | Gate: `ctx+0x6E` set; first-time-while-receiving `0x140a6e5e0`; ball height vs `BP+0x44 ≤ byte(+0x31)/100 + 0.1`; keeper > 25 m via `0x140a4ee40`, `0x140a76240`, `0x140a8f890`; mate scan 11 slots with b6c status, predicted position `0x141e532e0/6`, `0x140507740`, lane `0x14063d2f0` (754 B), one-touch `0x142128c10`, predicate `0x142124950`. Output type 2, b=3 (THROUGH, 24). **Live: most adopted pass (155 of 892).** |
| `0x14212d910` | Feint (17) | C-now, 1.4 KB | Stateful: tier at +0x60, record +0x40..+0x5C. Helpers `0x14212c0f0`, `0x14212cb30` NOT decoded. |
| `0x14212b8b0` | SideChange (5) | NOT decoded | Not defined in the shared bridge (`u_14212b8b0` is 18 bytes). Plan 7 is never admitted in GF. |
| `0x14063cd90` | **lane core** (rule 8's lane battery) | C-now, 5.2 KB, read to about line 150 of 170 | Wrapper `0x14063dc10` (1.3 KB) returns `count == 0` on the lane-core output, so **lane free ⇔ no entry**. Core: per-mate loop with `0x140a48a10`, `0x14043c410`, `0x140474c40`, `0x140470d60`, `0x140a8f7c0`, `0x140a53df0`, `0x140a90d80`, `0x140a62690`. **OPEN:** `0x140a53df0`, `0x140a90d80`, `0x140470d60`, `0x14043c410`, `0x140a62690` are not decoded, and the tail of the loop is not re-read. |

**Other sizes (for the next dispatch):** `0x1421226c0` 18.8 KB (OPEN, the valid-receiver test used by BackSpace and the cut-back loop); `0x142126b70` 21 KB (Safety short, OPEN); `0x142128490` 6 KB (Safety through, OPEN); `0x14063c9f0` 3.6 KB (OPEN, used by `0x14063c560`); `0x140a4ee40` 176 B (opponent-line picker, OPEN); `0x140a4c4e0` 556 B, `0x140a92d40` 418 B (decoded above, atan2 wrapper); `0x14063d2f0` 754 B (lane struct fill, OPEN).

## 3. Why no unit was ported

1. **Lane core `0x14063cd90` is the critical path.** BackSpace's gate, GoalFront's gate and Safety's "free lane 80 m / 3.0" all call `0x14063de20`, which calls the core. One-touch `0x142128c10` calls the core too, so OneTwo and DirectHighBall inherit it. Rule 8's lane battery is blocked by the same five undecoded helpers. The brief said to port the battery only "if its helpers are decodable"; they are not yet.
2. **Receiver predicate `0x142124950`** needs `0x14063c9f0` (3.6 KB) and the b6c record semantics (dwords 0x39–0x3d). The b6c builder `0x1421174f0` (`CarrierAIRE/d_1421174f0.c`) was not read this session, so the field meanings are INF.
3. **Pickers:** BackSpace's cut-back `0x142125140` (12 KB, about 25 helpers) and its valid-receiver test `0x1421226c0` (18.8 KB); Safety's short `0x142126b70` (21 KB) and through `0x142128490` (6 KB); GoalFront's `0x1421265c0` (5 KB, zone helpers `0x140698cc0/d10`).
4. **Sweep parameters (`0x140a4e3f0`)** have an unresolved carrier argument (Ghidra artifact). Needs a raw re-check of the call site before OneTwo's closing term is ported.

Per the owner's order, the four required units are blocked this way. The rest of §10 falls under the same chain: PassTarget (plan 9 only), SideChange (undefined), Feint (stateful, two undecoded helpers).

## 4. Recommended order (decode first, then port)

1. Decode `0x140a53df0`, `0x140a90d80`, `0x140470d60`, `0x14043c410`, `0x140a62690`, and the tail of `0x14063cd90`. Port the lane core as a pure function with tests on synthetic opponents, using the install's 20 m (`0x14259be1c`) and 80° (`0x142596e6c`) constants. This unblocks GoalFront, BackSpace and Safety gates and Shoot's lane eval.
2. Decode `0x14063c9f0` and read `d_1421174f0.c` for the b6c layout. Then port the predicate `0x142124950` and the OneTwo picker `0x142126a80`.
3. Port one-touch `0x142128c10` (its helpers are all under 700 B except the lane core).
4. Port OneTwo (10), then DirectHighBall (11): the largest live adoption (155/892), with the same chain.
5. Decode `0x1421226c0` (18.8 KB) and port BackSpace (3) with its cut-back picker.
6. Port Safety (6) after the 21 KB short picker and the 6 KB through picker are decoded.

## 5. Checks

- `pes_carrier_test`: built and run after the comment edit with `PES21_ROOT` (result in the final message).
- Gate not run (Main gates). No smoke run: no behaviour change, so `a3d5e43a3b7b03af` cannot move.
- Peer: Hygiene acknowledged (message sent).

## 6. Open items (numbered)

1. Lane core `0x14063cd90` and five helpers (see §4.1).
2. Receiver predicate `0x142124950` and `0x14063c9f0`; b6c field meanings (`0x1421174f0`).
3. One-touch `0x142128c10` (depends on item 1).
4. OneTwo closing term: `0x140a4e720` carrier argument, Ghidra artifact (raw re-check).
5. BackSpace: `0x142125140`, `0x1421226c0` (18.8 KB), `0x14063e270`, `0x14069a3e0`, `0x140a4ee40`.
6. Safety: `0x14212b520` lines 41–110 and 114–137 not read; short `0x142126b70`, through `0x142128490`.
7. DirectHighBall: `0x140a6e5e0`, `0x140507740`, `0x14063d2f0`, `0x140a76240`.
8. GoalFront: `0x1421265c0` zone helpers `0x140698cc0/d10`; angle table `0x140a71260` content.
9. Feint: `0x14212c0f0`, `0x14212cb30`. SideChange `0x14212b8b0`: needs `dec.sh` (undefined in the shared bridge). PassTarget: `0x14063c560` (blocked by item 2).
