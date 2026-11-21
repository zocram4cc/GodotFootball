# pes-port-StallBreaker5: HEAD does not pass its own gen --check (drift predates round 5); the slide handlers are NOT landed; branch clean at 09a5f7e2 (pushed)

Status: BLOCKED on a drift root cause. Nothing committed this round. Branch `StallBreaker` = **09a5f7e2** (`git ls-remote` confirmed), worktree clean (`git status` empty, excluding the godot-cpp symlink note).

## 1. The drift predates round 5 (CONFIRMED)
- `gen_translated.py --image pes_full.bin --check` on **clean HEAD 09a5f7e2** (no round-5 edits; layer, tools and universe reset to HEAD) fails: **12 drifts**: 6 `differs` (closure/pes_fn_1404121D0, closure/pes_fn_1406E47A0, closure/pes_fn_1415A174A, objinit/pes_fn_140440FC0, pes_dispatch_gen.cpp, translated.hpp), 6 `unexpected` (closure/pes_fn_{1406E5F50, 140724820, 140782570, 140789E60, 14079A310, 141AF6E40}.cpp: committed, not produced). Log: `gf-StallBreaker-scratch/r5/headcheck2.log`.
- **Deterministic**: the same 12 drifts with `PYTHONHASHSEED=0` and `=12345` (`hs0.log`, `hs1.log`), and the same set as the first HEAD check. So it is not hash-order noise.
- **Not the universe**: the same 12 with `universe.json` = the pre-merge a9cda866 copy (`check_pre_merge_universe.log`). The HEAD universe was restored afterwards (checked clean).
- **Not my round-5 edits**: reproduced on a clean HEAD tree.
- **When it appeared** (CONFIRMED from git): the six `unexpected` files do not exist at a9cda866 and first appear at merge commit **55ff8f64**. The merge's own layer regeneration (gen1) passed `gen --check` (bg_62, "10471 functions byte-identical") at the time. Since then the inputs were not changed in git (only the compile fixes in 09a5f7e2, which touch no generator input). So gen1's inputs and HEAD's committed inputs differ in something git does not track, or gen's output depends on something outside the repo. I have not identified it.
- Candidate ranked list, none verified: (a) an untracked or gitignored input under `tools/dev/` or `tools/dev/diffharness/specs/` present when gen1 ran (gen reads spec directories via `driver.universe_of`, so a stray spec file changes the universe), (b) a cache read by `driver.runnable_extra` under `<out-dir>/cache` or `GF_DIFF_CACHE` that was populated by another run, (c) a results or closure JSON that another agent's regeneration wrote after gen1 and that was later reverted. Settling it needs `git stash -u` on a clean checkout of 55ff8f64 and a fresh `gen --check` (about 15 minutes each).

## 2. The slide handlers (round 5 work, uncommitted, saved)
- The full uncommitted round-5 diff is saved as `gf-StallBreaker-scratch/r5/wip-r5.patch` (41,386 lines, includes regenerated layer churn). The useful, reviewable part is small and is listed here so it can be reapplied by hand:
  1. `tools/dev/translate/gen_translated.py`: `INDIRECT_EDGES["0x1406F6CE0"] = ("0x14073DD50", "0x140789D00", "0x140791DD0")` with the survey citation (pes-port-StallBreaker4.md, return 0x443c8f in pes_fn_1406F6CE0). Plus a closure-walk change: an indirect edge target joins the closure even when the set's universe lacks it. Without the walk change the edge is silently dropped (verified in the earlier gen run).
  2. Three pilot specs in `tools/dev/diffharness/specs/trapaction/` and `specgen/sets/closure-slide.txt` (from `closure.py --spec-set trapaction --tag slide`). Adding spec files changes `universe_of` for that set, so they are a suspect for the churn too; they were removed for the HEAD check.
- Decompiles (CONFIRMED, `r5/dec3.txt`): 0x14073DD50 is a one-byte copy (no calls). 0x140791DD0 is field copies and bit moves (no calls). 0x140789D00 makes two dynamic vtable calls on `param_3+0xAB8` (vtable +0x20, then +0x38), so it cannot be closed statically; a proof needs the object's real vtable from a capture.
- Gen with those edits (gen3) makes the three functions closure members, and the layer then changes 10 other files. Four of those changes replace `pes_dispatch_missing` aborts with real calls (1404121D0 -> 1403FB730, 1406E47A0 -> 1406E7A60, objinit/140440FC0 -> 1403FB5B0), which are behaviour changes on the sim path and are not explained. Six closure copies are deleted. Since HEAD already fails its check (section 1), I cannot attribute these to my edits.
- **Not proven.** No synthetic spec for the three exists yet (0x140789D00 needs a real object vtable; no live capture of these handlers is in `live2/`).
- **Hook lines**: not produced. `untranslated_callees.py --hooks` prints no rows for the three targets. The predicate has to be written by hand: in-play, SLIDING (P+0xAC8 == 11), EVERY 1, cap 6, on the slot 0x1425E1908.

## 3. Walker and acceptance
Not started (the abort is still on the pushed branch). The `ANonOwnerWalker...` red test is in 09a5f7e2 unrun.

## 4. Next steps for the handoff (in order)
1. Find the drift input: run the clean-checkout gen check on 55ff8f64 (`git stash -u` discipline; scratch worktree, not the repo) and compare with HEAD's check. Fix at the source in its own commit (regenerate the layer from the committed inputs, after explaining each `pes_dispatch_missing` -> real-call change).
2. Then apply the INDIRECT_EDGES change and the walk fix as their own commit, with the three handler closure files only, then synthetic proofs (needs an object vtable from a real slide-adjacent capture).
3. Hook line for 0x140789D00 (EVERY 1, in-play, SLIDING, cap 6).
4. Walker census (the red test `ANonOwnerWalker...`) and the acceptance set (PesStall, PesHang, throw-in, pass-target, seed-7 smoke, gen --check, ledger --check).

## 5. Verification this round
| Check | Result |
|---|---|
| `gen --check` on clean HEAD 09a5f7e2 | FAIL, 12 drifts (deterministic over hash seeds and universe choice) |
| `gen --check` with pre-merge universe | FAIL, same 12 |
| worktree after round | clean, HEAD 09a5f7e2 |
| builds, tests, smoke, ledger | not run this round |
