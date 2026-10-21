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

## 5. Two strikers dropping equally (external follow-up, 20-10-26)
Question: in a two-striker team (one Fox in the Box CF, one Goal Poacher SS), both drop equally on their line. Why, and is the Goal Poacher partner the reason?

> **Main audit 20-10-26:**
> - `0x1406831E6` `imul rax,r8,0x68` / `0x1406831EE` `movzx r15d,byte [rax+r14+0x58]` (the count byte at linestruct + 104·class + 88): match.
> - `0x140682477` `inc byte [r8+0x58]` / `0x14068247F` `inc byte [r8+0x59]`: match.
> - `0x140683272` `cmp r10d,2`; `0x140683285` `cmp r15b,r10b`; `0x140683288` `jbe 0x1406832E7`; `0x14068328A` `cmp r13d,esi`; `0x14068328D` `je 0x1406832E7`: match.
> - `0x14068330E` loads 1.0 (`0x14256AE1C`); `0x140683350` `je 0x140683561` (level 0: no floor); `0x14068335A` `movss xmm0,[rax+0x44]`; `0x140683363` `maxss xmm0,xmm7`: match.
> - The agent's own citation fix: the count compare is the pair `0x140683285`/`0x140683288`; `0x14068328D` is the designated-player test.

**Equal drops are the expected behaviour for a two-forward shape:**
1. **With exactly two forwards both get the same target.** sub_140682320 (called at 0x14068210B) walks the 11 slots, skips role 0 and off-pitch players, classes roles 7/8/9 as forwards and increments count (+0x58) and active (+0x59). A CF (9) and an SS (7) give count = 2. The rank block needs count >= 3 (`cmp r15b, r10b; jbe` with r10b = 2 proven by the class gate) and "not designated". Even with 3+ forwards the ×0.2 / ×0.65 multipliers also need the mode-0 rank test sub_1406405C0 to pass and level < 4. So both strikers compute target = deep + (high − deep)·factor from the same class-2 anchors and the same factor.
2. **The one asymmetry is conditional.** The floor fmax(−ref, target) (0x140683363, re-applied at 0x1406837B6) applies to the designated forward v21 only, and bites only when the raw target is deeper than the reference ([unitctx+0]+0x44, a half-length-like pitch value; inferred from construction, not runtime-proven). If the −1 target sits above that bound, the floor is slack and both ride the identical line.
3. **At level 0 (−2) the floor is switched off** (`test bl,bl; je` at 0x140683350). The dial is live-tuned by sub_14051E970 (scoreline, clock, dice), so a team set to −1 can be sitting at −2 mid-match, which alone gives the observation.
4. **The style layer never differentiates depth.** The +8 m pull applies to every forward past the two gate flags; the Fox in the Box, CF-role and designated exemptions only skip the 5 m window widening, a clamp half-width (target ± xmm6 from 0x1405E7B49), not a depth target.

**The Goal Poacher partner reason is dead, at three levels:**
1. **Composition.** The strike-partner group (producer sub_14053BB90) takes anchors with the GOAL_POACHER or DUMMY_RUNNER card (record +0x8828 dword == 2, |y| <= 8) and partners with style 9 = FREE_ROAMING (Hole Player), |y| <= 12, |dy| <= 15. A Fox in the Box CF can't be the partner (wrong card), and no SS or CF can pass the partner test at all: CheckStyleAtPosition needs the style bit in PositionCompatibilityTable[role], and the SS row (7) and CF row (9) don't carry bit 9. Only a role-5/6 midfielder with the Hole Player card fills the partner slot. (The agent's earlier "Goal Poacher partners" label was a mask/registry conflation.)
2. **Consumption.** The group block (counts +0xB8CC; 8-byte {mask, member} records at +0xB8F8, 5 per group, rebuilt every think tick) has no displacement readers across 0x1404F0000-0x140700000; the only +0xB8D8 hits are a different field. It looks write-only (a displacement search can't exclude computed-base readers; a runtime read-watch on +0xB8F8 would close it).
3. **Even if read, nothing in the pull chain consults it:** sub_140682770 and the master pull branch sweep clean.

**Correction to §3:** the `cmp edi, 9` exemption reads the position-family array at raw TeamData +0x3FB8 (not array B at +0x4C00); r15 in the master is raw TeamData (the +0x80A0 directive word and the +0x80E0 stride-2 phase copies ride the same register), and sub_140682770 reads the same array at 0x1406834DE. Value 9 = CF stands. The +0x80D4 skip byte is in the per-slot byte row 0x80D4-0x80DE.

**The v21 selection loop** reads the same two bytes: active count (+297) > 1 at 0x140683049 (else take and break); count (+296) > 2 at 0x140683064 gates its −10 rank bump. With two forwards the selection score is the lineup feed (+30 if role 9 = CF), then keep-replace decides by slot order.

**To split the two explanations live:** one read of TeamData+0xB8A4 at the moment of the equal drop: byte 1 = −1 with a slack floor; byte 0 = −2 with the floor off.

## 6. Live check (S1Watch round 34, 20-10-26)
The owner moved his managed team's dial by hand (−1, then −2); S1Watch logged TeamData+0xB8A4 and the forwards' depths for 2224 frames.
- **The dial doesn't move on its own:** the home byte changed only at the owner's inputs (−1 → −2 at frame 1573942); the COM team's byte stayed at 2 in all 2145 frames. The retune claim (sub_14051E970 from scoreline, clock and dice) found no support in this window.
- **This lineup had three forwards** (one CF role 9, two SS role 7), so the rank block was live (count >= 3). For the two SS: at −1 the median |Δx| was 10.5 m (under 1 m in 8% of frames; slot 9 higher in 747); at −2 the median was 1.6 m (under 1 m in 38%; slot 9 higher in 704, slot 8 in 508). This fits §5: at −1 the rank multipliers and the floor separate them; at −2 the factor is 0 (every forward goes to the deep line) and the floor is off.
- **Not tested:** the two-forward case itself (§5 predicts identical lines at −1 and −2), v21, the score and clock at each change.

## 7. Disputed by the port plan (unaudited)
docs/plans/pes-plan-mentality.md §4 disputes four points above: forwards take the quota's first `need` sorted entries (not excluded); the protection scan also breaks on the scalar advance byte [ctx+0x118]+0x129; the pick arms are classes 7/8/9 (the directive doc's 6/7/8 is off by one); at levels 1-3 FUN_1405AAF00 can admit every class > 1 slot without the quota (RNG-gated). Plan v3 then found the forward pull's P is RoleArrays' S = T+0xC48 (so the pull reads RoleArrays B, and A for line membership), and that the pull is gated by the possession flag F (EC600 arg4 → local_458+0x10C, inverted on even frames once MI+0x119C > 1), not by the phase. To be settled by the RE slice before porting.
