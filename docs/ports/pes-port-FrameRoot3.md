# FrameRoot3: live pump-ring records through the pump's routing; handler reach; PROVEN per subsystem; why two functions had no replayable case (26-10-26)

Branch `FrameRoot`, pushed: `a575f398` (`git ls-remote origin FrameRoot` confirms). Earlier this session: `fc03afd5`, `58634146`. Gate not run (Main gates). Scratch: `<workdrive>/gf-FrameRoot-scratch/` (PES-derived, not committed). No new SIGSTOP snapshot; no live reads. No wait models: each run calls the pump's routing function `0x141B3EDD0` directly with the pump object and one live pending record, one record per run.

## Result

- **Live records: 39 pending records in 17 snapshots** (every snapshot with a dump; `ring_records.py`). Every record's node type is 2 (`w0 = 0x400002`); the sections walked are type 4 (see §1 caveat on the walk).
- **Dispatch: 39 of 39 ran** (`--root entry --entry 0x141B3EDD0`, rcx = pump, rdx = the record). Each run executed 214–260 functions; union 359.
- **Presentation handler reach: 0.** None of the 639 pure presentation specs (replay, audio, camera, HUD, crowd, AI) is executed by any dispatch.
- **Cutscene reach: 18 functions** (of 833 pure cutscene specs), reached in 1 to 39 runs.
- **Replay of the 18 reached cutscene functions** (snapshot `cut/snap1-213436`, trace `reccut/cut_snap1-213436_s7`): **13 PROVEN**, 1,918 traced calls, bit-exact, 0 mismatches. **2 with mismatches**: `1403FB710` and `1403FB730` (7 calls each, TLS accessors, §3). **3 with no traced call** in that snapshot.
- **Why 1404F6C30 and 1405CE4F0 had every case excluded:** both call `0x140A79640`, which is unmodellable (§4). The driver excludes every case that enters it. The old note said only "no replayable call (0 cases)".
- **Driver note now loud** (commit `a575f398`): an all-excluded function's note lists the reasons and the blocked callees. The reasons were already in the results JSON; they are now in the note and the summary line. The gate's `driver.py` still has the silent note (open 6).

## 1. Records and routing

- Pump `0x12F19C0`, vtable `0x142AC1D38` (checked per snapshot). Container `[pump+0x10]`. Pending records: `idx..idx+cnt-1` (mod 32) at `C+0x40000+slot*16`. Each snapshot has 2 or 3 pending records (e.g. `stop/snap4-163107`: idx 18, cnt 3).
- Hub routing (`0x141B3EDD0`, hub = pump; `records.md` §1.1, CONFIRMED by disassembly): per section, channel 0 → `[pump+0x1020+t*8]` slot 2, channel 2 → `[pump+0x820+t*8]`, else → `[pump+0x20+t*8]`; then always `[pump+0x1820+t*8]` slot 2. Live targets (`stop/snap4`): type 4 channel 0 → router `0x14197C8C0`; channel 2 → `0x14197C8E0`; default → `0x14197C8A0`; type-table listener → `0x1419B5570`. Type-2 listeners → `0x140421F40` (return-0 stub).
- **Caveat (not fixed):** the section walk (`w>>0x11` as a byte stride from `rec+0x20`) never sees the terminator (`w == 3`) and reaches its 64-section cap on every record. So the per-record section list is not a verified layout; the dispatch does not depend on it. Channel values seen are a sample (0, 2, and 398–460).

## 2. Handler reach

Reach by universe subsystem, union over 39 runs (`reach.py`; each executed address mapped to its pdata function, then to `tools/dev/ledger/universe.json`):

| universe subsystem | distinct functions | notes |
|---|---|---|
| not in universe.json | **253** (70% of 359) | no row; subsystem cannot be stated |
| pre-match | 67 | |
| cutscene | 13 | |
| in-match menu | 5 | |
| gameplay | 3 | |
| replay | 3 | `0x1403F3360`, `0x1419C8840`, `0x141B38C00` |
| restart | 2 | `0x1403FF430`, `0x1403FF490` (shared utilities) |
| AI | 1 | `0x14159F3E0` (`__security_check_cookie`) |
| HUD | 1 | `0x1403FB4F0` (shared utility, not a HUD handler) |
| camera | 1 | `0x141B8A5C0` (in 2 runs) |
| animation | 1 | |
| out: save/load | 9 | |

Handler reach against the pure spec sets:

| spec set | specs | reached by the dispatch |
|---|---|---|
| presentation (replay 182, audio 168, camera 149, HUD 53, crowd 42, AI 17) | 639 | **0** |
| cutscene (cutscene 751, AI 25, in-match menu 16, restart 9, pre-match 7, ball 6) | 833 | **18** |
| impure-sample | 50 | 0 |

- Per handler class: **camera 0, HUD 0, crowd 0, audio 0, FixDemo 0.** Replay: 3 universe-tagged utilities in the union, none in the replay spec set. The one camera-tagged function (`0x141B8A5C0`) is in two runs only, and has no spec.
- **Why the presentation consumers are not reached:** the queue-append `0x14197C900` runs in 36 runs and the type-table listener `0x1419B5570` in all 39. The channel routers (`0x14197C8A0/C0/E0`) and the queue drain `0x14197C2A0` run in **0** runs. So the consumers behind the queues are not reached from a type-2 record through the hub. They are also in neither spec set nor in universe.json.
- Stops: 36 runs complete. 2 stop on `UC_ERR_INSN_INVALID`; 1 on the unmodelled import `InitializeCriticalSectionAndSpinCount` (`snap1-142012` slot 18).

## 3. Replay of the 18 reached cutscene functions (snapdiff, `cut/snap1-213436`, record s7)

Universe tags of the 18: cutscene 11, pre-match 2, gameplay 2, referee 2, in-match menu 1. All 18 are in universe.json.

| VA | universe tag | traced calls | replayed | mismatches | own-block cov | status |
|---|---|---|---|---|---|---|
| 14026A9A0 | pre-match | 2 | 2 | 0 | 0.60 | PROVEN |
| 1402A51C0 | cutscene | 14 | 14 | 0 | 1.00 | PROVEN |
| 1403D27D0 | gameplay | 124 | 124 | 0 | 1.00 | PROVEN |
| 1403DB1F0 | cutscene | 44 | 44 | 0 | 1.00 | PROVEN |
| 1403DB210 | cutscene | 56 | 56 | 0 | 1.00 | PROVEN |
| 1403DB240 | cutscene | 881 | 881 | 0 | 1.00 | PROVEN |
| 1403DB250 | cutscene | 731 | 731 | 0 | 1.00 | PROVEN |
| 1403DB280 | cutscene | 12 | 12 | 0 | 1.00 | PROVEN |
| 1403DB410 | cutscene | 4 | 4 | 0 | 1.00 | PROVEN |
| 1403DB590 | referee | 4 | 4 | 0 | 0.75 | PROVEN |
| 1403FB710 | in-match menu / (TLS accessor) | 7 | 7 | **7** | 1.00 | **MISMATCH** |
| 1403FB730 | (TLS accessor) | 7 | 7 | **7** | 1.00 | **MISMATCH** |
| 140A74000 | cutscene | 25 | 25 | 0 | 1.00 | PROVEN |
| 1415FB180 | gameplay | 4 | 4 | 0 | 1.00 | PROVEN |
| 141794410 | pre-match | 17 | 17 | 0 | 1.00 | PROVEN |
| 140A3CD90 | (none, see note) | 0 | 0 | - | - | no traced call here |
| 141030AB0 | (none, see note) | 0 | 0 | - | - | no traced call here |
| 141B33D60 | (none, see note) | 0 | 0 | - | - | no traced call here |

The universe tag column is from `universe.json`, not from the spec file. For `1403FB710` and `1403FB730` the table prints the tag I checked only for the first; use the universe file for the authoritative value. The three zero-call rows are not excluded; the snapshot has no traced call for them.

- **PROVEN: 13 functions, 1,918 traced calls, 0 mismatches.** Coverage: 12 of 13 at 0.75 or higher.
- **Mismatches (CONFIRMED by disassembly and first differing write):** `1403FB710` (`mov rax,gs:[0x58]; …; mov [r8+rdx], eax`) and `1403FB730` (`mov rax,gs:[0x58]; …; mov r8,[rax+r8*8]`) read the ThreadLocalStoragePointer and index it. The harness writes the slot through the snapshot TEB. The translated side writes nothing in case 0 of `1403FB710` (`translated.w = ""`), and for `1403FB730` writes a different word. So the translated code does not reproduce the same gs-based TLS access. **OPEN:** whether the translated binary models the gs base at all (`gen` does not pass a TEB to the translated binary, as far as the code shows), or the `gs:` lowering in `xlate.py`. The mismatch is reported, not excluded.

## 4. Why 1404F6C30 and 1405CE4F0 had every case excluded

- Both have every case excluded for the same reason: `unmodellable callee 0x140a79640 entered` (`sd-h16.json`: 6 and 3 cases).
- `0x140A79640` is recorded as unmodellable in `tools/dev/translate/stubs.json` with these blockers: indirect calls at `141E535F9` and `141E5362F` inside `0x141E53540`; a computed switch jump at `140A40D52`; an import slot `14252F6E8` with no name in the image (`qword ptr [rip + 0xf8fdd1]`); a global store chain; and call depth > 10 (`140227930 → 1403FB2E0`). Its callers include both excluded functions.
- Unblocking needs either the indirect targets of `0x141E53540` resolved, or a model of `0x140A79640`. Neither was attempted here.

## 5. Changes this round

- `tools/dev/translate/snapdiff.py` (commit `a575f398`): an all-excluded function's note lists the reasons and the blocked callees.
- No change to `frame_trace.py` this round. The dispatch uses the existing `--root entry` with `--entry 0x141B3EDD0`, `--reg rcx=0x12F19C0`, `--reg rdx=<record>`, with the cutscene spec directory for the second pass.

## 6. Verification

- `ring_records.py` (51 snapshots listed, 17 with dumps used; 39 pending records).
- 39 dispatches with `--trace-addrs`, all-spec pass (297 s); 39 dispatches with the cutscene spec pass (1,135 s).
- `reach.py` (union 359 functions; per-run subsystem counts). Totals above come from its output.
- `snapdiff.py` on the cutscene spec set for the 18 reached functions (134 s). Totals (1,918 calls, 13 PROVEN, 14 mismatched calls) are from `sd-cut.json`.
- PII: grep for machine paths and machine paths over `snapdiff.py`: clean.
- Not run: the gate, unit tests, mutations, `check_pii.sh` from the main checkout.

## 7. Open items (numbered)

1. **Route from the pump to the presentation consumers: OPEN.** The channel routers and the queue drain never run from a type-2 record. The queue consumers (`0x1419C4260`, `0x1419C3C40`, `0x141981270`, `0x141985100`, `0x1419879D0`, `0x141986E20`, `0x141973C40`, `0x1419738C0`, `0x1419728F0`, `0x141974320`, `0x1419C1330`, `0x1419C12F0`) are in no spec and no universe row. Next: find the caller that drains the queues, and whether a record's channel selects it.
2. **TLS mismatch (`1403FB710`, `1403FB730`, 14 calls):** the translated side does not reproduce the snapshot's `gs:[0x58]` access. Next: check whether the translated binary has a gs base, then the `gs:` lowering in `xlate.py`.
3. **Section walk** (§1 caveat): the record layout (stride unit, terminator) is not yet confirmed against the record writers.
4. **Universe coverage: 253 of 359 reached functions have no universe row.** Next: universe rows for the dispatch's functions.
5. **Camera, HUD, crowd, audio, FixDemo handler reach: 0.** The one camera-tagged function is in two runs with no spec.
6. **Driver note (`driver.py`, the gate's driver):** the silent "no replayable call" note is unchanged there.
7. **1404F6C30 and 1405CE4F0:** blocked by `0x140A79640` (§4). Not attempted.
8. **Gate:** `snapdiff.py` changed (note text only); Main gates before merge.
