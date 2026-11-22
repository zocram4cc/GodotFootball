# CutsceneAnchor13 (CutWire): the group-list builder is decoded at its real entry 0x141E5C1E0, the boot chain and the holder/loader objects are mapped, and the first translation step is blocked by an undecoded harness import (CreateSemaphoreW). Nothing landed; the wide test is still RED.

Branch `CutsceneAnchor`, head `aef12c1fab081e2002fcc6cba22fab5c6aad841d` (unchanged; no commit this round). Worktree `<workdrive>/gf-CutsceneAnchor` is clean. Labels: CONFIRMED = read, decompiled or run this round; INF = inference; OPEN = named blocker. PES was not live-read this round: all decodes are static on `pes_full.bin` (flat, VA - 0x140000000) plus the scratch live node dumps from CutsceneAnchor11.

## 0. Outcome

- Assignment item 1 (translate 0x141E5C274, run at world build, entry 0x71 test): NOT landed. The function is not 0x141E5C274; its entry is 0x141E5C1E0 (CONFIRMED, `.pdata` 0x141E5C1E0..0x141E5C332; 0x141E5C274 is the loop label inside it). A bounded proof of 0x141E5C1E0 needs the import model in section 4.
- Assignment item 2 (stadium key): NOT resolved. Stadium id writer still unknown (section 5). Key-to-stadium mapping re-confirmed.
- Assignment item 3 (wide test, neighbours, gen/ledger, push): not run; nothing changed, so no gate.
- Correction to the brief: the live node 0x71 holds `-80.0` at `+8` and `16.0` at `+0xC` (CONFIRMED from `cutlib/live_node_71.bin`, a dump of PES's live node). The brief's "16.0 at +8, -80.0 at +0xC" is reversed. Node 0x9E: `+8 = 74.094498`, `+0xC = 6.233300`.

## 1. The group list builder (CONFIRMED decode, Ghidra headless, `dec.sh`)

- `0x141E5C1E0(G)`, one argument in rcx, G = `[0x1437F4800]` (the group table; `0x141E5BAF0` creates it, 0x40 bytes, a vector at `+0`/`+8`/`+0x10`, a lock at `+0x18`). For each slot i: if the element e is non-null, `0x141E5DD60(e)` frees its list (`+8`). Then:
  - mode `e+0x90 == 0`: holder = `0x141E5D700(e+0x10, i)` (kind = slot index i);
  - mode `e+0x90 == 1`: holder = `0x141E5D7F0(e+0x10)` (this loads the file: `0x141E5D980`);
  - any other mode, or a failed holder: return 0.
  - On a holder: `addref`, `while (--e+0x94 > 1) addref`, `G.vec[i] = holder`, release e (`0x141E5D4E0`, free 0x98). Return 1.
- Group element (0x98 bytes): `+0` loader pointer (CONFIRMED from `0x141E5D8C0`), `+8` list (0x204 block: count u32 at `+0x200`, up to 0x80 u32 cut ids), `+0x10` name (0x80 bytes), `+0x90` mode (0 = name only, 1 = loaded), `+0x94` counter.
- The names come from the boot step `0x140419610`: loop over 9 paths at `0x143445E70` (CONFIRMED: `cpk_dat/common/match/constant/constant_{match,player,stadium,team,tutorial,tutorialConsole,positionCK,positionPK,shootAging}.bin`), each `0x141E5BC40(G, path)`. `0x141E5BC40` either calls `0x141E5BEE0` (named holder, `0x141E5D700`) or `0x141E5C060` (`0x141E5D7F0`), chosen by the flag at `0x141B34850(&UNK_142B48368,0)` and a name test.
- Callers (CONFIRMED): `0x141E5C1E0` has no direct E8 caller in the image except `0x14041BD32` (in `0x14041BD00`) and `0x1420B89E3` (in `0x1420B83B0`). `0x14041BD00` runs `0x141E5D180` (clears the cut library, CONFIRMED: frees each 0x18 entry through `0x141E5DE90`) when `[0x1437F4808]` is set, then `0x141E5C1E0(G)` when `[0x1437F4800]` is set. `0x1420B83B0` (the world state machine, case 6 and 5) calls `0x141E5C1E0(G)` too.
- `0x141E5BA70(G, kind, list)` (the notify) is called only from the fill `0x141E5D560` (CONFIRMED: `0x141E5D6C9`).
- `0x141E5D4C0(key)` (the else arm's lookup, CONFIRMED): `[0x1437F4808]` header, slot `key`, returns entry `+8` (the node). `0x141E5DDF0(list)` clears the cut-library slots named by a group's id list (CONFIRMED).

## 2. The holder and loader (CONFIRMED decode)

- `0x141E5D700(name, kind)`: `0x1403FB410(0x98)`, zeroes `+0x10..`, `0x141E5D8C0(holder, name, kind)`, then `+0x90 = 0`.
- `0x141E5D8C0`: `0x1403FB410(0xA0)` (loader object), `0x1414A8790(loader, name, ...)` (a file-stream constructor: sets vtable `0x14296D150`, then the caller overwrites it with `0x142B48C80`), `*(loader+0x98) = kind`, copies the name to `+0x10` through `[0x142530660]`, then `0x1414A9570(loader)`.
- The fill `0x141E5D560` is the loader's vtable slot `+8` (CONFIRMED, see the 2025-11-22 CutsceneAnchor12 record). Its list argument is the `(count, recordsOffset, (argOff, flag, nameOff))` blob.

## 3. What the fill's list is (INF, not decoded)

- `0x141E5D980` (the element loader) reads the file through the file-stream object, walks the WESYS object table with `0x140387C60`/`0x140385960`/`0x140383FE0`, resolves each object's basename through `0x141E5CFB0` (cut id find-or-create), and writes the 0x204 id list. That is not the fill's list (the fill wants 12-byte `(argOff, flag, nameOff)` records).
- The fill's list is therefore built on the loader's file completion path, which the loader calls through its vtable, not decoded (OPEN). The WESYS table `(count, +4 table, (dataOff, size, nameOff))` has the same record size, so the blob may be the unwrapped table with `flag = size`; the meaning of `flag` in the name match is not decoded (INF).

## 4. The blocker for the first proof (CONFIRMED)

- A bounded proof of `0x141E5C1E0` (synthetic states over the `fstates/all_exist` mem, written with `tools/dev/translate/synth.py`, spec `scratch/cutlist/grp_spec.json`: one group, a null slot then one group, and the nine stock paths) runs `0x1414A8790` → `0x141B38980` → `KERNEL32!CreateSemaphoreW` (`CreateSemaphoreW(0, initial, max, 0)` through IAT slot `0x14252F1B8`, CONFIRMED). The harness refuses: `frame_trace.py recorded`: "unmodelled OS export KERNEL32.dll!CreateSemaphoreW ... needs an import model". The trace file and record exist; the run dies inside the callee, not after the root (rax = 1 is the root's own return value).
- The cut-name and cut-fill proofs never reached this constructor (they do not create holders), so the harness has no model for it.
- Options (for Main): (a) add a code_hex-checked import model for `CreateSemaphoreW` (returns a fixed non-zero handle on both the PES side and the translated side; the file object's semaphore is only used by PES's own file loading, which GF replaces with PesVfs), labelled as a stand-in with a `ponytail:` note; (b) stop the holder path and decode the loader's file completion first. Option (a) is a harness change outside this assignment, so it is not made here.

## 5. The stadium key (OPEN)

- Key-to-stadium mapping (CONFIRMED, `cutlib/names.txt` from `DevelopData/common/match/constant/`): index 112 = `demoarea_st000`, 113 = `demoarea_st001` (key 0x71 at seq204), 158 = `demoarea_st046` (key 0x9E this launch of seq321). So `key = 112 + stadium id` holds for the names; that the key is that expression for the match stadium is INF.
- GF already resolves a team's home stadium: `pes::TeamStadiumId(vfs, teamId)` through `ReadTeamList` and `ResolveStadiumId` (`src/pes/stadium.hpp`). A GF key writer could use `112 + TeamStadiumId(home)` once the home team is known to the selector; the selector has no team input yet (INF that this is PES's source).
- The writer of `T+0x1C` (and `+0x20`) is still not found (CutsceneAnchor11 section 6). The wide test may load the capture's key 0x71 as pre-state (boundary policy 6) if GF's own key is not wired; that choice is for Main.

## 6. Checks

- No code changed; nothing committed or pushed. `git status` on the worktree is clean (CONFIRMED).
- Not run: `gen_translated.py --check`, `ledger.py --check`, the wide test, neighbours, the gate (no change to check). The last wide-test result is CutsceneAnchor10's: RED, 9 EXPECT failures (`T[0]` -41.5502167 vs -41.6211472, `pos[0]` 0 vs -41.5502167, `pos[2]` 0 vs 34).
- Scratch kept (small): `<workdrive>/gf-CutsceneAnchor-scratch/cutlist/` (`dec_*.txt` decompiles, `xref.py`, `grp_spec.json`, the state specs). The 3 synthetic states' `mem.bin` copies were deleted after the trace failed (each was 3.8 GB).
- Tool notes for the next agent: `closure.py --members-file` needs a JSON list and the root as a positional argument; a static synth state inherits `stop`/`entry` from its source state (write from the real capture, not from a synthetic state).

## 7. Open items (numbered)

1. Decide the `CreateSemaphoreW` import model (section 4, option a) or decode the loader's file completion first (option b).
2. Decode the fill's list builder and dispatcher: the loader's file completion, and the `flag` meaning in `0x141E5CB40`.
3. Translate `0x141E5C1E0`, `0x141E5D700`, `0x141E5D8C0`, `0x1414A8790`, `0x141B38980` (after the model), then the bounded proof with the synthetic states of section 4.
4. Translate the element path for the world build: `0x141E5BAF0`, `0x141E5BC40`, `0x141E5BEE0`, `0x141E5C060`, and the flag `0x141B34850`.
5. Decide the GF key source (`112 + TeamStadiumId(home)` through the selector) and the writer of `T+0x1C` (a goal sampled in a live match, then a hook).
6. Re-run the wide test after items 1-4 (and 5 or the pre-state choice).
