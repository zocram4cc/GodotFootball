# PES21 attack level (mentality) −1: how forwards are pulled back

Source: an outside RE agent (Q&A, 20-10-26), re-derived on the vanilla IDB. Audited by Main against the image.

> **Main audit 20-10-26:**
> - The gauge strings at `0x1425C3B64` read `@----`, `-@---`, `--@--`, `---@-`, `----@` (8-byte stride): match.
> - The team-setting store `0x1404F86E7` is `mov [rcx+0xB8A4],eax`: match.
> - The depth factor `0x140682BE4` is `mulss xmm7,[0x142592228]` = 0.25: match.
> - The style pull `0x1405E7AE3` is `mulss xmm0,[0x14259BDF4]` = 8.0: match.
> - The CF-role exemption `0x1405E7B25` is `cmp edi,9; je`: match.
> - `0x14259222C` = 5.0 (the window; the add at `0x1405E7B39` is `addss xmm6,xmm9`, i.e. from a register loaded earlier): consistent.
> - The rank multipliers: `0x1406832D9` loads `0x14259D5D0` (0.65) before the `mulss` at `0x1406832E1`: match for 0.65 (0.2 not traced).
> - **Correction:** the forwards' stamped score at `0x140500693` is `movl $0xC2C80000` = **−100.0**, not −88.0 (0xC2B00000).

## 1. The dial
The attack level is the byte at TeamData+0xB8A4, scale 0..4 (0 = −2, 4 = +2); −1 is byte 1 (`-@---`). The gauge formatter (snprintf switch at 0x1404F8761) prints the strings straight from that byte. D-pad command 0x6C steps it, clamped 0..4 (0x1404F8493/B6); the team-setting path stores it at 0x1404F86E7.

## 2. The defensive-block pull (sub_140682770)
Players are classed from the role array; 7/8/9 = forwards (switch at 0x140682D61). The forward line's depth target:

    depth = deep + (high − deep) · clamp(level, 0, 4) · 0.25

(the factor at 0x140682BE4). At −1 the factor is 0.25: the line stands 75% of the way to the all-out-defensive depth; at −2 it is 0.0, a full drop.

Exactly one forward per tick is protected:
- a Counter Target holder takes it at once (CheckSpecialCondition id 15, break at 0x14068303F);
- otherwise, with 2+ live forwards, the loop keep-replaces at 0x140683095 under a guard `v73 >= −100000.0` that only NaN fails, so **the last live forward in slot order wins**.

Protection is the floor fmax(−ref, target) at 0x140683367, re-applied after the cross-line blend at 0x1406837B6, active only when level != 0. So at −1 in a front two, the later-listed CF is held and the earlier-listed one drops. With 3+ forwards the unprotected ones can drop harder by rank tests (×0.65 or ×0.2, 0x1406832D3/E1). A Counter Target forces factor 1.0, fully high (0x14068330E). No RNG in the forward selection (the only dice in this function are in the midfield spread).

## 3. The style layer (master positioning, 0x1405E7AD1)
```
1405E7ACD  test al, al            ; skip flag
1405E7ACF  jnz  skip
1405E7AD1  cmp [rbp+var_3C4], 0   ; gate flag
1405E7AD5  jz   skip
1405E7AE3  mulss xmm0, [0x14259BDF4]  ; 8.0
1405E7AEE  addss xmm8, xmm0       ; depth += 8 m toward own goal
1405E7AFA  call sub_140A63680      ; designated-CF gate
1405E7B01  jnz  skip
1405E7B0B  edx=2; call CheckStyleAtPosition
1405E7B1A  jnz  skip               ; FOX_IN_THE_BOX exempt
1405E7B25  cmp edi, 9 ; jz skip    ; CF-role exempt
1405E7B39  addss xmm6, (5.0)       ; +5 m window
```
The +8 m applies to every forward past the two gate flags. The extra 5 m window hits only forwards that are not designated, not FOX_IN_THE_BOX, not CF-role, and have byte +0x80D4 clear (semantics unpinned). The `edi == 9` test is the array-B role value 9 = CF (edi = [r15 + slot·4 + 0x3FB8]), not a playing style.

Style attribution: CheckStyleAtPosition (0x140A64030) → CheckStyleBitmask (0x1408CB0B0) tests bit N of the style dword at player+0x48 AND PositionCompatibilityTable[role] at 0x142613D20. The GK row carries bits 19/20 (OFFENSIVE_GK, CLASSICAL_GK) and the CF row bits 0..3 (poacher, dummy, Fox in the Box, post), in the 0x1434A10D0 mask-table order, so bit 2 = FOX_IN_THE_BOX. The designated-CF gate sub_140A63680 fires only with an instruction record id 2 armed; its CF-pair logic picks via the count byte +0x87CC, the pair byte and the saved-lineup depth records (+0x46E4), deterministic per lineup.

## 4. The attack side (sub_140500200)
Mentality also trims who may join attacks (the permit mask at TeamData+0xB8B8):
- non-forward quota: level 1 → 2, 2 → 4, 3 → 6, 4 → everyone (the ladder at 0x1405002F4). At −1 only the top 2 non-forwards by opponent-threat score (minus a ball-distance falloff, minus 3 on the designated man's strong side) keep attack duty;
- forwards are force-permitted at every level, stamped score −100.0 (Main-corrected; 0x140500693) so they never compete on score;
- at level 0 (−2): quota 0, and the level <= 0 flag at +0xB8A3 also blocks non-forward through-ball receivers.
The dial also steers the registry style state: 0/1 → 1, 2 → follows the level <= 0 flag, 3/4 → 0 (0x140501061-121). sub_14051E970 re-tunes the dial itself from the scoreline, the clock and dice.

## Plain-language summary
- The gauge is one number, 0 to 4; −1 is the second notch from the bottom.
- The front line's standing depth scales with gauge/4: at −1 they stand 75% of the way to the "everyone back" position.
- One attacker is always excused: the last forward on the team sheet, or the Counter Target, or the CF an id-2 instruction marks. In a strike pair, the man listed first drops.
- Cards bend it: Fox in the Box and CF-role slots skip the extra pull; a normal second striker listed ahead of his partner is the typical victim.
- Going forward at −1, only 2 non-strikers may join attacks; strikers always may.
- The dial itself moves during a match (scoreline, clock, dice). Who drops has no randomness: slot order and saved lineup data decide.

Open (the agent's): the +0x80D4 skip byte; the var_3C4 gate's meaning; a runtime watch to settle the floor's direction (inferred from construction, not live-proven).
