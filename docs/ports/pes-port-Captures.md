# Captures: mid-play capture tool, argument typing, union impure measurement (26-10-26)

Branch `Captures` (worktree `<workdrive>/gf-Captures`, base `origin/main` 1cbe5ac5, `origin/RealCases` 6a7a1ee merged). Pushed: `6355753c` (merge), `49159c16`, `780e62d4`, `dff5515c` (head, `ls-remote` confirmed). Scratch data (captures, seed banks, driver outputs, logs) is PES-derived and stays in `<workdrive>/gf-Captures-scratch/`; nothing of it is committed.

## Status

- Done: targeted capture tool with object-level gates; 5 accepted mid-play captures; argument-root typing (15 slots over 15 specs, evidence committed); union seed banks; a full driver run over the 50 impure specs with the union banks; `results_impure.json` committed, with no previously bit-exact spec lost.
- Not done: open-play captures (none accepted; see §1); per-capture coverage (not measured; see §3); typing for 35 specs (§2).

## 1. Captures

**Tool.** `tools/dev/translate/capture_midplay.py` (read-only: `sudo -n` `pread` of `/proc/53052/mem`; no write, pause, signal or attach). It reads the spec-walked graph only: G (0x2E58), the image globals the specs root at, the on-pitch records (2 teams, 22 anime, 22 man, 22 slot), MI, MA, and each named chain node. A capture is 7 to 30 MB, about 50 to 170 ms, and about 1.7k preads. A whole-process snapshot is 3.8 GB, so I did not use it.

**Model (owner, via Main).** MI (G+0x270) and the team slots (G+0x1928, G+0x1950) are published per match step and cleared between steps. The objects persist: the MI object was `0x1BC04608` and G was `0x2C0C3390` across every match in this session (CONFIRMED, `capture.log`). The tool therefore remembers the G-slot values seen in step windows, reads the objects at those addresses, and patches the zero slots of the G blob from memory. Each capture lists its `patched_slots` (34 to 36 per capture) in `live.json`.

**Gates.** (a) MI and both team objects keep the same vtable (first 8 bytes) across the snapshot. (b) The MI frame counter (MI+0x1198) advances by at most 2 across the snapshot. (c) The G blob is not torn (MI and team slots present, from the epoch).

**Evidence on the gates (CONFIRMED from logs).**
- A 16-byte header compare rejected every open-play snapshot (`headers_same=False`). I narrowed it to the vtable, which is the identity check Main specified. The open-play frame gate is unverified: the earlier build (`captures_e`) saw frame deltas of 3 to 10 on its reads, and the vtable-gated build saw no open-play state before the game froze.
- `t011-restart4-p1-f23874` (first accept, an earlier build) was torn: its G blob had null MI and team slots. It was retracted.
- The frame counter sat at 57365 from about 10:53:51 to at least 10:56:11, in a kick-off-type state (play 2, restart 1, reason 3, per 10). The game was not advancing, so every capture was gated out. You reported "currently on penalties"; the game was frozen after the shoot-out. I cannot see the screen, so this is INFERENCE.

**Accepted captures used in the union (all mid-play stoppages).**

| label | per / play / restart / reason / side | situation (documented, see below) | frame delta | MB |
|---|---|---|---|---|
| `captures_f/t034-restart6-p9-f49264` | 9 / 3 / 6 / 9 / 0 | penalty shoot-out kick (INF from docs) | 2 | 9.3 |
| `captures_f/t037-restart1-p9-f49682` | 9 / 2 / 1 / 3 / 1 | kick-off-type stoppage | 2 | 8.2 |
| `captures_f/t041-restart0-p9-f51349` | 9 / 2 / 0 / 0 / 2 | restart kind 0 in a stoppage (unclassified) | 2 | 7.9 |
| `captures_f/t046-restart1-p10-f57363` | 10 / 2 / 1 / 3 / 0 | kick-off-type stoppage | 2 | 7.5 |
| `captures_g/t001-stop1-p10-f57365` | same as t046, 2 frames later | duplicate moment | 0 | 7.5 |

Excluded from the union, not mid-play: `captures_f/t030` (per 8, restart 7, reason 1) and `t031` (per 9, restart 7, reason 1). These are period-end stoppages.

**Situation mapping.** Restart kinds: 1 kick-off, 2 throw-in, 3 goal kick, 4 corner, 5 free kick, 6 penalty, 7 period end. Stop reasons: 1 period end, 3 goal or kick-off, 4 ball out, 5 foul or offside, 9 shoot-out, 10 abandon. Sources: `docs/re/pes-re-A5Judge.md` (table at line 40, J7 "penalty shoot-out kick: restart 6, reason 9"); `docs/re/pes-re-foul-choreography.md` §85 (INF(hi)). Period map `FUN_140a74d00` 1→0, 3→1, 5→2, 7→3, 9→4 (`pes-re-foul-choreography.md` §87), so per 9 is the shoot-out period. Period 10 is not in that map (OPEN). The probe at 10:52:56 to 10:52:58 (CONFIRMED live read, pread only) showed per 9, play 3, restart 6, reason 9, which then went to open play (play 1).

**Read times.** Every capture and state change is time-stamped in `captures_*/capture.log` (wall clock, `+s` from start). Probe: `probe_now.py`, 10:52:56 to 10:52:58.

**Open play: not captured.** Every open-play attempt was rejected (`headers_same=False` under the 16-byte gate; then the frozen frame under the vtable gate). The watcher (`captures_h`) is stopped; its only accept is the t001 duplicate.

## 2. Argument roots (item 3): typed with evidence, partial

`tools/dev/translate/argroots.py` (committed) scans the unpacked image for direct E8/E9 call sites into each impure VA: 242 sites over 50 specs (CONFIRMED, byte scan). It labels the argument registers rcx, rdx, r8, r9 at each site from the 40 instructions before the call (capstone), and propagates through 64-bit pass-throughs of the enclosing spec's own entry alias. The accessor classes come from `docs/re/pes-re-player-record.md` §1.1: `0x140A48A30` team, `0x140A489D0` side, `0x140A48910` anime, `0x140A48A10` man, `0x140A489F0` slot (CONFIRMED by the accessor VAs and their row bases). Conflicts: 0.

Typed (15 root slots over 15 specs; `impure_roots.json`, evidence in `impure_roots_evidence.json`):
- Seed classes from RealCases (INF, not re-derived here): A0 of 1404F47A0, 1404F6C30, 1404F8C70, 1404FA720 = team; A0 of 14064DE60, 140978E70 = man.
- Accessor result or pass-through: A0 of 1404FB2A0, 1404FE9C0, 1404FEDC0, 14057BA50 = team (pass of 1404F6C30.A0, `mov rcx, rdi`); A1 of 140503430 = team (pass of 1404FEDC0.A0, `mov rdx, rdi`); A0 of 1406486D0, 14064DC50, 14065C2A0 = man (pass of 14064DE60.A0); A3 of 140654E60 = man (pass of 14064DE60.A0, `mov r9, rbx`).

Not typed (35 specs): the call sites are in functions outside the impure set, or the register holds a field of an unknown record (e.g. `fld:r14+0x460`). The approximation "enclosing function = nearest spec VA below the site, within 0x8000" is recorded in the evidence.

## 3. Union measurement (item 4)

**Banks.** `seed.py --live` over the 5 captures, `--bank-classes impure_roots.json`, `--cap 24`: 5 instances per capture for each spec (25 total). The seed now resolves `IMG.<X>` roots from image global X's value (the regions in the specs, e.g. `IMG.1437F4808.0`, were previously zero). Smoke test before the full run: `driver.py --seed-bank ... --perturb --only 1405055E0 1404F8C70`: 2 of 2 bit-exact (82 and 128 cases).

**Run.** `driver.py --image pes_full.bin --seed-bank banks_union --perturb --record results_impure.json --jobs 2 --timeout 900`, 513 s wall, rc 0. Attempted 50, translated 46, diffed 13. Record: `tools/dev/translate/results_impure.json` (committed). The four specs not translated are "unmodellable callee": 1404F6C30, 1404FEDC0, 14080EBA0, 140975710.

**Results (own-block coverage of the spec's entry VA, from the record).**

| measure | main (1cbe5ac5 record) | union (5 captures, real + perturb) |
|---|---|---|
| specs with coverage | 46 | 46 |
| bit-exact specs (every case) | 8 | 13 |
| floor: coverage ≥ 0.50 | 9 | 14 |
| owner's floor: ≥ 0.50 AND bit-exact | 8 | **13** |
| median own coverage | 0.067 | 0.288 |
| mean own coverage | 0.218 | 0.336 |

No previously bit-exact spec was lost (the eight on main still diff).

**Single-snapshot comparison (RealCases, cited, not re-run here).** Floor ≥ 0.50 over 50 specs: g (G seed) 16, p (real + perturb) 13. Median: g 0.207, p 0.272. Like-for-like (both real + perturb): p 13 floor / median 0.272 versus the union's 14 / 0.288. The union lifts the floor by one and the median by 0.016. That is essentially flat, not a floor lift. The union has no open-play state (§1), and only 15 specs are typed, so the union adds little over one snapshot.

**Caveats.**
- 33 of the 46 specs with coverage show `harness cannot run` (translated side not diffed). The RealCases F3 pattern (small integers read as pointers) is the likely cause, not checked here. 14051CF70 is at the floor (0.70) but does not diff.
- Coverage fell for some specs against main: 14065DAB0 0.771 → 0.533, 1404FE9C0 0.214 → 0.146, 14097FCE0 0.204 → 0.143, 140978E70 0.090 → 0.078. The main record was unseeded; I did not investigate the drops.
- The vtable gate does not compare class bytes. The objects' identity rests on the vtable alone.

## 4. Unfinished (numbered)

1. Open-play captures: none. Needs the game to resume (frame advancing). Then re-run `capture_midplay.py --settle 1e9 --max-caps 30` (per-event stoppage triggers, open play every 15 s), re-build the banks over all accepted captures, and re-run the driver to update the record.
2. Per-capture coverage: not measured. One driver run per single-capture bank (about 8.5 min each for 50 specs); take `coverage[va].fraction` per run.
3. Typing for the 35 untyped specs (§2).
4. `harness cannot run` on 33 specs: the unmodelled-read failures are not addressed here.
5. The duplicate kick-off (t046 and captures_g/t001) should be dropped from future banks.
6. Penalty shoot-out kicks (restart 6, reason 9) are captured by the per-event stoppage trigger. Further shoot-out kicks will arrive during any future shoot-out, with no code change.

## 5. Reproduce

- Capture: `sudo -n python3 tools/dev/translate/capture_midplay.py --out <scratch>/captures_x --settle 1e9 --max-caps 30 --dur 3600` (`--settle 1e9` turns off the match and period triggers; a `FORCE` file in the output dir captures the current state once).
- Typing: `PES_EXE=<pes_full.bin> python3 tools/dev/translate/argroots.py`.
- Banks: `python3 tools/dev/translate/seed.py --live <dirs> --spec-dir tools/dev/diffharness/specs/impure --bank-classes tools/dev/translate/impure_roots.json --out <dir> --cap 24`.
- Driver: `env PES_EXE=<pes_full.bin> GF_DIFF_PYTHON=<venv python> python3 tools/dev/translate/driver.py --image <pes_full.bin> --out-dir <dir> --spec-dir tools/dev/diffharness/specs/impure --seed-bank <banks> --perturb --record tools/dev/translate/results_impure.json --jobs 2 --timeout 900`.

Tests: none new. The tooling is a harness tool, not engine code. The driver smoke above is the only verification run; the gate was not run (agents do not run the gate).


## Main's reading of the union record

Of the 50 impure functions, 38 are bit-exact on every case they ran (13 at the floor, 25 below it at a median
own-block coverage of 14%); 8 have an empty corpus; 4 have an unmodellable callee. So where impure code runs, the
translation agrees with PES (38 of the 42 that ran any case). The open problem is purely reaching the code: the five
stoppage captures did not lift the floor (13 vs 13 single-snapshot). Open play, where most match logic runs, is still
uncaptured.
