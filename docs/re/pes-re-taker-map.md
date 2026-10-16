# PES 2021 set-piece taker map: `FUN_1404a8ad0` (free-kick kicker chooser)

Scope: decode `FUN_1404a8ad0` (VA 0x1404a8ad0) for `Wave1TSheet` (`53b9742`). Read-only: no commits, no repo edits. Scratch: `the scratch dir/` (probe.py, watch.py, watch.log).

## 0. Verdict

- `FUN_1404a8ad0` is the **free-kick** kicker chooser. It is called only from `FUN_1404a7d60` case 5 (MODE 5 = free kick). Corner (`FUN_1404a8620`) and penalty (`FUN_1404a90d0`) are separate functions with the same shape, and do not call it.
- The **designated table it reads is `S+0x143c` (six u32, `FUN_1408c5080`), not `S+0x1454` (`FUN_1408c5060`, three takers).** The `53b9742` comment that "PES reads TeamSheet::Taker (T+0x1454) first" is wrong for this chooser. `S+0x1454` is read by other functions (§5.3).
- The `53b9742` comment "FUN_1404a8ad0's waterfall without its first branch" is accurate: the first branch is the designated lookup. The fallback branches (keeper, current kicker, role loops, rating) are decoded below.
- Two corrections to `docs/re/pes-re-setpiece-choreography.md` §2 row 5 (see §8).
- Live: a read-only watch of the running match logged 47 kicker/MODE change records over 19 min (09:36:44–09:55:03). **No free kick (MODE 5) occurred in the window**, so the live confirmation of this chooser is **not yet done**. See §6.

## 1. Signature and inputs

```
uint64 FUN_1404a8ad0(ctx, R, excluded, scratch)
  ctx      = *(FUN_1404a7d60.param_1 + 0x17860)   unused in effect (passed to helpers that ignore it)
  R        = FUN_1404a7d60's param_3: restart record (int* in the caller)
               R+0x00 int   MODE (5 here)
               R+0x04 int   side 0/1                       (iVar1)
               R+0x08 f32   restart spot x                 (local_e0)
               R+0x10 f32   restart spot z                 (fVar2)
  excluded = 0xff, or the first kicker's global slot (<22) for the second kicker
  scratch  = FUN_1404a7d60.param_1 + 0x58dc   (passed to FUN_1404a8930 only)
Output: global slot 0..21 (= 11·side + in-team slot), or 0xff (no free-kick taker).
```

Globals used: `G = *(0x1436F3DC8)` (match context), `MI = *(G+0x270)` (match instance), `online = (*(G+0x400) != 0 && byte[*(G+0x400)] != 0)` (bVar5, the online-practice object).

Caller-side inputs that gate the call (FUN_1404a7d60, case 5): the first kicker is re-picked only when the published kicker is invalid (`local_1d8`); the second only when the published second is invalid (`bVar4`). The second call happens only when `*(param_4) ∈ {1,2,3}` (the FK type, `Flow+0x2ab4` per the RE notes; OBS in the decompile, name INF).

## 2. Team, lineup and sheet tables (install offsets, reader VAs)

All offsets are into the match context G or the team record T. `side` ∈ {0,1}.

| Field | Address | Reader / accessor | Notes and live sample (side0 / side1) |
|---|---|---|---|
| Team record T | `*(G + (0xa1+side)·0x28)` = G+0x1928 / G+0x1950 | `FUN_140a48a30` | pointer |
| Lineup/roster base P | `*(G + (0xa3+side)·0x28)` = G+0x1978 / G+0x19a0 | `FUN_140a489d0` | pointer |
| Slot→roster id | `P + 4 + 4·i`, i = 0..39 | `FUN_140a3b000` (search), `FUN_140a3af60` (player) | live side0 slots 0..10: 11,14,2,1,7,6,8,21,9,3,20; side1: 0,16,18,22,1,11,6,7,19,3,10 |
| Player record | `P + roster·0x128 + 0xa8` | `FUN_140a3af60` | valid-player test `FUN_140a3b520` (player+0x34 nonzero, plus a global id pair) |
| Attack sign s | `int8 T+0x254` | inline | live: side0 +1, side1 −1 |
| Sheet S | `T + 0x8e08` | — | |
| **Designated k (k<6)** | `S + 0x143c + 4k` = **T+0xa244+4k** | `FUN_1408c5080` (0xff if k≥6) | live side0 [1,1,7,1,1,1]; side1 [6,10,6,6,6,10]. Install default 10 (immediate at VA 0x1408c585a, `c7 01 0a 00 00 00`, one store, six slots) |
| Takers (k<3), NOT read here | `S + 0x1454 + 4k` | `FUN_1408c5060` | install default 0xff (imm at 0x1408c5872 / 0x1408c5881 / 0x1408c588c). See §5.3 |
| Slot role A | `T + 0x4bd4 + 4i`, i = 0..10 | inline (loops of FUN_1404a8ad0) | role 0 = keeper. live side0 [0,1,1,2,2,3,6,6,7,7,9] |
| Slot role B | `T + 0x4c00 + 4i`, i = 0..10 | inline (`FUN_140a62610`, `FUN_140a621f0`, goal kick) | identical to role A in all 8 live samples |
| Slot target x,z | `T + 0x4b4c + 12i` (x at +0, z at +8) | inline (loops 1 and 2) | writer not decoded (RE notes); not sampled live |
| Loop-1 exclusion | `T + 0x4b40` (u32) | inline | live 255 in all samples; meaning open |
| Restart MODE | `MI + 0x116c` | inline (gate `== 3`) | live: 1, 2, 3 observed |
| Published kicker | `MI + 0x173c` (`MI + 0x1740` second) | inline | |
| On-pitch flags | `MI + 0x1748 + g`, g = 0..21 (global slot) | inline | live "1111110000111100011111" (side0 slots 6–9 and side1 slots 14–16 are 0; unexplained, held for 6 min) |
| Field table F | `*(G + 0x2aa8)`, f32 | inline | F[0]=52.56 (half-length), F[6]=36.14 (penalty-line x), F[7]=20.12 (penalty half-width), live F[0] read 52.56 |
| Online object | `*(G + 0x400)` | inline | only for online practice |

Slot ↔ player: `global = 11·side + in-team slot` (in-team slot i ∈ 0..10 are the starters; the lineup table has 40 entries).

## 3. Constants (PES2021.exe, install; read through PeImage::ReadVa in the port)

| Use in FUN_1404a8ad0 | VA | Value | Raw bytes |
|---|---|---|---|
| free-kick short/long split `d < 32` | 0x14259be28 | 32.0 | `00 00 00 42` (f32) |
| free-kick far cut `d < 60` / loop-2 gate | 0x14259a764 | 60.0 | `00 00 70 42` |
| own-goal distance test `< 28` | 0x1425a2004 | 28.0 | `00 00 e0 41` |
| sign flip for the own-penalty-box test | 0x142588218 | −1.0 | `00 00 80 bf` |
| loop-1 radius / initial best distance | 0x14259be48 | 65.0 | `00 00 82 42` |

The bit-mask `0x142565d30` is `|x|` on the double (`7fff ffff ffff ffff`-style sign mask), not a tunable.

## 4. Algorithm (reconstruction; decompile-derived, confidence OBS unless tagged)

Let `s = T+0x254`, `x = R+0x08`, `z = R+0x10`, `F = G+0x2aa8` (F[0], F[6], F[7]), `dAtt = |(x,z) − (s·F0, 0)|`, `dOwn = |(x,z) − (−s·F0, 0)|`.

1. **Kind.** If `excluded < 22`: kind = 2 (second kicker). Else if `dAtt < 32`: kind = 1. Else if `dAtt < 60`: kind = 0. Else kind = 6 (no designated taker).
2. **Designated.** If kind ≠ 6: `r = S.designated[kind]`; `p = FUN_140a3b000(P, r)` (position 0..39 in the lineup, or 0xff); `g = p + 11·side`. If `g < 22` and on-pitch(g) (online: FUN_140a59af0; else `MI+0x1748[g]`) and `g ≠ excluded`: **return g**. (If `g ≥ 22` → fall through; if `g == excluded` → fall through.)
3. **Current kicker reuse.** If `MI+0x116c == 3` and `excluded > 21`: `c = MI+0x173c`; if `c < 22` and on-pitch(c): **return c**.
4. **Keeper.** If `|z| < F7` and (`dOwn < 28` or `s·x < −F6`): `k = FUN_140a62690(side)` = `11·side +` first in-team slot i with on-pitch and `T+0x4c00[i] == 0` (FUN_140a62610, no online test, no exclusion). If `k < 22` and on-pitch(k) (online-aware): **return k**. There is no exclusion check on this path.
5. **Loop 1, own half.** If `s·x < 0`: over the 11 slots of `side`, candidates with on-pitch (online-aware), role A ∈ {1,2}, and `g ≠ T+0x4b40`. Distance from the slot target `(T+0x4b4c+12i .x, .z)` to `(x,z)`. Best must be `< 65`, strict. If a best exists: **return it**.
6. **Loop 2, far from goal.** If `dAtt > 60`: over the 11 slots, candidates on-pitch (online-aware) with role A `< 7` (includes the keeper), **no exclusion**. Distance to slot target, strict, starting bound 65 (unchanged from loop 1). If a best exists: **return it**.
7. **Rating fallback** `FUN_140a621f0(side, kind′, excluded % 11 or 0xff)`, where `kind′ = kind` if kind ≠ 6, else 0 (so kind′ ∈ {0,1,2}). Over the 11 in-team slots i (uses `MI+0x1748` directly, no online test):
   - skip if `i == excluded % 11`, not on-pitch, or player invalid (FUN_140a3b520);
   - role filter: `role_B(i) ≠ 0 || kind′ == 0` (so a keeper is eligible only for kind 0);
   - rating `= *(u8)(R_k + 0x16e + kind′)`, where `R_k = P + roster·0x128` (playerdata+0x96 + kind; GF calls this the per-player set-piece rating);
   - choose the maximum, strict `>`, first in slot order wins ties.
   If found: **return 11·side + i**.
8. **Otherwise**: if online → `FUN_1404a8930` (online ranking; not decoded here); else **return 0xff**.

Notes:
- The exclusion of the first kicker is applied in step 2 (`g ≠ excluded`), step 7 (`i ≠ excluded % 11`), and step 3 (`excluded > 21` gate). It is **not** applied in steps 4, 5 (uses `T+0x4b40` instead), and 6. So for the second kicker, a keeper (step 4) or a loop-2 player can equal the first kicker. Open whether `T+0x4b40` equals the first kicker in PES's own flow; live it reads 255 in every sample, so it does not.

## 5. Callers and readers (VAs)

### 5.1 Callers of FUN_1404a8ad0 (exhaustive: call/jmp scan of the whole image + xrefs; 0 absolute data references)

| Call site | In | Role |
|---|---|---|
| **0x1404a8591** | FUN_1404a7d60 (kicker selector), case 5 | first kicker: `excluded = 0xff`, result → `out[0]` (only if first kicker invalid) |
| **0x1404a85c7** | FUN_1404a7d60, case 5 | second kicker: `excluded = out[0]`, result → `out[1]` (only if second invalid and FK type ∈ {1,2,3}) |

Callers of FUN_1404a8930 (online fallback, only from here): 0x1404a9079 in FUN_1404a8ad0.

### 5.2 Upstream: callers of FUN_1404a7d60 (kicker selector entry; which MODE each reaches not decoded)

0x140447fca (FUN_140447b50), 0x1404485a6 (FUN_1404483e0), 0x14044d2b7 (FUN_14044d110), 0x14044d8b0 and 0x14044d937 (FUN_14044d2e0), 0x1404517a0 and 0x140451803 (FUN_1404514f0), 0x1404a5031 (FUN_1404a4f20).

### 5.3 Other readers of the same tables (for the rewire)

- Designated `S+0x143c` (`FUN_1408c5080`): corner chooser FUN_1404a8620 at 0x1404a8718 (kind 3 if `s·R.z ≤ 0`, else 4); penalty chooser FUN_1404a90d0 at 0x1404a9198 (kind 5); FUN_14048ddf0 at 0x14048e164; FUN_140520930 at 0x140520d30; FUN_141fc26e0 at 0x141fc2a34; 0x14052162a, 0x140521686; 0x140a1a219, 0x140a1a2b3 (unnamed contexts).
- Takers `S+0x1454` (`FUN_1408c5060`): FUN_14051b8b0 (0x14051ba7a), FUN_140520930 (0x140520d70), FUN_1405fc860 (0x1405fc955), FUN_1405f63c0 (0x1405f6632), FUN_141fc26e0 (0x141fc2a58), and **FUN_140a63220** (0x140a632ae). FUN_140a63220 is a membership predicate: returns 1 if the slot (in-team, `% 11`) is one of takers 0..2. It is not a chooser.
- Corner `FUN_1404a8620` and penalty `FUN_1404a90d0` both call the rating fallback `FUN_140a621f0` with excluded `0xff`.

## 6. Live evidence (read-only, `/proc/<pid>/mem`, PES2021.exe pid 2510857)

Watcher `watch.py` (read-only `/proc/2510857/mem`, PES2021.exe): logs each change of `(MI+0x173c, MI+0x1740, MI+0x116c)` with the designated, lineup and flag tables. Ran 09:36:44 to 09:55:03 (1100 s). 47 change records, 35 read errors (EIO on unmapped pages). Full log: `the scratch dir/watch.log`.

Constant across all 47 records: second kicker 255; on-pitch vector `1111110000111100011111`; side0 sign +1, side1 −1; side0 designated [1,1,7,1,1,1]; side1 [6,10,6,6,6,10]; `T+0x4b40` 255; role A = role B.

Publish sequence, `time MODE116c kicker` (kicker = global slot):

09:36:44 1 12 · 09:36:53 2 10 · 09:37:57 1 10 · 09:38:27 2 1 · 09:38:39 3 1 · 09:38:47 1 1 · 09:40:02 2 10 · 09:40:56 1 10 · 09:41:24 2 10 · 09:42:14 3 10 · 09:42:16 1 10 · 09:42:54 3 17 · 09:43:03 1 17 · 09:43:07 2 2 · 09:43:18 3 2 · 09:43:23 1 2 · 09:43:48 2 0 · 09:44:02 3 0 · 09:44:05 1 0 · 09:44:51 2 10 · 09:45:40 1 10 · 09:46:00 2 3 · 09:46:04 3 3 · 09:46:06 1 3 · 09:46:47 2 17 · 09:47:03 3 17 · 09:47:09 1 17 · 09:48:13 2 0 · 09:48:31 1 0 · 09:49:50 1 17 · 09:50:06 3 12 · 09:50:07 1 12 · 09:50:35 3 17 · 09:50:41 1 17 · 09:50:54 2 10 · 09:51:37 1 10 · 09:53:11 2 17 · 09:53:12 3 17 · 09:53:19 1 17 · 09:53:44 2 17 · 09:53:56 3 17 · 09:54:07 1 17 · 09:54:09 2 17 · 09:54:26 3 17 · 09:54:29 1 17 · 09:54:33 2 0 · 09:55:03 1 0

Reading (unconfirmed): the MODE field cycles 2→3→1 at each publish. The kicker changes at MODE 2 in most records (at MODE 3 at 09:42:54 and 09:50:06). Publishes at MODE 2 with kicker 0 (side0 keeper) fit a goal kick (09:43:48, 09:48:13, 09:54:33). Publish 09:46:00 (MODE 2, kicker 3 = side0 slot 3 = roster 1, which side0's designated kinds 0, 1, 3, 4, 5 all name) fits a free-kick or corner pick, but the restart record is not read, so it is not a test of this chooser. The MODE field is therefore not simply the selector's MODE number, and no MODE 5 was observed.

Predictions (untested), for a free kick with the restart spot read: side0 → slot 3 (kinds 0, 1), slot 4 (kind 2); side1 → slot 17 (kinds 0, 2) or slot 21 (kind 1), each subject to the on-pitch and exclusion rules of §4.

Status: **live confirmation not achieved.** `FUN_1404a8ad0` was not exercised in the 19-minute window.

## 7. Decode status

Decoded, with VA-level evidence: the whole waterfall of §4 (decompile of FUN_1404a8ad0 plus helpers FUN_1408c5080, FUN_140a3b000, FUN_140a48a30, FUN_140a489d0, FUN_140a62690, FUN_140a62610, FUN_140a621f0, FUN_140a3af60, FUN_1408c75a0, FUN_141b59e20, FUN_140a3b520, FUN_140a59b20 (partial)). Constants read from the install (§3).

Not decoded (named, so the port knows what is missing):
- `FUN_140a59af0` / `FUN_140a59b20`: the online-practice on-pitch gate (a per-team rank, `FUN_140a40ca0`, then a table compare). Used only when `G+0x400` is active.
- `FUN_1404a8930`: online-practice fallback (sorted buckets, `FUN_141030ab0` mode). Only when `G+0x400` is active.
- `FUN_1415a1780`: called as a distance (sqrt-shaped: arguments are sums of squares). Its target is an indirect call through the global at 0x1425301f0, which is not an import slot in the on-disk image; identity inferred from use (INF).
- `T+0x4b40` (loop-1 exclusion), `T+0x4b4c` (slot targets; writer not decoded), `MI+0x116c == 3` semantics beyond "set-piece mode" (RE notes), the flags pattern in §6.
- The keeper/role semantics of `T+0x4bd4` vs `T+0x4c00` (identical live; different readers).
- Upstream MODE mapping for the 8 callers of FUN_1404a7d60 (§5.2).

## 8. Corrections to existing notes

`docs/re/pes-re-setpiece-choreography.md` §2 (table row 5 and the note below it):
- The fallback "`d < 28`" is the distance to the **own** goal (`−s·F0`), not the attacked goal. The second test is "the ball is beyond the own penalty-area line (`s·x < −F6`)", not "own half". Both gated by `|z| < F7`.
- "Nearest role-1/2 player within 65 m" is correct for own-half balls (`s·x < 0`); "nearest role < 7" applies only when the ball is more than 60 m from the attacked goal (`dAtt > 60`), and includes the keeper.
- Kind 2 (second kicker) is designated index 2, used when `excluded < 22`; it is not distance-based.

`pessetpieceteam.cpp` / `pessetpieceteam.hpp` (branch `53b9742`): the comment that PES reads "TeamSheet::Taker (T+0x1454)" for the set-piece kicker applies to the wrong table for this chooser; FK reads `Designated(k)` (T+0xa244+4k).

## 9. Terms Wave1TSheet still needs

Exact list, for the port of FUN_1404a8ad0 and the rewire of takers:

1. A source for the six designated values (`TeamSheet::Designated(k)`, S+0x143c): the accessor exists in `53b9742`, but nothing fills it. Install default 10 (immediate at 0x1408c585a). Needs an importer or an explicit default.
2. The lineup record P per side: slot i (0..39) → roster index (`P+4+4i`) and the roster index of each Player. GF needs the equivalent of `FUN_140a3b000` (roster → slot) and `FUN_140a3af60` (slot → player).
3. The aliasing rule: `g = p + 11·side` with no bench check, so a bench roster at p ≥ 11 aliases onto the other side's slots (side0 p 11–21 → side1 slots). Decide whether the port reproduces it or refuses p ≥ 11 (PES's behaviour here is not live-tested).
4. On-pitch per global slot 0..21 (`MI+0x1748`), and the valid-player test (`FUN_140a3b520`). GF equivalent: `SetPieceCandidate.onPitch` plus an active-player check.
5. The attack sign `s` (`T+0x254`) and the restart spot `(x, z)` and side: `dir`, ball position and side arguments already exist in `FreeKickTaker`.
6. The second-kicker FK type `Flow+0x2ab4 ∈ {1,2,3}` (caller gate in FUN_1404a7d60 case 5). Needs a GF field.
7. The re-pick gates: first kicker re-picked only when invalid; second only when the second is invalid; second call only with FK type ∈ {1,2,3}. Needs to be mirrored in `PrepareSetPiece`.
8. The current-kicker reuse gate `MI+0x116c == 3` with `excluded > 21`, and `MI+0x173c` (published kicker). GF equivalent: the "already chosen" kicker.
9. Constants read from PES2021.exe through `PeImage::ReadVa`: 32.0 (0x14259be28), 60.0 (0x14259a764), 28.0 (0x1425a2004), −1.0 (0x142588218), 65.0 (0x14259be48). F[0], F[6], F[7]: already in GF's FieldInfo (`pesjudge.hpp` `halfLength_m`, `penaltyLineX_m`, `penaltyHalfWidth_m`); confirm the names match the values 52.56 / 36.14 / 20.12.
10. Slot targets (`T+0x4b4c + 12i`, x and z). Writer not decoded. GF needs a per-slot formation target for loops 1 and 2 (x, z per slot) or an explicit substitute, labelled as an approximation.
11. Slot roles (`T+0x4bd4 + 4i` and `T+0x4c00 + 4i`, PES role codes 0..9). GF's `e_PlayerRole` → PES code map is needed for the loop filters (role 0 keeper, 1–2 centre-backs, `< 7` for loop 2, `== 0` for the keeper test).
12. Six-kind set-piece rating per player (`R_k + 0x16e + kind`, kinds 0, 1, 2). GF has no such byte (`SetPieceCandidate.suitability` is GF's stand-in). Needed for the rating fallback (step 7) to match PES.
13. `T+0x4b40` (loop-1 exclusion): meaning and writer. Open. Live it is 255.
14. Online branch (`G+0x400` active): `FUN_140a59af0`/`b20`, `FUN_1404a8930`. Needed only if online matches are in scope; otherwise the port is the CPU branch.
15. Takers `S+0x1454` (`Taker(k)`) is not an input to the FK chooser. If Wave1 also rewires "takers", the membership predicate `FUN_140a63220` and the other readers of §5.3 are a separate item.
16. The corner (kind 3/4 from `s·R.z`, `FUN_1404a8620`) and penalty (kind 5, `FUN_1404a90d0`) choosers use the same table and fallback; they are not in `53b9742`.
17. Live confirmation of §4 against a MODE 5 event: **not achieved** (no free kick in the 19-minute window, §6).

## 10. Open items

- The live free-kick event (MODE 5) is the missing confirmation. Re-run `watch.py` (`the scratch dir/`) during a free kick, or read `probe.py` once one is taken.
- Restart spot (x, z) at the moment of a free kick: needs the ball finder (`pes_state.py`, which writes to /tmp; not used here).
- The flags in §6 (four side0 slots 6–9 and three side1 slots 14–16 at 0) are unexplained.
