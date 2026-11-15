#!/usr/bin/env python3
"""Regenerate GF_PES_FBUILDER_STATES: the live-in state and the frame F of REAL calls of PES's AI frame builder FUN_1409F1F20.

PES-derived runtime memory: the output goes to scratch and is NEVER committed (tests/sim/pes_decision_frame_test.cpp reads it through
the GF_PES_FBUILDER_STATES environment variable and skips when it is unset).

Method. Each capture (live2/pidN/seqM, hook fbF_1409F1F20) is replayed by prove_bounded.py (frame_trace recorded + snapdiff --bounded),
built from a SCRATCH COPY of this tool tree and of src/pes/translated whose pes_state.hpp logs every byte the snapshot replay reads
(PES_RLOG): the repository's headers are untouched. The read bytes that the call has not written first are the call's live-in set; each
is filed under the object that holds it (S, T, Q = A+0xC8, G's per-slot records, MI, pitch, ball, P, the event store behind P+0xE0, the side
MA behind P+0x18, G's side records and tables), with the capture's value. The frame F the call left is the capture's entry bytes with the replay's writes into F applied (the replay is proven bit-exact against PES).

Usage: fbuilder_states.py --live2 DIR --scratch DIR --out FILE [--limit N]
Environment: PES_EXE (the memory dump), RV_HOOKS (a hooks.cfg naming fbF_1409F1F20; a one-line file is enough) as for prove_bounded.py.
"""
import argparse, glob, json, os, shutil, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
ROOT = "0x1409F1F20"
HOOK = "fbF_1409F1F20"
F_BYTES = 0x880
OUT_OFFSETS = (0, 4, 0x5C, 0x5E4, 0x14, 0x18, 0x1C, 0x2C, 0x30, 0x34, 0x54, 0x58)

RLOG_HOOK = '''
#include <cstdio>
#include <cstdlib>
#include <set>
struct PesRLog {
  std::set<uint64_t> reads, writes;
  ~PesRLog() {
    const char* path = std::getenv("PES_RLOG");
    if (!path) return;
    FILE* f = std::fopen(path, "a");
    if (!f) return;
    for (uint64_t a : reads) std::fprintf(f, "%llx\\n", (unsigned long long)a);
    std::fclose(f);
  }
};
inline PesRLog g_rlog;
inline void pes_rlog_note(int write, uint64_t a, unsigned b = 0) {
  if (write) {
    g_rlog.writes.insert(a);
    // The bytes the call writes into its output frame F (PES_WF = F's address): the final F is the entry bytes with these applied.
    static const char* wf = std::getenv("PES_WF");
    static uint64_t base = wf ? std::strtoull(wf, nullptr, 16) : 0;
    if (base && a >= base && a < base + 0x880) {
      FILE* f = std::fopen(std::getenv("PES_WLOG"), "a");
      if (f) { std::fprintf(f, "%llx %02x\\n", (unsigned long long)(a - base), b); std::fclose(f); }
    }
  } else if (!g_rlog.writes.count(a)) g_rlog.reads.insert(a);
}
'''


def scratch_copy(scratch):
    """A copy of tools/dev and src/pes/translated whose pes_state.hpp logs snapshot reads and writes."""
    dst = os.path.join(scratch, "rlcopy")
    if os.path.exists(dst):
        shutil.rmtree(dst)
    os.makedirs(os.path.join(dst, "src", "pes"))
    os.makedirs(os.path.join(dst, "tools"))
    shutil.copytree(os.path.join(REPO, "src", "pes", "translated"), os.path.join(dst, "src", "pes", "translated"))
    shutil.copytree(os.path.join(REPO, "tools", "dev"), os.path.join(dst, "tools", "dev"))
    p = os.path.join(dst, "src", "pes", "translated", "pes_state.hpp")
    s = open(p).read()
    a = "        if (const uint8_t* b = snap->at(addr + i)) v |= (uint64_t)*b << (8 * i);"
    b = ("      for (unsigned i = 0; i < n; ++i) {\n        pes_rlog_note(1, addr + i, (unsigned)((v >> (8 * i)) & 0xff));\n"
         "        if (uint8_t* b = snap->at(addr + i)) {\n          *b = (uint8_t)(v >> (8 * i));\n"
         "          log_bytes(addr + i, v >> (8 * i), 1);\n        }\n      }\n      return;")
    c = ("      for (unsigned i = 0; i < n; ++i)\n        if (uint8_t* b = snap->at(addr + i)) {\n          *b = (uint8_t)(v >> (8 * i));\n"
         "          log_bytes(addr + i, v >> (8 * i), 1);\n        }\n      return;")
    assert a in s and c in s, "pes_state.hpp changed: update the read-log patch"
    s = s.replace(a, "        { pes_rlog_note(0, addr + i); if (const uint8_t* b = snap->at(addr + i)) v |= (uint64_t)*b << (8 * i); }", 1)
    s = s.replace(c, b, 1)
    i = s.index("struct SnapView")
    s = s[:i] + RLOG_HOOK + s[i:]
    open(p, "w").write(s)
    return dst


def discover(live2):
    caps = []
    for pid in sorted(glob.glob(os.path.join(live2, "pid*"))):
        for seq in sorted(glob.glob(os.path.join(pid, "seq*"))):
            ix = os.path.join(seq, "index.json")
            try:
                if '"hook": "%s"' % HOOK in open(ix).read():
                    caps.append(seq)
            except OSError:
                pass
    return caps


def unpack(src, dst):
    shutil.rmtree(dst, ignore_errors=True)
    os.makedirs(dst)
    for f in os.listdir(src):
        shutil.copy(os.path.join(src, f), dst)
    if os.path.exists(os.path.join(dst, "mem.bin.zst")):
        subprocess.run([sys.executable, os.path.join(REPO, "tools/dev/rendezvous/capz.py"), "unpack", dst], capture_output=True, check=True)


def regions(m, call, image):
    sys.path.insert(0, os.path.join(REPO, "tools/dev/translate"))
    u = lambda a, k=8: int.from_bytes(m.read(a, k), "little")
    S, T, A = call[2], call[8], call[3]
    slot = u(S + 4, 4)
    G = u(0x1436F3DC8)
    P = u(S + 0x38)
    MI = u(G + 0x270)
    ordered = [("rec%x" % o, u(G + o), 0xA300) for o in (0x1928, 0x1950, 0x1978)]
    ordered += [("S", S, 0x50), ("T", T, 0x1A00), ("Q", A + 0xC8, 0x700), ("G", G, 0x2E58), ("MI", MI, 0x1780), ("P", P, 0x3C00),
                ("human", u(G + 0x19C8 + 0x28 * slot), 0x200), ("man", u(G + 0x1DD8 + 0x28 * slot), 0x700),
                ("ai", u(G + 0x25F8 + 0x28 * slot), 0x1500), ("pitch", u(G + 0x2AA8), 0x48), ("ball", u(G + 0x40), 0x3000),
                ("G3b0", u(G + 0x3B0), 0x100), ("G180", u(G + 0x180), 0xB00), ("ring", u(P + 0xE0), 0x30000),
                ("obj18", u(P + 0x18), 0x30000)]
    for i in range(22):
        if i != slot:
            ordered += [("human#%d" % i, u(G + 0x19C8 + 0x28 * i), 0x200), ("man#%d" % i, u(G + 0x1DD8 + 0x28 * i), 0x700),
                        ("ai#%d" % i, u(G + 0x25F8 + 0x28 * i), 0x1500)]
    return slot, ordered


def final_frame(m, F, wlog):
    """F as the call leaves it: the capture's entry bytes with the call's writes (in order) applied."""
    frame = bytearray(m.read(F, F_BYTES))
    for line in open(wlog):
        off, val = line.split()
        frame[int(off, 16)] = int(val, 16)
    return bytes(frame)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--live2", required=True)
    ap.add_argument("--scratch", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--limit", type=int, default=0, help="only the first N captures (a smoke run)")
    a = ap.parse_args()
    image = os.environ["PES_EXE"]
    hooks = os.environ["RV_HOOKS"]
    caps = discover(a.live2)
    assert caps, "no %s captures under %s" % (HOOK, a.live2)
    if a.limit:
        caps = caps[:a.limit]
    os.makedirs(a.scratch, exist_ok=True)
    copy = scratch_copy(a.scratch)
    sys.path.insert(0, os.path.join(REPO, "tools/dev/translate"))
    from full_snapshot import SnapMem
    img = open(image, "rb")
    work = os.path.join(a.scratch, "fb_work")
    out = open(a.out, "w")
    out.write("# PES-derived runtime memory (live-in bytes of FUN_1409F1F20 calls and the frame F each left): never committed.\n"
              "# ROW <pid> <seq> slot <n> / OUT (F words %s) / FULL (F, 0x880 bytes) / <object> <offset> <bytes>.\n"
              "# Generated by tools/dev/translate/fbuilder_states.py\n" % ",".join(hex(o) for o in OUT_OFFSETS))
    for n, src in enumerate(caps, 1):
        d = os.path.join(a.scratch, "fb_cap")
        unpack(src, d)
        log = os.path.join(a.scratch, "fb_rlog_%d.txt" % n)
        if os.path.exists(log):
            os.remove(log)
        shutil.rmtree(work, ignore_errors=True)
        wlog = os.path.join(a.scratch, "fb_wlog_%d.txt" % n)
        if os.path.exists(wlog):
            os.remove(wlog)
        open(wlog, "w").close()
        F = struct.unpack("<49Q", open(os.path.join(d, "call.bin"), "rb").read(49 * 8))[1]
        env = dict(os.environ, PES_RLOG=log, RV_HOOKS=hooks, PES_WF="%x" % F, PES_WLOG=wlog)
        subprocess.run([sys.executable, os.path.join(copy, "tools/dev/translate/prove_bounded.py"), ROOT, work, "--tag", "fb", "--real", d],
                       env=env, check=True, stdout=subprocess.DEVNULL)
        m = SnapMem(d)
        call = struct.unpack("<49Q", open(os.path.join(d, "call.bin"), "rb").read(49 * 8))
        slot, regs = regions(m, call, image)
        frame = final_frame(m, call[1], wlog)
        os.remove(wlog)
        w = lambda o: struct.unpack_from("<I", frame, o)[0]
        pid, seq = src.rstrip("/").split("/")[-2:]
        out.write("ROW %s %s slot %d\n" % (pid, seq, slot))
        out.write("OUT %s\n" % " ".join("%x" % w(o) for o in OUT_OFFSETS))
        out.write("FULL %s\n" % frame.hex())
        reads = sorted(set(int(l, 16) for l in open(log)))
        cur = None
        for addr in reads:
            tag = None
            if 0x140000000 <= addr < 0x144000000:  # the image: only bytes the capture holds differently from the memory dump
                img.seek(addr - 0x140000000)
                if m.read(addr, 1) == img.read(1):
                    continue
                tag = ("img", addr)
            else:
                for name, base, size in regs:
                    if base and base <= addr < base + size:
                        tag = (name, addr - base)
                        break
                if tag is None:
                    tag = ("va", addr)
            v = m.read(addr, 1)
            if cur and cur[0] == tag[0] and cur[1] + len(cur[2]) == tag[1]:
                cur = (cur[0], cur[1], cur[2] + v)
            else:
                if cur:
                    out.write("%s %x %s\n" % (cur[0], cur[1], cur[2].hex()))
                cur = (tag[0], tag[1], v)
        if cur:
            out.write("%s %x %s\n" % (cur[0], cur[1], cur[2].hex()))
        shutil.rmtree(d, ignore_errors=True)
        os.remove(log)
        print("call %d/%d %s/%s slot %d" % (n, len(caps), pid, seq, slot), flush=True)
    out.close()


if __name__ == "__main__":
    main()
