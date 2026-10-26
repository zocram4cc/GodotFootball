# Captures: canonical union impure measurement over complete captures (26-10-26, final)

Branch `Captures`. Head `3084a4cd` (ls-remote confirmed). Lineage: `6355753c` (merge RealCases), `49159c16`, `780e62d4`, `dff5515c`, `f5a42e32` (17-capture union), `57d4f1aa` (OpenPlay: `.pdata`-based argument typing, merged ff), `0b0919e1` (17-capture union re-measured on the 8 supported roots), `3084a4cd` (canonical 9-capture union + order-independent banks). Scratch data (captures, banks, driver outputs, logs) is PES-derived and stays in `<workdrive>/gf-Captures-scratch/` and `<workdrive>/gf-FrameTrace-scratch/`; nothing of it is committed.

## Done

1. **Owner audit incorporated.** Main audited every capture from its bytes (`main_audit.py`): my `captures_i` set (14 accepted, 8 open play) is **incomplete** — team record 1 holds 28,672 of 49,464 bytes, and most slot records are patched or uncaptured (1 to 4 of 22). The 17-capture union therefore ran open-play cases without most per-player state; "open play doesn't help" was not established from it. The match is over; no more captures are possible.
2. **Canonical union: 9 complete, consistent captures** (audit verdict `consistent`: both teams whole, 22/22 slot records captured, MI in G ok):

   | capture | dir | play | restart | frame |
   |---|---|---|---|---|
   | f01 | `gf-FrameTrace-scratch/cap1/f01-open-p3-play1-fr30490` | 1 | 0 | 30490 |
   | f1323 | `…/cap1/f1323-open-p1-play1-fr13724` | 1 | 0 | 13724 |
   | f1559 | `…/cap1/f1559-open-p1-play1-fr19044` | 1 | 0 | 19044 |
   | f501 | `…/cap1/f501-open-p3-play1-fr43009` | 1 | 0 | 43009 |
   | t002 | `captures_e/t002-start-p3-f37279` | 1 | 0 | 37279 |
   | t025 | `captures_e/t025-restart4-p3-f39484` | 3 | 4 corner | 39484 |
   | t028 | `captures_e/t028-restart2-p3-f40208` | 2 | 2 throw-in | 40208 |
   | t033 | `captures_e/t033-restart3-p3-f42494` | 2 | 3 goal kick | 42494 |
   | t037 | `captures_e/t037-open-p3-f45451` | 1 | 0 | 45451 |

   So the canonical union does include a throw-in (t028) and a goal kick (t033). Restart kinds per `docs/re/pes-re-A5Judge.md` line 40. `captures_e/t029` (MI differs) and all `captures_f/g/h/i` are excluded.
3. **Order-independence fix.** `seed.py bank_multi` sorted its `--live` dirs by path internally (`3084a4cd`). The per-case instance binding `n % N` made the bank depend on CLI capture order: an unsorted order gave floor 13 for the same 8 roots (OpenPlay's observation, reproduced). Verified: a deliberately shuffled `--live` list now produces byte-identical banks.
4. **Union measurement, full run.** 618 s, rc 0, 8 supported roots, `--seed-bank --perturb`. `results_impure.json` committed from that run; `--check` (192 s): all 15 recorded-diffed still diff bit-exact.
5. **Per-capture coverage measured** — nine single-capture full runs (the owner's "per-capture" ask), §3.

## Results (own-block coverage of the entry VA)

| measure | main | 17-capture record* (stale, `f5a42e32`/`0b0919e1`) | **union9 (9 complete captures)** |
|---|---|---|---|
| specs with coverage | 46 | 46 | 46 |
| bit-exact (every case) | 8 | 16 | **15** |
| floor: coverage ≥ 0.50 | 9 | 17 | 16 |
| **owner's floor: ≥ 0.50 and bit-exact** | 8 | 16 | **15** |
| median own coverage | 0.067 | 0.299 | 0.299 |
| mean own coverage | 0.218 | 0.357 | 0.355 |

`*` measured on `captures_i` (incomplete per the audit) and `captures_f` stoppages; kept here as the comparison against the earlier committed record.

- **Versus the stale record:** 1405AD720 drops 0.5068 to 0.4658 — **bit-exact on all 118 cases in every capture, but below the 0.50 floor** (recorded as `harness cannot run` / "bit-exact 118 cases at own-block 0.47 < floor 0.50", which is how the driver marks below-floor bit-exact, `driver.py` line 740). No other function lost bit-exact. Nothing gained.
- **14051CF70** is at floor (0.700 in every capture) with an empty corpus: runs no cases at all.
- **14064DE60:** 0.0517, below floor, 111 cases (was `translator bug` on the 17-capture record; on the complete captures the note is below-floor bit-exact, not a diff failure).
- Single-snapshot comparison (RealCases, cited): floor 16 (G seed) and 13 (real plus perturb); median 0.207 and 0.272.

## 3. Per-capture coverage (own-block fraction of the entry VA)

| VA | f01 | f1323 | f1559 | f501 | t002 | t025 | t028 | t033 | t037 | union9 | status |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1404F8C70 | .704 | .704 | .620 | .627 | .553 | .563 | .574 | .574 | .518 | .627 | exact |
| 1404FB2A0 | .750 | .750 | .750 | .750 | .625 | .750 | .625 | .625 | .625 | .750 | exact |
| 140503430 | .581 | .593 | .606 | .606 | .610 | .606 | .614 | .610 | .606 | .602 | exact |
| 1405055E0 | .849 | .849 | .849 | .849 | .849 | .849 | .849 | .849 | .849 | .849 | exact |
| 140510D30 | .291 | .291 | .291 | .291 | .291 | .873 | .764 | .873 | .291 | .818 | exact |
| 14051CF70 | .700 | .700 | .700 | .700 | .700 | .700 | .700 | .700 | .700 | .700 | corpus empty |
| 140529490 | .717 | .717 | .717 | .717 | .717 | .717 | .717 | .717 | .717 | .717 | exact |
| 14052A190 | .417 | .417 | .417 | .389 | .389 | .750 | .639 | .722 | .389 | .750 | exact |
| 14052A820 | .576 | .576 | .545 | .573 | .532 | .529 | .576 | .576 | .532 | .529 | exact |
| 14053B110 | .296 | .296 | .296 | .296 | .259 | .630 | .296 | .630 | .259 | .630 | exact |
| 14057BA50 | .545 | .545 | .545 | .545 | .545 | .545 | .545 | .545 | .545 | .545 | exact |
| 1405AD720 | .466 | .466 | .466 | .466 | .466 | .466 | .466 | .466 | .466 | .466 | **below floor, bit-exact all 118 cases** |
| 140639100 | .829 | .843 | .829 | .843 | .829 | .829 | .829 | .829 | .829 | .829 | exact |
| 140654E60 | .721 | .788 | .721 | .760 | .760 | .760 | .760 | .712 | .721 | .779 | exact |
| 14065DAB0 | .486 | .581 | .495 | .486 | .486 | .486 | .486 | .486 | .571 | .571 | exact |
| 140A539F0 | .692 | .692 | .827 | .827 | .692 | .827 | .827 | .827 | .692 | .692 | exact |
| 140A57650 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | exact |

Per-capture bit-exact counts: f01 10, f1323 12, f1559 11, f501 10, t002 11, t025 14, t028 13, t033 14, t037 12. Note the G-only (untyped) specs — 1405055E0, 14051CF70, 140529490, 14057BA50 — show identical coverage in every capture: their single G instance per capture is the same region layout, so they contribute no union growth. The capture-to-case variation is entirely in the typed-root (team/man) bindings.

## 2. Argument roots

My original pass typed 15 root slots over 15 specs using the nearest spec VA as the enclosing function. OpenPlay's stricter `argroots.py` (on `origin/OpenPlay` `57d4f1aa`, merged ff into this branch) resolves each call site through the image's own `.pdata` unwind table and withdrew 7 of the 15: 1404FB2A0.A0, 140503430.A1, 14057BA50.A0, 1406486D0.A0, 14064DC50.A0, 140654E60.A3, 14065C2A0.A0 (at each site the argument register is a stack reload or a value outside any argument). The supported file types **8 of 97 roots**: 6 seeds (1404F47A0, 1404F6C30, 1404F8C70, 1404FA720 team; 14064DE60, 140978E70 man) plus 1404FE9C0 and 1404FEDC0 (team, through the typed root of 1404F6C30). Evidence: `impure_roots_evidence.json` (58 sites dropped at `.pdata` bounds; 0 conflicts). The committed record is measured on these 8 roots.

## 4. Method notes

- **Team header gate.** Team record `+0x0` is a per-frame counter (OpenPlay probe: ~46 changes/s). The 8-byte team header gate is therefore a counter-change gate, not a vtable check; MI's header stays in the gate.
- **Bank.** `seed.py --cap 24` over 9 captures gives 3 instances per capture for typed roots (34 instances for the team-rooted specs, 27 for man — the man table has fewer live rows per capture); untyped specs get 1 G-only instance per capture (9).
- **The `captures_i` captures remain available** in scratch (14 accepted, 0 rejected, 1.15 MB, 113 to 125 preads, 1.5 to 6 ms each) but are not part of the canonical union.

## 5. Not done / open

1. **1405AD720 just under floor.** 0.4658 in every capture (bit-exact on all 118 cases each). A small coverage gain — one more block reachable, or a capture that exercises the missing branch — would push it over 0.50 and make the floor 16.
2. **14051CF70 corpus empty** at 0.700 coverage. No cases run at all in any capture.
3. **14064DE60** at 0.0517 (111 cases, below-floor bit-exact). Was `translator bug` on the incomplete 17-capture record.
4. **~33 specs unrunnable** (`harness cannot run` / `unmodellable callee`).
5. **Typing for the remaining roots** (89 of 97 untyped). OpenPlay is continuing on their branch; their stricter analysis is the base.

## 6. Reproduce

- Canonical banks: `python3 tools/dev/translate/seed.py --live <the 9 capture dirs, any order> --spec-dir tools/dev/diffharness/specs/impure --bank-classes tools/dev/translate/impure_roots.json --out <dir> --cap 24` (dirs are sorted internally).
- Driver: `env PES_EXE=<pes_full.bin> GF_DIFF_PYTHON=<venv python> python3 tools/dev/translate/driver.py --image <pes_full.bin> --out-dir <dir> --spec-dir tools/dev/diffharness/specs/impure --seed-bank <banks> --perturb --record tools/dev/translate/results_impure.json --jobs 2 --timeout 900`; then the same with `--check`. Per-capture: repeat with `--live` as a single dir and no `--record`.
- Capture (match is over; tool kept): `sudo -n python3 tools/dev/translate/capture_midplay.py --out <scratch>/captures_x --max-caps 14 --open-every 30 --frame-max 2 --rows 4 --dur 900`.

Tests: none new (harness tooling, not engine code). The gate was not run (agents do not run it). Verification: full run rc 0, `--check` 15/15, bank order-independence byte-identity check, owner-audit re-run of all 33 capture dirs.


## Main's audit of every capture (26-10-26)

Main re-checked all 33 captures from their bytes (`main_audit.py` in the Captures scratch), not from their metadata:
G's MI pointer against the capture's MI, MI's play state against the label, both team records captured whole
(0xC138), and the per-player slot table.

| Set | Captures | Consistent and complete | Defect |
|---|---|---|---|
| `captures_e` | 6 | 5 (2 open play, 3 stoppages) | t029: G's MI pointer differs from the captured MI |
| `captures_f/g/h` (shoot-out, periods 8-10) | 8 | play state and teams consistent | only 1 of 22 slot records set: shoot-out moments, not match play |
| `captures_i` (the 8 open play + 6 stoppages in the 17-capture union) | 14 | **0** | team 1 only 28,672 of 49,464 bytes captured; 18-21 of 22 slot records unset or uncaptured |
| FrameTrace `cap1` | 4 | **4, all open play**, H object captured | none |

**The 17-capture union's open-play captures lacked most per-player state**, so "open play doesn't lift impure
coverage" is not established. The union is being re-run over the 9 complete captures (`captures_e` minus t029, and
FrameTrace's 4).

## Main's reading of the canonical union (9 complete captures)

Floor 15/50 (median 0.30), the same as before: complete open-play captures don't lift impure coverage. The per-capture
runs show why: specs whose arguments are untyped run against G only, and G is a single instance per capture, so their
coverage is identical in every capture; all of the union's growth comes from the 8 typed argument roots. The lever is
binding real arguments (the 89 untyped roots), which static typing can't do and the frame-loop trace can, once a capture
includes the asset registry (`docs/ports/pes-port-Emulate.md`).
