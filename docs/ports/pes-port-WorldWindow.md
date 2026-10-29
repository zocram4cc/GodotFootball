# PES Clear chain on GF state: world window and oracle (WorldWindow, 28-10-26)

Branch `EndClear` (worktree `gf-EndClear`). Pushed head **`83022c25`** (`git ls-remote origin EndClear` confirmed): the merge of `origin/main` `15f5aaac` into `c5c7a999`. No new GF code is committed. The Clear chain does **not** run on GF state yet. Steps 3 and 4 are not done; step 5 is only partly done (an oracle is designed and red, not green).

Tags: CONFIRMED = read in tool output or a build log this session; INF = inference; OPEN = not settled.

## 0. Verdict

- Step 1 (merge, push): done, CONFIRMED. `git -c status.submodule=none` merge and push work; the `extern/godot-cpp` symlink error is printed but does not block the commit.
- Step 2 (record identities and bindings): identities listed in §2. No record gained a GF writer. The window does not hold them.
- Step 3 (danger flag): not ported. The decompile is in hand (§3). Blocked on hooks (Main) and on a port of about 30 callees.
- Step 4 (FillUnitTable kind 1, `forcedClear`, stand-in deletion): not done. Blocked on step 3.
- Step 5 (oracle): a test is written and red (CONFIRMED, §4). It is not committed, because a red test breaks the build for every target. The layer that would make it green cannot be generated yet (§5).
- Counts: no `pes_*` test binary was run in this session. The only build was the red target, which fails to compile. Gate not run (Main gates). The EndClear3 counts (34 binaries, 758 passed, 0 failed, 3 skipped) are unchanged, since no source changed.

## 1. Git and the layer attempts

- Merge: `origin/main` 15f5aaac (adds `docs/ports/pes-port-EndClear3.md`) merged clean into EndClear; pushed `c5c7a999..83022c25`.
- Five proven records are `diffed: false` in `tools/dev/translate/results_core.json` ("harness cannot run": the unicorn corpus is empty). Their proof is the snapdiff replay (`results_endclear_angle.json`, `results_endclear_clear.json`): 0x142108630 6/6, 0x14063A290 6/6, 0x14063A490 6/6, 0x14063D2F0 12/12, 0x142109EB0 5/5 (danger-0 arm only). I flipped them to `diffed: true` and ran `gen_translated.py --image ~/ghidra-projects/scratch/Unpacker/pes_full.bin` three times (logs in `gf-EndClear-scratch/logs/gen*.log`, 7.6 to 8.8 min each):
  1. Run 1: `2 conflicts, first: 0x142108630 translates differently under core`. An earlier set (`presentation`, reached through the closure) translates the angle under a universe with different stubs. Both variants are gap-free, so the generator refuses.
  2. Run 2 (own-record rule: a recorded function commits its own translation): `0x14060CFF0 has a gap under its own record's universe`. The rule was too strong: the old "gap-free variant wins" logic is needed there.
  3. Run 3 (own gap-free translation seeded before the closure, others keep the old rule): still `2 conflicts, first: 0x142108630 under core`.
- Root cause (CONFIRMED by a probe, `gf-EndClear-scratch/probe/gap.py`): the angle's own universe (the `core` set) has gaps, so it has no gap-free own translation at all:
  - 0x142108630 calls `0x140477130`, `0x140A6CF30`, `0x140470D00` (out of set).
  - 0x14063A490 calls `0x140A3E940`, `0x140A57650`, `0x140A6CF30`, `0x14063DC10`.
  - 0x142109EB0 and 0x14063A290 and 0x14063D2F0 are gap-free.
  - Specs: `0x140A6CF30`, `0x140470D00`, `0x140A3E940` are in `specs/presentation`; `0x140A57650` is in `specs/impure`; `0x140477130` and `0x14063DC10` have no spec in the repo or in `gf-EndClear-scratch/specs-gen`.
  - The snapdiff proof built these callees from outside the committed generator path (`gf-EndClear-scratch/sd5/gen/142108630/pes_fn_140477130.cpp` and others). So the proven closure is not reproducible by `gen_translated.py` as committed.
- Decision: the flips and the generator change are not committed. They are saved as `gf-EndClear-scratch/wip/wip.patch` (129 lines). The worktree is restored to the pushed head (`git status` clean).
- What would close it (§6, item 1): specs (via specgen) for `0x140477130` and `0x14063DC10`, and for the presentation and impure members `0x140A6CF30`, `0x140470D00`, `0x140A3E940`, `0x140A57650`, or a documented decision to admit the snapdiff-built closure. Then rerun the generator with `wip.patch` and check `--check` (drift).

## 2. Records the angle and the Clear unit read (identity, GF state, writer)

Sources: `docs/re/pes-re-record-ids.md` (§0 operand VAs, §2 Clear carrier), `docs/re/pes-re-record-layouts-chains.md` (§1 to §4, §7). All offsets are PES's.

| Record (how reached) | Fields the angle reads (range) | Identity | GF has it? | PES writer (VA) |
|---|---|---|---|---|
| ctx = A0 = Clear unit `+8` (`0x142109ED5 MOV RCX,[R9+8]`) | `+0x18` pointer to record A0.18 (fields `+0x2482C`, `+0x24830`, `+0x24834`); `+0xE0` E0 ring (count `+0x1900`, head `+0x1904`) | CONFIRMED layout, INF identity (= sanction A1) | E0 ring exists (`pesevents.hpp:20`, 800-byte slots) but is not this ctx's | OPEN |
| G (image global `0x1436F3DC8`) | `+0x180` table (stride 0x74, count ≥ 24, OPEN) | CONFIRMED | `PesWorld` header (`pesworldrecord.hpp`) | n/a |
| G `+0x40` carrier record | `+0x4` f32, `+0x4C` u8, `+0x80` u8, `+0x2920` u32, f32 `+0x2770..+0x2950` | OPEN (not the Clear's carrier; not an anime/man/slot entry) | none | OPEN |
| G `+0x270` MI | `+0x1168`, `+0x116C` (play state), `+0x1198`, `+0x11B2`, `+0x11C8`, `+0x11D4`, `+0x1554`, `+0x155C`, `+0x1560`, `+0x1564`, `+0x173C` | CONFIRMED offsets; GF names `+0x116C` play state (`pesclock.hpp:96`) and `+0x1198` frame (`pesclock.hpp:159`); MI identity INF | partly (clock) | OPEN |
| G `+0x360` record | `+0x8` | OPEN | none | OPEN |
| G `+0x2AA8` record | `+0x0`, `+0x4`, `+0x8`, `+0xC`, `+0x14`, `+0x18`, `+0x1C` | OPEN | none | OPEN |
| Clear unit (r8 of `0x142109EB0`) | `+0` carrier record pointer (not a G entry, 0/5), `+8` ctx, `+0x28` imageA | CONFIRMED (5/5) | none | OPEN |
| imageA `+0x31C1` (danger byte) | read by the Clear unit, `0x142109EC0` | CONFIRMED (5/5, value 0 in all 5) | `danger` field (`pescarrier.hpp:575`), writer is a stand-in | writer `0x140699960` (builder `0x142119390`), not ported |
| carrier record `cr+0` (u32 id) | id passed as angle `edx` | CONFIRMED values, owner OPEN | none | OPEN |
| lane reader `0x140A714E0` | `[RCX+0x22B9C]`, `[RCX+0x229B4]` | CONFIRMED | none | OPEN |

Anime `+0x1A/+0x1C` conflict (`pes-re-record-layouts-chains.md` §1, §4): PES reads these on the anime entry; GF names them as slot `kSlotLo/HiOffset`, which nothing reads. Not touched here; it is a rename to do with step 2.

## 3. Danger writer (step 3): decode status

- `0x140699960` returns a byte. The builder `0x142119390` stores it at imageA `+0x31C1` (`*(param_1+0x31C1) = uVar7`, near `0x1421196A7`). A second rule sets `+0x31C1 = 1` under `func_0x142118CB0(param_1+0xB6C, 3, 1) == 0` and the distance test. Decompile: `gf-EndClear-scratch/dec_danger.txt` (449 lines, CONFIRMED by the dec.sh output).
- Callees named in the decompile (not all decoded or proven): lane readers `0x140A48A30`, `0x140A489F0`, `0x140A48A10`, `0x140A48910` (translated accessors); predicates `0x140A6E450`, `0x140A6E0F0`, `0x140A6E3C0`, `0x140A6E4B0`, `0x140A6E580`, `0x140A6E300`; `0x140439FE0`, `0x140439F50`, `0x14043A0A0`; `0x140A4C930`, `0x140A4C720`, `0x140A4FEA0`, `0x140A4C820` (the opponent-count test, step 4); `0x140A57720`, `0x140A57800`; `0x140A8F890`; `0x140A3E190`, `0x140A3E7D0`; `0x140A60530`; `0x140A6CE10`; `0x140A71480`; `0x140A72E70`; `0x140477130`; `0x140458740`; `0x14069A3E0`; `0x140481B00`; `0x140A794C0`; `0x140A92D40`; `0x141E532D0`, `0x141E532E0`; `0x14124DAD0`; `0x1415A1780` (sqrt).
- No capture exists: neither entry is hooked (`hooks.cfg` has none of the two). Main owns hooks. The requests in `docs/ports/pes-port-EndClear3.md` §6 still stand (danger writer, danger builder, a danger-1 sampler for the Clear unit).
- Not translated or ported in this session. Porting it by hand without proof would be the "plausible" port the brief forbids; translating it needs specgen, the hooks, and the closure in §1.

## 4. Oracle (step 5): designed and red, not committed

- Captured call: `live2/pid407792/seq021` (the angle, entered from Clear). Reference: the PES-emulator row `corpus.jsonl` (rax, xmm0, 1.2 MB write log). Memory: `mem.bin` through `SnapView`, the harness's view, with the call's delta page (`ok_delta.bin`, `pages.bin`). Scratch: `gf-EndClear-scratch/oracle/seq021/`.
- Test written: `wip/pes_angle_oracle_test.cpp` (not committed). Two tests: (a) the translated angle reproduces the reference bit for bit (rax, xmm0, every written byte); (b) each of seven record bindings, changed by one byte (XOR 0xFF) in the captured memory, must change the result. The bindings are: G `+0x40` record `+0x2920`, MI `+0x1168`, G `+0x360` record `+0x8`, G `+0x2AA8` record `+0x0`, ctx `+0x18` record `+0x2482C`, E0 ring word 0, ring count `+0x1900`.
- Red (CONFIRMED, `gf-EndClear-scratch/logs/red.log`): `pes_fn_142108630` was not declared. A second error (my `Run` helper clashed with `testing::Test::Run`) was fixed by renaming to `Replay`.
- Deviation from the brief: the oracle runs on the capture's own memory (the harness's view), not on GF's `PesWorld` window. GF's window does not hold the records (§2), so the window version needs the writers that do not exist yet.
- Mutations: none run. The byte-flip tests are the mutation design; they cannot run until the layer exists. No source mutation was run.

## 5. Why the window is not wired

- `PesWorld` (`pesworldrecord.hpp`) holds G's header (`kWorldHeaderBytes` 0x2E58) and 22 blocks. The angle needs ctx (+0x26020 read extent), A0.18 (+0x24840), G `+0x40` (+0x2960), MI, `+0x360`, `+0x2AA8`, and the E0 ring. None has a GF writer, and `PesMem` maps one window plus `segs`.
- Designed (not built): extra record regions in the window tail (before `scratch`), pointer fields for G `+0x40/+0x270/+0x360/+0x2AA8` in the header, and a `RunAngle` helper running the translated angle over the window. Needs §2's identities first.

## 6. Open items (numbered)

1. Closure for the angle and the 14063A490 (the five records): specs for `0x140477130` and `0x14063DC10`, and for `0x140A6CF30`, `0x140470D00`, `0x140A3E940`, `0x140A57650`, via specgen. Then rerun the generator with `gf-EndClear-scratch/wip/wip.patch` and `--check`. The generator's own-record rule is in the patch; it is only safe if the drift check stays green.
2. Commit the oracle test and its registration only together with the layer (red test = broken build).
3. Hooks (Main): danger writer `0x140699960`, builder `0x142119390`, and a danger-1 sampler for the Clear unit (EndClear3 §6).
4. Danger writer: specgen spec, translation, and proof from real calls; or a reviewed port with a red test per branch. Then imageA `+0x31C1` is written from GF state and the two stand-ins (`elizacontroller.cpp:2774`, `:2951`) go in one change.
5. Identities OPEN: G `+0x40` record, G `+0x270` MI (full), `+0x360`, `+0x2AA8`, the Clear's carrier `cr` slab owner (`pes-re-record-ids.md` §2), the anime `+0x1A/+0x1C` writer. Each needs a Ghidra xref (bridge down) or a targeted capture.
6. Step 4 (FillUnitTable kind 1 with `{type 5, a = angle}`, `walk.forcedClear` with `0x140A4C820`) only after item 4.

## 7. Blockers for Main

- No hooks or captures are needed beyond what EndClear3 §6 already asks for.
- Main may gate `EndClear` at `83022c25` (merge only; it equals `origin/main` content plus the EndClear3 doc).
