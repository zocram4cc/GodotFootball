# PES tackle sub-kind (ContactPort stage C)

Scope: `docs/re/pes-re-contact-port.md` pass 4 item C. Owner: TackleKind.
Commits on branch `tackle-kind` (main has piece 1 as `8ee7949`):
- `227a41f8` pestackle: the pick as a pure layer (merged).
- `2b1b4e38` the standing tackle plays the sub-kind's clip (not merged).

## Done

### 1. The pipeline, decoded end to end

Slot 13 `FUN_1407902c0` → `FUN_140791870` calls `FUN_14078e5a0` (readiness mask)
and `FUN_14078f4f0` (speed class), then `FUN_14078b550` (the pick):

```
for kind 1..8:
  if (mask >> kind) & 1:                       FUN_14078e5a0's bit k = kind k
    bits = FUN_14078f290(...)                  the kind's clip mask
    for each (row, bit) of kind's group table (FUN_14078fe90):
      if (bits >> bit) & 1 and FUN_14078a640(row):   sub-kind = kind; done
nothing: sub-kind = 7
```

**`FUN_14078e5a0`, bit k ⇒ kind k, verified 1:1** against every `OR EDI,imm` in
the function (`0x14078ebf1`/`0x2`, `0x14078ec0b` and `0x14078ec5f` and
`0x14078ec95`/`0x4`, `0x14078ec18`+`0x14078ecb8`/`0x8`, `0x14078eda1`/`0x10`,
`0x14078eb77`+`0x14078ec27`/`0x60`, `0x14078eb6d`/`0x80`, `0x14078e5f6`/`0x100`):

| bit | kind | gate |
|---|---|---|
| 0x2 | 1 | may play (param_7) + the post-tackle cooldown at think+0x2c clear |
| 0x4 | 2 | touching (param_4) + FUN_140477130's opposed body inside 5 m |
| 0x8 | 3 | the second-man flag + think+0x60 clear (no other tackle committed) |
| 0x10 | 4 | pair past 1.44 m² (0.09 m² when the ball is lifted past 0.434744), three angles under 135°/36°/36°, and the frame-delay roll |
| 0x60 | 5, 6 | the same touching gate, set as one pair |
| 0x80 | 7 | the fallback: neither contact flag live |
| 0x100 | 8 | the "not in play" early-out |

**`FUN_14078fe90`** = `DEC ECX; CMP ECX,7; JA default` then a jump table at
`0x14078ff4c` of eight `MOV EAX,ECX; LEA RCX,[RIP+disp32]` cases (kind 7's case
builds its address arithmetically instead). Slot = `FUN_14078f4f0`'s speed class
**0..2, not 0..3** — the third class output is a flag, not a class; verified from
its `SETBE` chains. Kinds 4/5/6 carry **two** tables per slot (stride 0x20,
second at +0x10); kind 7 is indexed `(param_2 + slot*2)` where param_2 is
standing vs in-contact.

**`FUN_14078f290`**, the clip mask: every bit for kinds 5 and 6 (gated off
wholesale in the exe) and for a record that is not live; otherwise the standing
flag plus one of three branches — keeper's hands (+4, +2 with it when the ball is
in front), the record's own ahead flag (+2, +4 with it), or committed (+2).

### 2. The row-id discovery (the key)

The group tables' ids are **Animetable row ids, not motion-table indices**.
Reading them as motions is what left the per-kind clip families "unnamed" in
pass 3 (they come out as kick clips). As rows they resolve through the install's
own Animetable to **`tacklefoot_*`** (rows `0x96b`–`0x9a4`), plus the neighbouring
families the same tables reach:

```
tacklefoot_*        tacklefoot_near_0_0_000 / _intercept, near_3_0_000,
                    mid_0_0_000 / _090_block / _L_dodge / _R_dodge,
                    mid_3_0_000 / _090, far_0_0_000 / _3_0_000,
                    parallel_3_2 / _3_3, side_mid_*, front_blockcut_*
tacklehold_*        the hold family (waist_*, shoulder_*, 0_3)
tacklepulled_*      pulled_3_3_000
tackleshoulder_*    the shoulder family (3_0/3_3 interrupt, weakly, grab_arm)
block090_3_0_y00_000_far_L      row 0x980's clip
js_idle_0_0_000_y00_tacklefoot_passcut[_side|_v2|_v3]   rows 0x9a1–0x9a4
```

35 distinct clips across the 8 kinds, **86 install clips in the family, all 86
converting** (verified with the real rig: `missing=0 parsefail=0 convfail=0`).
`pesclips.cpp` carries all six prefixes as `PesClipFamily::kTackle = 101`.

### 3. The constants, all opcode-checked

Read through their referencing instructions, so a different exe fails the load
rather than gating on wrong numbers:
- the eight kind-case `LEA RCX,[RIP+disp32]`, preceded by `MOV EAX,ECX`;
- `DEC ECX; CMP ECX,7; JA` head and `MOV [RCX+0x44],imm32` (the default kind,
  read as an immediate, not assumed);
- the eleven `FUN_14078e5a0` RIP-relative loads (`MOVSS`/`MULSS`/`COMISS`, with
  the two REX-prefixed ones): 5 m, 1.44 m², 0.09 m², 0.434744, 135°, 36°,
  6 s, 60 fps, 0.15 s, 0.125 s, 0.5 s.

## The solver seam — named, not fabricated

`FUN_14078a640` runs `FUN_140789f70` → `FUN_1406ecbb0`: it steps a **physics
prediction of the tackle capsule** (`param_1+0x20`, `+0x2c`) over the clip's own
contact record and tests the reach against the carrier's predicted position. GF
carries no tackle capsule, no clip contact window and no physics environment to
predict through. Bit 4's three angles likewise read a resolved contact point only
the solver produces (`FUN_140439e50` over the clip's hit position), and its delay
roll draws on PES's per-player RNG streams.

So the port covers the **readiness mask, the kind order, the per-kind clip mask,
the group tables and the row → clip chain** — all real — and leaves the per-clip
fit as `PickSubKind`'s `solverOk` argument, with bit 4's angles and delay roll as
caller-supplied inputs. Nothing is invented.

## The wiring question, plainly

**On main (`8ee7949`, piece 1 merged) the pick is computed but NOT driving any
clip.** `SetPesChallenge(kTackle)` carries the action class only; nothing calls
`PickSubKind`/`ChooseTackle` from the match, and `oppStandingTackleKind1` is still
the hardcoded `false`. It is a pure, tested layer with no in-match effect.

**Piece 2 (`2b1b4e38`, on `tackle-kind`, not merged) does drive it, and it fires.**
- `_TackleCommand` issues the Special command naming `(kTackle, motionNo)`, the
  way `_DodgeCommand` does, and the player carries the sub-kind via
  `SetPesTackleSubKind`.
- `Match::ProcessPesContacts` reads it for cause 7's gate
  (`oppStandingTackleKind1`), replacing the hardcoded `false`.
- `MatchData` counts them and the report prints `[tackle-kind]`.
- Seed-7 20-min smoke: `[tackle-kind] k7 270-135`, `tackles 0-0 | slides 18-11`.
  The kind is picked and its clip is played; the readiness mask lands on the
  neither-flag fallback in this match's tackles, which is itself a finding (see
  below).

What it would take to drive it **honestly** on main: merge piece 2, and supply
`solverOk`'s stand-in. The lazy, non-faking stand-in is the one `ChooseTackle`
already takes — answer every kind "solvable", so a ready kind's first entry is
played — which is a coarser answer than PES's reach test but uses the real
gates, order, family and bearing. Tightening it further needs a GF capsule the
engine does not have.

## Measured, seed 7, teams 1 v 2, 20 min

| | base `f9260dab` | piece 1 only (main `8ee7949`) | piece 1+2 (`2b1b4e38`) |
|---|---|---|---|
| challenges | tackles 2-0, slides 18-13 | tackles 2-1, slides 13-11 | tackles 0-0, slides 18-11 |
| fouls | 23-16 | 15-12 | 23-16 |
| yellow | 0-0 | 0-1 | 1-0 |
| dodge ball | 4648-6125 | 5796-7454 | 5326-7696 |
| dodge net | 556-622 | 2064-1007 | 1338-1365 |
| dodge post | 1325-1046 | 1106-782 | 1110-671 |
| dodge human | 1287-1351 | 1549-1596 | 1498-1482 |
| stalemate forced | 17588 | 20395 | 13662 |
| tackle-kind | — | (not wired) | k7 270-135 |

`tackles 2-0 → 0-0` under piece 2 is the wired path doing what it should: a
committed tackle whose clip names a sub-kind is no longer double-counted as a
separate challenge by `Referee::ContactEvent`'s `AddChallenge` (same player,
same challenge key). The `fouls`/`dodge` deltas are the same magnitude as the
base-vs-piece-1 column, which is main's six in-between commits, not stage C.

**The `k7 270-135` finding.** Every tackle the smoke ran picked kind 7, the
neither-contact-flag fallback. That is PES's own answer for a tackle where
neither the touching nor the second-man flag is live, and GF's tackles do
commence from a running approach rather than an engaged body — so this is
plausibly correct, but it means **the smoke does not yet demonstrate a non-7
sub-kind playing.** The install tests do: every kind 1..8 resolves to a
`tacklefoot`-family clip, and the synthetic tests exercise kinds 1, 4, 5 and 7.
A non-7 sub-kind firing in a match is the open measurement.

## Verification

- **Gate: 658/658, 100%, standing skips 59 and 147** (VfsInstall.NestedOriginals,
  EditSaveInstall.DecryptsAndMatchesTheRunningGame). Main reports 659/659 on the
  merged piece.
- `pes_tackle_test`: 21/21 (12 synthetic, 2 install).
- `pes_clips_test`: 4/4 (the dodge family, the tackle family, a named tackle
  clip's Special query).
- `pes_dodge_test`: 11/11 (unchanged).
- **Mutations killed:** bit 1's `0x2 → 0x4`; the pick's readiness test inverted;
  the mirror bit dropped; the keeper-hands branch's condition; the committed
  branch's `+2`.
- `tools/check_pii.sh` clean before each commit.

## Not done / next

1. **Piece 2 is not on main.** Merging `2b1b4e38` is what turns the pure layer
   into a played clip.
2. **A non-7 sub-kind has not been measured in a live match.** Its gates (the
   touching / second-man / bit-4 windows) never all held in the seed-7 smoke.
   Next: instrument `[tackle-kind]` per gate, or run seeds 11 and 3, and see
   whether kinds 1/4/5 ever win.
3. **`solverOk` is answered optimistically.** The honest stand-in is in place and
   named; a real fit needs a GF tackle capsule (reach + clip contact window).
4. **`FUN_14078f6c0`'s frame count and the delay roll's per-player draw** are
   decoded but not ported: they feed the action's timing, not the sub-kind.
