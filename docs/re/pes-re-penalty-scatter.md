# PES 2021: penalty kick aim scatter and strike (external agent Q&A, 20-10-26)

Source: an outside RE agent, archived in substance. Main's audit is marked; everything else is the agent's claim.

> **Main audit 20-10-26:** each cited VA lands on an instruction of the claimed kind (byte stat read at `0x1406b4050`; conditional jumps at `0x1406b4116` (PK Specialist) and `0x1406b4132` (panenka); a call at `0x140815cc8` (the Kicking Power read); a float load at `0x1406e3426` (jitter)). The formulas and the semantics are NOT re-derived by Main.

## No keeper-specific modifier
The penalty pipeline never checks the taker's position. A goalkeeper taking a penalty runs the same code as a striker; only his stat values differ.

## Aim scatter (sub_1406B4000, penalty kick kind 54)
1. Accuracy core: Place Kicking x0.6 + Finishing x0.4, ramped over 40..99 (reads at 0x1406b4050-8c).
2. PK Specialist: base scatter 0.5 -> 0.4 (0x1406b4116).
3. Panenka: +0.4 scatter, halved to +0.2 with Chip Shot Control (0x1406b4132-4f).
4. Aim point: toward the posts adds up to +0.15; aiming high adds up to +0.3; both scaled down by stats (0x1406b41d3-0x1406b4273).
5. Shot charge (power parameter 0.2..1.0): the largest single term, up to +0.8, scaled by stats. The power-versus-accuracy tradeoff is explicit in code.
6. Radius x1000 = random jitter in mm applied to the aim point (0x1406e3426).

## Strike (sub_140815BE0)
Reads Kicking Power (live stat 42) through the penalty tuning table (0x140815cc8). The panenka goes through a separate lob generator: gate player+0x2D12 == 2 -> sub_140815960.

## Condition layer
Fatigue degrades Finishing by up to 20% (Place Kicking is exempt). Out-of-breath affects Finishing and Kicking Power. No shootout exemption.

## Open (not claimed)
Whether the AI aims differently for keepers (upstream decision, untraced). Which factors of the general kick-error layer (run-up, pressure from the nearest opponent, an AI-bits factor 0.9/0.7) stay live for a set-piece ball.

## Port note (Main)
GF's penalty shootout is GF's own controller (PenaltyShootoutController); none of this is ported.
