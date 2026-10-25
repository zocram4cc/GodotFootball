# Coverage: what "bit-exact" proves (Coverage agent + Main's full runs, 25-10-26)

Branch `origin/Coverage` `a38c32b`, landed as batch Y. The agent wrote no report; Main compiled this from its result and
Main's full runs of both sets.

## Mechanism

- `run.py`: coverage-guided case generation. Guards (compares feeding conditional branches) are decoded and traced back
  to spec inputs through MOV/LEA/MOVSXD/SET/CMOV, single-MOV callees and call results; integer compares are solved
  directly, float compares steered, other guards mutated round-robin. Deterministic: base cases first, then steered
  cases appended, same seed.
- `driver.py`: `COVERAGE_FLOOR = 0.50` (provisional). A function is diffed only if its cases execute at least that
  fraction of its own basic blocks; `dead_blocks()` drops abort-trap blocks and int3 padding from the denominator, each
  with its reason.

## Results at the floor (Main's full runs)

| Set | Bit-exact before | At the 50% floor | Left the record | Own-block coverage, median |
|---|---|---|---|---|
| Pure pilot | 216/252 | **183/252** | 23 below the floor, 10 new spec gaps | 86% |
| Impure sample | 31/50 | **7/50** | 21 below the floor, 3 spec gaps | ~6% |

- **Pure translation holds up:** 73% passes with real coverage. The 10 new spec gaps are guided cases reaching pointers
  the specs never linked.
- **Impure functions aren't proven:** their branches depend on game state the specs don't model (records, objects with
  vtable pointers, pointer graphs under G), so they exit early. Guided inputs barely move them (spot checks: 35% to
  40%, 1% to 11%). Real state is the fix: emulating PES's own setup (StateSynth) or a full process dump of a running
  match.
- **One translator bug exposed:** `0x140503430` returns a wrong pointer on case 123 (want `0x22800000`, got
  `0x1001FF00`). Not yet fixed.
