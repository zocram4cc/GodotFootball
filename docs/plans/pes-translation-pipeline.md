# Plan: the PES port becomes a translation pipeline, proven on one subsystem

Owner directive, 24-10-26. Supersedes the one-slice-per-agent hand-port method for all new work.

## Why

The ledger (`docs/re/LEDGER.md`) measured the hand-port method at about 5 functions ported and 11 decoded per
agent run. The in-scope universe is ~10,000 functions and growing (Ledger3 is adding pre-match, in-match menus
and presentation). At that rate completion needs 1,000+ runs, which is out of reach. The method changes; the goal
does not. PES fidelity stays imperative: bit-exact where the harness can prove it, evidence-backed everywhere else.

## Scope (owner rulings 24-10-26)

In scope: everything a player goes through to play a match.

- Pre-match: team / league select, side select, kit and settings before kick-off.
- The match: gameplay, ball, AI, referee, restarts, animation.
- Presentation: replay, broadcast / cameras, audio cues, crowd, scoreboard and overlays, cutscenes.
- In-match menus: pause, game plan / tactics, substitutions, the UI overlay.

Out of scope: edit mode, Master League and other career modes, save / load, online.

## The model

1. **Translate, don't hand-port.** A tool takes a PES function (Ghidra decompile plus disassembly) and emits C++:
   one function per PES function, named `pes_fn_<va>`, constants read from the image (VA-cited, named, never
   retyped), structs mapped onto GF state through one explicit field map per PES record type, in one table.
2. **The diff harness is the judge.** Every translated function runs against PES's own bytes in the unicorn
   harness (`tools/dev/diffharness/`) on generated inputs. It is `diffed` only when outputs match bit for bit.
   Corpora are generated at test time from the user's executable, never committed.
3. **Agents handle only failures:** functions the harness can't run (engine calls, GPU, live state, heavy heap
   graphs) and functions whose diff fails. These get the decode / port treatment, stand-ins named `// ponytail:`
   at their VAs.
4. **The ledger is the only progress measure.** Statuses gain `translated` (emitted, undiffed) and `diffed`
   (bit-exact). Progress is reported per subsystem from the ledger, never from plan items.

## The pilot (first; nothing else changes until it reports)

- **Choose one subsystem** of 100-300 functions, mostly pure computation, with existing GF callers. Use Ledger3's
  subsystem tags; justify the choice in one paragraph. Candidates: the mark table / marking, the lane and
  mentality step, team-tick steps, ball physics.
- **Build the pipeline:** the translator (decompile to C++, constants from the image); the struct field map for the
  subsystem's records; per-function harness specs (inputs, stubs for callees outside the pilot, outputs); a driver
  running translate, build and diff over the whole subtree and writing per-function results into the ledger.
- **Plug it in:** the translated subtree replaces GF's hand ports of the same functions (clean cutover, no second
  copy). GF mode unchanged (gate on `PesLoop()`). Gate green. 5-min and 20-min repros reported with causes.
- **Measure and report:** attempted / translated / diffed bit-exact / failed, each failure classified (engine call,
  live state, GPU, translator bug, struct map gap); agent runs and tool requests spent; projected runs for the whole
  in-scope ledger at the measured rate, assumptions stated.

### Staffing

The next free slots: one `football-reverse-engineer` on the struct map and the harness specs, one `task` agent on
the translator and the driver, split by file. Main lands everything. Concurrency stays at 4 agents total.

## Decision rule (from the pilot's numbers, not argument)

| Pilot diffed bit-exact | Action |
|---|---|
| >= 70% | Roll out subsystem by subsystem, ordered by what the player sees on screen first. |
| 30-70% | Fix the top failure classes in the translator and struct map, then re-pilot. |
| < 30% | Report why with the failure breakdown and propose the alternative. Never fall back to the old method silently. |

## Owner ruling 26-10-26 (latest): correctness is I/O bit-exact on real calls; coverage is a confidence measure

**What matters is gameplay outcome, code readability and a faithful recreation.**

- A function, pure or impure, is **proven** when every case it is run on is input/output bit-exact: the same return
  value and every written byte, from the same inputs and memory. That holds whatever its coverage.
- Own-block coverage is **recorded and reported** per function and per subsystem, but **never gates** a function out of
  the record or the committed layer. The 50% floor is dropped everywhere (driver, records, `gen_translated.py`, gate
  tests).
- Real-call cases (snapshots plus call-time deltas) are preferred over synthetic ones. Coverage is raised over time with
  more snapshots, and low-coverage functions are reported so they get attention.

This supersedes the 50% coverage floor and the "union of captures" rule below.

## Owner ruling 26-10-26: impure code is translated now; emulation continues in parallel

Impure code is translated and committed under **tracked coverage**: a function counts when every case it runs is
bit-exact against PES, with its coverage over the union of captures reported (not gated) and raised over time. In
parallel, one RE agent works on running PES's own match loop in the emulator from complete captures, so functions are
exercised as the game calls them; as coverage rises, functions move to the gated record.

## Owner ruling 25-10-26: impure code is judged on the union of captures

A function counts as bit-exact when every case agrees with PES and its own-block coverage, **counted across all
captured match situations together**, reaches the 50% floor. Each capture (a mid-play snapshot of the running game at a
different moment: kick-off, open play, set pieces, and so on) adds cases; coverage accumulates over captures. Captures
are taken mid-play, never at a match's end, where records are already torn down.

## Owner ruling 25-10-26: the translated code is committed, and readable

- **Commit the translated layer.** Translated C++ and the constants baked into it are committed to the repo. Builds no
  longer need a dump or a translation step; a user builds from the repo, with assets still coming from their own install
  through the import scripts. This replaces the "no PES-derived bytes" rule **for code only**: assets, dumps, corpora,
  snapshots and seeds stay out of the repo.
- **The dump becomes a developer tool,** needed only by the harness and the gate to keep proving the committed code
  equals PES.
- **Drift check in the gate:** regenerating from the image must reproduce the committed code; once a function is
  hand-refined, the harness must keep proving it bit-exact.
- **Readable, in stages** (each stage must keep every function bit-exact in the harness):
  1. structure: real signatures instead of a register file, locals instead of stack slots, `if`/`while` recovered from
     gotos, records accessed through named fields from the field map;
  2. names: functions, fields and constants named from the RE docs and the ledger, in one table the translator reads;
  3. idiom: flags folded into comparisons, float ops as expressions, vtable calls as virtual calls on recovered classes;
  4. hand refinement of hot or interesting functions, proven bit-exact, which then becomes the committed version.

## Owner ruling 25-10-26: priorities within scope

- **Out:** match commentary (the commentator audio system).
- **Menus** (in-match and pre-match): nice-to-have; a reimplementation from PES's own data (layouts, strings, tables)
  is acceptable instead of translating the menu code.
- **Priority:** presentation and cutscenes (replay, broadcast cameras, cutscenes, crowd, scoreboard and overlays,
  audio cues other than commentary), alongside the gameplay core.

## Owner ruling 24-10-26: ship translated code; the harness stays the oracle

PES 2021 is off sale with no forward path for its mods; reading the user's own running copy for interoperability is in
scope. Translated code is generated at build time from an image the user's install produces, and the diff harness
stays the correctness oracle in the gate. GF's hand port remains the fallback only where no image is present.

The image is `pes_full.bin`: a read-only dump of the running game's image range (`/proc/<pid>/mem`, opened O_RDONLY;
offset = VA - 0x140000000), not a decryption of the file on disk. It becomes one script in `tools/pes21_import/`, with:

- a defined dump moment, or specs that never read the writable `.data` pages (their content depends on when the dump
  was taken);
- a committed SHA-256 of the read-only and code sections (a hash, not PES bytes), so the gate can check a user's dump
  is the image the specs were authored against.

## Owner ruling 24-10-26: GF's state takes PES's record layout; the end state is PES itself

Record functions (a PES record pointer in a register, fields at PES offsets) are the bulk of the work. GF's PES-path state
is laid out exactly as PES lays out its records (team T, player slots, MI, MA, then the rest), record by record, so
translated functions read GF's own memory and switching over is mechanical. No marshalling shims.

The end state of this engine: **no sign of GF is left; what remains is a clean, modifiable reproduction of PES 2021.**
GF code is scaffolding to be replaced, never a constraint on the reproduction.

## Constraints (unchanged)

- No PES-derived bytes committed; everything regenerates from the user's PES21 by script.
- Constants script-read from the image, never retyped or tuned; no magic numbers.
- Clean cutovers; GF mode untouched by PES-path work; the gate green.
- In-flight work finishes: batch Q, the throw-in batch and AuditFixes land as planned.

## Reporting

- Sitreps lead with main's SHA, the gate result and the smoke hashes, then the ledger's per-subsystem counts.
- After the pilot: the measured rate, the projection, and the decision-rule outcome.
- Say plainly if the numbers say completion is still out of reach.
