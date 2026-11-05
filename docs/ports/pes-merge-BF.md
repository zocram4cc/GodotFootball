# Batch BF: FlatKeys (merged to main)

**Contents:** branch `FlatKeys` (`92b2aa8d`), merged onto main `72663070` as `e64eb418`. Since the branch point `aa4e051d`, main gained only docs and `tools/dev/rendezvous/hook_plan.py`, so the gate verdict holds. Report: `local://pes-gate-FlatKeys.md` (session artefact). Measurement: `local://pes-gate-SimMemory.md`.

**Change:** `blunted::NodeAnimation::animation` (PES clip keyframes) moves from `std::map<int, KeyFrame>` to a sorted `std::vector<std::pair<int, KeyFrame>>`. Every keyframe is kept, with values, order, duplicate-key and lookup semantics unchanged. The audit of about 20 live sites in `animation.cpp` / `animcollection.cpp` found no iterator or pointer into a key that survives an insert or erase.

## Memory

- **Per key:** 96 B before (map node plus malloc header), 56 B after (one record), measured with `mallinfo2`. The test bound is 64 B; main fails it with 96.
- **Steady VmRSS, one match:**

| Mode | Before | After | Delta |
|---|---|---|---|
| switch-off | 3,724 MiB | 2,257 MiB | −1,467 MiB |
| pes-loop | 3,738 MiB | 2,327 MiB | −1,411 MiB |

## Proof

- **Smokes** (seed 7, 2 min): switch-off `399e180f39459c2b` and pes-loop `85c7d7c515cf92ae`, identical to main.
- **`GATE fast: GREEN`:** 2,347 of 2,347.
- **`animation_keyframes_test`:** 5/5. Six mutants (unsorted insert, no shrink, lookup ignoring frame, delete skipping erase, overwrite keeping the old position, swapped interpolation weights) were each caught and reverted.

## Left

- `FootballAnimationExtension` keeps its own per-clip `std::map<int, FootballKeyFrame>` (same pattern, not measured).
- SimMemory's other candidates are not started: lazy clip materialisation, and lazy GANI decode (149 MiB retained).
- Dead commented code in `Hax()` still names the old map type.
