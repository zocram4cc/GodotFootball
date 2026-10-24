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

## Constraints (unchanged)

- No PES-derived bytes committed; everything regenerates from the user's PES21 by script.
- Constants script-read from the image, never retyped or tuned; no magic numbers.
- Clean cutovers; GF mode untouched by PES-path work; the gate green.
- In-flight work finishes: batch Q, the throw-in batch and AuditFixes land as planned.

## Reporting

- Sitreps lead with main's SHA, the gate result and the smoke hashes, then the ledger's per-subsystem counts.
- After the pilot: the measured rate, the projection, and the decision-rule outcome.
- Say plainly if the numbers say completion is still out of reach.
