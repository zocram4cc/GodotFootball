#!/usr/bin/env python3
"""synth_passget_leaves.py: writes synth_specs/passget_leaf_e9ef0.json and passget_leaf_b8dd0.json, the SYNTHETIC cases of two LEAF
functions the ActionPassGet route walk's rate classifier (FUN_140706350, reached through FUN_1406B9270) calls through vtable slots:
  0x1406E9EF0  slot of the anime interface vtable (this = P+0xAC0; the trapvirt family): eax = [this + (edx ? 0x1CEC : 0x1CD8)]
  0x1406B8DD0  slot 1 of vtable 0x1425E3690: rax = 0x1425DD660 + sext(edx) * 4 (a pointer, nothing is read)
Both are leaves (no unwind info, no .pdata entry): no hookable entry exists. P is pc+0x40 of the real ActionPassGet tick capture
pid480109/seq001 (P = 0x36191BC0), whose memory holds the cells. Usage: PES_EXE=<pes_full.bin> python3 synth_passget_leaves.py;
then synth.py over the capture and prove_bounded.py --synth (docs/ports/pes-port-PassReceiver3.md)."""
import json
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "synth_specs")
P = 0x36191BC0  # pid480109/seq001: pc+0x40
THIS = P + 0xAC0
CELL0, CELL1 = THIS + 0x1CD8, THIS + 0x1CEC
# --- 0x1406E9EF0: the two cells and the selector (edx != 0 picks the second)
vals = [(0, 0), (1, 2), (0xFFFFFFFF, 0x7FFFFFFF), (0x12345678, 0x9ABCDEF0)]
cases = []
for flag in (0, 1, 0x80000000, 0xFFFFFFFF):
    for k, (a, b) in enumerate(vals):
        cases.append({"name": "f%x_v%d" % (flag, k), "this": THIS, "flag": flag, "c0": a, "c1": b})
spec = {"label": "passgetleafe9ef0", "seed": 1, "as": "0x1406E9EF0",
        "fields": [{"name": "this", "at": "rcx", "type": "u64", "values": [THIS]},
                   {"name": "flag", "at": "edx", "type": "u32", "values": [0]},
                   {"name": "c0", "at": "va:0x%X" % CELL0, "type": "u32", "values": [0]},
                   {"name": "c1", "at": "va:0x%X" % CELL1, "type": "u32", "values": [0]}],
        "cases": cases}
json.dump(spec, open(os.path.join(OUT, "passget_leaf_e9ef0.json"), "w"))
# --- 0x1406B8DD0: movsxd rax, edx
idx = [0, 1, 2, 3, 0x15, 0x16, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE]
spec = {"label": "passgetleafb8dd0", "seed": 1, "as": "0x1406B8DD0",
        "fields": [{"name": "this", "at": "rcx", "type": "u64", "values": [THIS]},
                   {"name": "idx", "at": "edx", "type": "u32", "values": [0]}],
        "cases": [{"name": "i%x" % i, "this": THIS, "idx": i} for i in idx]}
json.dump(spec, open(os.path.join(OUT, "passget_leaf_b8dd0.json"), "w"))
print("e9ef0", 16, "b8dd0", len(idx))
