#!/usr/bin/env python3
"""type13_advance.py SNAP_DIR --hooks CFG [--frames N] [--record-kind K] [--out FILE]

The type-13 body (0x140A115A0) run forward from one rendezvous capture's real entry state. Frame 1 is the recorded
call (frame_trace.run_recorded: captured GPRs, XMM, RFLAGS, MXCSR, GS base, over the snapshot's memory). Frames 2..N
re-enter the body on the same emulated memory with the same captured register file; the caller's per-frame loop is
not emulated, so this is the body alone, frame after frame.

The body reads the record kind as the dword at its r8 argument and dispatches on kind-1 (a jump table of 0x25
entries; any other kind takes the epilogue). Per frame the report gives the record kind found at r8 after the frame,
whether the frame-counter increment at 0x140A128E4 (`inc [rsi+0x1198]`, rsi = E+0x180C60) executed and the counter
before and after, the roster gate 0x140A0E690 (calls, and eax at entry), whether the exit 0x140A1282C was reached, the
distinct code addresses, and the written bytes (total, and outside the stack window around the captured rsp).

--record-kind K is a what-if, not the real state: from frame 2 on, the dword at r8 is set to K before each entry (the
record page is mapped only once frame 1 has read it). Frame 1 is always the real call. Exit 0 always; the answer is in
the JSON.

Usage: PES_EXE=<pes_full.bin> python3 type13_advance.py SNAP_DIR --hooks CFG [--frames 3] [--record-kind K] [--out FILE]
  SNAP_DIR is a rendezvous capture (mem.bin, index.json, call.bin); CFG is the rv.dll hook config.
"""
import argparse
import collections
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "diffharness"))
import frame_trace as ft  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402

BODY = 0x140A115A0    # type-13 body (rendezvous hook type13_body_A115E9)
INC = 0x140A128E4     # inc [rsi+0x1198]: the frame counter
GATE = 0x140A0E690    # roster scan; returns 9 > 0x15 at rest (docs/re/pes-re-type13.md §3)
EXIT = 0x140A1282C    # the exit the rest state takes after the gate
COUNTER_OFF = 0x1198  # frame counter field inside the E-side block
STACK_WINDOW = 0x4000  # written bytes within this window of the captured rsp count as stack
TRAIL = 30


def rd32(uc, va):
    try:
        return int.from_bytes(bytes(uc.mem_read(va, 4)), "little")
    except Exception:  # noqa: BLE001 - unmapped cell: report None, never a guess
        return None


def rerun(uc, call, budget):
    """Frame 2..N: the body entered again on the same memory with the captured register file."""
    for name in ft.GPR_ORDER:
        uc.reg_write(getattr(xc, "UC_X86_REG_" + name.upper()), call["gpr"][name])
    for i, v in enumerate(call["xmm"]):
        uc.reg_write(xc.UC_X86_REG_XMM0 + i, v)
    uc.reg_write(xc.UC_X86_REG_EFLAGS, call["flags"])
    uc.reg_write(xc.UC_X86_REG_MXCSR, call["mxcsr"])
    uc.reg_write(xc.UC_X86_REG_GS_BASE, call["teb"])
    try:
        uc.emu_start(call["entry"], call["ret"], count=budget)
    except Exception as e:  # noqa: BLE001 - unicorn raises on a fault
        return "emu error: %s (rip %#x)" % (e, uc.reg_read(xc.UC_X86_REG_RIP))
    rip = uc.reg_read(xc.UC_X86_REG_RIP)
    if rip != call["ret"]:
        return "did not return: rip %#x after %d-instruction budget" % (rip, budget)
    if uc.reg_read(xc.UC_X86_REG_RSP) != call["gpr"]["rsp"] + 8:
        return "stack not balanced"
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("snap")
    ap.add_argument("--hooks", required=True, help="rv.dll hook config: the bytes each hook patch displaced")
    ap.add_argument("--frames", type=int, default=3)
    ap.add_argument("--record-kind", type=int, default=None,
                    help="what-if: from frame 2 on, set the record kind (dword at r8) before each entry")
    ap.add_argument("--budget", type=int, default=ft.INSTR_BUDGET)
    ap.add_argument("--out", default=None)
    a = ap.parse_args()

    orig = ft.hook_bytes(a.hooks)
    call = ft.load_recorded_call(a.snap)
    rsp, rec = call["gpr"]["rsp"], call["gpr"]["r8"]
    cur = {}
    trail = collections.deque(maxlen=TRAIL)

    def new_frame(i):
        return {"frame": i, "addrs": set(), "writes": set(), "inc": False, "rsi": None,
                "counter_before": None, "gate_calls": 0, "gate_key_eax": None, "exit": False}

    def on_code(uc, address, size, data):
        f = cur["f"]
        f["addrs"].add(address)
        trail.append(address)
        if address == INC:
            rsi = uc.reg_read(xc.UC_X86_REG_RSI)
            f["inc"], f["rsi"], f["counter_before"] = True, rsi, rd32(uc, rsi + COUNTER_OFF)
        elif address == GATE:
            f["gate_calls"] += 1
            f["gate_key_eax"] = uc.reg_read(xc.UC_X86_REG_EAX)
        elif address == EXIT:
            f["exit"] = True

    def on_write(uc, access, address, size, value, data):
        writes = cur["f"]["writes"]
        for k in range(size):
            writes.add(address + k)

    frames, uc = [], None
    for i in range(1, a.frames + 1):
        cur["f"] = new_frame(i)
        if i == 1:
            uc, err = ft.run_recorded(a.snap, orig=orig, budget=a.budget, on_code=on_code, on_write=on_write)
        else:
            if a.record_kind is not None:
                uc.mem_write(rec, a.record_kind.to_bytes(4, "little"))
            err = rerun(uc, call, a.budget)
        f = cur["f"]
        outside = sorted(w for w in f["writes"] if not (rsp - STACK_WINDOW <= w < rsp + STACK_WINDOW))
        counter_after = rd32(uc, f["rsi"] + COUNTER_OFF) if f["rsi"] is not None else None
        frames.append({
            "frame": i,
            "err": err,
            "record_kind_at_r8": rd32(uc, rec),
            "rax": hex(uc.reg_read(xc.UC_X86_REG_RAX)) if err is None else None,
            "inc_executed": f["inc"],
            "counter_va": hex(f["rsi"] + COUNTER_OFF) if f["rsi"] is not None else None,
            "counter_before": f["counter_before"],
            "counter_after": counter_after,
            "gate_calls": f["gate_calls"],
            "gate_key_eax": f["gate_key_eax"],
            "exit_reached": f["exit"],
            "distinct_code_addrs": len(f["addrs"]),
            "body_range_addrs": sum(1 for x in f["addrs"] if BODY <= x < BODY + 0x10000),
            "written_bytes": len(f["writes"]),
            "written_outside_stack": len(outside),
            "first_outside_write": hex(outside[0]) if outside else None,
            "trail": [hex(x) for x in trail],
        })
        if err:
            break

    advanced = sum(1 for f in frames if f["counter_after"] is not None and f["counter_before"] is not None
                   and (f["counter_after"] - f["counter_before"]) & 0xFFFFFFFF == 1)
    out = {
        "snap": a.snap,
        "entry": hex(call["entry"]),
        "record_kind_forged_from_frame_2": a.record_kind,
        "frames_run": len(frames),
        "frames_counter_advanced": advanced,
        "writes_outside_stack_total": sum(f["written_outside_stack"] for f in frames),
        "frames": frames,
    }
    text = json.dumps(out, indent=1, sort_keys=True)
    if a.out:
        with open(a.out, "w") as fh:
            fh.write(text + "\n")
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
