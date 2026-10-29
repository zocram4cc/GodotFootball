#!/usr/bin/env python3
"""pdata.py: function starts, extents and decode from the image's .pdata (the exception directory's RUNTIME_FUNCTION table).

The one place the tools learn what a function is without the Ghidra bridge. A .pdata entry is NOT always a function:
- A chained entry (UNWIND_INFO flag 4: a PGO hot/cold or unwind-state split; 40,135 of 158,648 entries on the PES
  image) is a chunk of the function its unwind info names, directly or through other chunks.
- An unchained entry that starts where the code of the entry before it falls through (that code's control flow reaches
  its end without a ret or jump: 0x140005530's range ends in `jl`, and 0x140005549 holds the rest of the body) is the
  continuation of that function.
- A function's extent is its head entry, its chunks and its continuations (and theirs), adjacent ranges joined.
- A function with no entry (a leaf or a thunk: x64 needs no unwind data for a frameless function) is a function start
  that runs to the next entry start at most.
- A decode never leaves the extent, and a closure walk never admits an address inside an extent other than the head's
  start (a chunk or continuation start, a label, the middle of an instruction) as a function.
- decode(): the instructions of a function. Each range of the extent decodes from its first byte and stops at its first
  ret or tail jump with no forward branch of that range still pending (a range can end in an inline jump table); code
  that only a jump from another range reaches (a cold chunk jumping back into the hot part) is decoded from the target.
"""
import bisect
import json
import os
import struct

IMAGE_BASE = 0x140000000
MAX_RANGE = 0x10000  # bytes of one range decoded (1405DC350 spans > 0x2000); an entry over MAX_FN is corrupt
MAX_FN = 0x100000
UNW_FLAG_CHAININFO = 0x20  # flag 4 in the high five bits of the UNWIND_INFO's first byte
# A range whose last instruction is one of these does not fall into the entry after it. A call may be a noreturn call
# (the next entry is then another function), so it is no fall-through either: ponytail: a continuation entered after a
# call ends its range is not joined; the replay of a function that loses code is the check (driver / snapdiff).
NO_FALL = ("ret", "jmp", "call", "ud2", "int3", "hlt", "nop")
KNOWN = None  # function starts known from the ledger (known_starts); a test sets frozenset() to use none
LEDGER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ledger", "universe.json")


def known_starts():
    """The VAs the ledger universe knows as functions (Ghidra's list, the specs, every recorded function). An entry
    among them heads a function even when the entry before it falls into it: 113 of them are such (0x1400167D0)."""
    global KNOWN
    if KNOWN is None:
        with open(LEDGER, encoding="utf8") as f:
            KNOWN = frozenset(int(k, 16) for k in json.load(f)["functions"])
    return KNOWN


class Pdata:
    def __init__(self, img):
        """img: a binary file object over the flat image (file offset = VA - IMAGE_BASE)."""
        self.img = img
        img.seek(0x3C)
        lfa = struct.unpack("<I", img.read(4))[0]
        img.seek(lfa)
        if img.read(4) != b"PE\0\0":
            raise SystemExit("pdata: not a PE image")
        _, nsec, _, _, _, optsz, _ = struct.unpack("<HHIIIHH", img.read(20))
        opt = img.read(optsz)
        exc_rva, exc_size = struct.unpack_from("<II", opt, 112 + 8 * 3)
        entries, parent = {}, {}
        for i in range(0, exc_size - 11, 12):
            begin, end, unwind = struct.unpack("<III", self.read(IMAGE_BASE + exc_rva + i, 12))
            if not 0 < end - begin <= MAX_FN or IMAGE_BASE + begin in entries:
                continue
            entries[IMAGE_BASE + begin] = IMAGE_BASE + end
            if unwind & 1:
                continue  # an indirect unwind reference: none on the PES image
            info = self.read(IMAGE_BASE + unwind, 4)
            if info[0] & UNW_FLAG_CHAININFO:  # the parent's RUNTIME_FUNCTION follows the (even-padded) unwind codes
                at = IMAGE_BASE + unwind + 4 + ((info[2] + 1) & ~1) * 2
                parent[IMAGE_BASE + begin] = IMAGE_BASE + struct.unpack("<I", self.read(at, 4))[0]
        self.entries = entries  # every entry: begin -> end
        self.starts = sorted(entries)
        self.root = {b: self._root_of(b, parent) for b in parent}  # chunk begin -> the unchained entry it chains to
        self.ends = {b: e for b, e in entries.items() if b not in self.root}  # unchained entries: begin -> end
        self.chunks = {}  # unchained entry -> its chunk entries
        for c, r in self.root.items():
            self.chunks.setdefault(r, []).append(c)
        self.by_end = {}  # end -> the entries ending there
        for b, e in entries.items():
            self.by_end.setdefault(e, []).append(b)
        self._md = None
        self._fall, self._head_of, self._extent = {}, {}, {}

    @staticmethod
    def _root_of(begin, parent):
        for _ in range(len(parent) + 1):
            if begin not in parent:
                return begin
            begin = parent[begin]
        raise SystemExit("pdata: a chain of chained unwind entries loops")

    @staticmethod
    def _join(ranges):
        """Sorted ranges with the adjacent ones merged (a chunk that continues the code before it falls through)."""
        out = []
        for b, e in sorted(ranges):
            if out and b <= out[-1][1]:
                out[-1] = (out[-1][0], max(out[-1][1], e))
            else:
                out.append((b, e))
        return out

    def read(self, va, n):
        self.img.seek(va - IMAGE_BASE)
        return self.img.read(n)

    def _falls_into(self, b):
        """The unchained entry that the code of entry b falls into at its end (adjacent, entered by fall-through),
        else None."""
        if b not in self._fall:
            nxt = self.entries[b]
            self._fall[b] = nxt if nxt in self.ends and nxt not in known_starts() and self._falls_off(b) else None
        return self._fall[b]

    def _falls_off(self, b):
        """True if the walk of entry b reaches its end without a ret or jump (a last instruction that is no
        terminator)."""
        return self._walk(self._md_get(), b, min(self.entries[b], b + MAX_RANGE), {}, set(), set())

    def _head(self, b):
        """The entry that heads the function holding entry b: its chunk's root, and the entry whose code falls into
        it, repeatedly."""
        if b not in self._head_of:
            h = self.root.get(b, b)
            before = [p for p in self.by_end.get(h, ()) if self._falls_into(p) == h]
            self._head_of[b] = self._head(before[0]) if before else h
        return self._head_of[b]

    def end_of(self, va):
        """The end of the .pdata range that starts at va (None if va is not an unchained entry)."""
        return self.ends.get(va)

    def _static(self, b):
        """The ranges of unchained entry b by the unwind data alone: its range, its chunks and its continuations (and
        theirs), adjacent ranges joined."""
        todo, seen = [b], []
        while todo:
            e = todo.pop()
            seen.append(e)
            todo.extend(self.chunks.get(e, ()))
            nxt = self._falls_into(e)
            if nxt is not None:
                todo.append(nxt)
        return self._join([(e, self.entries[e]) for e in seen])

    def extent(self, va):
        """The sorted (begin, end) ranges of the function with a .pdata entry that starts at va (None if va does not
        head such a function): its range, its chunks and continuations, and the unchained entries its conditional
        jumps land in (an unwind region without a chain: 0x14008C980 `je 0x14008C9EA`), adjacent ranges joined. A `jmp`
        to another entry is a tail call, never part of the body (0x14084DF20 `jmp 0x140857010`).
        ponytail: a conditional tail call to another function is absorbed too (it executes inline, same result), and
        an entry more than MAX_RANGE away is never absorbed."""
        if va not in self.ends or self._head(va) != va:
            return None
        if va not in self._extent:
            ranges = self._static(va)
            while True:
                cond = set()
                self._walk_ranges(self._md_get(), ranges, {}, set(), cond)
                new = {self._entry(t) for t in cond if not any(b <= t < e for b, e in ranges)}
                new = [e for e in sorted(x for x in new if x is not None)
                       if e not in self.root and abs(e - va) < MAX_RANGE]
                if not new:
                    break
                ranges = self._join(ranges + [r for e in new for r in self._static(e)])
            self._extent[va] = ranges
        return self._extent[va]

    def next_start(self, va):
        """The first .pdata entry start (chunks included) strictly above va (None past the last)."""
        i = bisect.bisect_right(self.starts, va)
        return self.starts[i] if i < len(self.starts) else None

    def _entry(self, va):
        i = bisect.bisect_right(self.starts, va) - 1
        return self.starts[i] if i >= 0 and va < self.entries[self.starts[i]] else None

    def containing(self, va):
        """The start of the function whose extent holds va (None if none does)."""
        b = self._entry(va)
        return None if b is None else self._head(b)

    def is_boundary(self, va):
        """True if va can start a function: the head of a .pdata function, or an address outside every .pdata entry (a
        leaf or a thunk has no entry; 0x140849D70 follows a jump table, not int3 padding). An address inside an extent
        other than its head's start, a chunk or continuation start included, is a label or mid-instruction: never a
        callee."""
        if va in self.ends:
            return self._head(va) == va
        return va > IMAGE_BASE and self._entry(va) is None

    def same_body(self, a, b):
        """True if a and b can lie in one function: the same extent, or both outside every entry with no entry start
        between them (one pdata-less leaf region).
        ponytail: two pdata-less leaves with no entry between them count as one body; the pdata-less region has no
        function ends to tell them apart (a ledger of Ghidra function starts would)."""
        ca, cb = self.containing(a), self.containing(b)
        if ca is not None or cb is not None:
            return ca == cb
        lo, hi = sorted((a, b))
        nxt = self.next_start(lo)
        return nxt is None or nxt > hi

    def indirect_kind(self, va):
        """'call', 'jmp reg' or 'jmp mem' if the instruction at va is an indirect branch (FF /2, FF /4; legacy and REX
        prefixes skipped), else None. A `jmp reg` inside a function is a switch dispatch (cases are labels of that
        function) or a register tail call."""
        blob = self.read(va, 8)
        i = 0
        while i < len(blob) and (blob[i] in (0x66, 0x67, 0xF0, 0xF2, 0xF3, 0x2E, 0x3E, 0x26, 0x64, 0x65, 0x36)
                                 or 0x40 <= blob[i] <= 0x4F):
            i += 1
        if i + 1 >= len(blob) or blob[i] != 0xFF:
            return None
        reg, mod = (blob[i + 1] >> 3) & 7, blob[i + 1] >> 6
        return "call" if reg == 2 else ("jmp reg" if mod == 3 else "jmp mem") if reg == 4 else None

    def ranges_of(self, va):
        """The ranges a function at va may span: its extent if it has a .pdata entry; a leaf or thunk (no entry, still
        a boundary) runs to the next entry start at most. None if va is not a function boundary."""
        if va in self.ends:
            return self.extent(va)
        if not self.is_boundary(va):
            return None
        nxt = self.next_start(va)
        return [(va, va + MAX_RANGE if nxt is None else min(va + MAX_RANGE, nxt))]

    def decode(self, md, va):
        """The capstone instructions of the function at va, sorted by address; None if va is not a function boundary.
        md: a capstone Cs (x86-64); its detail setting is the caller's."""
        ranges = self.ranges_of(va)
        if ranges is None:
            return None
        out = {}
        self._walk_ranges(md, ranges, out, set(), set())
        return [out[a] for a in sorted(out)]

    def _md_get(self):
        if self._md is None:
            import capstone
            self._md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        return self._md

    def _walk_ranges(self, md, ranges, out, targets, cond):
        """Walk every range of a function from its first byte, then from each jump target in a range that no walk has
        decoded (code only another range reaches). out: address -> instruction; targets: every direct jump target;
        cond: the targets of conditional jumps."""
        tried = set()
        todo = [b for b, _ in ranges]
        while todo:
            b = todo.pop(0)
            if b in out or b in tried:
                continue
            tried.add(b)
            r = next(r for r in ranges if r[0] <= b < r[1])
            self._walk(md, b, min(r[1], b + MAX_RANGE), out, targets, cond)
            todo.extend(t for t in sorted(targets) if t not in out and any(x <= t < y for x, y in ranges))

    def _walk(self, md, begin, end, out, targets, cond):
        """Decode [begin, end) into out, stopping at the first ret or tail jump out of the range with no forward
        branch into the range still pending, or at an address already decoded. Adds every direct jump target to
        targets. True if the walk reached the end of the range with a last instruction that falls through."""
        pending, last = set(), None
        for ins in md.disasm(self.read(begin, end - begin), begin):
            if ins.address in out:
                return False
            out[ins.address] = last = ins
            m = ins.mnemonic
            tgt = None
            if m.startswith("j"):
                try:
                    tgt = int(ins.op_str, 16)
                except ValueError:
                    pass  # jmp reg / jmp [table]: a switch, resolved by the translator's prepass
            if tgt is not None:
                targets.add(tgt)
                if m != "jmp":
                    cond.add(tgt)
                if begin <= tgt < end:
                    pending.add(tgt)
            if m == "ret" or (m == "jmp" and tgt is not None and not begin <= tgt < end):
                pending = {t for t in pending if t > ins.address}
                if not pending:
                    return False
        return last is not None and last.address + last.size == end and last.mnemonic not in NO_FALL
