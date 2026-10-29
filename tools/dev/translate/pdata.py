#!/usr/bin/env python3
"""pdata.py: function starts, extents and decode from the image's .pdata (the exception directory's RUNTIME_FUNCTION table).

The one place the tools learn what a function is without the Ghidra bridge.
- A function is a .pdata entry that no other entry chains to. A chained entry (UNWIND_INFO flag 4, a PGO hot/cold or
  unwind-state split: 40,135 of 158,648 entries on the PES image) is a chunk of the function its unwind info names,
  directly or through other chunks; the function's extent is its own range plus its chunks, adjacent ranges joined.
- A function with no entry (a leaf or a thunk: x64 needs no unwind data for a frameless function) is a function start
  that runs to the next entry start at most.
- A decode never leaves the extent, and a closure walk never admits an address inside an extent other than the root's
  start (a chunk start, a label, the middle of an instruction) as a function.
- decode(): the instructions of a function. Each range of the extent decodes from its first byte and stops at its first
  ret or tail jump with no forward branch of that range still pending (a range can end in an inline jump table); code
  that only a jump from another range reaches (a cold chunk jumping back into the hot part) is decoded from the target.
"""
import bisect
import struct

IMAGE_BASE = 0x140000000
MAX_RANGE = 0x10000  # bytes of one range decoded (1405DC350 spans > 0x2000); an entry over MAX_FN is corrupt
MAX_FN = 0x100000
UNW_FLAG_CHAININFO = 0x20  # flag 4 in the high five bits of the UNWIND_INFO's first byte


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
        self.entries = entries  # every entry, chunks included: begin -> end
        self.starts = sorted(entries)
        self.root = {b: self._root_of(b, parent) for b in parent}  # chunk begin -> the function it belongs to
        self.ends = {b: e for b, e in entries.items() if b not in self.root}  # function starts: begin -> end
        chunks = {}
        for c, r in self.root.items():
            chunks.setdefault(r, []).append((c, entries[c]))
        self._extent = {}
        for r, cs in chunks.items():
            self._extent[r] = self._join([(r, entries[r])] + cs)

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
            if out and out[-1][1] == b:
                out[-1] = (out[-1][0], e)
            else:
                out.append((b, e))
        return out

    def read(self, va, n):
        self.img.seek(va - IMAGE_BASE)
        return self.img.read(n)

    def end_of(self, va):
        """The end of the .pdata range of the function that starts at va (None if va is not a function start)."""
        return self.ends.get(va)

    def extent(self, va):
        """The sorted (begin, end) ranges of the function with a .pdata entry that starts at va: its own range and its
        chunks, adjacent ranges joined (None if va is not such a function's start)."""
        if va not in self.ends:
            return None
        return self._extent.get(va, [(va, self.ends[va])])

    def next_start(self, va):
        """The first .pdata entry start (chunks included) strictly above va (None past the last)."""
        i = bisect.bisect_right(self.starts, va)
        return self.starts[i] if i < len(self.starts) else None

    def _entry(self, va):
        i = bisect.bisect_right(self.starts, va) - 1
        return self.starts[i] if i >= 0 and va < self.entries[self.starts[i]] else None

    def inside(self, va):
        """True if va lies in an entry of a function other than at that function's start: after its start, or in a
        chunk (a chunk's first byte included)."""
        b = self._entry(va)
        return b is not None and (b != va or va in self.root)

    def containing(self, va):
        """The start of the function whose extent holds va (None if none does)."""
        b = self._entry(va)
        return None if b is None else self.root.get(b, b)

    def is_boundary(self, va):
        """True if va can start a function: a .pdata function start, or an address outside every .pdata entry (a leaf or
        a thunk has no entry; 0x140849D70 follows a jump table, not int3 padding). An address inside an extent other
        than its root's start, a chunk start included, is a label or mid-instruction: never a callee."""
        return va in self.ends or (va > IMAGE_BASE and self._entry(va) is None)

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
        out, targets, tried = {}, set(), set()
        todo = [b for b, _ in ranges]
        while todo:
            b = todo.pop(0)
            if b in out or b in tried:
                continue
            tried.add(b)
            r = next(r for r in ranges if r[0] <= b < r[1])
            self._walk(md, b, min(r[1], b + MAX_RANGE), out, targets)
            todo.extend(t for t in sorted(targets) if t not in out and any(x <= t < y for x, y in ranges))
        return [out[a] for a in sorted(out)]

    def _walk(self, md, begin, end, out, targets):
        """Decode [begin, end) into out, stopping at the first ret or tail jump out of the range with no forward
        branch into the range still pending, or at an address already decoded. Adds every direct jump target to
        targets."""
        pending = set()
        for ins in md.disasm(self.read(begin, end - begin), begin):
            if ins.address in out:
                return
            out[ins.address] = ins
            m = ins.mnemonic
            tgt = None
            if m.startswith("j"):
                try:
                    tgt = int(ins.op_str, 16)
                except ValueError:
                    pass  # jmp reg / jmp [table]: a switch, resolved by the translator's prepass
            if tgt is not None:
                targets.add(tgt)
                if begin <= tgt < end:
                    pending.add(tgt)
            if m == "ret" or (m == "jmp" and tgt is not None and not begin <= tgt < end):
                pending = {t for t in pending if t > ins.address}
                if not pending:
                    return
