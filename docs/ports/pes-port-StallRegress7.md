# PES press chain, round 5: the callback path located, not yet proven; no code change (PressWire)

Branch `StallApproach` at `bb52fa12` (no new commit this round; `git ls-remote` unchanged). Nothing wired; the walk stays; no census run (last measured 151).

**Result: the blocker is narrowed to one branch of `FUN_141B55730`, with its exact condition read from the translated closure. The synthetic proof is not yet built, because the input that reaches the branch is not yet known. Construction, A2 per-tick fields, r9 and wiring are not done.**

## The branch (CONFIRMED from the translated code, `closure/pes_fn_141B55730.cpp`)

Let O = rcx on entry (the object the constructor's frame passes), Q = *O, and F = the caller's frame (the callback slot is at F+0x28; the callee reads it at its `rsp+0x68` after its pushes):

1. `gpr14 = *O` (= Q). If Q != 0 (line 46):
2. `gpr0 = *Q`; call `vt(gpr0)[0x70]` with rcx = Q; `gpr6 = rax`.
3. If rax == 0 (line 62): call `vt[0x78]` (lines 70-75) if gpr3 != 0; then, unconditionally on this path, the callback `call [F+0x68]` (line 86-89) which is `0x14041F2F0` (stored by the caller `FUN_1404234B0` at its frame +0x28, 0x1404234B0 translated line 98).
4. If rax != 0 (line 105, the else branch): the callback is not called on this path (vt[0x78] only).

So the callback 0x14041F2F0 runs exactly when Q != 0 and `vt[0x70](Q)` returns 0. The six ctor captures took the other branch (rax != 0), which is why the closure has no proof for the callback (own block 20/25).

## What was checked

- The match-info state at the ctor's entry is the same in GF and in the capture: MI+0x116C (play state) = 0 in seq353 and seq358, MI+0x1554 = 1, MI+0x1574 = 0. So the difference is not MI state alone.
- The callback's frame slot F+0x28 holds 0x14041F2F0 (caller `FUN_1404234B0`), CONFIRMED from the translated caller.

## Not done, and the named blocker

- The input that makes `vt[0x70](Q)` return 0 is not known. Q's vtable and the function at its slot 0x70 are not decoded. A synthetic state (`synth.py`) needs a field to perturb, and the perturbation must be real memory the function reads. I did not decode `vt[0x70]`, so I cannot name the field.
- Next step: read Q's vtable word from a capture of the ctor at the `141B55730` call (the synth `stop` at `0x141B55730` entry), decode the slot-0x70 function, and name the field whose value flips its return to 0. Then synth a case with that field set (`reach` = 0x14041F2F0, `stop` = the ctor's entry, `start` = call), prove the callback, and re-run `prove_bounded` with the synth state merged.

## Hygiene

- No files changed this round; the worktree is clean at `bb52fa12`. `build/` was removed in round 4; no build this round.
- Scratch: `<local>/scratch-press`.
