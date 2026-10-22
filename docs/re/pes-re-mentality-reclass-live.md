# PES mentality dial: line reclassification and the two defensive blocks (live, 22-10-26)

> **Source and status.** Findings from the owner's separate RE agent, relayed by the owner on 22-10-26, from live captures
> (Cheat Engine breakpoints on the two block call sites, game unpatched, coach-vs-coach sweeps of the dial on both teams).
> Not yet re-verified in this repo; the watcher script that produced them is in the owner's tree, not on this main.
> They supersede parts of `docs/plans/pes-plan-mentality.md` (v3), listed in §5.

## 1. The dial

- The mentality dial is a byte at `[[unitctx+16]+0xB8A4]`, values 0..4 = −2..+2, one per team, moving independently.
  The gauge strings `@----` .. `----@` are at `0x1425C3B64`. One D-pad press steps it by one through command `0x6C`
  (`sub_1404F8350`). The auto-adjuster `sub_14051E970` moved a team 2 → 1 → 0 mid-match unprompted.

## 2. The dial's main output is line reclassification, not depth scaling

The per-slot class records at `planObj+0x7B88+8*slot` (class dword; the rank byte at `+0x7B8C+8*slot`) are rewritten
as the dial moves. The role array (`+0x4C00`) never moved.

| Team (formation) | Block | −1 | −2 | +2 |
|---|---|---|---|---|
| 2 CF (away) | A: `sub_140682770` (via `0x140682198`) | one CF class 2 → 1 (folded into the midfield line) | both CFs folded | a fullback promoted to class 2 |
| 1 CF + 2 SS (home) | B: `sub_14067B640` (via `0x1405ECC2A`) | one SS folded | both SS folded; **the CF never folds at any setting** | a fullback promoted to class 2 |

Every fold reverses fully at neutral. Both blocks run every think tick, roughly 1:1 with the think pass, one per team.

## 3. The rank tests

The x0.65 / x0.20 rank drops in block A are real code, but **rank is not team-sheet order**: live ranks ran
s10 k0 / s9 k1, the reverse of slot order, stable across matches (data-ordered). Any selection story built on sheet
order is dead. The out-of-possession analysis (latch `+0x255` = 0: the protected forward is the last live forward in
engine-slot order, a counter target overrides, active count ≤ 1 stops it) describes block A only; in possession the
machinery switches record family, and mode 1 becomes a pitch-position Y-sort.

## 4. Block B

Block B has its own pipeline: a +3/+4 ceiling clamp (all 11 targets capped at `dir*(min(anchor, ballX) − 2.2)`) and
the CF shield. Its fold-picking rule is in the list machine `sub_14067C4B0` family (729 lines; the fold writes go
through computed bases).

## 5. What this changes in `docs/plans/pes-plan-mentality.md` (v3)

- The plan models the dial as depth scaling of the forward line plus a protected forward. The primary mechanism is
  the class reclassification in §2; the depth terms act on the reclassified lines.
- The plan's block choice is the team-tick flag F (`local_458+0x10C`; F ≠ 0 → `FUN_14067B640` at `0x1405ECC2A`,
  else `682770`). The live run saw one block per team. Both can be true (the two teams carried different F); this is
  the first thing to reconcile.
- The "first-listed / last-listed" sheet language for the rank tests is withdrawn (§3).

## 6. Open

1. Block B's fold-picking rule (`sub_14067C4B0` family), and why it shields the CF while block A folds a CF at −1.
2. Whether a team's block is fixed or flips across matches (reconcile with flag F).
3. The writer of the +2 fullback promotion.
4. The flag byte at TeamData+0x255: whether "ball-side extreme" means list rank or a pitch-position sort, and which
   formations set it.
5. The discriminator log the owner's agent proposed: per think pass, the dial (`+0xB8A4`), the latch (`+0x255`), and
   per slot the role (`+0x4C00`), class (`+0x7B88+8*gidx`) and rank (`+0x7B8C+8*gidx`), next to player names.
