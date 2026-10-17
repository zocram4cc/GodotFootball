# OffBallSelectors: PES runtime fields the AI reads (decode-only, feeds the foundation inventory)

Method. Literal-offset search over the `.text` section of `pes_full.bin` (disp32 stores, SIB stores, `lea`, SSE stores): `<workdrive>/gf-OffBallSelectors-scratch/find_writers.py`, `find_writers_sib.py`. Each hit was then decompiled through `dec.sh` at its address. "NOFUNC" = Ghidra made no function at that address (dec.sh runs with -noanalysis), so the base object is unverified. Hits are candidates until the base object is confirmed. A zero-hit result only rules out literal-offset writes, not computed-base writes.

## Team struct T = a48a30(G, side)
| field | readers decoded | PES writer(s) | status | GF |
|---|---|---|---|---|
| T+0x255 possession byte | Chance team head, Counter/PostPlay masks, run stops | setters `FUN_1404167c0` (AL; also copies T+0xb6e0/c12b/c12c from RSI), `FUN_140a428d5` (AL, param_1), `FUN_1415d983e` (AL, param_2) | verified stores; caller and value source not traced | stand-in: last toucher (teamAIcontroller ProcessOffBall) |
| T+0xb6e0 concept-running byte | Counter mask, defensive-shape gate, duels gate | setters `FUN_14057acec` (writes 0), `FUN_14057ad05` (writes 1), both guarded by the same byte read through `*(G+0x6d50)`; `FUN_1404171bf` (AL); copy in `FUN_1404167c0`. Candidates NOFUNC: 0x140a41691, 0x14215d68b | verified for the setters | unknown mapping (pesteamconcepts?) |
| T+0xa508 counter enable | counter trigger (chance/counter gates) | single runtime writer `FUN_140581980` (call 0x1405819c0) | from pes-re-counter-trigger.md | missing (counter roll deleted) |
| T+0xb8c0 possession change time | support search wait (3 s) | `FUN_14041732c` (= EAX), `FUN_1404f870c` (= EAX, param_2). Candidate `FUN_1400bf872` (byte add at RBP+0xb8c0): base unclear | verified | possessionChange_ms (existing) |
| T+0xc12b supporter wanted | designated supporter | `FUN_140417534` (AL), copy in `FUN_1404171bf` and `FUN_1404167c0` | verified | missing (stand-in: supporter whenever carrier in play) |
| T+0xc12c supporter request | designated supporter | set `FUN_1405020ea` (= 1); clear `FUN_140501fbd` (= 0); setter `FUN_140417541` (AL) | verified | missing |
| T+0xb8b8 attack-slot mask | Chance mask | `FUN_140500200` (decoded earlier) | from earlier decode | AttackSlotMask (existing) |
| T+0xa2a0 + 4·slot run state | all run readers | `FUN_140a429a0` (take), `FUN_140a40820` (release, from off-ball docs); unread stores at 0x14055d63a, 0x14057b92b, 0x1405a72c1, 0x14060b9ce, 0x1409849db, SIB 0x140a40842, 0x140a4295d, 0x140a429c5 | partial | RunRecord map (existing) |
| T+0x78 pass target | pass receiver, PASS_GET | kick code (humanoid.cpp comment) | from GF comment | SetPassTarget (existing) |
| T+0x4c00 + 4·slot registered position | everywhere | formation setup (not traced) | not verified | Position() (existing) |
| T+0x4b4c + 12·slot slot point (z at +0x4b54) | support, Counter writer | formation adaptation (not traced) | not verified | slotPoint (existing) |
| T+0x288 / T+0x28c + 0x40·slot run record bytes (lane, sub-kind) | PostPlay stop, SecondLine | written at take (FUN_140a429a0 copies the executor's link byte) | from PostPlay decode | missing |
| T+0xb8a4 attack level | RunCap, Counter cap, Overlap cap | in-play writer `FUN_1404f8350` (counter-trigger doc) | from doc, not re-verified | mentality, approximate (existing) |

## Match struct MA = G+0x18
| field | readers decoded | PES writer(s) | status | GF |
|---|---|---|---|---|
| MA+0x2601c ball owner id | carrier (FUN_140a627e0) everywhere | clear `FUN_140a71df9` (writes 0xff); set: store of EDI at 0x14045725a (`mov [rbp+0x2601c],edi`, NOFUNC) | clear verified; set base unverified | designated possession (existing) |
| MA+0x2483c fallback id | Chance/Counter/PostPlay fallback | setter `FUN_140a72a68` (value = param_2; source not traced) | verified store | missing |
| MA+0x2482c ball x (z at +0x24834) | every lane and stop | store of ESI at 0x140a6f29e (`mov [rdi+0x2482c],esi`, NOFUNC) | base unverified | ball Predict (existing) |
| MA+0x44 + 0x14·t team line x | offside line | not traced | not verified | AI_GetOffsideLine (existing) |
| MA+0x22e4c + 200·id per-player sorted distance lists; rank byte at +0xb2+id | `FUN_14061f8b0` (PostPlay score) | not traced | not verified | computed on demand (missing as a table) |
| MA+0x24710 ball distance table; rank byte MA+0x247be+id | `FUN_1404c8e60` (fallback test) | not traced | not verified | Dist2 from positions |
| MA+0x25cf8 / MA+0x25d38 team instruction id / value tables (team·4+slot) | `FUN_140a63950` (advanced instruction test) | none found: disp32, SIB, lea and SSE store forms all searched in .text, 0 hits | no literal writer | missing (stand-in: never set) |

## Match info MI = G+0x270
| field | readers decoded | PES writer(s) | status | GF |
|---|---|---|---|---|
| MI+0x116c | open-play test `== 1` (Counter mask, PostPlay stop, Overlap select, Diagonal stop) | `FUN_140a1197a` (+= param_1; period divisor +0x1168 from EAX; output +0x728 = 0x116c / 0x1168); clear `FUN_1411821c5` (= 0) | CONFLICT: the writer behaves as a clock, the readers as a state. Base object of FUN_140a1197a not established. Resolve before relying on the open-play reading | IsInPlay && !IsInSetPiece (existing) |
| MI+0x11a0 possession age (frames) | Chance mask, support, Counter | setters `FUN_1408ae72b` (= 10), `FUN_14100ce84` (= EAX), `FUN_14118221c` (= EDI); byte clears `FUN_141183362`, `FUN_141184546` (base unclear) | dword setters verified | possessionChange_ms (existing) |
| MI+0x1198 frame counter | everywhere | 12 store candidates, not decoded | not verified | GetActualTime_ms·54/1000 (existing) |

## Player record P = a48a10(G, id)
| field | readers decoded | PES writer(s) | status | GF |
|---|---|---|---|---|
| P+0x5d0 action state (PASS_GET = 5, 0x2b, 0x59) | PASS_GET test, Counter stop, PostPlay gates | about 89 store candidates; not validated | not verified | partial (PassGetReceiver for 5) |
| P+0x48 skill mask u64 (low dword read by `FUN_140a64030`) | skill vetoes (PostPlay, Diagonal, Chance) | PES in-match block (not traced) | reader verified | PlayerSkills (existing), SuitsRole |
| P+0x55c/+0x564 position, +0x574/+0x57c velocity, +0x554 facing | everywhere | motion system | reader verified | existing |
| P+0xc0 AI class; P+0xc4 pad flag | stop tests | copies of +0x40 (+0xc4 setter not traced) | reader verified | aiClass = role stand-in; pad flag never set (no pad player in gfsim) |
| G+(id+0xa5)·0x28 + 1: action byte | `FUN_140a6e340` (bit 16), `FUN_140a6e580` (bit 6), `FUN_140a6e150` | not traced | not verified | missing (carrier clause stand-in) |

## Holder dwords (O5)
| field | readers | writer | status | GF |
|---|---|---|---|---|
| rec+0xd4+4k, k = 0..2, rec = FUN_141b59e20(in-match block)+8 | `FUN_140a50250` (tail call 0x1408c75c0; raw entry verified at 0xa50250-0xa502e2) | not found | reader verified | missing (stand-in 0) |

## Event state
| field | readers | writer | status | GF |
|---|---|---|---|---|
| ctx+0xe0 latest event: kind +0, (team,idx) +4, time +0x10 (struct at container+0x180698) | `FUN_140a3e190(k=0)`, `FUN_140a51320` | not traced | reader verified | Match::PassEvent (single latest) |
| ring records at container+0x1908 + idx·800; count +0x3208; head +0x320c | `FUN_140a3e7d0`, `FUN_140a3e840`, `FUN_140a3e190(k>0)` | not traced | CONFLICT: `FUN_140a3e190` returns record+0x14 so (team,idx) sits at +0x18; `FUN_140a3e7d0` reads (team,idx) at +0x1c. One of the two is wrong | partial |

## Install tables (read-only; read via PeImage::ReadVa)
- 0x1426484F0: u32 per action id; bit 16 is `FUN_140a6e340`, bit 6 is `FUN_140a6e580`.
- 0x142613d20: u32 per registered position; skill suitability in `FUN_140a64030`.
- 0x1434A4D50: 126-entry action-name table.
- K = 10000 at 0x14259d634 (lane-0 score); 42.25 at 0x1425c721c (fallback ball distance squared); 6000.0 at 0x14260b390 (state-timer default).

## Corrections to earlier docs
- attack-leftovers §1.5: `FUN_140a50250` is not a team-instruction reader; it reads the holder record (raw verified).
- MI+0x116c open-play reading: unresolved conflict (above).
- Event ring qword offset: unresolved conflict (above).
