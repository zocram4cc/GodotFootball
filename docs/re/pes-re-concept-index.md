# PES concept-row index: the 46-row table (PES vs GF), mismatches, fixes

Title: GF's five defending concept ids are each one high; one captured row label is
wrong too. What the bytes say, row by row.

## §0 How the true ids were settled (read this first)

`FUN_140579E10` (VA `0x140579E10`) is a row-address selector: for concept id `edx`
it returns `base + OFF[edx]`, `NULL` for `edx > 0x2F`. CONFIRMED by bridge
decompile (`dec-579e10.txt`, same session): `local_198[0x14] = 0` and
`local_198[0x2F] = 0` are the two NULL arms; every other slot `0x00..0x2E` gets a
`base + OFF` LEA. 46 LEAs + 2 NULLs = 48 slots = ids `0x00..0x2F`.

True id → offset map (decompile + disassembly agree, slot-contiguity checked):

```
00:+0x0 01:+0x20 02:+0x60 03:+0x90 04:+0x5A8 05:+0xC8 06:+0x108 07:+0x140
08:+0x178 09:+0x1B0 0A:+0x1E0 0B:+0x500 0C:+0x260 0D:+0x2A8 0E:+0x540
0F:+0x568 10:+0x6D8 11:+0x3D0 12:+0x400 13:+0x440 NULL:0x14 15:+0x488
16:+0x4D0 17:+0x2E8 18:+0x5D0 19:+0x610 1A:+0x640 1B:+0x668 1C:+0x340
1D:+0x388 1E:+0x698 1F:+0x318 20:+0x700 21:+0x730 22:+0x748 23:+0x760
24:+0x778 25:+0x7A0 26:+0x7B8 27:+0x7D8 28:+0x7F0 29:+0x808 2A:+0x820
2B:+0x838 2C:+0x868 2D:+0x880 2E:+0x898 NULL:0x2F
```

The live dump `rows_live.txt` MISSED true row `0x15` (`+0x488`): its `idx0x14
off+0x488 NULL-ARM` line conflates the NULL slot `0x14` with row `0x15`'s offset,
and every label `0x15..0x2D` is one short (label = true id − 1). The T12b doc's
measured table (§3) carries the same shift from `0x15` up, and stops at `0x2D`,
dropping true `0x2E` (`+0x898`). All 45 other rows' METHOD vectors are intact;
only the id labels shift. Every claim below uses TRUE ids; the table notes the
old label where it differs.

Slot protocol (from `FUN_140579840`, VA `0x140579840`, image bytes):
`+0x8` gate, `+0x10` start, `+0x18` end-test, `+0x20` commit, `+0x28` execute,
`+0x30` interrupt stub. The selector loop runs `edi = 0..0x2F`
(`cmp edi,0x30; jl`, `0x140579993/0x140579B9E`): without possession, ids
`<= 0x20` are stopped (`cmp edi,0x21` at `0x140579AE9`); with possession, ids
`>= 0x21` are stopped (`cmp edi,0x21; setge` at `0x140579A33`). So `0x20` is the
last ATTACKING row, `0x21` the first DEFENDING row. CONFIRMED.

Gate semantics (image bytes, all eleven distinct `+0x8` gates share one shape:
`cmp [rdx+8],2` then `FUN_140A62180(ctx, side, r8d)`): `r8d` = sheet-toggle
index, `sete/test` polarity = required value. CONFIRMED pairs:

```
gate VA    r8d  test            meaning
0x140543F40 0  test,eax,eax;sete  Attacking Style == 0 (Counter)
0x14053DE40 3  cmp eax,1;sete     Positioning == 1
0x14054C3D0 1  test;sete          Build Up == 0
0x14056FBB0 1  cmp eax,1;sete     Build Up == 1
0x140558A10 2  cmp eax,1;sete     Attacking Area == 1 (Centre)
0x14056B630 2  test;sete          Attacking Area == 0 (Wide)
0x140574910 4  test;sete          Defensive Style == 0 (Frontline Pressure)
0x1405767F0 4  cmp eax,1;sete     Defensive Style == 1 (All-out Defence)
0x1405788D0 5  test;sete          Containment == 0 (Middle)
0x140579650 5  cmp eax,1;sete     Containment == 1 (Wide)
0x140579810 6  test;sete          Pressuring == 0
```

Row `0x1C` (`+0x340`) is the only row whose gate is the stub `0x141863350`
(always-true): its start runs ungated.

## §1 The 46-row table: PES (true ids, live methods) vs GF (`origin/main`)

`+0x8/+0x10/+0x18/+0x20/+0x28` below; `stub` = `0x141863350`/`0x141794410`
(empty). GF file:line from `origin/main` (`src/sim/onthepitch/`).

```
true  off    PES gate/start/end/ex20/ex28              GF id    GF binds to            verdict
0x00  +0x0   53D730/stub/53D600/53D5F0/stub            —        not ported             OPEN (no GF id)
0x01  +0x20  53DE40/53D8B0/53D780/53D760/53E940        —        not ported             OPEN
0x02  +0x60  543F40/53F710/53F600/53E950/53FEF0        —        not ported             OPEN
0x03  +0x90  543F40/540780/5404A0/540440/5415B0        0x03     SideCounter* (exact)   CONFIRMED match
0x04  +0x5A8 543F40/577A10/577800/576A20/577C30        —        not ported             OPEN
0x05  +0xC8  543F40/stub/543890/542D30/544B80          —        not ported             OPEN
0x06  +0x108 54C3D0/5465B0/545E50/544BB0/546FB0        —        not ported             OPEN
0x07  +0x140 54C3D0/5485F0/5483E0/547850/5494B0        —        not ported             OPEN
0x08  +0x178 54C3D0/54AD10/54A8A0/54A130/54B560        —        not ported             OPEN
0x09  +0x1B0 54C3D0/54C080/54BF80/54BD40/54C850        —        not ported             OPEN
0x0A  +0x1E0 54C3D0/54D9B0/54D4B0/54D0F0/54E480        —        not ported             OPEN
0x0B  +0x500 56FBB0/56C5E0/56C490/56BAF0/56CB00        —        not ported             OPEN
0x0C  +0x260 56FBB0/551900/5516F0/5516C0/552E90        —        not ported             OPEN
0x0D  +0x2A8 56FBB0/550520/550220/54F170/550D30        —        not ported             OPEN
0x0E  +0x540 56FBB0/56EC30/56E520/56D580/56F5B0        —        not ported             OPEN
0x0F  +0x568 56FBB0/stub/56FA60/56FA10/56FDE0          —        not ported             OPEN
0x10  +0x6D8 56FBB0/553C40/553B30/553300/5541E0        —        not ported             OPEN
0x11  +0x3D0 558A10/556C40/5568A0/556880/557DD0        —        not ported             OPEN
0x12  +0x400 558A10/5585F0/558400/558320/55A060        —        not ported             OPEN
0x13  +0x440 558A10/55BC60/55BAB0/55AE80/55C260        —        not ported             OPEN
0x14  —      NULL arm                                 —        —                      CONFIRMED (dec)
0x15  +0x488 (NEVER live-read; vtable+methods OPEN)   —        not ported             OPEN re-read
0x16  +0x4D0 56B630/565150/564FB0/563F10/5654B0        —        not ported             OPEN
0x17  +0x2E8 56B630/555730/555220/554CA0/556380        —        not ported             OPEN
0x18  +0x5D0 56B630/566C10/5666E0/565AF0/567F90        —        not ported             OPEN
0x19  +0x610 56B630/568BB0/5688A0/568510/569FE0        —        not ported             OPEN
0x1A  +0x640 56B630/stub/56A7A0/56D580/56AA40          —        not ported             OPEN
0x1B  +0x668 56B630/56B350/56B240/56B220/56BAB0        —        not ported             OPEN
0x1C  +0x340 stub/55ED70/55E910/562490/55FC20          —        not ported             OPEN
0x1D  +0x388 53DE40/561150/560C00/560510/561BA0        —        not ported             OPEN
0x1E  +0x698 53DE40/570970/570420/570380/572000        —        not ported             OPEN
0x1F  +0x318 53DE40/55D3C0/55D170/stub/55E240          —        not ported             OPEN
0x20  +0x700 53DE40/stub/573040/5728C0/5738E0          —        not ported             OPEN
0x21  +0x730 574910/5747F0/574710/stub/stub            0x21     Press (5747F0-fam)     CONFIRMED match
0x22  +0x748 574910/574EA0/574B00/stub/stub            0x22     LineDown (574EA0-fam)  CONFIRMED match
0x23  +0x760 574910/575300/5752A0/stub/stub            0x23     LineUp (575300-fam)    CONFIRMED match
0x24  +0x778 574910/575510/575410/579680/575DA0        0x24     Chase (575510-fam)     CONFIRMED match
0x25  +0x7A0 5767F0/575EA0/575DB0/stub/stub            0x25     Retreat (575EA0-fam)   CONFIRMED match
0x26  +0x7B8 5767F0/stub/575EE0/stub/stub              —        not ported             OPEN
0x27  +0x7D8 5767F0/stub/576350/stub/stub              —        not ported             OPEN
0x28  +0x7F0 5767F0/stub/576650/stub/stub              —        not ported             OPEN
0x29  +0x808 5788D0/5781D0/578170/stub/stub            —        not ported             OPEN
0x2A  +0x820 5788D0/stub/578870/stub/stub              —        not ported             OPEN
0x2B  +0x838 5788D0/stub/578930/578900/578DB0          —        not ported             OPEN
0x2C  +0x868 579650/578EF0/578E90/stub/stub            —        not ported             OPEN
0x2D  +0x880 579650/5794E0/579480/stub/stub            —        not ported             OPEN
0x2E  +0x898 579810/stub/5796A0/579680/stub            —        not ported             OPEN
0x2F  —      NULL arm                                 —        —                      CONFIRMED (dec)
(VAs `0x14…`; prefix elided. Old rows_live/doc labels equal true ids for
0x00–0x14, read one low for 0x15–0x2E: old `0x15+i` = true `0x16+i`.)
```

Row-identity notes settled at the bytes (all CONFIRMED unless marked):

- True `0x21` IS the press-window family: `0x1405747F0` = MI check, fps×5+0.5
  window vs `MI+0x11A0` (`0x14259222C/0x142565600` paired), ball-own-frame
  corridor, instruction-`0xD` veto (`0x140A63950` r8d=0xd), then
  `0x140574200` count; `0x140574710` = same window with the 2 s hold
  (`0x142565604`) and −10 floor (`0x14259BE84`). GF's `ForeCheckPress*`
  ports exactly these bodies at the same id — match, no fix.
- True `0x22` IS LineDown: `0x140574EA0` = keeper-hold veto (`0x140A53F90`)
  then `574A30` (ball window) + `574AB0` (|0x89A0−0x875C| ≥ 3,
  `0x14258C264` paired) + (`B8A3` level-0 or `574940` LONG_PASS facing);
  `0x140574B00` = in-play + same window + gap test. GF's `ForeCheckLineDown*`
  ports these at the same id — match, no fix.
- True `0x23` IS LineUp: `0x140575300` = keeper-hold veto + `B70A` check +
  `575210` + `574F40` containment; `0x1405752A0` = its negation. GF's
  `ForeCheckLineUp*` ports these at the same id — match, no fix.
- True `0x24` IS the chase family: `0x140575510` = 2208-byte chaser pick
  (3·fps window `lea ecx,[rax+rax*2]` at `0x1405755C3`, F6−10 `0x142588210`,
  F7−2 `0x142565604`, d² ≤ 100 `0x14253EE30`, goal-side ≤ 100° / inside ≥ 90°
  `0x14259BE5C`, second man via `0x140575930` walking to d² 400
  `0x1425A2670` with 15 m slack `0x14259BE0C`, trap point from the chaser 8 m
  `0x14259BDF4` along own attack heading ±160° `0x14259BE6C`); scalar tail at
  `0x140575DA0` jumps to `0x140575B60`, which stamps orders tagged concept
  `0x24` (`mov [rdi+rax*4+0xB748],0x24` at `0x140575D79`). Tag == row.
  GF ports this family at id `0x24` — match, no fix. The ticket's "0x24
  Chase (gate 0x1405767F0, start 0x140575EA0)" is really true `0x25` (old
  label of `+0x7A0`); the ticket's "0x23 +0x10" is really true `0x24`.
- True `0x25` IS RetreatBlockCreate: `0x140575EA0` = MI check then
  `call [rcx+0x18]; sete` — the logical NOT of its own row's end slot
  (`0x140575DB0`, the ≤5 m bands / 8−F6 / 18−F6 line-gap test,
  `0x14259222C/0x14259BDF4/0x14259BE18` paired). GF's Retreat port
  (`StartConcept`: `inPlay && !RetreatBlockCreateEnd`,
  `pesteamconcepts.cpp` constants, `teamAIcontroller.cpp` dispatch) implements
  exactly this pair at id `0x25` — match, no fix.
- `0x140579680` (true `0x24` +0x20) = ff-fill commit stub, shared with true
  `0x2E` +0x20. No GF counterpart needed (GF's `ExecuteConcept` for Chase
  calls the trap-point port directly). INF: whether PES's `57A1F0`-family
  caller rides `+0x20` for these rows is untested; GF doesn't model it.
- True `0x15` (`+0x488`): vtable + all seven methods OPEN — never live-read
  (the dump's NULL-ARM line skipped it). Blocks naming that row.

## §2 Mismatches with fixes

The ticket's suspected Chase/Press off-by-one DOES NOT EXIST at the bytes:
with true ids, GF's `0x21..0x25` bindings equal PES rows `0x21..0x25`
(gates, starts, ends all match; possession sides agree — true `0x21..0x25`
are all defending, `> 0x20`, same as GF's `> kLastAttackingConcept`). NO GF
ENGINE CHANGE. What exists instead:

M-doc — stale row labels (docs-only fix). `docs/re/pes-re-t12b.md` §3 table
and any `idx0xNN` cited from `rows_live.txt` for NN ≥ `0x15` are one low; the
table stops at `0x2D`, dropping true `0x2E` (`+0x898`: gate `579810`, start
stub, end `5796A0`, ex20 `579680`); true `0x15` (`+0x488`) is unread. Fix:
relabel per the §0 map, append true `0x15` (after a live re-read) and true
`0x2E`, and keep the NULL arms `0x14`/`0x2F` (already correct). Effect if
ignored: the next RE re-discovers the same phantom shift. Files: doc table
only; `pesteamconcepts.*`, `teamAIcontroller.*`, tests need NO id changes.

M-gap — unported live rows (known port-subset state, NOT a mismatch; listed
for completeness, all OPEN):
  - `0x20` (`+0x700`, gate `53DE40`, start stub, end `573040`, ex
    `5728C0/5738E0`): live attacking concept, unported.
  - `0x26` (`+0x7B8`, end `575EE0` carrier-identity + bearing/distance test:
    65° `0x14259BE48`, 120° `0x14256AE30`, radius ramp 9/3.5 m
    `0x14259D60C/0x14259BDE8`, floor 4 m, margin −5 m): unported.
  - `0x27` (`+0x7D8`, start stub, end `576350`: keeper-hold veto + `B70D` +
    `576260` + `5763E0` + `5760E0`): unported.
  - `0x28` (`+0x7F0`, end `576650`: `875C` band + `877D` count ≥ 4 press
    test): unported.
  - `0x29..0x2E`, `0x00..0x02`, `0x04..0x1F`: per the port doc (dead stubs,
    containment no-ops, or unstarted patterns). Unchanged.

GF constants verified UNCHANGED (still correct under true ids): gate VAs
`0x140574910` (Press/LineDown/LineUp/Chase, `pesteamconcepts.cpp`
`ConceptGate`, `:699-711`) and `0x1405767F0` (Retreat) match the live `+0x8`
slots of true rows `0x21..0x24`/`0x25`; `kLastAttackingConcept = 0x20`
matches the selector CMPs (`0x140579A33/AE9`); `kConceptCount = 48` matches
the 48-slot selector (`0x00..0x2F`); `kConceptSideCounter = 0x03` matches
true row `0x03`. `RunConcepts` (`teamAIcontroller.cpp:2195-2233`),
`Start/End/ExecuteConcept` (`:2687-2780`), `IsPressing` (`:2365-2370`),
push-up guards (`:569-575`): no id changes needed. Tests in
`tests/sim/pes_team_concepts_test.cpp` reference ids symbolically; no
updates needed.

## §3 Status ledger

| item | verdict | evidence |
|---|---|---|
| 579E10 = 48-slot selector, NULLs 0x14/0x2F, OFF map §0 | CONFIRMED | bridge `dec-579e10.txt` + slot-contiguity check §0 |
| rows_live labels 0x15–0x2D each one low; true 0x15 unread | CONFIRMED | decompile slot ids vs dump offsets (§0) |
| ticket's "0x24 Chase / 0x23 +0x10" = old labels of true 0x25 / 0x24 | CONFIRMED | §1 table |
| GF 0x21–0x25 bindings = true rows 0x21–0x25 (gates+starts+ends) | CONFIRMED | live vectors vs GF VA citations (§1) |
| Chase order tag 0x24 == its row 0x24 | CONFIRMED | `0x140575D79` bytes |
| kLastAttackingConcept/kConceptCount/SideCounter ids | CONFIRMED | selector CMPs, slot count, row 0x03 |
| true 0x15 vtable+methods | OPEN | needs live re-read at `[H+0x450]+0x62B8+0x488` in open play |
| true 0x20/0x26/0x27/0x28+ bodies and names | OPEN | bodies disassembled, not reduced |
| 0x24 +0x20 (579680) caller family | INF | stub shared with 0x2E; GF doesn't model +0x20 |

## §4 Recommended next step

1. Relabel the T12b doc table per §0 (docs-only, ~10 lines) and cite this
   note; no engine change.
2. (One live session, optional but cheap:) re-read true row `0x15`
   (`+0x488`) vtables+methods in open play and append it; that closes the
   last hole in the 46-row table.
3. Decode true `0x20` (`573040/5728C0/5738E0`) next if an attacking port is
   wanted — it is the only live attacking row besides `0x03` with a full
   method vector in the captured set.
