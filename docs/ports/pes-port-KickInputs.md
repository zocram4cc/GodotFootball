# PES port: KickInputs (branch KickSoftMin): kick-scatter stand-ins, not closed

## Verdict

NOT CLOSED. The ticket's acceptance is not met. No stand-in in `src/sim/onthepitch/peskickerror.cpp` was replaced by a PES-layout record read. No code changed in this session. Each stand-in needs at least one PES fact that is not decoded, not in the image, or not in any GF record, and the scatter has no oracle. Below: what was done, what each stand-in needs, the blocker for each, and the one request that would unblock the most.

## Done

- `git merge origin/main` into `KickSoftMin`: origin/main was `643a1808`, two docs commits (`fc77d56a` FoulOffside4 report, `643a1808` KickTerms2 report). The merge adds 3 docs files (207 insertions), no code. Merge commit `8e849361`. Pushed; `git ls-remote origin KickSoftMin` = `8e849361fb02b38e3d308536c698d73d611d3988`.
- The symlink: `git fetch` and `git merge` print "expected submodule path 'extern/godot-cpp' not to be a symbolic link" but succeed. Merged with the symlink in place. The swap-for-empty-dir step in `tasks/27-10-26.md` was not needed for this merge.
- Tests: none run. The merge is docs-only; `pes_kick_error_test` 14/14 was last run on `f1c82642` (KickTerms2 report).

## Where the inputs live (GF side)

- Caller: `src/sim/onthepitch/player/humanoid/humanoid.cpp:82-103` (`KickQuality`, `ShotLaunchPenalty`/`PassLaunchPenalty`, `PressureFactor`, `BuildScatter`, `DrawKickError`, `ApplyKickError`). The `player` is GF's `Player`.
- `Player::GetPesRandom()` (`src/sim/onthepitch/player/player.cpp:448`) is PES's P+0x32d0 slot-2 generator. So PES's `ap`/`P` is the GF Player.
- GF's PES per-player state is `pes::MoveStepRecord` (`src/pes/movestep.hpp:75`) plus `IssuedClip` and `PesDecision`. PES offsets appear only in comments (P+0x245c, 0x245d, 0x245a, 0x245b, 0xac8, 0x27b1, 0x3778, 0x377c, 0x3758, 0x3750, 0xbec). It is not a flat PES-layout record.
- The kick inputs have no GF field at their PES offsets. A grep of `src/` (non-comment) finds none of: 0x2752, 0x2755, 0x2756, 0x2764, 0x2460, 0xbe4, 0x247c, 0x2bf4, 0x2d14, 0xab8, 0x2458, 0x2480, 0x2BD0.
- Block geometry (`src/sim/onthepitch/pesworldrecord.hpp`, `docs/re/pes-re-player-record.md` §1.2-1.3): one 0x3F40 block per slot = anime +0x0 (bound 0x32C), man +0x32C (bound 0xDC4), slot +0x10F0 (bound 0x2E50). PES's `ap` is not any of these records by this layout: `ap+0x32d0` (Random) and `ap+0x2752` do not sit in a single 0x3F40 block if `ap` is the anime start. The kick doc says `ap` is "~0x2c00 bytes", which conflicts with `+0x32d0`. **OPEN: which PES object `ap` is.** This is the first gap; without it there is no PES-offset record to write into.

## Per stand-in (PES facts with VA; GF site; blocker)

Labels: CONFIRMED = disassembly or install read cited in `docs/re/pes-re-kick-scatter-inputs.md` (re-checked where marked); INF; OPEN.

1. **Q (`kSituationNotPorted` = 1.0, BuildScatter `q`)**
   - kind `ap+0x2752` (s16, 151-row table index, or -1). CONFIRMED. Six writer VAs: `0x1406eb6d7`, `0x1406f6b6b`, `0x140719dcf`, `0x14071bb69`, `0x140724f26`, `0x140725fff`. GF writes none.
   - kind -1 fallback: action from `vtbl+0x28` of the anime interface (`ap+0xab8`); remap `0x33→0x2a`, `0x3f→0x1c`, `0x41→0x41`, else `0x27`. CONFIRMED (`pes-re-kick-scatter-inputs.md` §(a)/§7.1). INF: GF's `MoveStepRecord::actionAnime` (P+0xac8, `movestep.hpp:80`) holds the anime action the record last ran in; not checked against `vtbl+0x28`.
   - class/subclass: install table `0x14344e380`, 16 B rows, byte 13 = class (0..4), byte 12 = subclass (0..13). CONFIRMED layout. Readable with `PeImage::ReadVa`.
   - class base 0.9 / 0.88 (class 1) / 0.86 (classes 2,3): OPEN per doc ("exact per-class split is read from the lossy decompiler class branch").
   - gates `0x140724590`, `0x140726610`, `0x140726640`, `0x1407240f0`, `0x1407246e0`, `0x1407241a0`, `0x140724740`: not translated; predicates not decoded.
   - first-time flag `ap+0x2756`, window `ap+0x2764` (f32 < 360), uVar14 `ap+0x2755`, AI bit `ap+0x2460` bit 29, touch bool `vtbl+0x30`, body turn `ap+0xbe4 - ap+0x247c`: writers OPEN.
   - Blocker: the predicates and the per-class base, plus writers of the flags. Not closable without them.

2. **F1 / F2 (`f` = 1.0)**
   - descriptor `FUN_1406b0250(tbl, motionId, flag)`: `motionId==0xe42→0x22`, `0xf7c→0x18`, `≥0x1125→base+1`, else `tbl[1] + motionId·0x390 + 1 + flag`. CONFIRMED.
   - `tbl` = lazy singleton `0x1436f23c0`, built at run time by `0x1406aecf0`. Not in the image. OPEN.
   - Blocker: the table is built at run time from data the harness does not have. Needs a live read or a translated builder.

3. **R run term (run = 0 in `PressureFactor`)**
   - `FUN_140723320`: contact vector `ap+0x2bf4/0x2bf8/0x2bfc` minus kicker position (`FUN_140a6cf70`), state id `vtbl+8` of `ap+0xab8`, per-opponent table `r13+rbx·4+0x16A8`, 8-float weight table `0x14344ef78..0x14344ef98` (+ `0x14344ef98/9c/a0`), final scale k = 0.1 (`0x142588200`, CONFIRMED by KickTerms2), 3-pass running max.
   - Per-case affine: table `0x14072394c` selected by `movsx(byte[r15+1]) − 8`. Constants TENTATIVE per the doc. R caller's `xmm14/xmm11/xmm12/xmm9/xmm8` and the `vtbl` object at entry: OPEN.
   - Blocker: R caller trace (open item 2 in the kick doc).

4. **xmm8 contact factor (code has no explicit term; 1.0 is the fast path)**
   - `obj = 0x140A48910(FUN_1406EA340(kicker))`. `0x140A48910` is translated (`pes_fn_140A48910`, used by `PesWorld::AnimeRecord`). `FUN_1406EA340` is not translated.
   - `byte[obj+1] == 0x48` → 1.0 (CONFIRMED). Anime +0x1 is GF's `AnimeClass` (`pesworldrecord.hpp`); the GF writer is OPEN (player-record doc §2.3).
   - else `B = byte[obj+0x2c]` (CONFIRMED). GF has no field; **no PES writer VA is documented.**
   - `V = 54` from global `0x14351AE50`, set by `0x141E532F0` (KickTerms2 correction 1, CONFIRMED). `hi = floor(V/6)`.
   - t(B): 1 for B ≤ 9, (54−B)/45 for 9 < B < 54, 0 for B ≥ 54 (CONFIRMED affine, doc (d)).
   - Blocker: writer of anime +0x2C. Also `FUN_1406EA340` untranslated.

5. **angRamp (xmm7; omitted)**
   - `ramp = clamp((err+20)/40)·0.4`, `err = byte[obj2+0x28] − byte[r13+0x28]`, `obj2 = anime(byte[r13+0x2458])` (CONFIRMED mechanics, constants `−20 0x1425a946c`, `20 0x14259be1c`, `40 0x14259be2c`, `0.4 0x14259b89c`). The doc has `r13` = kicker.
   - Writers of anime +0x28, of P+0x28, and of P+0x2458 (GF's `GetID()` stands in, `player.cpp:454`): OPEN. `FUN_1406e6660` untranslated.
   - Blocker: writers of the two bytes. A ponytail for xmm7 already names this.

6. **Post-scales `FUN_1408414A0` (absent)**
   - Tier 0 needs `0x140a6e340(action)`: translated (`pes_fn_140A6E340`). Action = `vtbl+0x28`. CONFIRMED.
   - Tier 1 needs `0x1407246e0(player)` and `0x1407241a0(kind)`: untranslated, undecoded.
   - Tier 2 flag tables at install `0x1408417f8` (0x6E bytes) and `0x140841884` (0xD8 bytes); motion id = word at `vtbl+0x18`. CONFIRMED.
   - Tier 3 `u²` blend: `u` = caller's 7th stack arg (`[rsp+0x78]`, caller `[R+0x30]`). Caller of `FUN_1408414A0` (`0x14083f038`) not traced: OPEN.
   - Tier 3-alt predicate `0x1406ead40`: untranslated.
   - Blocker: `u` provenance, and the untranslated predicates above.

7. **Yaw redraw `FUN_1408398D5` loop (skipped)**
   - Mechanics CONFIRMED (`docs/re/pes-re-kick-scatter-inputs.md` (f)): `r14 = [rbp−0x78]`, sign of `x·y` picks xmm11 vs xmm9, up to 6 draws via `0x140a92590` (Normal) and `0x1406ebf30` (translated).
   - `[rbp−0x78]`/`rbp` provenance: OPEN (needs the `FUN_1408398D5` caller).
   - Blocker: caller trace.

8. **Shot pitch g (−0.02·g term in `ShotLaunchPenalty`; ponytail in code)**
   - `xmm14 = 0.0` (`0x14083cf19`), `xmm7 = 10.0` (`0x142588210`, `0x14083da10`), `xmm12 = 1.0` (CONFIRMED, KickTerms2 / doc (g)).
   - `xmm11 = F·60·60/1000·sqrt(|dir|²)`: `F = 0x141e532d0()` (getter; value not established), `dir = [rsi]/[rsi+4]/[rsi+8]` (provenance OPEN). `xmm1 = (double)xmm11`.
   - Blocker: `F` getter value, and provenance of `dir`.

9. **Header predicate (task item "follow PES")**
   - GF has disagreeing predicates: `src/sim/onthepitch/team.cpp:26` `IsKickFunction` excludes Header and gates `inKickBeforeContact` (`team.cpp:401`); `src/sim/gamedefines.hpp:73` includes Header; the kick doc cites `match.cpp:1643` as also including it (path not re-checked this session). `PesActionKindOf` (`team.cpp:32`) sends Header to action 7 (touch).
   - PES side: the scatter's header predicate `0x1407246e0` = action 8..10 AND `0x140724740` Fox motion check AND kind-table byte+0xc ≤ 1 (doc (e) Tier 1 / (a)). Which action byte PES uses for a header is **OPEN**, so "PES's header" cannot be pinned from the docs.
   - Not changed. Changing `IsKickFunction` would move possession (`inKickBeforeContact`), outside the scatter terms, and the PES header action is not established. Blocker: the header action byte in PES.

## Oracle status

- No oracle exists for the scatter. `FUN_14083CC30` is not translated and has no spec under `tools/dev/diffharness/specs/`. Its callees are also untranslated: `0x1408413C0` (soft-min; calls CRT qsort `0x141E53540`, which the harness does not model), `0x140A92590` (Normal; GF hand port), `0x140723320`, `0x1408414A0`, `0x1408398D5`, `0x1406EA340`, `0x1406E6660`, `0x1407205A0`, `0x14071E520`, `0x1407240F0`, `0x140724590`, `0x140726610`, `0x140726640`, `0x1407246E0`, `0x1407241A0`, `0x140724740`, `0x1406B0250`, getters `0x141E532D0/E0/F0`.
- Translated and usable (checked): `pes_fn_140A92C80` (sine), `pes_fn_1406EBF30` (Range/sign roll), `pes_fn_140A48910` / `pes_fn_140A48A10` (accessors), `pes_fn_140439EF0` (bearing), `pes_fn_140A6E340`.
- Cases: 0 for the per-draw quality. Prior: SinTable 72 (pilot), SoftMin 54,000 (hand port, KickTerms2).
- Kick-time captures: **none**. The 25 hooks in `<workdrive>/PES21/rendezvous/hooks.cfg` have no kick hook. The 141 captures in `gf-Rendezvous-scratch/live2/` and 100 in `live/` are camera, restart, referee, replay, HUD, type-13 hooks only. Each capture is a full `mem.bin` (about 3.5 GB) with a mapping table, so real records are available once a kick hook exists.
- Mutations: none run (no code change). Oracle diff: none.

## Remaining stand-ins (each with reason; all of the eight listed remain)

1. Q: `kSituationNotPorted` = 1.0. Reason: flag/AI/body-turn writers OPEN; class-base split OPEN; gate predicates untranslated; kind writers present in PES but GF's kind namespace is different (`PesActionKindOf`).
2. F1/F2: 1.0. Reason: motion table is a runtime singleton, not in the image.
3. R run term: 0. Reason: caller registers and per-case affine constants TENTATIVE/OPEN; contact-vector and state-id sources not in GF records.
4. xmm8 contact factor: effectively 1.0 (no term). Reason: anime +0x2C writer not documented; `FUN_1406EA340` untranslated.
5. angRamp xmm7: omitted. Reason: writers of anime +0x28, P+0x28, P+0x2458.
6. Post-scales `FUN_1408414A0`: absent. Reason: `u` provenance, untranslated predicates `0x1407246e0/0x1407241a0/0x1406ead40/0x140726610`.
7. Yaw redraw loop: skipped. Reason: `[rbp−0x78]` provenance OPEN.
8. Shot pitch g: omitted. Reason: `F` getter value and `dir` provenance OPEN.
9. Header predicate: not changed. Reason: PES's header action byte not established; change moves possession.
10. Soft-min qsort (not in list; blocks any oracle of `FUN_14083CC30`): hand port `SoftMin4` stays. Reason: CRT qsort unmodelled in harness.

## Next steps, ranked

1. **Request for Main (hooks; Main owns `hooks.cfg`).** Add kick-time entry hooks, each with a sample count in the hook plan:
   - `FUN_14083CC30` (VA `0x14083cc30`, 15 bytes): `hook kick_scatter_14083CC30 0x14083cc30 15 1 <N> 488bc44c8948204c89401848895010`. Args: rcx = player, rdx = out Y, r8 = out P, r9 = out S, stack = mode, vec, w, out.
   - `FUN_1408414A0` (VA `0x1408414a0`, 15 bytes): `48895c240848896c24104889742418`. Post-scales; args rcx = player, rdx/r8/r9 = x/y/z records, stack `u`.
   - `FUN_1408398D5` (VA `0x1408398d5`, 15 bytes): `440f299c2460030000e83d4ceeff8b`. Yaw redraw; this captures the `[rbp−0x78]` vector at entry.
   The prologue bytes were read from `pes_full.bin` at file offset VA − 0x140000000 (`xxd`, this session). Main must verify them against the live image before use.
2. Decode `ap`: which PES object is it, its size, and where the anime/man/slot records sit relative to it. Without this no PES-offset field can be written.
3. Decode the writers of anime +0x2C, anime +0x28, P+0x28, and the flag bytes (`ap+0x2756`, `ap+0x2764`, `ap+0x2460` bit 29).
4. Translate the untranslated callees listed above (or give each a stub with a named ponytail), then translate `FUN_14083CC30` and prove it on the kick-time captures from (1). This is the oracle the ticket asks for; it needs a qsort model for `0x1408413C0`.
5. Only then: GF fields at PES offsets on the Player's PES record, writers at the GF kick-issue sites (`humanoid.cpp` caller, `team.cpp:32/383` action chooser), and the stand-ins replaced.

## Blockers needing a decision

- Main: the three hooks above (kick-time capture). Without them, there are no kick-time cases; the captures in `live2/` do not reach the scatter.
- The header action byte in PES (item 9): needs a decode, not a decision.

## Budget

About 55 of about 150 requests used. Work stopped at the blocker rather than shrinking acceptance silently; this report is the shortfall statement.
