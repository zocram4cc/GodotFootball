#!/usr/bin/env python3
"""usage: roster_rows.py OUT.tsv CAPDIR [CAPDIR ...]

For every slot of each full rendezvous capture (mem.bin + index.json) read PES's roster row, the block the stat rebuild
0x14092AF10 starts from (G = [0x1436F3DC8]; team t's roster at G+(0xA3+t)*0x28, row = roster+0xA8+e*0x128+0x38, e = the
slot's entry at roster+4+4*(slot%11); the row's player id is the u32 at +0xE0), and write one line per distinct player:
`id speed jump height foot` = row bytes 0x27, 0x2C, 0x31, 0x34. GF reads the same four through the save-patched
PlayerDatabase (tests/pes_playerdb_test.cpp EveryCapturedRosterRowEqualsTheDatabaseWithTheSaveApplied,
env GF_PES_ROSTER_ROWS). The output is PES's data: keep it out of the repository. A capture that does not map the roster
(slim, or taken outside a match) is skipped with a note on stderr.
"""
import bisect, json, struct, sys

G_VAR = 0x1436F3DC8
ROW_BYTES = 0x128
SLOTS = 22
ROSTER_STRIDE = 0x28
ROSTER_TABLE_FIRST = 0xA3
ROW_ID_AT = 0xE0
SPEED, JUMP, HEIGHT, FOOT = 0x27, 0x2C, 0x31, 0x34


class Capture:
    def __init__(self, d):
        index = json.load(open(d + '/index.json'))
        self.maps = sorted((m['va'], m['size'], m['off']) for m in index['mappings'])
        self.starts = [m[0] for m in self.maps]
        self.mem = open(d + '/mem.bin', 'rb')

    def read(self, va, n):
        out = b''
        while n > 0:
            i = bisect.bisect_right(self.starts, va) - 1
            if i < 0 or va >= self.maps[i][0] + self.maps[i][1]:
                raise KeyError(hex(va))
            base, size, off = self.maps[i]
            k = min(n, base + size - va)
            self.mem.seek(off + va - base)
            out += self.mem.read(k)
            va += k
            n -= k
        return out

    def u64(self, va):
        return struct.unpack('<Q', self.read(va, 8))[0]

    def u32(self, va):
        return struct.unpack('<I', self.read(va, 4))[0]


def rows_of(cap):
    g = cap.u64(G_VAR)
    for slot in range(SLOTS):
        roster = cap.u64(g + (ROSTER_TABLE_FIRST + slot // 11) * ROSTER_STRIDE)
        entry = cap.u32(roster + 4 + 4 * (slot % 11))
        row = cap.read(roster + 0xA8 + entry * ROW_BYTES + 0x38, ROW_BYTES)
        yield int.from_bytes(row[ROW_ID_AT:ROW_ID_AT + 4], 'little'), (row[SPEED], row[JUMP], row[HEIGHT], row[FOOT])


def main():
    out, dirs = sys.argv[1], sys.argv[2:]
    seen = {}
    for d in dirs:
        try:
            for pid, row in rows_of(Capture(d)):
                if pid:
                    seen[pid] = row
        except (KeyError, OSError, ValueError) as e:
            print('skip', d, type(e).__name__, e, file=sys.stderr)
    with open(out, 'w') as f:
        for pid in sorted(seen):
            f.write('%d %d %d %d %d\n' % ((pid,) + seen[pid]))
    print(len(seen), 'players')


if __name__ == '__main__':
    main()
