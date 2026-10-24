# Ledger: engine boundary, leaf accessors, table and vtable edges

Branch `Ledger`: `f0bd5b8` (the cut), then `8308c86` (`joins.py`, left out of the first commit). `git ls-remote origin Ledger` = `8308c86c6b4236b629245c0c4628dc6fdca334eb`. Parent of the cut: `2db69f1`.
Files: `tools/dev/ledger/universe.py` (rewritten), `universe.json`, `joins.py` (function list), `joins.json`, `offpath_allow.json`, `tools/dev/ledger.py`, `docs/re/LEDGER.md`.

## Roots

The 38 doc-cited seeds are unchanged (`SEEDS` in `universe.py`, each with its cite).

## Headline correction

The brief's example is not engine code. `FUN_1403B9780` lists 12,705 lines because the bridge returns Ghidra's whole body range, which runs over neighbouring functions. Its body is 7 instructions: it is a getter (`return *(p+8)+0xa0`), and the decompile says the same. It is now a leaf accessor. The same artifact gave six rows a size of 20,000 (`FUN_140809070` is 3,495 instructions, `FUN_140426A80` is 20).

## What the crawler now does (rules, with evidence)

1. **body**: the body is the instructions reachable from the entry by control flow (fall-through, direct jumps and branches; calls return). Replaces the listing range.
2. **names**: the bridge's named symbols are kept. The old crawl dropped all but `FUN_`, so 2,387 library names were invisible. Interior call targets alias to their containing function (`FUN_1402B04BD` -> `FUN_1402B0462`).
3. **boundary rules** (rows recorded, not walked; their callees are not expanded):
   - `library` (10 rows). Ghidra symbols naming CRT, STL, zlib, libxml or Winsock code (`__scrt_*`, `_Init_thread_*`, `__security_check_cookie`, `?_Facet_*`, `inflateResetKeep`, `xmlThrDefSet`, `Wspiapi*`). None of the 2,387 bridge symbols is in a `match` namespace.
   - `doc` (6 rows). A report names the VA as the engine side of a call: phrases such as "engine call is", "Engine side", "AnimMovePlugin", "BodyAnimControl::", "fox::", with the VA after the phrase on the same line. VAs before the phrase are callers and stay walked. Rows: `141AF3180` (AnimMovePlugin scale), `141AF61C0` (SetMotion), `141AF6E40` (SetSpeed), `141AFB090`, `141B97880` (collision frame response, fox physics), `141EB6840` (face GenAnimControl).
   - `rtti` (607 rows). A walked function references a `.rdata` vtable whose COL signature and self-RVA check out; the TypeDescriptor's outermost namespace is engine (`fox`, `cobra`, `KS`, `gr`, `draw`, `menu`, `flow`, `std`, `collision`) or a template. Its code slots are engine. Roots: `?` templates 235, `draw` 185, `cobra` 97, `fox` 87, `collision` 2, `std` 1. Validated against the docs: `0x142564B10` = `fox` (EntityClassDictionary), `0x1425E34D8` = `match` (AnimePlayer), `0x1426329A8` = `match` (match::Player).
   - `crt` (33 rows). FUN_ functions inside `[0x14159EF7C, 0x1415B49B0]`, the span of the library-named symbols in `0x1414xxxx`-`0x1417xxxx`. The docs place the CRT's entry at `0x14159F2BC` and its EH handlers at `0x1415A16FC` and `0x1415A1738`; sampled rows (`0x1415A02B0`-`0x1415A1798`) are in that region. [INF] the other rows in the span are CRT too; not each was read.
4. **leaf accessor**: at most 10 instructions, no call, no jump out, no indirect or table jump, no vtable reference. 1,232 rows; 1,140 of them have no port, stand-in or boundary cite and carry the status `leaf accessor`. The team getter `140A48A30` (9), the player getter `140A48A10` (9) and `141E532D0` (the 54 Hz fps read, 2) are leaves.
5. **table edges**: relative jump tables are read from the image at the VAs in the code, in two shapes. Byte-indexed: dispatcher `0x1409F8030`, RVA table `0x1409F8170`, byte map `0x1409F81C0`, 19 arms (the docs' decode). Direct-indexed: the MOVESTEP state dispatch `0x1406BA900` through table `0x1406BB50C`, 24 arms; the first arms (`0x1406BAA90` kInit, `0x1406BB33B` kIdle) match `pes-re-runloop-arms.md` §4. 396 jumps resolved.
6. **vtable edges**: every code slot of a vtable a walked function references (validated as above) is a callee. This is what brings the concept and AnimePlayer classes into the universe.

## Counts (rows), before -> after

| status | before (HEAD `docs/re/LEDGER.md`) | after |
|---|---|---|
| total | 9426 | 11801 |
| ported | 491 | 527 |
| diffed | 8 | 7 |
| stand-in | 94 | 96 |
| decoded | 1054 | 1089 |
| leaf accessor | 0 | 1140 |
| engine | 0 | 655 |
| undecoded | 7776 | 8284 |
| inert | 1 | 1 |
| out of scope | 2 | 2 |

The headline gap (undecoded + stand-in) went from 7,870 to 8,380. It rose because the walk now reaches 2,375 more rows through table and vtable edges, and most of the new reach is undecoded. The cut (655 engine rows) and the leaves (1,140) remove rows from the gap, but they do not offset that reach. Decoded rows that are leaves moved to `leaf accessor` (same rank, no regression).

`offpath_allow.json`: 192 -> 161 entries. 37 old entries are now reached and dropped; 6 new ports are off-path and added. The 161 remaining each state the same reason: no walked edge reaches the port's function (no direct, tail, table or vtable edge from the universe). That is an attribution gap, not an explanation per entry; see residual 4.

## Top-10 gap lists over the cut universe

By callers (undecoded or stand-in):

| VA | name | status | callers | size | sub |
|---|---|---|---|---|---|
| 0x140A48A30 | FUN_140a48a30 | stand-in | 822 | 9 | TeamTick |
| 0x140A48A10 | FUN_140a48a10 | stand-in | 759 | 9 | Player |
| 0x14043A0A0 | FUN_14043a0a0 | stand-in | 339 | 35 | Player |
| 0x140A48910 | FUN_140a48910 | stand-in | 323 | 9 | Player |
| 0x140A3E190 | FUN_140a3e190 | stand-in | 309 | 21 | TeamTick |
| 0x140A627E0 | FUN_140a627e0 | stand-in | 271 | 28 | Player |
| 0x140A489F0 | FUN_140a489f0 | stand-in | 258 | 9 | Player |
| 0x140227930 | thunk_FUN_1403fb2e0 | undecoded | 230 | 1 | Restart |
| 0x140A57800 | FUN_140a57800 | stand-in | 213 | 2 | Ball |
| 0x140A92D40 | FUN_140a92d40 | stand-in | 211 | 32 | Ball |

By size (undecoded or stand-in, size > 0):

| VA | name | status | callers | size | sub |
|---|---|---|---|---|---|
| 0x140809070 | FUN_140809070 | undecoded | 1 | 3495 | Player |
| 0x1405C8070 | FUN_1405c8070 | undecoded | 1 | 2883 | Player |
| 0x141F4D2F0 | FUN_141f4d2f0 | undecoded | 5 | 2081 | Restart |
| 0x142315CCC | FUN_142315ccc | undecoded | 1 | 1944 | MatchTick |
| 0x140651430 | FUN_140651430 | undecoded | 1 | 1905 | Player |
| 0x140656270 | FUN_140656270 | undecoded | 1 | 1860 | Player |
| 0x14070C6F0 | FUN_14070c6f0 | undecoded | 2 | 1780 | Player |
| 0x14064E4B0 | FUN_14064e4b0 | undecoded | 1 | 1698 | Player |
| 0x140A0E940 | FUN_140a0e940 | undecoded | 1 | 1665 | MatchTick |
| 0x140706350 | FUN_140706350 | undecoded | 1 | 1606 | Player |

The top-by-callers list is led by leaf-shaped GF stand-ins (see residual 5).

## Residual gaps

1. **787 unresolved code rows (not protected)**: the bridge has "No function found" for them. They are vtable-slot targets (2,468 edges), table targets (31) and tail targets (6). The bytes exist in the image, so a capstone decode from the image would close them. Not done in this pass.
2. **150 protected rows**: `thunk_EXT_FUN_143xxxxxx`-style stubs into `.impdata`. No body on the bridge; they are not engine by evidence (the docs show match logic behind some of them, e.g. the CornerKick slot).
3. **5,410 unresolved indirect sites** in walked bodies: `CALL qword ptr [reg+disp]` (vtable calls, most of them), `CALL qword ptr [0x14252FD58]` (a global function pointer, 1,050 sites), and `JMP RCX`/`RDX`/`RAX` switch shapes other than the two decoded. Each is a boundary the walk does not cross.
4. **161 off-path ports**, each allow-listed with the same reason. Per-entry attribution (which indirect site or vtable reaches each) is not done.
5. **Stand-in precedence**: a leaf-shaped function with a GF stand-in keeps `stand-in` (`140A48A30`, `140A48A10` and others). The leaf accessor status applies only without a port or stand-in cite. Main should decide whether a trivially read getter should be a leaf accessor regardless of the GF stand-in.
6. **Doc rule sensitivity**: it takes only VAs after the phrase. Engine callees named before the phrase stay walked (`141EB5810` under `FUN_1406EB210`), so the doc count (6) is a floor.
7. **RTTI rule over-approximation**: a function that only tests a vtable also makes its slots engine. [INF] small effect; not measured.
8. **diffed**: 21 spec VAs in the spec; 7 are in the universe (8 before). The spec is HEAD's `diffed` map, passed as `--diff`.
9. **Unnamed CRT rows**: the crt span is a range rule. [INF] the 33 rows are CRT; not every row was read.

## Verification

- Crawl runs with `--image` against the live bridge: 7,178 rows (closure, no table edges), then 11,801 rows (with both table shapes); 177 s and 316 s.
- Probes: `1403B9780` = 7 instructions, leaf; `140A48A30` = 9, leaf; `141E532D0` = 2, leaf; `1402B04BD` -> `1402B0462`; vtable roots `0x142564B10` fox, `0x1425E34D8` match, `0x1426329A8` match; the MOVESTEP table's first arms match the doc.
- `python3 tools/dev/ledger.py --check`: green (stamp, regression, off-path allow). Output: `off-path ports: 161 (all allow-listed)`, `LEDGER.md current (11801 rows)`.
- No PES bytes committed: VAs, sizes, counts, RTTI namespaces and Ghidra symbol names only. a grep for machine paths over the committed files: clean.
- Not run: the gate and any test binary (Main gates). No C++ code changed.

## Reproduce

```
# bridge function list (names included) -> scratch
curl -s "http://127.0.0.1:8080/list_functions?limit=1000000" > <scratch>/list_functions.txt
python3 tools/dev/ledger/universe.py --image <PES image dump> [--out <path>]   # bridge; re-crawl only when roots/rules/image change
python3 tools/dev/ledger/joins.py --diff <spec.json> --funclist <scratch>/list_functions.txt
python3 tools/dev/ledger.py            # regenerate docs/re/LEDGER.md
python3 tools/dev/ledger.py --check    # gate
```

The `--diff` spec is HEAD's `diffed` map from `joins.json` (21 VAs; their source is `tools/dev/diffharness/specs/strategy_minute0.json`).
