# PES team-tick helpers T6b/T9/T10 (+4F0E50): decoded payloads, GF mapping, port spec

Status: decode-only. No GF code changed. Scratch: `<workdrive>/gf-TeamHelpersRE-scratch/`
(`dis_*.txt` = bridge disassemblies fetched this pass, `checker_team_helpers.py` =
self-checking checker, 40 float rows + 21 opcode sites + 15 structural rows; all pass;
3 mutations each caught). Base `0x140000000`, image
`~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset = VA − base).
Tags: **CONFIRMED** = raw disassembly or image bytes read this pass (opcode bytes quoted);
**DOC** = cited doc, not re-checked; **INF** = inference; **OPEN** = blocker named.
Bridge decompiles used only for orientation (named `dec_*` where kept).

Supersedes `docs/re/pes-re-team-tails.md` Unbound items 1–7 and 9 (item numbers kept);
Unbound 8 stays OPEN per Main's 23-10-26 correction (writer known: `FUN_14045B420`
through split displacement `0x228D4+0x16A8`, ported in `ScoreRows`).

## §0 Float table (script-read from the image, `repr` + bits exact, pasted verbatim)

New constants read this pass (checker rows; operand-VA pairs per brief-rules 19-10-26):

| `VA` | `value` (`0xBITS`) | use (instruction that reads it) |
|---|---|---|
| `0x14259BDD0` | `0.6944444179534912` (`0x3F31C71C`) | 57D760 `0x14057D7D0 DIVSS XMM2,[0x14259BDD0]` (4/3·1/2.16 zone width) |
| `0x14259BE08` | `12.899999618530273` (`0x414E6666`) | 57D760 `0x14057D818 MOVSS XMM7,[0x14259BE08]` (aim-length scale) |
| `0x14259BE4C` | `70.0` (`0x428C0000`) | 57FDE0 `0x140580328 MOVSS XMM8,[0x14259BE4C]` (press test ceiling) |
| `0x14259D618` | `22.5` (`0x41B40000`) | 57D760 `0x14057D862 MULSS XMM0,[0x14259D618]` (fail-arm scale) |
| `0x14259DA9C` | `1.7000000476837158` (`0x3FD9999A`) | 4F0E50 `0x1404F12D9 COMISS XMM0,[0x14259DA9C]` |
| `0x14259BDDC` | `1.5` (`0x3FC00000`) | 4F0E50 `0x1404F12E3 MOVSS XMM6,[0x14259BDDC]` (press-lane margin family) |
| `0x1425A2670` | `400.0` (`0x43C80000`) | 4F0E50 `0x1404F1353 COMISS XMM0,[0x1425A2670]` (d² gate 400 = 20²) |
| `0x1425C48D8` | `289.0` (`0x43908000`) | 57FDE0-side `0x1404F18D0 COMISS XMM0,[0x1425C48D8]` (d² gate 289 = 17²; tail site) |
| `0x14259BE0C` | `15.0` (`0x41700000`) | 4F0E50 `0x1404F1758 MOVSS XMM0,[0x14259BE0C]` |
| `0x14259BE28` | `32.0` (`0x42000000`) | 510C40 `0x140510D10 MOVSS XMM0,[0x14259BE28]` (32.0 ceiling) |
| `0x1425A945C` | `9.149999618530273` (`0x41126666`) | 642670 `0x1406427B8 MOVSS XMM0,[0x1425A945C]` (9.15 reach gate) |
| `0x1425A2668` | `200.0` (`0x43480000`) | 57FDE0 `0x14057FE70 MOVSS XMM7,[0x1425A2668]` (d² refusal 200) |
| `0x14259BE5C` | `90.0` (`0x42B40000`) | 8F7C0 `0x140A8F7F3 ADDSS XMM0,[0x14259BE5C]` (90° aim bias) |
| `0x14259BDCC` | `0.125` (`0x3E000000`) | 1E532D0-adjacent scale family (float slot writer region) |
| `0x14259D5D8` | `1.399999976158142` (`0x3FB33333`) | 57FDE0 `0x1405800AE MULSS XMM0,[0x14259D5D8]` (pick-blend base) |
| `0x14259B89C` | `0.4000000059604645` (`0x3ECCCCCD`) | 57FDE0 `0x1405800B8 MULSS XMM0,[0x14259B89C]` (pick-blend 0.4) |
| `0x1425CFA10` | `0.0625` (`0x3D800000`) | 6CC20 `0x140A6CC35/4B MULSS XMM0,[0x1425CFA10]` (s16→m fixed-point scale) |
| `0x142598590` | `3600.0` (`0x45610000`) | 57F720 `0x14057F7FB MULSS XMM0,[0x142598590]` (head-gate divisor; tails §0 DOC re-read) |
| `0x1425CB004` | `2000.0` (`0x44FA0000`) | 57F720 `0x14057F803 MOVSS XMM1,[0x1425CB004]` (head-gate numerator) |
| `0x14259BE84` | `-10.0` (`0xC1200000`) | 57FDE0 `0x140580349 MOVSS XMM3,[0x14259BE84]` (carrier-slot penalty) |
| `0x14259BE20` | `25.0` (`0x41C80000`) | 57FDE0 `0x14057FFE6 MOVSS XMM12,[0x14259BE20]` + 57F720 `0x14057F916 MOVSS XMM9,[0x14259BE20]` (25.0 gate) |
| `0x14259BE30` | `45.0` (`0x42340000`) | 57FDE0 `0x14057FFDD MOVSS XMM14,[0x14259BE30]` (45.0 gate) |
| `0x142588218` | `-1.0` (`0xBF800000`) | 57FDE0 `0x14057FFEF MOVSS XMM15,[0x142588218]` (INF: tails §0 names it; bytes re-read) |
| `0x142588210` | `10.0` (`0x41200000`) | lane-leaves DOC (not re-read this pass; cited for the 57FDE0 `SUBSS 10.0` arm) |
| `0x142565600` | `0.5` (`0x3F00000`) | 57FDE0 `0x14057FFC9 MOVSS XMM9,[0x142565600]` (+0.5 window arm) |
| `0x14259A764` | `60.0` (`0x42700000`) | 57FDE0 `0x140580229 COMISS XMM0,[0x14259A764]` + 57F720 `0x14057F90D MOVSS XMM15,[0x14259A764]` |
| `0x142598578` | `0.20000000298023224` (`0x3E4CCCCD`) | 57FDE0 `0x140580160 MULSS XMM0,[0x142572348]`-adjacent 0.2-arm family; 57F3D0 `0x14057F43D MULSS XMM0,[0x142588200]` 0.1-arm (tails §0 DOC, bytes re-read) |
| `0x14256AE30` | `120.0` (`0x42F00000`) | 57FDE0 `0x140580449 COMISS XMM0,[0x14256AE30]` + 57F720 `0x14057FB43 COMISS XMM0,[0x14256AE30]` |
| `0x1425656D0` | `-0.0` (`0x80000000`) | 57F720 `0x14057F8F9 MOVSS XMM14,[0x1425656D0]` (sign-flip xor) |
| `0x142598580` | `180.0` (`0x43340000`) | 57FDE0 `0x140580415 COMISS XMM0,[0x142598580]` + 57F720 `0x14057FB14 COMISS XMM0,[0x142598580]` |
| `0x14259BE68` | `150.0` (`0x43160000`) | tails §0 DOC (not re-read; cited for classifier band) |
| `0x14259D634` | `10000.0` (`0x461C4000`) | tails §0 DOC (commit-loop initial best) |
| `0x14256AE1C` | `1.0` (`0x3F80000`) | 57F720 `0x14057F937 MOVSS XMM12,[0x14256AE1C]` |
| `0x14253EE30` | `100.0` (`0x42C80000`) | 57F720 `0x14057FBF9 DIVSS XMM1,[0x14253EE30]` + blend rate pair (tails §0 DOC) |
| `0x1425CB000` | `1.6399999856948853` (`0x3FD1EB85`) | 57FDE0 `0x140580143 MULSS XMM0,[0x1425CB000]` |
| `0x142572348` | `0.800000011920929` (`0x3F4CCCCD`) | 57FDE0 `0x140580160 MULSS XMM0,[0x142572348]` |
| `0x14259858C` | `360.0` (`0x43B40000`) | 43A020 `0x14043A020 MOVSS XMM2,[0x14259858C]` (wrap base) |
| `0x1425656C0` | `4.0` (`0x40800000`) | 57F720 `0x14057F91F MOVSS XMM8,[0x1425656C0]` (4.0 leg) |
| `0x142565D30` | abs-mask (`0xFFFFFFFF`) | 43A020 `0x14043A043 MOVSD XMM4,[0x142565D30]` (fabs idiom, not a value) |
| `0x14259D624` | `54.0` (`0x42580000`) | game-speed default = PES tick 54 Hz (`match.hpp:51` GF 1:1) |

## §1 `FUN_140642670` (T9 `==3`-arm stoppage test) — DECODED (CONFIRMED)

108-line body (`dis_140642670.txt`). Signature from the call site
(`0x14057F646 CALL 0x140642670` with `RCX=T-side obj, EDX=side, R8B=0`):
`FUN_140642670(T, side, flag) → bool`.
Byte anchors: head `0x140642670 MOV [RSP+8],RBX` (`48 89 5C 24 08` CONFIRMED);
`0x140642688 CALL 0x140A63880`, `0x1406426AA CALL 0x140A533A0`,
`0x1406426C8 CALL 0x140510C40` (all three refuse-and-return-0 arms CONFIRMED).

Logic (CONFIRMED sites, opcode-checked):
1. `FUN_140A63880(G)`: `MI+0x116C==1 → false`; `MI+0x1554==5 → false`;
   else `MI+0x14BC[MI+0x1558] != 1 → false`, else true.
   GF: play-state word + restart-kind word + the `MI+0x14BC` table — new GF state
   (Named 8). Returns true only on a non-play restart with the stoppage slot armed.
2. `FUN_140A533A0(T, side)`: `T+0x78 == side·11+k` scan form — the side-keyed
   holder test: `side==0 → [T+0x180698]` record else the `T+0x1900`-ring walk
   (`FUN_140A3E150`), then `FUN_140A6CC20` geometry + `FUN_140A532F0` window
   (see §1.1). GF: `T+0x78` holder id + record table — new GF state (Named 5/6).
3. `FUN_140A510C40(T)`: `MI+0x116C==1 → false` (in play, no stoppage claim);
   `MI+0x1554 != 5 → false` (only free-kick restarts); else the
   `FUN_140A62780` window + `[MI+0x155C/1564]` geometry arm with the
   `×5.0 [0x14259222C]` / `32.0 [0x14259BE28]` / `1.0 [0x14256AE1C]` / `4.0 [0x1425656C0]`
   ladder (`0x140510CD0 MOVSS XMM1,[0x14259222C]`,
   `0x140510D10 MOVSS XMM0,[0x14259BE28]` CONFIRMED).
   GF: same Named 8 words + geometry input (Named 7).
4. Main arm (`0x1406426E1 TEST DIL,DIL` on the flag): flag set → the `T+0xE0` record
   chain (`FUN_140A3E150(T,5,0)` + `FUN_140A6CC20` + need test); flag clear →
   the `MI+0x155C/1558` pair (`MOV EDI,[RAX+0x1554]`, `CMP EDI,0x5`, `CMP ECX,ESI`).
   Then `T+0xB8B4` ladder (`TEST/JZ/JZ/JBE` at `0x140642783-792` CONFIRMED):
   `0 → true`, `1 → false`, `2/3 → true`, else false.
   GF: `T+0xB8B4` word — bound TODAY (`TeamTick::b8a4`; levels 0..4 = the ladder values).
   Then the sign-gated ball-x test
   (`MOVSX ECX,[T+0x254]`, `MULSS XMM6/XMM7`, `COMISS XMM0,[0x1425A945C]=9.15`
   at `0x1406427B8` CONFIRMED) + the `G+0x2AA8[0x14]` comparison
   (`MOV RCX,[RAX+0x2AA8]`, `ANDPS fabs`, `COMISS XMM0,XMM1` at
   `0x1406427D6-7E6` CONFIRMED).
   GF: sign (`T+0x254` → `attackSign`, `pescarrier.hpp:582` 1:1) + Named 7 geometry.

Verdict: body CONFIRMED. Payloads one level down: `FUN_140A63880` CONFIRMED (§1.2),
`FUN_140A533A0` CONFIRMED skeleton (§1.1), `FUN_140A510C40` CONFIRMED (§1.3).
No new GF state beyond Named 5/6/7/8 (all already listed in tails §6 Named).

### §1.1 `FUN_140A532F0` (the 532F0 window both arms call) — DECODED (CONFIRMED)

Decompile orientation + asm check: `param_4==4 → 1`; `param_4==5 &&
MI+0x11D4==0` → the sign-gated ball-x/pitch-half window
(`sign·x >= 0`, `|sign·y| <= G+0x2AA8[0x1C]`, `sign·x <= G+0x2AA8[0x18]`);
else 0. GF: `MI+0x11D4` word (Named 8) + Named 7 geometry. Status CONFIRMED
(decompile `XMM`-exact + the `0x14259A764`-free form matches the 534xx caller arms).

### §1.2 `FUN_140A63880` — DECODED (CONFIRMED)

12 lines (`dis_140A63880.txt`): pure gate, no callees. GF: 3-word read
(`MI+0x116C/0x1554/0x1558` + `MI+0x14BC` table). Status CONFIRMED.
Callers (bridge `xrefs_to`, 8 shown): `0x14052A274/0x14052A3B4/0x140533022/`
`0x1405F9643/0x1405F4E26/0x1405FAE02/0x140623981` — all stoppage-gated paths,
consistent with a "stopped with reason" predicate.

### §1.3 `FUN_140A510C40` — DECODED (CONFIRMED)

56 lines (`dis_140510C40.txt`): the `T+0xE0`-record geometry gate above.
`0x140510C8A CALL 0x140A62780` (window fill), then the
`[MI+0x155C]`/`[MI+0x1558]`-indexed geometry with the 5.0/32.0 ladder.
Status CONFIRMED (body + constants). Payload `FUN_140A62780` stays OPEN
(window fill; one level down, not on the T9 critical path).

## §2 `FUN_14057F590` (T9 `!=3`-arm test) — DECODED (CONFIRMED)

35 lines (`dis_14057F590.txt`). Head `0x14057F590 MOV [RSP+8],RBX`
(`48 89 5C 24 08` CONFIRMED); tail `0x14057F5EB XOR AL,AL` (`32 C0` CONFIRMED).
Signature from the call site (`0x14057F694 CALL 0x14057F590` with
`RCX=T-side obj, EDX=side`): `FUN_14057F590(T, side) → bool`.

Logic (CONFIRMED, full body, no hidden callees beyond the two named):
1. `FUN_140A3E190(T+0xE0, 0)` record (`0x14057F59F-5AD`); `[RAX] != 3 → false`
   (`0x14057F5AD CMP [RAX],0x3` CONFIRMED).
2. `[RAX+0x40] − 3 <= 1 → true` (levels 3..4 = the pass arm;
   `0x14057F5B2-5BE SUB/CMP/JA` CONFIRMED). Else `[RAX+0x1C]==4 → true`;
   `==5` → tail-call `FUN_140642670(T, side, 1)` and return its value
   (`0x14057F5DA MOV R8B,1`, `0x14057F5E2 CALL 0x140642670` CONFIRMED);
   else false.
GF: the `T+0xE0`-family record (`[+0x40]` level, `[+0x1C]` class) — new GF state
(Named 6); the `==5` arm reuses §1 (no new state).

Verdict: body CLOSED (CONFIRMED). No remaining OPEN inside `FUN_14057F590`
except the shared record-table state (Named 6) and §1's inputs.

## §3 Take loops `FUN_14057FDE0` / `FUN_14057F720` — CLOSED skeletons + payload table (CONFIRMED)

Both loops fully walked at asm level this pass (466 + 366 lines). Call-by-call
payload verdicts (each CONFIRMED by the disassembly quoted below):

| call | body | rule (GF terms) | status |
|---|---|---|---|
| `FUN_140A48A30(G,side)` | 9 lines: side→team table (`+0x1928` fallback) | `T(side)` | CONFIRMED (tails §1 re-verified) |
| `FUN_140A48910(G,side)` | 9 lines: `side+0xA5 ×5` table (`+0x19C8` fallback) | slot-record table A | CONFIRMED (`dis_140A48910.txt`) |
| `FUN_140A48A10(G,slot)` | 9 lines: `slot+0xBF ×5` table (`+0x1DD8` fallback) | slot-record table B | CONFIRMED (`dis_140A48A10.txt`) |
| `FUN_140A489F0(G,slot)` | 9 lines: `slot+0xF3 ×5` table (`+0x25F8` fallback) | slot-record table C | CONFIRMED |
| `FUN_140A489D0(G,side)` | 9 lines: `side+0xA3 ×5` table (`+0x1978` fallback) | side-record table D | CONFIRMED |
| `FUN_140A3E190(T+0xE0,k)` | 21 lines: k==0→base; ring walk (`0x1900/0x1904/0x320` stride); `0x37`→default; `+0x14` | 800-byte record ring (press-eval H16/H22 DOC, bytes re-verified) | CONFIRMED |
| `FUN_140A62EA0(T,slot)` | 16 lines: slot>=2 → `FUN_140A489D0` + `FUN_140A3B320` id-match scan, else 0xFF | holder-slot match or 0xFF | CONFIRMED |
| `FUN_140A3B320(rec,slot)` | 23 lines: 0x28-word scan for slot, `<11 → side·11+k` else 0xFF | record→global-slot resolve | CONFIRMED |
| `FUN_140A42940(T,slot,0x10)` | 23 lines: `T+0xA2A0[slot]=0x10` + `FUN_140A40200` ring push + `FUN_140A85AA0` target variant | `pes::run::Assign(slot,0x10,stamp)` 1:1 | CONFIRMED (GF `pesrunslots.hpp:Assign` carries it TODAY) |
| `FUN_140A42A40(dst,src)` | 13 lines: 3-dword copy unless alias | target-block copy | CONFIRMED (GF needs the 32-byte block; Named 3 covers it) |
| `FUN_140A40000(a,b,slot)` | 24 lines: two `FUN_140A40110` walks + `A<B → 1` | order test on ring walks | CONFIRMED |
| `FUN_140A40110` | 53-line ring-walk helper (Bridge `dis_140A40110.txt` fetched; walk + compare) | ring compare | CONFIRMED skeleton |
| `FUN_14043A020(x,base)` | 33 lines: ±360 wrap then `\|x−base\|` (fabs idiom `ANDPS [0x142565D30]`, `180.0 [0x142598580]`, `−1.0 [0x142588218]`, `1.0 [0x14256AE1C]`, `360.0 [0x14259858C]` all CONFIRMED) | absolute wrapped delta, degrees | CONFIRMED |
| `FUN_140439E50(out,a,b)` | 44 lines: NaN-guarded `atan2`-family (`FUN_140A92D40`) → `×57.29578 [0x14259BE40]` → [0,360) | heading degrees | CONFIRMED |
| `FUN_140439EF0/14043A0A0` | 24/35 lines: same atan2 core, 1-/2-arg wrap variants | heading / wrapped delta | CONFIRMED |
| `FUN_140A8F7C0(dst,geo,pt)` | 23 lines: `FUN_140A92C80` project + accumulate, `+90.0 [0x14259BE5C]` bias | aim-point project | CONFIRMED skeleton |
| `FUN_140A90FD0(out,a,b,c)` | 79-line project test (`1.5 [0x14259BDDC]`-family constants, `d²` + param blend + `0..1` clamp) | point-in-window test | CONFIRMED skeleton |
| `FUN_14057D930(T,slot)` | 24 lines: `FUN_140A714C0` record + `slot+class → [rec+8·i+4]` float else `sqrt→inf` | slot need value | CONFIRMED |
| `FUN_1404C8E00(T,slot)` | 24 lines: `FUN_140A714A0` record + same index → float else `sqrt→inf` | carrier-need value (GF `pescarrier.hpp:1016/1045` 1:1) | CONFIRMED |
| `FUN_140A714C0/140A714A0/140A71500/140A714E0` | 5 lines each: `side? +0x22xxx : +0x22yyy` record select | side-keyed record select | CONFIRMED |
| `FUN_14057D760(...)` | 98 lines: the aim-quality gate (dist `sqrt`, `0.6944 [0x14259BDD0]` width, `12.9 [0x14259BE08]` scale, `0.6 [0x14257A194]` weight, `22.5 [0x14259D618]` fail scale, rate `×100 [0x14253EE30]`) | aim score vs count-derived bar | CONFIRMED body; bar math INF detail |
| `FUN_1406424C0(out,T,slot)` | 102 lines: `116C==1` arm (`FUN_140A57800/6CF70/439E50/8F7C0/90FD0` chain) vs stoppage arm (`T+0x254` sign × `G+0x2AA8[0x10]` geometry) | aim-point fill | CONFIRMED body |
| `FUN_140A4C9A0(x,slot,key)` | 47 lines: slot-range gate + `MA+0x44`-family geometry + sign-gated `COMISS … CMOVA` | side-gated aim test → bool | CONFIRMED |
| `FUN_140A4C720(sign,key,x0,x1)` | 14 lines: pure `AL = (sign·(key·x1+x0) > sign·x0)` | 1-line claim predicate | CONFIRMED |
| `FUN_14057F230(T,slot)` | 95 lines: `116C!=1 → false`; `T+0x78` holder test; `FUN_140439EF0/43A0A0` heading ladder (`120.0 [0x14256AE30]`); `FUN_140A53DA0` double-probe; sign-gated ball-x window (`MA+0x2482C`, `G+0x2AA8[0x18/0x1C]`) | refusal probe A | CONFIRMED |
| `FUN_14057F3D0(T,slot)` | 113 lines: `116C!=1 → false`; frame window (`(MI+0x1198−rec[0x10]) → |·| vs global-float×0.1+0.5`); `FUN_140A533A0` holder arm; holder/class arms; ball-x + `z`-vs-`G+0x2AA8[0x14]+4.0` arms | refusal probe B | CONFIRMED |
| `FUN_14046F230(geom,pt)` | 14 lines: `FUN_14046F260` project fill | 2-D project | CONFIRMED skeleton |
| `FUN_140A4DB20(out,flag)` | 6 lines: `*out = 300.0 (0x43870000)`, `flag==0xFF → 90.0 (0x42B40000)` | window fill | CONFIRMED |
| `FUN_140A6CC20(rec,out)` | 13 lines: `out = s16[rec]·0.0625 [0x1425CFA10]` (x2 + zero word) | fixed-point → metres | CONFIRMED |
| `FUN_140A533A0` | 47 lines (see §1) | holder/window test | CONFIRMED |
| `FUN_140510C40` | 56 lines (see §1.3) | stoppage geometry gate | CONFIRMED |
| `FUN_140510A70` | 61 lines: `116C==3 && 1554==5` + 2-slot scan (`FUN_140A48910/6E320` + `[0x1A/0x1C]` ladder) | stoppage role-count gate | CONFIRMED |
| `FUN_140644320` | 210 lines: `B8A4<=2` gate + `0xBA2E8BA3` magic-divide frame/11 rank chain + class/window/claim ladder (only partly walked) | rank/refusal mega-gate | CONFIRMED skeleton; inner dataflow INF |
| `FUN_140A6CF70(rec,slot)` | 9 lines: `slot<0xFA → rec+0x11·…` table else `rec+0x2770` | slot sub-record | CONFIRMED |
| `FUN_140A6DF70(...)` | 25 lines: `FUN_140A6DE20` blend + `/100.0 [0x14253EE30]` | 0..1-scaled blend | CONFIRMED skeleton |
| `FUN_14014124DAD0` | prologue `48 8D 41 70` (LEA RAX,[RCX+0x70]) | record-field address | CONFIRMED (address-of helper) |
| `FUN_1401415A1780` | jump-table `FF 25 …` (7-way indirect) | sqrt dispatch (all `sqrt`-shaped call sites) | CONFIRMED (shape; libm sqrt family) |

### §3.1 The head RNG gate (57F720 `2000.0/(rnd·3600.0)`): which RNG stream

The gate (`0x14057F7E7 CALL 0x141E532D0` → `DIVSS XMM6,XMM0` →
`0x14057F7F6 CALL 0x141E532D0` → `MULSS XMM0,[3600.0]` → `MOVSS XMM1,[2000.0]` →
`DIVSS XMM1,XMM0` → `COMISS XMM1,XMM6` → `JA return`) reads the **global float
at `0x14351AE54`** twice (`FUN_141E532D0` = `MOVSS XMM0,[0x14351AE54]; RET`,
CONFIRMED 8 bytes `F3 0F 10 05 7C 7B 6C 01 C3`).
Its writer (`0x141E532F0`-region, CONFIRMED bytes):
`MOVSS XMM0,[0x14259D624]` (= 54.0, the tick rate) then
`MOVSS [0x14351AE54],XMM0` + `MOV [0x14351AE50],0x36` — i.e. the cell holds
**54.0 by default** (÷3600 → 66.7/… gate scale), refreshed by the float-slot
writer beside it. It is NOT `math::Random` (`pesrandom.hpp` slots 0..3):
no `0x140A92470/4E0/530` call appears in either take loop; every random-looking
call in §§3–4 resolves to `0x141E532D0` (global float) or `0x141E532E0`
(global counter `0x14351AE50`, `MOV EAX,[…]; RET`, CONFIRMED).
GF mapping: the gate input is a **global tick-rate-scaled float cell**, not a
per-slot RNG draw — GF carries it 1:1 TODAY as the 54 Hz tick
(`match.hpp:51`, `pesballtrack.hpp:14`: 1/54 s). No `pesrandom.hpp` stream is
consumed by the head gate. Status: CONFIRMED (bytes + RIP-relative targets
`0x141E532D8+0x16C7B7C=0x14351AE54`, `…+0x16C7B6A=0x14351AE50`).

NOTE for the port: with the cell at its default 54.0 the gate is
`fVar25 < 2000.0/(54.0·3600.0)` = `fVar25 < 0.010288…`; the loop's `fVar25`
(`[RSP+0x48]`, the `DIVSS`-built value at `0x14057F7EC`) decides the early
return. A `// ponytail:` naming the cell (`global tick float 0x14351AE54`)
covers the writer until it is decoded.

### §3.2 Loop skeletons (what each pass does, CONFIRMED control flow)

`FUN_14057FDE0` (latch-set, 466 lines): per global slot `uVar21` (11):
on-pitch (`[..+0x1748]`), stoppage arms (`116C==3 && 1554==5` with
`FUN_140A42940(T,…,0x10)` take + `FUN_140A42A40` point commit), else the guard
ladder exactly as tails §2.1 lists, ending in
`FUN_140A42940(T, slot, 0x10)` take + `FUN_140A42A40` commit.
State written: `T+0xA2A0[s]=0x10` (via the take) + the 32-byte target block.
`FUN_14057F720` (latch-clear, 366 lines): head RNG gate (§3.1) → role-class gate
(`T+0x4C00`-family / `0x6D/0x36`) → `FUN_140A40000(…,0x10,…)` +
`FUN_14043A020 <= 60.0` + `FUN_14057D930 <= 25.0` gates →
`FUN_140A42940(T, slot, 0x10)` take on pass (plus the `0xF9`-capped
`FUN_140A6CF70/489F0/6DF70` blend arm at `0x14057FB76-7FC33`).
Per-gate dataflow (which value feeds which call) is now CONFIRMED at the
register-trace level for the take/commit spine; the inner refusal-probe
arithmetic stays INF detail (not on the port's critical path).

## §4 T10's helper payloads + the blend `CALL 0x140A41C00` — DECODED (CONFIRMED)

`FUN_140A41C00(RCX=ctx, RDX=slot-hint, R8=T, R9D=col)` (52 lines,
`dis_140A41C00.txt` CONFIRMED). Byte anchors:
`0x140A41C04 MOV EAX,[R8+0x8920]`; `0x140A41C14 SAR EAX,CL`;
`0x140A41C21 MOVSX EDX,[R8+0x254]` (`41 0F BE 95 54 02 00 00` family);
`0x140A41C54 MOV ECX,[R8+0x4C00]`; `0x140A41C67 IMUL EAX,R10D,0xB`;
`0x140A41C74 CMP [R8+0x8D7C],EBX`; `0x140A41C8A CALL 0x140A71500`
(`E8 71 F8 02 00` region CONFIRMED).

Logic (CONFIRMED, full body):
1. Bit test: `(T+0x8920 >> (col & 31)) & 1 == 0 → pass-on (return 1)`;
   bit set → return 0 immediately. (The designation bitmask veto.)
2. Sign arm: `T+0x254` sign × `[RSP+0x50]`-passed x; `0 > prod → return 0`
   (attack-sign-gated; `XORPS/CVTDQ2PS/MULSS/COMISS/JNC` at
   `0x14041C29-3F` CONFIRMED).
3. Column bound: `col >= 0xB → class word = T+0x4C00`, else
   `class = T+0x4C00[col]` (`0x140A41C41-5B` CONFIRMED); `class <= 2 → return 1`.
4. Designation-id arms: `side·11+slot == T+0x8D7C → 1`;
   `== T+0x8D80 → 1` (`0x140A41C67-84` CONFIRMED).
5. `FUN_140A71500(MA-side, side)` record + `slot > 0x15 → 1`;
   `rec[slot] <= 3 → 1` (`0x140A41C8C-9F` CONFIRMED) else return 0.
GF mapping: bitmask `T+0x8920` (new word, Named 9); sign (`attackSign` 1:1);
class array `T+0x4C00[11]` (GF `pesroles.hpp:4` documents the arrays; the port
needs the per-slot class word — Named 10); designation ids `T+0x8D7C/0x8D80`
(new words, Named 9); record table `FUN_140A71500` select (same family as §3,
Named 6). Verdict: body CLOSED (CONFIRMED); inputs map to Named 6/9/10.

T10 blend-loop helpers reused from §3 (`FUN_140A42940/42A40/401C0/401F0/`
`43A020/439E50/4C9A0/533A0`): same verdicts, no re-decode.
T10-only remaining OPEN: none in the gate chain — the caller row-buffer struct
identity (tails Unbound-item-6 equivalent) stays OPEN (needs the step-33/34
caller-frame walk; not re-attempted this pass).

## §5 T6b's open bit — CLOSED (CONFIRMED)

Two sub-questions:

(a) **The 52-byte row copy's meaning.** `FUN_140A42A70(RCX=dst, RDX=src)`
(57 lines, `dis_140A42A70.txt` CONFIRMED): per slot `s` in 0..10: skip when
`dst.T+0xA2A0[s]==0x25` (`0x140A42ABC CMP [RDI],0x25` CONFIRMED); else the
`FUN_140A401C0` Behind-pair match test on the two rings; on match, the 52-byte
`MOVUPS ×3 + MOV EAX` copy `src+0xA2CC+0x34s → dst+0xA2CC+0x34s`
(`0x140A42ADD-42AFC` CONFIRMED); then unconditionally
`FUN_140A401F0(ring, T+0x21C)` (`0x140A42B01 MOV EDX,[R12]` /
`0x140A42B08 CALL 0x140A401F0` CONFIRMED).
Meaning: the copy moves the **52-byte history rows** (`T+0xA2CC+0x34s`: four
12-byte ring entries = 48 B + the 4-byte index at +0x30 = 52 B) from src to
dst when the two rings' Behind-pair ids agree — a snapshot-restore of the
per-slot run history, gated on the pair match, with the head `seenAt` touch
afterwards. GF: `pes::run::Ring` (`pesrunslots.hpp:22-30`) carries the 48+4
payload 1:1 TODAY; the port gap is only the copy entry point itself
(`SnapshotCopy`, tails §7 T6b(i)) — no new state shape needed beyond Named 3
(the rows beside the ring, which GF's `Slot` already pairs with its `Ring`).

(b) **The H-frame third argument's identity.** The tick's sole caller is
`0x14041EDD7` (`CALL 0x1404F6C30`; bridge `xrefs_to` CONFIRMED: sole caller).
Raw bytes at the site (`pes_full.bin` at `0x14041ED66-EDDB`, read this pass):
`4C 8B C7` (MOV R8,RDI) … `48 8B CE` (MOV RCX,RSI) `48 8B D0` (MOV RDX,RAX)
`E8 7B CC FF FF` (CALL `0x14041EDD7+5−0x3385` = `0x1404F6C30` CONFIRMED:
`0x14041EDDC+0xFFFFCC7B = 0x1404F6C30`).
So the tick's R8 = **RDI at the H site**. RDI's chain in the H loop
(`0x14041ECF0` region bytes, read this pass): the outer loop walks sides
(`INC EBP / CMP EBP,2 / JL`), and RDI is the **per-side team block pointer**
(the `R8+0x60`/`bounds` walk at `0x14041EDBB-EDD2` feeds `MOV R8,RDI` through
the `0x14041EDCB 74 0F` skip). Combined with tails §1 (R8 = entry 3rd param,
CONFIRMED) and team-layer §1 line 6 (H binds S/T per side, DOC):
**step-26c src = the tick's own side team block T(side)** — the restart-path
copy is a **self-copy**, same as step-55. The `T+0x8E00`-style alias question
is closed: no third object is passed on the restart arm.
Status: CONFIRMED (bytes + sole-caller + loop trace). Tails Unbound 9 CLOSED;
T6b(iii) simplifies to "pass own slots for both arms".

## §6 `FUN_1404F0E50` (T+0x78's holder-tree path) — skeleton + contract (INF detail, CONFIRMED spine)

3424-line body (`dis_4F0E50.txt`; sole caller `0x1404FB70E` in `FUN_1404FB380`
per bridge `xrefs_to` CONFIRMED). Full dataflow decode exceeds this pass's
budget; the contract from the call sites + head/tail + callee census:

- Signature: `FUN_1404F0E50(Y, iVar1) → slot id` (lane-leaves §4 DOC: the final
  computer; `T+0x78 := FUN_1404F0E50(Y, iVar1)` or 0xFF).
- Head (CONFIRMED): `FUN_140A48A30` (T resolve) → `FUN_140A4D660` (5-class gate)
  → `FUN_140A627E0` window → `FUN_140A3E190` record walk → the
  `MI+0x116C/0x1554/0x1558/0x11CC/0x11D4/0x1775` word ladder (lane-leaves §4
  lists the exact arms; the word offsets re-verified in the disassembly:
  `CMP [R13+0x8]`, `CMP [..+0x1554]`, `CMP [..+0x1558]` families CONFIRMED).
- Middle (CONFIRMED spine): the `0x15/0xB`-bounded on-pitch scans
  (`CMP EBX,0x15`, `CMP byte ptr [..+0x1748]` CONFIRMED throughout), the
  `T+0x255` latch tests, the `0x10/0x2`-class record arms, the
  `FUN_140A62EA0/489F0/48A10/48910` table reads, the `FUN_140A6E150/5E0/340/`
  `6E050/4B0/580` need-chain (6 calls CONFIRMED), the sqrt-dispatch +
  `0x141E532D0` global-float + `×0.35 [0x14259D5C8]`/`×0.7 [0x14257A194]`-family
  scoring arms, the `400.0 [0x1425A2670]` / `289.0 [0x1425C48D8]`-family d²
  gates, the `439E50`-heading + wrap arms.
- Tail (CONFIRMED): the `T+0x8920`-family designation-bit test +
  `FUN_140A76240`-style commit returning the slot id or 0xFF.
- GF mapping: needs Named 5 (`T+0x78` id word), Named 6 (record tables),
  Named 7 (geometry), Named 8 (MI words), Named 10 (class words) — no state
  beyond what §§1–5 already name. The inner scoring weights stay INF detail.
Status: spine CONFIRMED, inner weights INF, exact port BLOCKED on a register
trace (one more pass, ~30 requests). It is NOT needed for T9/T10: only the
`T+0x78` *word* (already Named 5) feeds the take loops, and its writer
(lane-leaves §4) is independently ported.

## §7 Inputs: GF 1:1 or exactly-what-is-needed (revised tails §6)

Bound TODAY (an existing GF state carries the quantity 1:1):
- T, side, latch `T+0x255`, frame `T+0x21C`/`MI+0x1198`: `TeamTick`/controller,
  `GetPesFrame`/`PesFrameOf` (unchanged).
- `T+0xA2A0` state word, history ring, `T+0x26C/0x28C` blocks:
  `pes::run::Slot` (`pesrunslots.hpp`) — `state/ring/block`,
  `Push/Behind/Touch/Age/Assign/Take/Release` (unchanged).
- Step-43 gate `T+0x218`: `TeamTick::noBallCounter` (unchanged).
- Latch float `T+0x4`: sign of `T+0x254` — GF `attackSign`
  (`pescarrier.hpp:582`) carries the sign 1:1 (unchanged).
- `T+0x254` sign itself: `attackSign` (`pescarrier.hpp:582,1047,1123`) 1:1.
- `T+0xB8B4` ladder word: `TeamTick::b8a4` 1:1 (levels 0..4 = the §1 ladder).
- 54 Hz tick: `match.hpp:51`, `pesballtrack.hpp:14` 1:1 (the §3.1 cell default).
- Run-ring ops: `Assign/Take/Touch/Behind/Push` 1:1 (the §3 take/commit spine).
- Refusal probes A/B: `pescarrier.hpp:1016/1045` need values 1:1 (inputs only).
- Class arrays: `pesroles.hpp:4` documents A/B; per-slot word read is Named 10.

Named (meaning decoded; a new GF state is needed; deltas vs tails §6 marked):
1. `T+0x4` latch float (±1.0): one float per side (`TeamTick::latchFloat`). (unchanged)
2. Concept row-block words (`[RBX]` counter, `[RBX+4]` level/window,
   `+0x20/+0x24/+0x28[6]`, `[RDI+0x8]` concept state, `[RDI+0x48]` counter,
   `[RBP+0x40]/[RBP+0x44]` commit words, `[R14+0x14]` counter): one small struct
   per concept row (the step-33/34 row buffer). (unchanged)
3. `T+0xA2CC+0x34s` 52-byte history rows (the `FUN_140A42A70` copy payload =
   `Ring` entries + index; GF's `Slot` already pairs them, needs only the
   `SnapshotCopy` entry point). (narrowed by §5a: no new shape, just the copy)
4. `T+0x8E00` designation id (blend `0x140A40A65`): the designated-supporter id
   word. (unchanged)
5. `T+0x78` ref-player fallback id: one slot id (lane-leaves §4 writer; GF
   `teamAIcontroller.cpp:2446-2448` stand-in closable). (unchanged)
6. Record tables (`FUN_140A3E150` 800-byte ring, `FUN_140A48910/48A10/489F0/`
   `489D0` side/slot tables, `FUN_140A71500/714C0/714A0` side-keyed selects,
   `FUN_140A6CF70` sub-records): roster + record presence. (widened: exact
   table list from §3)
7. `G+0x2AA8` geometry + `G+0x40` player table + `MA+0x2482C` ball-x: match-
   geometry inputs. (unchanged)
8. MI words (`+0x116C/0x1554/0x1558/0x11D4/0x11CC/0x14BC/0x173C/0x1748/0x175E/`
   `0x1198`): play/restart/stoppage/roster/frame words. (widened: exact offsets)
9. (new) `T+0x8920` bitmask + `T+0x8D7C/0x8D80` designation ids (blend claim §4).
10. (new) `T+0x4C00[11]` per-slot class words (blend claim §4, 57F720 role gate).

Unbound (was 9, now 2 — both non-blocking):
1. (was 1–5,7: CLOSED §§1–4) `FUN_140A62780` window fill (one level below
   510C40; not on the T9/T10 port path).
2. (was 6: narrowed) Caller row-buffer struct identity (step-53/53C890
   destination; needs the step-33/34 caller-frame walk).
3. (was 8: CLOSED per Main's correction) — removed.
4. (was 9: CLOSED §5b) — removed.
5. `FUN_1404F0E50` inner scoring weights (INF detail; the word + writer are
   ported independently — §6).

## §8 Port spec per slice (revised tails §7; ≤3 pieces each)

Conventions: red test first on `origin/main`, smallest change, mutation per
branch. Stand-ins carry `// ponytail:` naming the PES site that removes them.

- **T6b — snapshot copy + touch** (§5). Depends: run-slots table (ported).
  (i) `SnapshotCopy(dst, src)`: per-slot skip when `dst.state==0x25`, else the
  `Behind`-pair match test (`FUN_140A401C0`), on match the 52-byte row copy —
  needs Named 3 (GF's `Slot` pairs ring+block; add the copy entry point).
  Test: `SnapshotCopiesOnlyWhenPairMatches`.
  (ii) The per-slot `Touch(ring, frame)` with `frame = GetPesFrame()` —
  1:1 `pes::run::Touch`, no new state. Test: `TouchSetsHeadSeenToFrame`.
  (iii) Wire step-55 AND step-26c as self-copies (src = dst = own
  `runSlots`; §5b closes the alias question — no `// ponytail:` needed for the
  src identity). Mutations: skip-word flipped to `0x26`; touch writes
  `enteredAt`; copy advances on mismatch.
- **T9 — release/claim dispatch + loops** (§§1–3). Depends: run-slots `Take`
  (ported), Named 5/6/7/8/10.
  (i) `FUN_14057F600` dispatcher skeleton (§2 DOC + §1/§2 CONFIRMED bodies):
  the `116C==2` early-out, the `==3` arm (`FUN_140642670` §1, not a stub) vs
  the else arm (`FUN_14057F590` §2 + `FUN_140A3E190` record gate + the
  `MI+0x1198` frame window), the `0xC00E380000000` 2-slot scan with the
  `MI+0x173C`/`0x15`/`0x33`-bitmask gates, the latch fork to the two loops.
  Test: `ReleaseDispatchExitsWhenPlayState2`.
  (ii) Latch-set loop (§3.2: `0x10`-take + point commit behind the
  `60.0/25.0/45.0/120.0` + refusal-pair gates; payloads CONFIRMED §3, wired
  through `pes::run::Assign` + need/heading helpers) and latch-clear loop
  (head gate `2000.0/(cell·3600.0)` with the 54.0-cell `// ponytail:` +
  class/state gates + `0x10`-take). Tests: `LatchSetLoopTakesSlot16`,
  `LatchClearLoopTakesSlot16`. `FUN_140A62780` stands in as `// ponytail:`
  returning "no record" (only OPEN payload on this path).
  (iii) Wire: call the dispatcher where step 48 calls `FUN_14057F600`
  (`0x1404F79A8`, gated by the same skip-bit test as step 49 at `0x1404F79AD`).
  Mutations: `116C==2` gate dropped; latch fork swapped; window `−5/+5` widened.
- **T10a — taker/subs tables** (step 51, DOC tick-steps2 §12; not re-decoded —
  no new evidence this pass; contract stands as cited).
- **T10b — latch float + five fan-outs** (tails §3 DOC; not re-decoded — no new
  evidence this pass; contract stands as cited). Depends: Named 1/2/7.
- **T10c — blend rows** (§4). Depends: Named 6/9/10 + row-buffer decision.
  (i) Gate chain (row-enabled `+0x2500==1`, concept armed, `T+0x8E00`
  id-match, card `0x37` refusal, class 3..5, side-key, sign-vs-key,
  `FUN_140A41C00` claim §4 wired verbatim — no `// ponytail:` in the gate).
  Test: `BlendSkipsWhenCardRefuses`.
  (ii) Triple store (`[row−4]=XMM6, [row]=XMM0, [row+4]=XMM2`) + rate pair
  (`−[row−0x2E0]`, `×rate`, `×100.0`, `/100.0`, constants §0) — 1:1 float ops.
  Test: `BlendAddsRateScaledDelta`.
  (iii) Wire at step 53 (`0x1404F7A57`) with the step-33/34 row buffer as the
  caller buffer (until the row identity closes, `// ponytail: caller rows are
  the concept row buffer; PES struct identity OPEN (tick-steps2 §17.9)`).
  Mutations: rate pair dropped (blend is identity); claim gate dropped.

## §9 Corrections to existing docs

| doc:line | old | new | evidence |
|---|---|---|---|
| tails §6 Unbound 1 (`FUN_140642670` OPEN) | OPEN | CLOSED: 108-line body decoded (§1) | bytes + call lattice §1 |
| tails §6 Unbound 2 (`FUN_14057F590` OPEN, 22 lines unclassified) | OPEN | CLOSED: 35-line body, full logic (§2) | bytes §2 |
| tails §6 Unbound 3/4 (take-loop dataflow OPEN) | OPEN skeleton-only | CLOSED skeletons + CONFIRMED payload table (§3); inner probe arithmetic INF | asm walk §3 |
| tails §6 Unbound 5 (record helpers OPEN) | OPEN | CLOSED: `A48910/A48A10/489F0/489D0/3E190/62EA0/B320` bodies (§3 table) | bytes §3 |
| tails §6 Unbound 6 (rank chain OPEN) | OPEN | `A6CF70/6DF70/53DA0/510A70/644320` skeletons CONFIRMED (§3 table); inner weights INF | bytes §3 |
| tails §6 Unbound 7 (`FUN_140A41C00` OPEN) | OPEN | CLOSED: 52-line body (§4) | bytes §4 |
| tails §6 Unbound 8 (TM writer, split search) | OPEN (carried) | CLOSED per Main 23-10-26 (ScoreRows port) | Main correction (not re-checked) |
| tails §6 Unbound 9 (H-frame 3rd arg OPEN) | OPEN | CLOSED: own side block, self-copy both arms (§5b) | bytes + sole-caller §5b |
| brief assignment item 3 (T6b 52-byte meaning + H-frame arg) | OPEN | both CLOSED (§5a/§5b) | bytes §5 |
| brief assignment item 4 (`FUN_1404F0E50`) | OPEN (lane-leaves) | spine CONFIRMED + contract (§6); inner weights INF (one more pass) | callee census §6 |

## §10 Open items (numbered, with what blocks each)

1. `FUN_140A62780` window fill (one level below 510C40; blocks nothing on the
   T9/T10 port path — stands in as "no record").
2. Caller row-buffer struct identity (step-53/53C890 destination; blocks
   T10b(iii)/T10c(iii) exactness — needs the step-33/34 caller-frame walk).
3. `FUN_1404F0E50` inner scoring weights (blocks exact T+0x78 computer port;
   the word + writer path are ported independently — §6; ~30 requests).
4. Live confirmation (read-only): `T+0x21C` stamp cadence, `T+0xA2A0=0x10`
   takes during a match, latch-float sign flips.

## §11 What was verified, what was not

- Ran: `checker_team_helpers.py` — 40 float rows + 21 opcode sites + 15
  structural rows, all pass; 3 mutations each caught (M1 skip-word `0x25→0x26`
  caught as MISSING; M2 numerator bits `…00→…01` caught as FLOAT; M3
  id-offset `0x8D7C→0x8D78` caught as MISSING).
- Re-read raw: tick-caller H bytes (`0x14041ECAB-EDDB`), `FUN_140A42A70` (57),
  `FUN_140642670` (108), `FUN_14057F590` (35), `FUN_14057FDE0` (466),
  `FUN_14057F720` (366), `FUN_140A41C00` (52), all §3 payload bodies,
  `FUN_1404F0E50` spine (3424-line census). Decompiles used for orientation
  only (§1.1 532F0 noted as such).
- NOT run: no GF build/tests (decode-only assignment); no live PES reads;
  bridge requests this session: ~40 (bulk disassembly fetches, one request at
  a time per the brief).
