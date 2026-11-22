#!/usr/bin/env python3
"""readset.py: the heap bytes one recorded call reads and writes, clustered by object, relative to its entry registers.

Pins which fields of the records a PES function reads (the field list GF must hold in PES layout): replay one rendezvous
capture (frame_trace.run_recorded) with a memory hook, keep every read and write outside the image, and print the
clusters with their offsets from rcx, rdx, r8, r9 and rsp.

Usage: PES_EXE=<pes_full.bin> python3 readset.py SNAP_DIR --hooks hooks.cfg --scratch DIR [--json OUT] [--windows OUT]
       (harness Python: unicorn)
"""
import argparse
import collections
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402
from unicorn import UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE  # noqa: E402

IMG_LO, IMG_HI = 0x140000000, 0x143800000  # PES's image: its reads are code and constants, not state
PAGE_GAP = 2  # pages apart that still make one cluster


def replay(snap, hooks, scratch):
    """-> (entry registers, {(addr, size): count} reads, same for writes, error) of one recorded call."""
    reads, writes, regs = collections.Counter(), collections.Counter(), {}

    def on_read(uc, access, addr, size, value, data):
        if not IMG_LO <= addr < IMG_HI:
            reads[(addr, size)] += 1

    def on_write(uc, access, addr, size, value, data):
        if not IMG_LO <= addr < IMG_HI:
            writes[(addr, size)] += 1

    def on_code(uc, addr, size, data):
        if not regs:  # the first instruction: the entry registers, then the memory hooks
            for r in ("rcx", "rdx", "r8", "r9", "rsp"):
                regs[r] = uc.reg_read(getattr(xc, "UC_X86_REG_" + r.upper()))
            uc.hook_add(UC_HOOK_MEM_READ, on_read)
            uc.hook_add(UC_HOOK_MEM_WRITE, on_write)

    trace = {"out": os.path.join(scratch, os.path.basename(snap.rstrip("/"))), "impure": {},
             "stubs": ft.stubs_for_run(ft.stub_table.load())}
    os.makedirs(trace["out"], exist_ok=True)
    _, err = ft.run_recorded(snap, orig=ft.hook_bytes(hooks), trace=trace, on_code=on_code)
    return regs, reads, writes, err


def clusters(addrs):
    """[(lo, hi, distinct addresses)] of the addresses grouped by 4 KiB pages PAGE_GAP or fewer apart."""
    pages = collections.defaultdict(list)
    for a, size in addrs:
        pages[a >> 12].append((a, size))
    out, cur = [], []
    for page in sorted(pages):
        if cur and page - cur[-1] > PAGE_GAP:
            out.append(cur)
            cur = []
        cur.append(page)
    out.append(cur)
    return [(min(a for p in c for a, _ in pages[p]), max(a + s for p in c for a, s in pages[p]),
             sum(len(pages[p]) for p in c)) for c in out if c]


STACK_BYTES = 0x100000  # PesMemory's stack (pesmemory.hpp kStackBytes): the window around the entry rsp
TEB_BYTES = 0x2000  # PesMemory's thread block (kTebBytes)


def classify(addr, rsp, teb):
    """The source of an access: the VirtualAlloc model's OS arena (import_stubs.json), the thread block, the stack, or the match
    (the rest of the capture's memory)."""
    if ft.OS_ARENA_BASE <= addr < ft.OS_ARENA_BASE + ft.OS_ARENA_BYTES:
        return "os_arena"
    if teb <= addr < teb + TEB_BYTES:
        return "teb"
    if abs(addr - rsp) < STACK_BYTES:
        return "stack"
    return "match"


def windows_of(accesses, classify_fn):
    """[(source, lo, hi, distinct)]: {(addr, size): count} grouped by source, clustered as clusters() does."""
    by_source = collections.defaultdict(dict)
    for (addr, size), n in accesses.items():
        by_source[classify_fn(addr)][(addr, size)] = n
    return [(src, lo, hi, n) for src in sorted(by_source) for lo, hi, n in clusters(by_source[src])]


def windows_tsv(label, hook, err, rsp, teb, reads, writes):
    """One call's TSV lines: `call LABEL HOOK ERR RSP TEB`, then `window SOURCE LO HI DISTINCT` per window of its read+write set."""
    acc = collections.Counter(reads)
    acc.update(writes)
    head = ["call", label, hook, "-" if err is None else str(err).replace("\t", " "), hex(rsp), hex(teb)]
    return ["\t".join(head)] + ["\t".join(["window", src, hex(lo), hex(hi), str(n)])
                                for src, lo, hi, n in windows_of(acc, lambda x: classify(x, rsp, teb))]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("snap")
    ap.add_argument("--hooks", required=True)
    ap.add_argument("--scratch", required=True)
    ap.add_argument("--json", default=None)
    ap.add_argument("--windows", default=None, help="write this call's windows (TSV, see windows_tsv) to this file")
    a = ap.parse_args()
    regs, reads, writes, err = replay(a.snap, a.hooks, a.scratch)
    print("call: err=%s regs=%s" % (err, {k: hex(v) for k, v in regs.items()}))
    for name, acc in (("reads", reads), ("writes", writes)):
        print("%s: %d distinct addresses" % (name, len(acc)))
        for lo, hi, n in clusters(acc):
            rel = ["%s%+#x..%+#x" % (r, lo - b, hi - b) for r, b in regs.items() if lo - 0x4000 <= b <= hi]
            print("  %#x..%#x  %d addrs  %s" % (lo, hi, n, " ".join(rel)))
    if a.json:
        with open(a.json, "w") as f:
            json.dump({"regs": regs, "reads": sorted([x, s, n] for (x, s), n in reads.items()),
                       "writes": sorted([x, s, n] for (x, s), n in writes.items())}, f)
    if a.windows:
        with open(os.path.join(a.snap, "index.json")) as f:
            hook = json.load(f)["rv"]["hook"]
        lines = windows_tsv(os.path.basename(a.snap.rstrip("/")), hook, err, regs["rsp"],
                            ft.load_recorded_call(a.snap)["teb"], reads, writes)
        with open(a.windows, "w") as f:
            f.write("\n".join(lines) + "\n")
    return 0 if err is None else 1


if __name__ == "__main__":
    sys.exit(main())
