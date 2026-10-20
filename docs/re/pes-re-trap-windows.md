

> **Main 19-10-26 (live, S1Watch):** 22 TRAP issues (action 2/3 -> 7): issuer distance to the ball (B+0x88) at issue median 9.5 m (min 0.54, max 17.0), consistent with the 10 m issue rule; the anime clip ids read at +0xAE0 are not yet reconciled with the row ids (base check pending). Player->AnimePlayer mapping: AnimePlayer +0x8/+0x10 = player anim position p+0x55C (20/22).
> **Main audit 19-10-26:** `0x142648668` = [0.15, 0.335, 0.56, 0.785] (T1 = 0.335, T2 = 0.56): match. The window test is CONFIRMED; the live trap is not measured (no action-7 pick in 3 min of play). PARKED until a loose-ball window.
> **Main audit 19-10-26 (§6):** default +0x70 `0x141863350` = `xor al,al; ret`, and the KickoffReceiver/DemoMove/OutofPlay vtables (`0x1425E1220/6428/2810`) +0x70 all point to it: match. A moving man (Move/StepMove) is admitted to TRAP by default.
## 8. Trap sample-N test at trigger (round 28, `srun1/trap.jsonl`, 23 triggers)

Recorded at the trigger frame (anime action becomes 7): issuer sim xz to ball sample N, for N = 0..8 (sample N at `B + (5N+17)·8`).

- Sample 0 (B+0x88) distance ≤ 10 m at trigger: **17 of 19** with a sample-0 value. Above 10 m: 10.26 and 10.63 m (two cases).
- First sample N with distance ≤ 10 m at the trigger frame: N = 0 in 17 cases, N = 6 in one, and none in one case.
- Sample N does not explain the two 10.3–10.6 m cases: for them the sample-0 distance is already above 10 m and the sample-N distances rise further with N in the logged cases (for example 10.63, 10.71, … 11.26 m for N = 0..8).

**Result.** At the trigger frame the sample-0 distance is ≤ 10 m in most cases, so sample 0 is the better match for a "within 10 m" trigger. The rule "first frame within 10 m" cannot be tested from trigger-frame values alone: it needs the distance on the frames before the trigger, which were not recorded. Status: sample-0 consistent, rule not yet confirmed, sample N not favoured.
