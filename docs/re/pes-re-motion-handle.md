# PES motion-handle system, ball holder/state bytes, kind-0xB inputs (StallBreaker rounds 4-5 follow-up)

Decode of `FUN_141F524F0`'s motion-table inputs, the holder byte, and every writer of
`G+0x40` (`ball`) `+0x2910` (holder dword), `+0x2914` (state) and `+0x2915`.
All VAs verified by `verify_vas.py` in `gf-MotionHandle-scratch` (62 checks green; one planted
wrong VA fails, exit 1). Displacements below come from a whole-section scan of `pes_full.bin`
(`.trace` flat `0x1000..0x252E800`, 10,513,287 insns): 30 hits `0x2910`, 17 `0x2914`,
1 `0x2915`, 12 `0x2924`, plus a RIP-target scan (21,339 refs to 4 globals).

## 0. Constants (script-read from the image, offset = VA - 0x140000000)

```text
control 0x14256AE1C = 1.0f                                  # mapping check
0x14351AE44 = 0xFFFFFFFF                                     # null motion handle
0x14351AE54 = 54.0f                                          # fps (FUN_141E532D0)
0x142588210 = 10.0f, 0x142565600 = 0.5f                      # slide-window 540 = 54*10+0.5
class table 0x1426484F0 + action*4: [11]=0x4080 [7]=0x4020 [13]=0x4040
  [15]=0x2000 [16]=0x3002 [17]=0x3000 [72]=0x8000            # bit7/6/9, 13, 5, 15, JOSTLE
```

## 1. Motion-table handle system end to end (CONFIRMED: translated `pes_fn_1414BB580` + bridge)

`FUN_1414BB580(table, handle)` (body `0x1414BB580-0x1414BB6D5`-class; entry VA verified):

```text
r1 = handle
handle >> 14 == 0                      -> table+0xD0B0EC   (handles 0..0x3FFF invalid)
handle == *(0x14351AE44) = 0xFFFFFFFF  -> table+0xD0B0EC   (null handle)
handle - 0x3FFF0 <= 14                 -> table+0xD0B0EC   (0x3FFF0..0x3FFFE invalid)
r1 = handle & 0x3FFF
r1 - 0x3FFD <= 1 (r1 in {0x3FFD,0x3FFE})-> keyed path: pes_fn_1414C10E0(*(DAT_143705E10)+0x50 + 0x35960, ...)
else if *(table+0x183789C) != 2:
    (u32)r1 > 750                      -> table+0xD0B0EC
    else return table+0xADF4BC + r1*1680                  (direct index, stride 0x690)
else:                                    search [table+0xADF4BC, +count*1680] via pes_fn_1414B8230,
                                         count = *(table+0xD0BCEC), key = low-14 + high id bits
```

- Handle format: 32-bit word, low 14 bits = row selector, high 18 bits = id space,
  `0xFFFFFFFF` = none. (The fixdemo side word packs a team id in the high bits: id = word>>14,
  docs `pes-re-cutscenes.md:725,739,778`.)
- The 0x690-stride array at `table+0xADF4BC` (11400380 = `0xADF4BC`, 1680 = `0x690`): direct index
  when `*(table+0x183789C) != 2` (bound 750 rows), hash-probed search (`1414B8230` over
  `1414B8520`/`140FC7ED0`) when `== 2` with count at `table+0xD0BCEC`.
- The `+0x50` path (`pes_fn_1414C10E0`, translated): keyed lookup in the table behind
  `*(DAT_143705E10)+0x50` at `+0x35960`, matching low-14 key and `0xFFF00000` high bits,
  returning a `+0x138`/`+0x7C8` row.
- Second array in `FUN_1414BAEC0(table, key)` (body `0x1414BAEC0-0x1414BAF72`, bridge decompile):
  base `table+0xC4CA0C`, stride 600 (`0x258`), count `*(table+0xD0BCF0)`, same `+0x183789C` mode
  flag, fallback `table+0xD0BA90`; special keys `0xFFFD/0xFFFE` go through
  `1414C20F0(*(table+0x50)+0x35960)`. `141F524F0` uses it for the per-slot loop state
  (`+0x251` track byte, `+0x220` list).
- Loader: the manager object is runtime-built, not file-mapped: `FUN_1414B60A0` allocates `0xE0`
  and runs ctor `1414B57A0`, storing `DAT_143705E10` (store at `0x1414B60DA`, clears at
  `0x1414B60E3`/`0x1414B742A`; sole creator call `0x140403A96` in `FUN_140403A90`, startup path).
  Which init pass fills the `0xADF4BC` rows / `0xD0BCEC` count from install motion data was not
  traced (INF; next: writers of `table+0xD0BCEC` / `+0x183789C`, or a live hook on `1414BB580`).
  GF-side precedent: motion/clip bytes come from the install (cpk/mtar/`animetable`/`clips`).

## 2. The 0x2830-stride record is a fixdemo side block, NOT the ball (CONFIRMED; corrects round-5)

- `S(s) = ctx+0xF8+s*0x2830`, 2 sides, built by `FUN_141F4C950` (`pes-re-cutscenes.md:685,768`).
  Each side: 40 player slots x 0x100, then a tail at `+0x2800`.
- `FUN_141F524F0(ctx, idx)` (`0x141F524F0-0x141F52AC0`, bridge) writes `ctx+idx*0x2830`
  (= `S(idx)-0xF8`): `+0x28F8/+0x28FC` replay ids from row `+0x444..0x449` (gated by row
  `+0x41C` bits `0x100000/0x200000`), `+0x2914` dword = `(row+0x420>>26)&7` (= `S+0x281C`),
  `+0x2920` byte = bit 25 of `row+0x420`, then the `+0xFA+0x100k` slot loop. Callers pass only
  idx 0/1: `0x141F51CB3/0x141F51D73/0x141F51E00/0x141F51EAF` (EDX=0),
  `0x141F51CDB/0x141F51E22` (EDX=1) in `FUN_141F51C40/1D00/1D90/1E40`.
- The `+0x2924` handle it reads (`0x141F525CB`) is `S+0x282C` = the team-id word
  (`docs pes-re-cutscenes.md:778`: written by `FUN_141F52470`, called from `FUN_141F51630`
  at `0x141F51A58` with `FUN_140A7F1E0(setup,s)`; translated + ledger "diffed real").
- The ball record (`*(G+0x40)`, `G=[0x1436F3DC8]`, 0x29F0-byte alloc, vtable `0x1425A0B48`
  set at `0x14046DC63`) is a different struct. So `141F524F0` NEVER writes the ball's state:
  round-5 §10.2-10.4's "state writer" premise is wrong. The ball's in-play writers are §3.

## 3. Ball `+0x2910` / `+0x2914` / `+0x2915` writers (whole-image scan; CONFIRMED)

- `+0x2910` holder dword. Disp writers, whole image: reset `0x140A57247`
  (`mov qword [rbx+0x2910], 0xFF`, in `FUN_140A571F0`, body `0x140A571F0-0x140A572E7`),
  restart `0x140A57902 (`mov dword [rcx+0x2910], 255`, in translated `pes_fn_140A57900`).
  CORRECTION to round-5: `140A57900` takes only the ball and writes constants
  (state byte = 0, `+0x291C` qword = 0, `+0x290C` = 255); there is no argument.
  In-play source: `FUN_140872C10` writes `ball+0x290C/+0x2910/+0x2914/+0x2918`
  from a contact struct (`+0x288/+0x28C/+0x290/+0x294`), `+0x291C` qword from `+0x298`,
  `+0x2924` from `+0x2A0`, plus a conditional kick-word refresh (`0x14087352C` line;
  function body contains `0x14087352C`, Ghidra names it `FUN_140872C10`).
  Propagation: `FUN_140430C30` ball-record copy (copies `+0x290C` qword incl. holder,
  `+0x2914` qword incl. state/`+0x2915`, `+0x291C` qword, `+0x2924` dword, tail to `+0x2960`).
- `+0x2914` state byte. Disp writers: restart `0x140A57913` (0), `141F524F0:0x141F5265E`
  (side `S+0x281C`, not the ball), `140872C10` (dword from contact `+0x290`, the true in-play
  writer). Readers include the `140A18690` pair (`0x140A186E3/0x140A187C3`), the in-play builder
  `140A18940:0x140A18A54`, and two getup-region reads (`0x14072D67A/0x14072D98B`, record INF).
- `+0x2915`: NO disp32 writer in the whole image; the single hit is the reader
  `0x140A187E5` (`movzx eax, byte [r15+0x2915]`, in `FUN_140A18690`). It is written only as
  upper bytes of the `+0x2910` qword (reset: 0), the `+0x2914` dword/qword (contact `+0x290`
  bytes 1..3, normally 0; record copy). So the `0xE` payload (`holder > 0x15` branch) is 0
  after reset/restart; its in-play nonzero values, if any, come from contact `+0x290` high
  bytes (OPEN). Caveat: writes via computed pointers (`[reg]` with no disp, `rep movs`)
  are invisible to both scans.
- `+0x2924` (ball handle dword): writers are reset-byte `0x140A57254`, the `140430C30` copy,
  and `140872C10` from contact `+0x2A0`. (Side `S+0x282C` writers use disp `0x282C`.)
- CORRECTION to round-5 §10.3: `0x141F516A3` is `mov qword [rax+0x18],rbx` in `FUN_141F51630`
  (not a `+0x2910` write, not in `141F4D2F0`). The real write is `0x141F517AD`
  (`mov byte [rdi+0x2910], al`) in `FUN_141F51630` (`0x141F51630-0x141F51C12`), and it stores
  `FUN_140A3BFC0(R14, 0)` = count of nonzero bytes in `R14+0x1748` (11 flag bytes) = the
  SIDE's players-on-pitch byte (`pes-re-cutscenes.md:720,774`; GF `fixdemo_select` stand-in),
  not the ball holder. Second call `0x141F517B6` stores `FUN_140A3BFC0(R14, 1)` at `+0x5140`.
- `FUN_140A3BFC0(a, s)` (body `0x140A3BFC0-0x140A3C00C`, bridge): counts set bytes in
  `a+0x1748+s*0xB` (11 bytes); returns 0..11. Translated + ledger "diffed real".

## 4. `FUN_140A18690` full decode (body `0x140A18690-0x140A18800`, bridge; CONFIRMED)

`ball = *(G+0x40)`, `holder = ball+0x2910` (dword, `0x140A186B7`), `state = ball+0x2914` byte:

```text
holder > 0x15  -> kind 0xE, payload[0xE] = ball+0x2915; (only +0x2915 reader)
else rec = FUN_140A48910(G, holder)      # man record; >= 0x1A -> *(G+0x19C8)
  state == 0 or > 6 -> kind 0xD
  else inband = (rec+5) in 1..3 (state<4) or 4..6 (state 4..6)
    class bit7 (FUN_140A6E4B0(rec+1)) -> 0xB
    if inband: bit6 (FUN_140A6E580) -> 0xC elif bit8 (FUN_140A6E070) -> 10
actor key = FUN_140A62FE0(*param_3) -> payload+0x4; slot = index of holder in
  [G+0x180]+i*0x74+0x20 (i<0x18, else 0x18) -> payload+0xC; state byte -> payload+0x20
```

Called once, from `FUN_140A17B30` at `0x140A17BA6`, which dispatches on `ball+0x291C`:
`==1` + `140A57950(ball)` -> in-play `140A18940`; `==2` + `140A57930(ball)` -> `140A18690`.
`140A17B30` itself is only DATA-referenced (`0x142640D20`, table-driven). `140A18940` (in-play)
reads the same ball fields (`+0x27DC` kind, `+0x2910/0x2914`, `+0x2818/+0x281C`).

## 5. Translated coverage in GF (`src/pes/translated`)

| fn | status |
|---|---|
| 1414B6A60, 1414BB580, 1414B8230, 1414B8520, 140FC7ED0, 1414C10E0 | translated, ledger "diffed real"; no engine caller (harness-only) |
| 141F524F0, 140A18690, 140A571F0, 140971BA0, 1414BAEC0, 141F51ED0, 141F51C40/1D00/1D90/1E40 | NOT translated (1414BAEC0 ledger "undecoded") |
| 140A57900, 140A3BFC0, 140A48910, 140A62FE0, 140A6E4B0/580/070, 140A3E190/8C0, 140A7F1E0, 141F52470, 141F4D2F0, gates 140A57950/930/940 | translated, "diffed real" |
| 141F51630 | stand-in (`fixdemo_select.*`, `gf_match.cpp` counts players) |
| consumer `BuildSecondBallEvent`/`DetectBallEvent`, `EventAt`/`NewestOrRing0`, `PesWorld::AnimeRecord` (via translated 140A48910), class tables baked | ported in `pesevents.*`/`pesworldrecord.*` |

GF never feeds the second-state path: `matchevents.cpp:119-147` hard-codes `ballState=1`,
`kickGate=true`, `state=0`, `holder=-1/0`, roster empty; `BallEventInput.holderClassKind`
defaults `0xD` with the three class tests left to the caller (`ponytail` at
`pesevents.cpp:579`). The 540-frame window scan itself (`EventAt`, n<4, kind `0xB` +
`140A60300` key equality) is the only unported line of refusal (d).

## 6. What GF state it needs in PES layout

`PesWorldTables` already models `G` (`+0x40` ball ptr, `+0x180` roster, `+0x19C8` anime table,
`+0x270` MI). Missing per-tick ball-record bytes: `+0x290C` lastToucher, `+0x2910` holder,
`+0x2914` state, `+0x2915` upper byte, `+0x2918`, `+0x291C` dispatch, `+0x2924` handle,
`+0x27DC` kick word (GF substitutes touch events + constants). Sources in PES order:
contact struct (`140872C10`: `+0x288..0x2A0`) -> ball record -> `140430C30` copies.
Motion side needs `DAT_143705E10` contents (rows `+0x420` state bits) behind `PesState`
memory; the resolver + search + keyed path are translated and callable today, but
`PesState` reads image zeros for that global (no GF backing).

## 7. Minimal faithful port order

1. Translate `140A18690` + `140A18940` (or keep the native builders; they are fully decoded
   above) and drive them from a `PesState`-backed ball record instead of `matchevents`
   stand-ins: fill `BallEventInput.holder/holderFlag/holderByte5` from GF possession
   (holder slot) + contact state, `holderClassKind` via the already-translated class tests
   on `AnimeRecord(holder)+1`, roster from `G+0x180`, actor key via `140A62FE0`.
2. Port the contact-to-ball write (`140872C10` block: struct `+0x288..0x2A0` -> ball
   `+0x290C..0x2924`) so holder/state are PES values, not GF guesses; `140430C30` copy
   as needed for the snapshot path. Red test: "a slide touch with ball state nonzero
   writes kind `0xB`" (round-4 §9.7.3, now unblocked).
3. Gate `_SlideCandidate` on the own-kind-`0xB` 540-frame window (`EventAt`, n<4;
   constants `10.0`/`0.5` from `§0`).
4. Motion-table backing (larger job, independent): back `DAT_143705E10` in `PesState`
   from install motion data, then `141F524F0` + `141F51ED0` + `1414BAEC0` translate +
   side-block state (`S+0x281C` etc.). Only needed for cutscene-condition fidelity,
   not for the breaker.
5. Still OPEN (unchanged): which of the 76 `1406B4580` callers admits a resting-ball touch.

## 8. Verification / open items

- `verify_vas.py`: 62/62 green; mutation (wrong mnemonic at `0x141F517AD`) fails exit 1.
- No engine change, no gate (decode-only branch work; Main gates).
- INF: row-`+0x420` bit semantics beyond 25..28; `1414B57A0`-downstream row fill (file?);
  record behind the getup `+0x2914` reads (`0x14072D67A/0x14072D98B`, Ghidra-unanalyzed,
  `.pdata` starts `0x14072D5EF/0x14072D885`); nonzero `+0x2915` provenance (contact `+0x290`
  high bytes); `140422030`-family `+0x2910` record identity (not the ball: has `+0x228`
  traffic, passes `&+0x2910` to `140A3BCF0`).
