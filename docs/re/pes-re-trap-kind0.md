# Trap kind-0 control touch: the PES chain, decoded (28-10-26, TrapClipPort)

Read-only RE. No code changed. Scratch: `<workdrive>/gf-StoppageFlow-scratch/tc/dec_chain{1..6}.txt` (headless `dec.sh`, PESLive project) and `<workdrive>/gf-OwnerPickup2-scratch/dec_{trap68,kind0}.txt`. Image: `pes_full.bin` (VA − 0x140000000).
Tags: **CONFIRMED** = read in decompile or image bytes; **INF** = inference (reason given); **OPEN** = not decoded (blocker named).

## 0. Answers

1. **Dispatch.** The Trap action `+0x68` slot `0x1407999F0` is `E9 CB 67 00 00` = `jmp 0x1407A01C0` (CONFIRMED, image bytes). `FUN_1407A01C0` reads the request's kind word and sends kind ≤ 0 straight to `FUN_14079A470`. It does not call `FUN_140799DB0` (that function's caller is OPEN).
2. **The first-to-the-ball race is a refusal, not a clip.** `FUN_140799440` (frame loop) calls `FUN_14079FBC0` per frame. `FUN_14079FBC0` returns a bool and writes no clip id (CONFIRMED). A `1` makes `FUN_14079A470` return 0 without touching the anime state (CONFIRMED control flow).
3. **The clip choice** is in the commit step `FUN_14079AFD0` (query `FUN_14084DF20`, select `FUN_140857250`) and its fallback `FUN_140857220 → FUN_140858B90 → FUN_1408586A0 → FUN_14085B900 → FUN_140858260`, ranked by `FUN_14085F150`. The search runs over anime-side candidates (candidate count read through the action vtable `+0x90`, candidate index `anime+0xDC`, state `anime+0x70`) (CONFIRMED structure). No call to the TRAP row picker `FUN_14081FF70` appears among the decoded callees (CONFIRMED). Whether the candidates are the motion-table clips is INF until the candidate test `FUN_14085B3E0` is read (OPEN).
4. **Output.** `*param_6` (the out word) is a refusal status (1, 3, 7, 8, 9) or, on success, `auStack_78[0]` of the selected candidate record (CONFIRMED write; INF that it is the clip/motion key). Return 1 = issued.
5. **Touch frame for kind 0.** Not computed in this chain. The entry-3 TRAP sets its contact frame with `FUN_1406B3D60(anime, 7)` (`pes-re-first-touch.md` §0). The Trap `+0x98` slot (`0x1407981F0`) is unread (OPEN).

## 1. Dispatch `FUN_1407A01C0` (CONFIRMED, `dec_trap68.txt:1-59`)

`kind = request[2]` (i32). The request's bytes `+0x1D`, `+0x21`, `+0x1E`, `+0x1F`, `+0x22` feed the anime fields and the call arguments.

| kind | callee | VA |
|---|---|---|
| ≤ 0 | `FUN_14079A470` (control, this doc) | `0x14079A470` |
| 1–2 | `FUN_14079A310` (not read) | `0x14079A310` |
| 3 | `FUN_14079B3D0(…, 3)` (not read) | `0x14079B3D0` |
| 4 | bearing `FUN_140A57450`/`FUN_14046F230`, then `FUN_14079B3D0(…, 4)` | `0x14079B3D0` |
| 5 | `FUN_14079AAE0` (not read) | `0x14079AAE0` |

## 2. `FUN_14079A470`, kind 0 (CONFIRMED structure, `dec_kind0.txt`)

Ordered steps. `t0` = the ball frame index `FUN_1406E8320(anime, DAT_1425F1217 = 20)`.

1. Flag byte (request `+0x1C`) ≠ 0 and the action's admission `vt+0x10` fails → status 9, return 0.
2. `FUN_140798A70(action, anime) ≠ 0` → return 0 (refusal; body 280 lines, UNREAD).
3. Reach: ball sample `FUN_140A6CF70(ball, t0+1)`. If the squared xz distance from the anime's player (`anime+8`, `anime+0x10`) to it is > 36 **and** the distance to sample 0 (`FUN_140A57800`) is also > 36 → status 1, return 0.
4. Height class `FUN_14084BCA0(anime, ball height)` (returns 1..6; 6 = out of table). If 6: the sample at `FUN_1406E8320(anime, DAT_1425F121E = 10)` and `FUN_1406E6660(anime, 0x31)/100`: if twice that is below the larger of the sample height and the ball height → status 8, return 0.
5. If the caller's float argument is > 0 and its class is below the ball's class → status 8, return 0.
6. **Race refusal** `FUN_140799440(anime, ball, t0)` ≠ 0 → return 0 (no status write). See §3.
7. Query: `FUN_140846C70`, `FUN_14084DB00` (`FUN_14084C020(anime, class)` + `FUN_14084ECA0(…, 0x15)`).
8. Bearing predicate `FUN_14085C070(query, ball)`: for `query+0x6C − 3 == 0` (kind 3 branch), `|wrap180(bearing(anime velocity) − query+0x20)| ≤ 50` (`0x14259BE34` = 50.0, script-read). Other kinds: UNREAD. False → status 3, return 0.
9. **Commit** `FUN_14079AFD0(action, anime, ball, …)` (§5). Success → return 1.
10. On commit failure: `FUN_140857220(query, …, out)`; if the candidate `+4` ≠ 1 → status `uStack_98`, return 0 (`FUN_140849400`, `FUN_140799A00` on the other branch; UNREAD). On success, `FUN_14079D310(action, anime, query)` commits (UNREAD) and `*param_6 = uStack_98`.

## 3. Race refusal `FUN_140799440` → `FUN_14079FBC0` (CONFIRMED structure; meaning INF)

`FUN_140799440(anime, ball, t0)` (`dec_trap68.txt:226-253`):

- `FUN_14079F920(anime, ball) ≠ 0` → release, return 1 (body UNREAD).
- Loop: `f = int(t0 · DAT_1425CD060)` (`0x1425CD060` = 0.66, script-read), then `f` runs up to `t0 − 1`: `FUN_14079FBC0(anime, ball, f) ≠ 0` → return 1. Else return 0. Frames `[int(0.66·t0), t0)`.

`FUN_14079FBC0(anime, ball, t)` (`dec_kind0.txt:227-343`, bool; lines 344–403 not read):

- Presser's predicted point, per axis = position + velocity from his player record `FUN_140A48A10(G, id)`: x = (`+0x55C`, `+0x574`), y = (`+0x560`, `+0x578`), z = (`+0x564`, `+0x57C`). Bearing `FUN_140439E50` of that point to the ball sample `FUN_140A6CF70(ball, t)`; `FUN_14043A020` wraps it.
- Team roster: `MA+0x22E4C + id·200`, 2 teams × 11 slots (`u32` ids, valid < 22).
- For each teammate: squared distance to the ball sample compared with `(d + 0.5)²` (`0x142565600` = 0.5), and against the constants 112.5 (`0x1425CB30C`), 45 (`0x14259BE30`), 90 (`0x14259BE5C`); arrival `FUN_140A4D340`; a capsule test `FUN_140A8FF10`. Returns 1 when a teammate's arrival test beats his; 0 otherwise. Which test each constant gates is INF.
- Meaning "teammate wins the race → presser refuses" is INF: the control flow of §2 step 6 is CONFIRMED; the semantics are not.

`FUN_14079F920` (UNREAD): OPEN.

## 4. Candidate clip search (CONFIRMED structure; per-candidate test UNREAD)

| VA | role | verdict |
|---|---|---|
| `0x14084ECA0` | query builder, class `0x15` | CONFIRMED structure (`dec_chain3.txt:1-23`) |
| `0x14084C020` | class `0..2` from the action (`FUN_14084DB00` argument) | UNREAD |
| `0x140858B90` | three flag-driven passes: `anime+0x19` → state 1, `+0x18` → state 0, `+0x1A` → state 2; each runs `FUN_1408586A0`; result in the candidate record (`uStack_68 = 0x11`, `uStack_64 = 5`, `uStack_2C = 10`) | CONFIRMED (`dec_chain2.txt:2-74`) |
| `0x1408586A0` | loop `anime+0x70` over `[min .. max]` (max `param+0x10`, capped at 5, 6→5), skipping the current value; per value `FUN_14085B900`; compare with `FUN_14085F150` | CONFIRMED (`dec_chain4.txt`) |
| `0x14085B900` | loop candidate index `anime+0xDC` in `[0, FUN_14084B440(anime))` (`FUN_14084B440` = vt `+0x90` count); per index `FUN_140858260`; compare with `FUN_14085F150` | CONFIRMED (`dec_chain5.txt`) |
| `0x140858260` | one candidate: `FUN_14085B3E0` (UNREAD); then `anime+0x1C` flag → `FUN_14084E910` + second test; `anime+0x22` → return `candidate+4 == 2` | CONFIRMED structure; `FUN_14085B3E0` UNREAD (OPEN) |
| `0x14085F150(a, b)` | ranking: `DAT_1425F20C0[kind]` (priority), `DAT_1425F2108[sub]`, then `d²` of record fields `+0x18`/`+0x20`; kind `8` wins | CONFIRMED structure (tables UNREAD) |
| `0x140857220` | `FUN_140858B90` gate; accept when the candidate's `+4 == 1` | CONFIRMED |

## 5. Commit `FUN_14079AFD0` (CONFIRMED structure, `dec_chain3.txt:24-183`; callees mostly UNREAD)

- Refuse: the action's `vt+0x28` must be `0x10` and `FUN_140798A70` must return 0; otherwise status 8, return 0.
- Query `FUN_14084DF20(q, anime, ball, &key, …)`: counts `iStack_114`, `iStack_110`; zero → status 7.
- Two-candidate branch (`iStack_108 < 2`): `FUN_140857250` (select), with action record keys `0xCA8` (3240), `0xCB7`, `0xCBB` as special cases and `FUN_140846B20(…, 0xBC7 = 3015, …)` as the fallback key.
- Success: `*param_6 = auStack_78[0]`; `FUN_14079D310(action, anime, q)` commits; return 1.
- Failure: status 3 (or 7 on empty query).

## 6. Constants (script-read from `pes_full.bin`, not typed)

| VA | value | used by |
|---|---|---|
| `0x1425A9464` | 36.0 | §2 step 3 (squared reach, m²) |
| `0x14253EE30` | 100.0 | §2 step 4; G1 `FUN_1409FA400` (§7) |
| `0x14259BE34` | 50.0 | §2 step 8 (bearing, degrees) |
| `0x1425CD060` | 0.6600000262 | §3 frame start `int(0.66·t0)` |
| `0x1425CB30C` | 112.5 | §3 teammate threshold |
| `0x14259BE30` | 45.0 | §3 teammate threshold |
| `0x14259BE5C` | 90.0 | §3 teammate threshold |
| `0x142588200` | 0.1 | §3 arrival margin |
| `0x14259BDDC` | 1.5 | §3 ball height (m) |
| `0x1425A3590` | 1.1 | §2 step 4 class (height × scale) |
| `0x142565600` | 0.5 | §3 |
| `0x142598580` / `0x14259858C` | 180.0 / 360.0 | bearing wrap |
| `0x1425F1217` | u8 = 20 | §2 frame index |
| `0x1425F121E` | u8 = 10 | §2 step 4 frame index |

## 7. G1 ball-distance limit (`FUN_1409FA400`, entry 3)

- **Constant: correct.** `0x14253EE30` = 100.0 m² (10 m), script-read above. GF reads the same VA (`pesreceive.cpp:119`) and compares `ballDistance² ≤ 100`. The doc's 10 m stands; no constant change.
- **Quantity: differs.** `FUN_1409FA400` (CONFIRMED) reads the presser's own **C1 table entry** `MA+0x24710 + (MA+0x247BE[id] + 11·team)·8` (f32 +0, squared distance, `pes-re-first-touch.md` §3.4) and gates `≤ 0x14253EE30`. GF takes a fresh 2-D distance from `Predict(0)`. The C1 writer (which ball sample, which axes) is not decoded here: OPEN.
- **"GF passed the presser at 14.5 m": not reproduced.** At this HEAD the code can only pass G1 when `ballDistance² ≤ 100`, i.e. ≤ 10 m. The probe distance column in `gf-OwnerPickup2-scratch/*.txt` was not re-read: OPEN. Any pass at 14.5 m would mean the probe measures a different quantity than `Predict(0)`.

## 8. Open items (numbered; each names what settles it)

1. `FUN_14079AFD0`'s callees: `FUN_140857250`, `FUN_14079D310`, `FUN_14084DF20`, `FUN_140798D50`, `FUN_140848900`, `FUN_14084B810`, `FUN_14084B9C0`, `FUN_140846B20`. Settles the clip id and the commit (dec.sh).
2. `FUN_14085B3E0` (candidate test) and `FUN_14084E910`: the motion-table lookup behind each candidate. Settles whether the candidates are motion-table clips.
3. `FUN_140798A70` (280 lines, refusal) and `FUN_14079F920` (race prelude).
4. `FUN_14079FBC0` lines 344–403 and `FUN_140A4D340` (arrival), `FUN_140439E50` (bearing), `FUN_140A8FF10` (capsule).
5. `FUN_14085F150` constant tables `DAT_1425F20C0`, `DAT_1425F2108`.
6. Touch frame for kind 0: the Trap `+0x98` slot `0x1407981F0` and `FUN_1406B3D60`.
7. `FUN_140799DB0`: its caller.
8. The C1 writer behind G1 (`MA+0x24710`).
9. The probe distance column behind "14.5 m".

## 9. Port blockers (why nothing is landed)

- **Struct map gap.** The chain walks the anime object (`+0x70` state, `+0xDC` candidate index, `+0xAB8` action interface with vt `+0x10/+0x18/+0x28/+0x90`, `+0x3B18` scale), the action object (`+0xC40`), the query records (`+0x6C`, `+0x20`), and the motion table. GF has a partial anime-interface model (`src/pes/movestep.hpp`, `derivation.hpp`) and no layout for these records.
- **Callee count.** The kind-0 path calls roughly two dozen functions. Those read in this pass are cited above; those still unread are listed in §8.
- **Vtable calls.** The candidate test and motion-table reads go through vtables; a unicorn harness needs stubs for each, which is a large part of the pipeline's failure classes ("stub model wrong", "unmodellable callee").
