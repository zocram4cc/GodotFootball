# PES port CardLevel3: the judge closure now reaches its thresholds, CFG thunk and copy-ctor; judge proofs 33/33 bit-exact (still no card capture); the GF cutover runs a foul to completion with no abort or hang, 3 of 5 red-first cases green, 2 red; not smoked (29-10-26 / 30-10-26)

Branches (confirmed by `git ls-remote`): `FoulSanction` `1a7c7adf` (tooling, layer, proofs; GF source = hand port, safe to merge) and `CardLevel2Cutover` `d4784b07` (= FoulSanction's tooling + the GF cutover; NOT safe to merge: 2 red tests, no smoke). Worktree `<workdrive>/gf-FoulSanction`, scratch `<workdrive>/gf-FoulSanction-scratch`. CONFIRMED = tool output this session; INF = inference; OPEN = blocker named.

## 0. Status against the ticket

| item | status |
|---|---|
| 1. merge `origin/main`, then `origin/ImageGap`, push | DONE (no conflicts; `7cb3aec8`) |
| 2a. thresholds `0x1405B82E0 -> 0x143FA8760` in the closure | DONE: `closure.py` now re-decodes the unresolved protected rows (`0x143F546F0`, `0x143F71690`, `0x143F738A0`, `0x143FA8420`: they were bridge rows with no callees) and takes a clean no-`.pdata` leaf at any code VA (Main's ruling). `0x1405B82E0` is a 1-insn `jmp` thunk below `.impdata`. Closure 84 -> 93 members |
| 2b. `0x1415A01DC` CFG thunk | DONE: `stubs.json` `engine-noop` `__guard_check_icall`. Evidence: it is `jmp [0x142530868]`; that slot holds `0x1415A02B0` in the image and `0x1415A02B0` is `ret`; `docs/re/pes-re-dead-code.md:245` says GuardCFFunctionTable = 0. Both `__ehvec` callers now emit the stub line |
| 2c. `0x143E1C3A1` out-of-range jump | NAMED, not a gap: it is in the bytes after the commit's `.pdata` end (`0x143F548B4` is the function's last `int3`); the capstone fallback decode in `xlate.py` runs past the `int3` because its pending-branch set is never pruned, so it emitted the next blob's `jmp 0x143E1C3A1`. No label of the commit targets anything after `0x143F548B4`. The path is unreachable (`__builtin_trap()` precedes it). I tried stopping the decode at `int3` with no forward branch pending: it is correct for the commit but changes 124 layer files (44,413 lines of dead tails removed, 30 closure files dropped); reverted, not committed (OPEN item 3) |
| 2d. regenerate, `gen_translated.py --check`, re-prove | DONE: layer 3,992 functions (3,685 recorded, 307 closure), `--check` byte-identical, rc 0 |
| 3. merge into cutover, rebuild, red-first, mutations, new captures | merged (`a977cb11`) and built (full layer, `-O0 -DNDEBUG`); red-first run below; **mutations NOT run** (2 cases red, budget); new captures replayed (section 2), none with a card |
| 4. smoke both paths | **NOT run**: the cutover has 2 red cases and was never exercised past `DecideContact`. Fast-tier interim line in section 5 |

## 1. What the cutover needed beyond the ticket (found by running it; both fixes are in the tools, regenerated)

1. **The builder aborted on `0x140A81910`** (`PES_DISPATCH_UNTRANSLATED`): `0x1405B8000 -> 0x140A81AC0 -> 0x140A81860` calls `__ehvec_ctor` `0x14159EFF8` with the element constructor `0x140A81910` in r9 (22 elements of 0x2C bytes). It is an indirect target no direct CALL names. Fix: `closure_file` on the `0x1405B7470` record (`closure_judge.json`, member `0x140A81910`), with `gen_translated.closure_members` **copied verbatim from `origin/StoppageFlow`** (`gen_translated.py` hunks identical, so that branch merges clean; Main merges StoppageFlow, so this is the same mechanism, not a second one).
2. **The builder hung** (10 minutes at 100% CPU, stack `pes_fn_144293D10 <- 140A81940 <- 140A81AC0 <- 1405B8000`): `0x144293D10` is the judge-state struct copy; its loop count is `[0x153F740B0] ^ 0xE9111263`. The generator emitted `s.rd(0x153F740B0, 4)` (a live read, `.impdata` is rwx), the GF window returns 0, so the count was 0xE9111263 instead of 11 (image word `0xE9111268`). The same pattern feeds the commit (`0x1482B3D70/74/78`), the thresholds (`0x1482B3DC8`) and the scorers (`0x1482B3DC4`). Fix in `xlate.py` `Image.writable`: a `.impdata` word outside the 31 measured `IMPDATA_VOLATILE_RVA` pages bakes as a constant (the same rule ImageGap serves them under). The volatile reads in the closure (`0x15B8C6059`, `0x14494F328`, `0x15B8FE28E`, `0x15B42757C`, `0x15AE75E24`, in `xchg`/`sub` junk pairs) stay live reads and read 0 in GF: INF they cancel (PES differs per launch there and is deterministic). Drift: 7 files (143FA8420, 143FA8760, 144293D10, 143F546F0, 1403D2E60, 141FF52B0 and the new 140A81910 file). `const_decls` also learned non-finite float constants (`__builtin_bit_cast`), `nanf` was not valid C++. **Main: 1403D2E60 (cutscene) and 141FF52B0 (gameplay) changed with it; their diff tests were not re-run, run `diff_gen_*` in the gate.**

## 2. Proof (CONFIRMED: `snapdiff.py`, harness runs PES's bytes, the translated closure the same snapshot; rax, xmm0, every written byte; unique link names per capture)

| function | calls | mismatches | own-block coverage |
|---|---|---|---|
| `0x1405B7470` decide | 4 of 4 (pid2670420 seq7473, pid91171 seq316, seq3147, **pid132854 seq5123**) | 0 | 0.43 (86/200; was 0.195) |
| `0x143F546F0` commit | 1 of 1 (pid2670420 seq7443) | 0 | 0.7037 |
| `0x14050E770` whistle | 28 of 28 (the 15 before, 3 new on pid132854: seq3142, seq3406, seq3428, and 10 more full captures on pid91171 that the earlier run had not replayed: seq875, 1092, 1549, 1590, 1880, 2116, 2784, 2911, 3151, 3433) | 0 | 0.5072 |

`results_judge.json` and the three records in `results_core.json` updated. **pid132854 seq5123 is the first capture past the early exit**: PES's own trace shows it calls the scorers `0x143F738A0`, `0x143FA8420`, the thresholds `0x143F71690 -> 0x1405B82E0 -> 0x143FA8760` and returns rax 0 (no foul). So the thresholds and scoring path are now proven on a real call (a below-threshold one). **Still no card:** no `judge_commit` capture with level >= 2 and no decide that reaches the pending-foul builder exists on any launch (live2: commit only pid2670420 seq7443; decide seq5123 is the lone `--when` hit; PES pid 132854 was still up at 01:29). The builder `0x1405B8000` and the level 2/3/4 commit paths remain unproven on real calls (OPEN item 1).

## 3. Cutover test run (CONFIRMED, `pes_judge_decide_test`, `-O0 -DNDEBUG`, final build)

| case | result |
|---|---|
| ATackleThatNeverPlayedTheBallAndIsNotAimedAtTheManIsNoFoul | pass |
| TheFaceFlagOutsideTheBoxIsAYellow | pass (runs the builder, the ctor, the struct copy: first run to complete) |
| AProfessionalFoulIsAtLeastAYellow | **FAIL**: `pending` false, expected a pending foul |
| TheSameTackleWithoutIntentIsBelowAYellow | **FAIL**: `pending` false |
| TheStagedActionBytesAreRestored | pass |

Reading (INF, not yet traced): the two red cases stage a tackle on a fallen man with the ball 36 m away and expect a pending foul at default facts; the translated judge says no foul, so either a staged fact is missing or wrong (the test was written from the decompile, never run before) or the expectation is. Not changed in this session: the fix must be traced against `0x1405B7470` with the staged bytes (use the red cases' memory as a synthetic snapshot through `frame_trace.py`), not by editing expectations. `pes_judge_test` 12/12 passed on the cutover's first build; not re-run on the final build. Mutations: none run (planned list in `pes-port-CardLevel2.md` section 4).

## 4. Open items (numbered)

1. Card proof: wait for a `judge_commit_43F546F0` capture with level >= 2 or a decide that reaches `0x1405B8000` (ask Main to check the `--when` predicates still fire; only seq5123 has so far). Replay with `tr.sh` + `sd.sh` in the scratch dir and append to `results_judge.json`.
2. The two red cases (section 3), then the mutations (victim fall, intent, played-ball flag, ctx cell unbound, fouler register, action-byte restore), the 2-minute smoke on both paths (two runs each, same hash), then merge `CardLevel2Cutover` and delete nothing more (the GF hand port is already deleted there).
3. `xlate.capstone_decode` does not stop at `int3` padding: 124 layer files carry dead tails after their last `ret` (44k lines). A guard `int3 and no forward branch pending -> break` fixed the commit and dropped them; I did not take it because it rewrites recorded translations en masse. A separate, reviewed change.
4. A cheaper regen loop: every new function in `translated.hpp` recompiles all 4,000 objects (28 to 50 minutes under load). Forward-declaring per function instead of one header would cut it.
5. Whistle/decide captures: `live2/pid91171` holds full captures I replayed late; none deleted or slimmed.

## 5. Checks run

- `test_closure_region.py` 11/11 (ImageGap's 8 kept, plus the widened leaf below `.impdata` and the interior-of-`.pdata` refusal; red before the change, mutation not run). `gen_translated.py --check`: 3,992 functions regenerate byte-identical, rc 0 (final tree).
- Fast-tier interim line (`GATE_TIER=fast` not run: the branches do not contain batch AO): **fast-tier interim: `pes_judge_decide_test` 3/5 (2 red, cutover only), `pes_judge_test` 12/12 (earlier build, cutover), `gen_translated.py --check` green; `FoulSanction` `1a7c7adf` builds the same sources as before with GF unchanged (not rebuilt after the revert commit).**
- Not run: full gate, smoke, mutations, the other `pes_*` binaries.
