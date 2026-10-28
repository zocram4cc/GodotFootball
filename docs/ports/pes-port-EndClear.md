# PES Clear unit (kind 1) and corridor angle: decode, port status, blocker (EndClear)

Branch `EndClear`, worktree `<workdrive>/gf-EndClear`, base `origin/main` 970c8b91 (`git ls-remote origin EndClear` = 970c8b910b89b250c0a3bdd75f96085351e658a8). **No code commit.** Tags: CONFIRMED (disassembly from `pes_full.bin`, decompile, or repo code at this commit), INF (inference), OPEN.

## 0. Verdict

- The Clear unit (kind 1) is decoded to the instruction (CONFIRMED). Its answer depends on the corridor angle `0x142108630`, which is **not translated** and whose inputs are PES world state that GF's PES loop does not carry.
- Porting the angle by hand would be unverifiable here: the translated layer is Main's lane (do not regenerate), and there is no harness oracle for the angle yet. A red test pinning a PES angle value would need that oracle. So nothing was ported, and no test or mutation was run.
- Blocker list and the VAs Main must add: §5.

## 1. The Clear unit, `0x142109EB0` (CONFIRMED, capstone on `pes_full.bin`)

```
142109EB0: push rbx
142109EB6: mov  rax, [r8+0x28]          ; rax = imageA (ctx+0x28)
142109EC0: cmp  byte ptr [rax+0x31C1], 0 ; danger flag imageA+0x31C1
142109EC7: je   142109EE5               ; no danger -> write nothing
142109EC9: mov  dword ptr [rdx], 5      ; answer.type = 5 (clear)
142109ECF: mov  rax, [r8]               ; carrier record pointer
142109ED2: xor  r8d, r8d                ; side/flag arg = 0
142109ED5: mov  rcx, [r9+8]             ; ctx pointer (r9 = r8)
142109ED9: mov  edx, [rax]              ; carrier id, as a float bit pattern
142109EDB: call 142108630               ; corridor angle, returns float in xmm0
142109EE0: movss dword ptr [rbx+8], xmm0 ; answer.a = angle
142109EE5: ret
```

- Gate: `imageA+0x31C1 != 0`. Answer when set: `{type 5, a = angle}` (answer record `+0` type, `+8` float; `rbx` = answer out).
- Answer layout matches carrier-ai §5.3 and carrier-units §3.2 (both cite `0x142109EB0` and `0x142108630`). The doc's "a = 0x142108630(ctx, carrier)" is confirmed: the call takes (ctx, carrier id as float, flag 0).
- Rule 6 (pass replaces clear) is in §4 of carrier-ai; GF's walk already implements it: `pescarrier.cpp:1196-1203` (`WalkCarrierUnits`, kinds 3,4,9,6 in that order).

## 2. What GF has today (CONFIRMED, repo at 970c8b91)

- `BuildUnitList` (`pescarrier.cpp:2359`) lists kind 1 in the base list `{23, 22, 0, 1, 17, 14, 11, 12}`, so the table has a kind-1 slot.
- `FillUnitTable` (`pescarrier.cpp:2417-2438`) gives answers only to kinds 0,4,9,7,2,3,6,10,11. Kind 1 stays unanswered. This is the gap in end-line §3.3.
- Danger stand-in (`elizacontroller.cpp:2948-2951`): `attackSign * carrierX < -pitchHalfW/3 && carrierPressureClass == 0`. It is a `// ponytail:` for imageA+0x31C1. The PES writer is `0x140699960` (untranslated; forced set at `0x142119714-0x142119770`, `pes-re-image-rows.md` §2.9, CONFIRMED condition).
- Forced-clear (rule 6/7 test): `walk.forcedClear` is never set (`elizacontroller.cpp:3325-3327`, "clears always replace"). PES's opponent-count test is cited as `0x140A4C820` (the GF ponytail names it; not checked in this session).

## 3. The corridor angle `0x142108630` (CONFIRMED structure, from the Ghidra decompile in `out/carunits3.c:714-1652`)

Entry: `(float *ctx, float carrierId, byte side)`. Returns the angle in `xmm0` (the trailing call `0x14159F3E0` is the stack-cookie check, no semantics).

Steps, in order:
1. `snap = 0x14063C560(ctx, carrierId)`; `cand = 0x14063A290(...)`: candidate and snapshot records.
2. Carrier height: `h = byte[carrierRec+0x31] / 100`, then `2h/3` (100 @`0x14253EE30` = 100.0; 3.0 @`0x14258C264` = 3.0; decompile `(fVar30+fVar30)/_DAT_14258c264`).
3. `bool`: a carrier-height/team-table test (`0x140A57800`, `0x140A57720`), plus `0x140A6D0D0` and `0x140A6CF30`.
4. Loop over 11 opponent slots (`uVar18 < 0xb`) with ids `< 0x16`: bearing gates 60 @`0x14259A764` (60.0), 120 @`0x14256AE30` (120.0), 180 @`0x142598580` (180.0), 90 @`0x14259BE5C` (90.0), 135 @`0x14259BE60`, and 25 m @`0x14259BE20` (25.0) as the lower distance bound. Each candidate corridor is validated by packer B `0x14063D2F0` (lane core `0x14063CD90`, ported in `peslane.hpp`) followed by `0x1421B7D40 == 0`. Clear (0) selects the candidate.
5. If no candidate is clear (`fVar31 < 0`), a 20-step angle scan (`uVar5 < 0x14`) rotates a vector by 90 (`0x14259BE5C`) via `0x140470D00`/`0x140470D60`, tests distances against `0x14259BE1C` (20.0), and keeps the minimum (`fVar28`).
6. Fallback: angle from the team attack direction `team+0x250` (`fVar31 = *(lVar6+0x250)`) with the 45/30/40 bounds (`0x14259BE30`, `0x142591140`, `0x14259BE2C`), then a `0x14063D2F0` check on each side.
7. Tie-break `0x141E532E0()/6` (the game speed, 54 fps, /6 = 9, not an RNG draw). Carunits §3.2 OPEN → now CONFIRMED as speed.

Constants (read from `pes_full.bin` by script, `float32 at VA - 0x140000000`):

| VA | Value | Use site (decompile operand) |
|---|---|---|
| 0x14253EE30 | 100.0 | height /100 |
| 0x14258C264 | 3.0 | 2h/3, width/3 term |
| 0x142565600 | 0.5 | composite weight |
| 0x142565604 | 2.0 | lane args |
| 0x1425656C0 | 4.0 | speed ramp |
| 0x14256AE1C | 1.0 | sign/step |
| 0x14256AE30 | 120.0 | bearing gate |
| 0x142588210 | 10.0 | lane arg |
| 0x142588218 | -1.0 | sign |
| 0x142591140 | 30.0 | side gate |
| 0x14259222C | 5.0 | step |
| 0x142596E6C | 80.0 | lane arg |
| 0x142598578 | 0.2 | margin (float32 0.20000000298) |
| 0x142598580 | 180.0 | bearing gate |
| 0x14259858C | 360.0 | wrap |
| 0x142598598 | -360.0 | wrap |
| 0x14259A760 | 35.0 | lane arg |
| 0x14259A764 | 60.0 | bearing gate |
| 0x14259BDEC | 6.0 | lane arg |
| 0x14259BE0C | 15.0 | attribute term |
| 0x14259BE1C | 20.0 | distance |
| 0x14259BE20 | 25.0 | distance lower bound |
| 0x14259BE2C | 40.0 | side |
| 0x14259BE30 | 45.0 | side |
| 0x14259BE4C | 70.0 | threshold |
| 0x14259BE5C | 90.0 | scan step / bearing |
| 0x14259BE60 | 135.0 | bearing |
| 0x14259BE68 | 150.0 | threshold |
| 0x14259D60C | 9.0 | scan step base |
| 0x1425A2008 | 270.0 | wrap |
| 0x142596E6C | 80.0 | (listed twice above; same value) |

Confirmed agreements with the docs: 25.0 @`0x14259be20`, 60 @`0x14259a764`, 120 @`0x14256ae30`, 180 @`0x142598580`, 2h/3 @`0x14253ee30`/`0x14258c264` (carrier-units §3.2, end-line §2.4).

INF: the exact role of each size-band constant (the 40/45/70/150 family) comes from the decompile's comparison sites, not from a full semantic decode.

## 4. Helper status (translation, from `tools/dev/translate/results_*.json` and `src/pes/translated/`)

Translated and diffed bit-exact (64+ cases each, CONFIRMED by results file): `0x1421B7D40` (lane check, results_cutscene 64), `0x14063CD80` (snapshot reset `*p=0xff`, results_core 64), `0x1421085B0` (results_core 64), `0x141587AA0` (results_core 64), `0x140DD39D0` (store 0; results_core 64), `0x141E532E0` (speed, results_cutscene 64), `0x140474C40`, `0x140A8F890`, `0x140A8F7C0`, `0x140470D60`, `0x140A57450`, `0x14046F230`, `0x140A4C7A0` (82), `0x140A638C0` (66), `0x140A40BD0`, `0x14124DAD0`, `0x140A3AE30`, `0x140A3AF30`, `0x141B59E20`, `0x1408C75C0`, `0x140A6E150`, `0x1405AEF10`, `0x140A489D0`, `0x140A48A30`, `0x140A489F0`, `0x140A48A10`, `0x140A48910`, `0x140A6D0D0`, `0x140A6CF30`, `0x140A58320`, `0x140A57800`, `0x140A57720`, `0x14043A020`, `0x14043A0A0`, `0x140A53DF0`, `0x140A6E3F0`, `0x140A62690`, `0x140A6E470`, `0x1405D06C0`, `0x140A92C80`, `0x140439FE0`, `0x140A6D120`.

Translated file but **not** bit-exact on any case: `0x14063C9F0` (snapshot filler; file `pes_fn_14063C9F0.cpp`, no diffed row), `0x14063D8A0` (5 failed), `0x14063E960` (3 failed), `0x14063A420` (no row), `0x14063D2F0` (packer B; results row `corpus empty (0 cases)`).

**Not translated** (needed by the angle chain):
- `0x142108630` (the corridor angle, body `0x142108630-0x142109EA9`, ~940 decompile lines).
- `0x142109EB0` (the Clear unit, §1).
- `0x14063C560` (zero + `0x14063A370` + `0x140DD39D0` + `0x14063C9F0`; decompile valid.c:35, CONFIRMED).
- `0x14063A290` (zero record + `0x14063A490`; decompile snap_helpers.c:140, CONFIRMED).
- `0x14063A370` (three stores of 1; snap_helpers.c:172, CONFIRMED).
- `0x14063A490` (head disassembled; body not decoded here: OPEN).
- `0x14063DE20` (lane packer; GF's `peslane.hpp` has a port, not checked against the image).
- `0x140699960` (danger writer, §2).
- `0x14159F3E0` (stack-cookie check; no semantics, not needed).

Decode artifacts: `<workdrive>/gf-EndClear-scratch/snap_helpers.c` (dec.sh output for 14063C9F0, 14063A290, 14063A370, 140DD39D0, 14063CD80). Ghidra bridge on 127.0.0.1:8080 is down (empty response), so decodes were via `dec.sh`.

## 5. Blockers and what Main must add (ordered)

1. **Translate and add to the committed layer** (Main's lane; I did not regenerate): `0x142109EB0`, `0x142108630`, `0x14063C560`, `0x14063A290`, `0x14063A370`, `0x14063A490`. The angle's deps are already bit-exact (§4), so the new translations should diff cleanly.
2. **Diff-prove the translated `0x14063C9F0`**, which the chain calls (currently no diffed row) and `0x14063D2F0` (packer B; corpus empty).
3. **PES world inputs for the angle**: the angle reads the match global `_DAT_1436F3DC8` (its `+0x40` carrier record, `+0x180` table with stride `0x74`, `+0x2AA8`, `+0x4BD4` per-slot table), per-player records via `0x140A48A10` (`+0x554`, `+0x55C`, `+0x560`, `+0x564`), team records (`+0x250`, `+0x254`), and `imageA` (`ctx+0x28`, `+0x31C1`). The GF bridge carries only the team record (RecordLayout stage 1, `docs/ports/README.md:91`). A player/match world record is needed before the translated angle can run from GF's state.
4. **Danger flag**: translate or port `0x140699960` (or the builder `0x142119390`, untranslated), then delete the GF stand-in at `elizacontroller.cpp:2948-2951`.
5. **GF wiring** (after 1-4): `FillUnitTable` answers kind 1 with `{type 5, a = angle}` when danger holds; `walk.forcedClear` gets the PES count test (`0x140A4C820`, cited by ponytail, not verified here). Delete the stand-in in the same change.
6. **Oracle**: a unicorn run of `0x142108630` over a live snapshot gives the expected angle for a fixed input (the red test then pins that relation, not a copied number). Needs `tools/dev/translate/driver.py` over `<workdrive>/gf-LiveState-scratch/live/` or a `live2` capture.

## 6. Docs that need correcting

- `pes-re-carrier-units.md` §3.2 says the corridor helpers' internals are "open" and the port is blocked there. Now: the leaves are translated and bit-exact (§4). The open part is `0x14063C560`/`0x14063A290` (small; decoded here) and the world inputs (§5.3).
- `pes-re-carrier-helpers.md` line 593 says the snapshot `0x14063C560` layout is OPEN and uses a ComputeShootZone stand-in. Decoded here: `0x14063C560` zeroes fields and fills via `0x14063C9F0` (about 20 world reads, `snap_helpers.c:1-137`).
- `pes-re-end-line.md` §3.3 is correct: kind 1 is listed and unanswered (`FillUnitTable`). The clear angle `0x142108630` is in §2.4 and CONFIRMED here.

## 7. Verification run

- Tests: none. No code changed, so no test binary links a changed file.
- Mutations: none.
- Smoke: not run. With no code change the EndClear build is identical to main by construction, so a 2-min seed-7 smoke would only repeat main's result. Main can run it on a merged tree if wanted.
- Gate: not run (Main gates).

## 8. Open items (numbered)

1. Main: translate `0x142109EB0`, `0x142108630`, `0x14063C560`, `0x14063A290`, `0x14063A370`, `0x14063A490` (§5.1).
2. Diff-prove `0x14063C9F0` and `0x14063D2F0` (§5.2).
3. Player/match world record for the angle's reads (§5.3).
4. Danger writer `0x140699960` port or translation (§5.4).
5. GF wiring of kind 1 plus `forcedClear` (§5.5).
6. Harness oracle for the angle on live snapshots (§5.6).
7. `0x14063A490` body not decoded (OPEN).
8. The 40/45/70/150 constant roles are INF (decompile comparison sites, not a full semantic decode).
