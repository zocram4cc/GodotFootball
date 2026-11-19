# Audit: the boot's atexit dispatch (0x14159F6C0 / 0x14159F670 / 0x1415A18F4 / 0x1415A18FA)

Status: classified. Read-only: no code changed, pesboot.cpp not edited. Branch analysed: `OneWorld-series-wip` (pesboot.cpp, pesinitializers.inc, 1,997 selected initializers), image `pes_full.bin`, captures `live2/pid900228/seq001` (animinit at load) and `live2/pid3319119/seq001` (registry_create), live PES pid 3336339 (read-only pread, no writes, no signals).

## Verdict (short)
- **0x14159F6C0 is the CRT `atexit`** (decompile: `return (_onexit() != 0) - 1`). **0x14159F670 is the CRT `_onexit`**, which chooses one of two registration paths by a selector: `ROR(cookie ^ enc, cookie & 63) == -1`. It is not a pure trampoline and not a gate on the initializer's own work: the answer decides only *where the exit-time destructor is recorded*.
- **The selector is true in every capture and in the live process: the live path is 0x1415A18FA**, not 0x1415A18F4. GF takes 0x1415A18F4 because `ZeroInitializedData` zeroes the cell the live CRT sets (see §2). So GF's boot is on a different branch from live PES. Both branches end in an answer of 0 (success) and register nothing in GF.
- **Classification of the live branch (0x1415A18FA):** the image slot holds a pointer into protector-owned RWX memory, not game code, and that memory is not in any capture. It cannot be translated with a proof, so it must not run. The answer GF needs is "registered = 0", which is what the stand-in already gives.
- **Recommendation:** translate 0x14159F6C0 (atexit) as a stand-in `return 0` with a `ponytail:` naming the evidence; stop routing it through 0x14159F670 and the zeroed cell. Nothing game-side reads what it would register (§4). The exit-time destructor list is the one thing GF loses, and it is OPEN (§5).

## 1. What the functions are (CONFIRMED, decompile + bytes)
- `atexit` (0x14159F6C0): `lVar1 = _onexit(); return (lVar1 != 0) - 1;` — 0 on success, -1 on failure.
- `_onexit` (0x14159F670): `bVar2 = cookie & 0x3f; if (ROR(cookie ^ enc, bVar2) == -1) iVar1 = FUN_1415a18fa(func); else iVar1 = FUN_1415a18f4(&DAT_143706720, func);` and returns `func` when `iVar1 == 0`, else 0. Constants: cookie = `DAT_14350b340` (the GS security cookie, `.data`), enc = `_DAT_143706720` (the encoded table head, in the zero-initialised tail of `.data`).
- The writer of the five onexit cells `0x143706720..0x143706748` is `FUN_14159F4B8` (0x14159F4B8, the CRT onexit-table init, called from FUN_14159F068). Its first branch (`!ucrt_in_use || mode != 0`) stores `ROR(-1, cookie&63) ^ cookie`, i.e. the encoded sentinel `-1` into the head, last and end cells. Readers of the cells: `_onexit` only (xrefs: `_onexit` READ/DATA; writer `FUN_14159F4B8`). No game code reads them.
- The two targets are indirect calls through `.rdata` slots: `FUN_1415a18f4` calls `*(0x142530318)` with the table pointer and the function (a legacy table insert, the CRT's `_register_onexit_function` shape); `FUN_1415a18fa` calls `*(0x142530348)` with the function alone (the UCRT `_crt_atexit` shape, reached through a protector import slot). Callers of both: `_onexit` only.

## 2. The selector on every capture (MEASURED)
| source | cookie | enc (cell 0x143706720) | ROR(cookie^enc, cookie&63) | branch |
|---|---|---|---|---|
| image dump `pes_full.bin` | 0x0000091049DB94F0 | 0xFFFFF6EFB6246B0F | -1 | 18FA |
| capture pid900228/seq001 (animinit at load) | 0x000023E4700039F2 | 0xFFFFDC1B8FFFC60D | -1 | 18FA |
| capture pid3319119/seq001 (registry_create) | 0x00003115889F5F23 | 0xFFFFCEEA7760A0DC | -1 | 18FA |
| live pid 3336339 (read-only pread, now) | 0x000062389AC07379 | 0xFFFF9DC7653F8C86 | -1 | 18FA |

In each row `enc == ~cookie`, which is exactly the sentinel `FUN_14159F4B8` writes. So PES's live CRT state takes 18FA on every atexit call.

GF: `ZeroInitializedData` (pesboot.cpp) zeroes `0x1436CF800..0x143AC5000`, which contains the cell `0x143706720`. The CRT init that would store the sentinel (`FUN_14159F4B8`) is never run. So in GF `enc = 0`, `ROR(cookie, cookie&63) != -1`, and the translated dispatch picks 18F4 (the `return 0` stand-in). The answer matches (0 = success), but the path does not.

## 3. The two targets, by what they do
- **0x1415A18FA (live branch):** the slot `0x142530348` holds a runtime pointer (live value `0x6FB296D66830`, inside an anonymous `r-xp` region `6fb296d51000-6fb296dfc000`). The image dump holds a stale copy (`0x6FFFFEC76830`), which is unmapped in the captures and in the live process. Read-only bytes (first 0x60 bytes): `mov rdx,rcx; lea rcx,[ctx]; jmp <stub>` followed by a wrapper that makes a direct call and an indirect call through an import slot. I did not decode the body further. Classification: CRT exit-table registration reached through protector-owned relocated code. It is not game logic and its code is not in the image or the captures, so it cannot be translated with a proof. **Do not run. Answer by meaning (registered = 0).**
- **0x1415A18F4 (not the live branch):** slot `0x142530318` → `0x6FB296D5A260` in the same region. Its first bytes set `errno = EINVAL` and run a case-insensitive `A..Z` compare loop. That looks like an argument or environment check (INF, not verified). It is not on the live path, and it must not run either. GF's current `return 0` stand-in for it is the right answer for GF only by accident of the zeroed cell.

## 4. Which initializers route through atexit (STATIC, with the gap stated)
Method: scanned `.trace` for every direct `E8` call to 0x14159F6C0 (`scan_atexit.py`, `reach.py`, `stubs.py`); mapped each to its containing function via `.pdata`; BFS of direct calls from the 1,997 selected initializers.
- Direct atexit call sites in the image: **790**, all inside `.pdata` functions (761 distinct functions); 766 of them carry a `lea rcx,[dtor]` immediately before the call (the dtor argument); 726 distinct dtor targets.
- Table initializers (0x142530868 table, 6,849 entries) that call atexit directly: **21 sites in 21 functions**, all of them among the 1,997 selected initializers.
- Static reach from the selected set through direct calls: **29 atexit sites** (21 direct + 8 via callees; 2,213 functions reached, depth ≤ 8).
- **GAP (OPEN):** Main's dynamic figure is 1,678 calls at boot. The static direct-call reach is 29. The difference has to come through indirect calls (function-pointer and vtable dispatch, `call reg`), which the static scan does not follow. I did not reproduce the 1,678 count. Method to close it: count `pes_fn_14159F6C0` entries during a GF boot (one counter in the closure stub), then attribute each call to its caller via the GF dispatch table.

Grouping of the 790 static sites by the containing function's 64 KiB region (site count): 1402E0000 54, 1402A0000 39, 141900000 32, 141650000 26, 1402B0000 25, 1416A0000 25, 140300000 23, 141660000 23, 141820000 23, 141830000 23, 141860000 23, 141800000 21, 1403F0000 20, 1416E0000 19 (116 buckets in all). These are the groups by target region; their content is not yet named (see §5).

## 5. What the registered destructors do, and what GF loses
- **Sampled destructors (5 of 726, Ghidra):** `FUN_142518720` calls `FUN_141794410`, `FUN_140C55710`, `FUN_140C55180`; `FUN_14250FD00` calls `FUN_1403B8450`, `FUN_1403B82A0`. These are engine cleanup calls. None of the five sampled is a file write, a save, or a log.
- **Not verified:** the other 721 dtor targets. Per-dtor review is OPEN. Method: for each dtor, list its direct and indirect callees, and flag any that reach file APIs, `ExitProcess`, or the save path.
- **What GF loses by not running them:** the process-exit destructor pass. Nothing in play reads what they would release at exit (the onexit cells have no game readers, §1).
- **Globals left unset in GF:** the onexit head, last and end cells `0x143706720..0x143706748` (zero in GF, sentinel in PES) and the UCRT exit table behind 18FA. Readers: `_onexit` and the CRT exit pass only. No game reader found (xrefs on all five cells).

## 6. Recommendation (pesboot.cpp; not edited)
1. Classify the dispatch as **CRT bookkeeping with a protector-owned live branch**: not translatable with a proof, not to be run or modelled. Answer by meaning.
2. Make `0x14159F6C0` (atexit) a translated stand-in that returns 0 and records nothing, with `// ponytail:` naming (a) the live selector (`cookie`, `enc == ~cookie`, branch 18FA, §2), (b) the protector slot `0x142530348`, and (c) the GF loss (exit-time destructors, §5). Keep the translation of 0x14159F670 out of the boot path. Once no caller remains, delete the 0x14159F670 / 0x1415A18F4 / 0x1415A18FA translations (clean cutover, per the brief). Check first: `FUN_14159F670` and the two thunks have no callers outside `_onexit` / `atexit` (xrefs above).
3. `ZeroInitializedData` can keep zeroing the cell once atexit is a stand-in: no reader depends on it.
4. Do not run `FUN_14159F4B8` (it writes the sentinel and reads the security cookie: the environment check's state). Do not run 18FA or 18F4.
5. Exit-time destructors: keep unrun for now; if the destructor review (§5, OPEN) finds any that write a file, add a GF shutdown list as the stand-in for them.

## 7. Open items (each with the method that settles it)
- Dynamic count of atexit calls at GF boot (1,678 vs 29 static reach): instrument the closure stub, attribute by caller (§4).
- The 721 unreviewed destructors: callee review for file, save and exit APIs (§5).
- The 18F4 body beyond its first bytes (environment check or table insert): not needed for the recommendation, since neither branch runs in GF; left as INF.
- Protector slot values in a fresh process: the anonymous-region addresses differ per launch; the captures do not contain them. Not needed for the recommendation.

## Method notes and scratch
- Scripts in `<workdrive>/gf-BootDispatch-scratch/`: `scan_atexit.py`, `reach.py`, `stubs.py`, `slots.py` (indirect-slot scan: zero hits), `peek.py` (read-only live pread), `atexit_all_sites.json`, `boot_atexit_reach.json`, decompiles `14159F6C0.c`, `14159F670.c`, `1415A18F4.c`, `1415A18FA.c`, `14159F4B8.c`, `dtor_*.c`.
- Reads: image dump; captures pid900228/seq001 and pid3319119/seq001 (streamed from `mem.bin.zst`, nothing unpacked, nothing written); live pid 3336339 `/proc/<pid>/mem` pread (read-only, no signals). No protector code was run, emulated or traced; the anonymous-region bytes were read to classify and not decoded further.
