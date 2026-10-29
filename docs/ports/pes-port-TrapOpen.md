# pes-port-TrapOpen: the trap chain's three open inputs decoded; install-built oracle run NOT done (stopped at handover) (29-10-26)

Branch `StoppageFlow`. Head after merging `origin/main` (`9dc9383f`) and `origin/TrapData` (conflict only in `docs/ports/pes-port-TrapData.md`, kept the path-scrubbed side): pushed, `git ls-remote origin StoppageFlow` is the head. **No source, test or doc change beyond the two merges.** Tags: CONFIRMED = read in a tool run this session (decompile, unicorn replay of a capture, capture bytes); INF = inference; OPEN = not settled.

## 0. State against the task

| Item | State |
|---|---|
| 1. merge `origin/main`, `origin/TrapData`, push | DONE |
| 2. open inputs: decode writers and derive | DECODED, not coded (sections 1-3) |
| 3. S3: oracle on `PesTrapData` + P from GF state, all kind-0 captures; build the image once per match | NOT DONE |
| 4. mutations, fast-tier line | none run (no code changed). `GATE fast: NOT RUN (no code change; stopped at owner's handover)` |

Build state: `build/` in `<workdrive>/gf-StoppageFlow` links `pes_trap_oracle_test` (built this session, 42 steps; `pes_trap_data_test` link was the last step and was not confirmed). A run of `pes_trap_oracle_test` on the six old oracle dirs was started in the background at handover; its result is not in this report.

## 1. New capture oracle dirs (prepared, not yet used by the test)

Six new kind-0 captures in `live2/pid2621613/`: `seq068, 097, 127, 281, 310, 360` (all `trap_kind0_79A470`; keep them, never slim). Pipeline per capture, scripts in `<workdrive>/gf-TrapOpen-scratch/` (Main removes it; recreate from `gf-TrapP-scratch/tr6.sh, sd6.sh, or6.sh`):
- `tr.sh` (frame_trace recorded): **6/6 rc=0** into `tr/pid2621613_seqNNN`.
- `sd.sh` (snapdiff, `--only 0x14079A470`, out `sd_trap`, a copy of TrapP's gen dir): started in the background, result not read.
- `trap_oracle.py` to `oracle/<pid>_<seq>` was not run for them (needs `sd.sh` to finish; usage is `or6.sh` with the six new seqs).
The test's oracle dir (`GF_PES_TRAP_ORACLE`) must hold all twelve dirs.

## 2. Decodes of the three open inputs

### 2.1 G (`P+0xA08` chain): CONFIRMED formula, derivable from GF state
- Chain read by the oracle: `*(P+0xA08)+0xF0` (8 B) then f32 at `+0xC0` (`FUN_141AF3180`, `0x141AF3187`; unicorn read attribution on seq4469). Both objects are plain heap objects; the chain never reads their vtables.
- Writer `FUN_141EB5810(P, block)` (decompile this session): `block` = the 0x44-byte physique object (`FUN_1408BFAA0` copies it to `PM+0x784`; `FUN_1406EB210` passes it on and also stores `P+0x3B18 = block[0]/180`). For the root bone (`FUN_141EBC930(0x13)` = 0) it computes `absS = FUN_14195CA70(block) * FUN_14195C710(block, hash("root_main"))[0]`, stores `P+0xA00 = clamp(absS, [0x1425DDF78, 0x1425DDF80])`, pushes it with `FUN_141AF4EB0` into `*(P+0xA08+0xF0)+0xC0`.
- `"root_main"` is in neither scale table (`pes-re-physique.md` section 2, `pes-re-physique-helpers.md` section 4.2), so the A factor is 1 and **G = clamp((1 + kL*L) * (1 + kh*g(h)), lo, hi)** with h = `block[0]` (height cm), L = `block[0x30]` (the leg-length slider, -1..1), `g` = `FUN_14195C4B0` (piecewise 132/180/220, uses `0x142A83540`, `0x142598580`, `0x142588214`, `0x1425CA310`, `0x14259BE2C`), `kh` = `0x142A83538` / `0x142A8353C` (g >= 0 / < 0), `kL` = `0x142A83530` / `0x142A83534` (L >= 0 / < 0), `1.0` = `0x14256AE1C` (`FUN_14195CA70`, translated at `src/pes/translated/cutscene/pes_fn_14195CA70.cpp` shows the instruction order). Prior live check in `pes-re-physique.md` section 0.3 (24 characters, 1e-7). The captured G values (175 cm 0.971146, 185 cm 1.027875) agree.
- **Next step (S1c):** port `FUN_14195CA70`/`C4B0` order-exact into `pestrapactor.cpp` with image reads behind opcode checks (as `LoadActorConstants`); facts `heightCm` (already there) and a new `legLengthSlider` (appearance nibble `0x11:0`, decoded `2n/14 - 1`, 15 -> 0, `FUN_141AD98B0`; GF has no slider source: ponytail stand-in 0 = neutral, naming `FUN_141AD98B0`/the EDIT record). Prove field-equal on every `match::Player` (vtable `0x1426329A8`) of all twelve captures: `P+0xA00`, `*(*(P+0xA08)+0xF0)+0xC0` against the port from `PM+0x784` (height) and `PM+0x7B4` (slider). Then allocate two small objects in the window (plug-in at some VA, `+0xF0` -> second object with G at `+0xC0`), point `P+0xA08` at it, and **replace `PesMovestepHost::PhysiqueG()` (`pesmovestep.cpp:261`, returns 1.0, used by ten movestep sites) with it**; that stand-in carries a `ponytail:` naming `FUN_141AF3180`.

### 2.2 `P+0x3AC0`: CONFIRMED structure, derivable in parts
- `P+0x3AC0` = `PM+0x7E0` = An, a `match::registry::AnalyzeInfoRef`. Readers (unicorn read hook on seq4469, attributed to functions): `0x1406EAAF0` (P vtable slot 6, returns An), then `An+0x18` read by `FUN_140798A70`, `FUN_14079FBC0`, `FUN_140A4EDF0`, `FUN_140A627E0`; `An+0xE0` read by `FUN_140854D40`, `FUN_140857010`.
- `X = *(An+0x18)` is **MA** (the match analyser record that `pes-re-first-touch.md`/`pes-re-press-coord.md` already call MA). Bytes of MA the chain reads (CONFIRMED, three captures classified): `MA+0x2601C` (holder slot, `0xFF` none; reader `FUN_140A627E0` param 0), and cells of the S1 NearestList `MA+0x22E4C + 200*slot + 8*(side*11 + rank)` (`FUN_140A4EDF0`: `u32` at `[(side*0xB + rank)*2]` = the listed player id). Examples: seq4469 slot 17 side 0 rank 0 -> 2; seq23906 slot 18 side 0 rank 0 -> 1, side 1 rank 0 -> 0x15. GF already computes both: `GetPesHolder` (`match.hpp:230`), `pes::BuildScoreRows` -> `ScoreRows::nearest[slot]` (`pesscorerows.hpp`: `SortedRow::Entry(side, rank).id`). GF writes no MA record into the window.
- `Y = *(An+0xE0)` is the analyser's event ring object (`pes-re-record-layouts.md` section 1: 8 x 800 B ring entries per lane, current record at `Y+0x180698`, `FUN_140A3E190(Y, 0)`). The chain reads the **current record's first 0x14 bytes**: `+0` kind (`FUN_140A6CE00`: kind - 9 < 6), `+4` side, `+8`, `+0xC`, `+0x10` tick (`FUN_140854D40`: `(MI+0x1198 - tick) / 54`). Captured examples: seq4469 `01 00 00 00 | 01 00 00 00 | 14 00 00 00 | 18 00 00 00 | 0x7221`; seq019 `1 | 0 | 0xA`. Meaning (INF): kind 1 = a ball-touch event, `+4` the touching side, `+8` the toucher's slot (20 is on side 1, 10 on side 0), `+0x10` its tick. Ring-1 entries (`Y+0x1908`, `FUN_140A3E7D0`) are read by `FUN_140A627E0` for param != 0 (not in these captures' reads).
- **OPEN (named): the event record has no GF carrier.** `pes-re-record-layouts.md` section 1 already says "NO 1:1 carrier". Writer: `FUN_140A3DC40` (current record written on its `ESI == 7` arm, `0x140A3E062-0A3`). Next step: decode which GF touch event feeds kind 1 (last toucher side/slot/tick) and what its `+8/+0xC` are; until then the oracle supplies these 20 bytes as a named capture fact.
- One more not-in-window record: **side-B record 12 dwords** (`G+0x1978 + 0x28*side`, read by `FUN_140A3B320` via `FUN_140A627E0`, 48 B in seq4531/seq019/seq23906; values e.g. `(0,0,6,11,4,3,2,5,7,8,9,10)` side 0 of seq019, `(1,11,19,1,7,2,15,8,20,10,5,9)` side 1 of seq4531). The window binds table B to the same records as table A (`pestrapworld`/`BindSlot` area), so these bytes are an extra capture today. OPEN: its writer and meaning (a squad-order list); next step: decompile `FUN_140A3B320` and find the table-B record writer.

### 2.3 `P+0x2FC` ring: writer still OPEN; new evidence
- Reader `FUN_141EB50B0` (idx >= 2: `k = min(idx/10, 5)`, 12 B at `P+0x2FC+12k`). The oracle reads only `k = 0`.
- Ruled out this session: P vtable (`0x1425E34D8`) slot 2 `0x1406F82F0` and slot 3 `0x1406EEDF0` run from a capture (unicorn, rcx = P) write nothing in `P+0x2FC..0x344` (seq23879); `FUN_141EB4650` and `FUN_141EB5E70` hit an unmodelled `VCRUNTIME140!memset` import in the runner (need `rn` import model), so they are untested. P vtable slot 4 (`+0x20`) is `0x141794410` = `ret`.
- **New data fact (CONFIRMED on seq068):** the ring is `D/(k+1)` there (`-0.00118, -0.00059, -0.00039, -0.00029, -0.00024, -0.00020` in x; z `0.00187, 0.00094, 0.00062...`), i.e. a prediction averaged over `10*(k+1)` ticks with all displacement in the first segment. In seq23879 / seq23906 it grows smoothly (cumulative displacement over 10..60 ticks about 0.18..3.4 m in seq23879). It is neither the man velocity ring (`P+0x2C`) nor `pos - prev`. INF: ring[k] = mean per-tick root displacement over the next `10*(k+1)` ticks of the player's own movement prediction (the 60-tick forward loop). `pes-re-ball-track.md` section 3 (round 13) saw a "static" ring on one player and a copy chain `0x32C20F44 -> p+0x9B8 -> p+0x2FC` through `0x140432AAD`; the disassembly of that function (`0x140432A30`, this session) is a 0x73C-byte struct copy from `*(*(RCX+0x10)+0x10)+0x6C0` to `+4`, not a per-player ring write, so that chain is questionable.
- **Next step:** a second SIGSTOP snapshot 1-2 ticks after a kind-0 capture of the same player, diff `P+0x2FC..0x344` against `FUN_1406E9800`'s predictor family (`pes-re-gaps-anime.md` round 3 section 1) and `P+0x9B8`; or run `FUN_141EB4650`/`FUN_141EB5E70` from the capture with the memset model added (`cand.py` in scratch is the runner: frame_trace `run_recorded` with a different entry and `rcx = P`).

## 3. What the S3 oracle will still capture (the named residual list)

After the work above, per-call captured inputs would be: the per-call world records (man/ball/MI, S1 `T` lines: allowed), plus **named**: (a) ring 12 B (2.3), (b) event record 20 B (2.2), (c) side-B 12 dwords (2.2), (d) the leg-length slider (2.1; GF stand-in neutral), (e) MA holder/NearestList cells (derivable from GF state, still read from the capture in the oracle until `Match` wires them), and the thread/stack-cookie items none (every non-loader `H` in six classified captures was MA, Y, G-chain or the side-B list; the `0x7ff...` ranges are MA/Y/motion-table heap, not TLS). Loader `H`/`I` lines (motion records, Animetable, registry, `trap.o`, file records) are what `PesTrapData::AddTo(ext)` replaces.

## 4. Next concrete actions (in order)

1. `or6.sh` for the six pid2621613 seqs (after `sd.sh` finishes); regenerate the six old ones if `tr/` was removed. Confirm `pes_trap_oracle_test` 12/12 as it stands.
2. In `pes_trap_oracle_test.cpp` `Build()`: skip every `H` line and the loader `I` lines; call `BuildTrapLoaderData(exe, vfs)` once per test (not per call) and `AddTo(ext)`; add the window objects of 2.1 (G chain) and 2.2 (An, MA, Y) from facts; keep only the named residuals. `PesTrapData` builds a 4.9 MB segment: share it (a `PesMem` view of one buffer) instead of copying per `RunTrapKind0`.
3. S1c (G port, 2.1) with its field-equality test on all `match::Player` records; delete the `PhysiqueG() = 1.0` stand-in.
4. Mutations per branch of the new code (G clamp ends, `g` segments, `kL` sign branch, row id cell offset, event-record kind range), each run, rebuilt, reverted.
5. `GATE_TIER=fast tools/dev/gate.sh` in sim slots 2-3.

## 5. Files and scratch
- Repo: only merges. No PII or PES bytes added.
- Scratch `<workdrive>/gf-TrapOpen-scratch/` (Main removes with the worktree): `dec/*.c` (decompiles of `141EB5810, 14195C710/830/CA70/C4B0, 141AF4BE0/4EB0/3180, 141E92E80, 1406EB210, 141EBF1B0, 140A3E190, 140A3E7D0, 140A6CE00, 140798A70, 140A627E0, 14079FBC0, 140A4EDF0, 140854D40, 140857010, 141EB50B0, 141EB5E70, 141EB4650, 1406EEDF0, 1406F82F0`), `watch2.py` (read attribution: which function reads each `H` range), `classify2.py` (H ranges as MA/Y offsets), `cand.py` (run a function from a capture, watch writes), `ring.py` (ring dump), `tr/`, `sd_trap/`.
