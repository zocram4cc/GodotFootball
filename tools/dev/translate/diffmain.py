#!/usr/bin/env python3
"""diffmain.py: generate the C++ diff-test main for one function.

Feeds the harness corpus into the translated C++ and compares outputs bit
for bit. Memory setup mirrors run.py set_inputs + regions_init/patch_addrs.
Floats compare by IEEE bits.
"""
import json
import struct

GPR = {"rcx": 1, "rdx": 2, "r8": 8, "r9": 9, "rax": 0, "rbx": 3,
       "rbp": 5, "rsp": 4, "eax": 0, "ebx": 3, "r8d": 8, "edx": 2,
       "ecx": 1}
XMM = {"xmm0": 0, "xmm1": 1, "xmm2": 2, "xmm3": 3}

def ident(name):
    # Region names carry dots (IMG.1436F3DC8, A1.28): not C identifiers.
    return name.replace(".", "_").replace("-", "_")


def write_instances(spec, instances, path):
    """Writes the diff main's instance file (inst_apply reads it at run time): u64 n, then n u64
    offsets of the instance records, then each instance's records in spec region order.
    A record is {u64 off, u64 len, len bytes}; len 0 leaves the region unseeded. instances: a
    sequence of seed artifacts (a Bank, a list, or [None] for one unseeded instance)."""
    sp = json.load(open(spec)) if isinstance(spec, str) else spec
    regions = sp.get("regions") or {}
    rnames = list(regions.keys())
    n = len(instances)
    offs = []
    with open(path, "wb") as f:
        f.write(struct.pack("<Q", n) + bytes(8 * n))
        for k in range(n):
            offs.append(f.tell())
            seeded = {r["name"]: r for r in (instances[k] or {}).get("regions") or []}
            for name in rnames:
                if name not in seeded:
                    f.write(struct.pack("<QQ", 0, 0))
                    continue
                r = seeded[name]
                blob = bytes.fromhex(r["hex"])
                off = int(r["off"])
                if off + len(blob) > int(regions[name]["size"], 16):
                    raise ValueError("seed for %s exceeds its region" % name)
                f.write(struct.pack("<QQ", off, len(blob)) + blob)
        f.seek(8)
        f.write(struct.pack("<%dQ" % n, *offs))

def gen(spec, va, callees):
    sp = json.load(open(spec)) if isinstance(spec, str) else spec
    V = va[2:]
    regions = sp.get("regions") or {}
    rnames = list(regions.keys())
    ids = {name: ident(name) for name in rnames}
    L = []
    L.append('// Generated diff main for %s (build tree only).' % va)
    L.append('#include "pes_state.hpp"')
    L.append('#include "pes_cond.inl"')
    L.append('#include "diffparse.hpp"')
    L.append('#include <cstdio>')
    L.append('#include <cstring>')
    L.append('#include <string>')
    L.append('#include <fcntl.h>')
    L.append('#include <sys/mman.h>')
    L.append('#include <sys/stat.h>')
    L.append('#include <unistd.h>')
    L.append('#include <vector>')
    fns = sorted({V} | {c[2:] for c in callees})
    for f in fns:
        L.append('void pes_fn_%s(PesState& s);' % f)
    # region base accessors (sanitized: dots are not C identifiers)
    for i, name in enumerate(rnames):
        L.append('static const uint64_t base_%s = %sULL;' % (ids[name], regions[name]["base"]))
        L.append('static const uint64_t size_%s = %sULL;' % (ids[name], regions[name]["size"]))
    stack = sp.get("stack", {"base": "0x10000000", "size": "0x20000"})
    L.append('static const uint64_t stack_base = %sULL;' % stack["base"])
    L.append('static const uint64_t stack_size = %sULL;' % stack["size"])
    # Instance records are read at run time from argv[3] (write_instances): a bank is too big to
    # compile in. The segments are fresh per case, so unseeded regions stay zero, as in run.py.
    L.append('struct InstFile { int fd = -1; uint64_t n = 0; std::vector<uint64_t> off; };')
    L.append('static bool read_full(int fd, void* buf, size_t n, uint64_t pos) {')
    L.append('  uint8_t* p = (uint8_t*)buf;')
    L.append('  while (n) { ssize_t r = pread(fd, p, n, (off_t)pos); if (r <= 0) return false; p += r; pos += (uint64_t)r; n -= (size_t)r; }')
    L.append('  return true;')
    L.append('}')
    L.append('static bool inst_open(const char* path, InstFile& f) {')
    L.append('  f.fd = open(path, O_RDONLY); if (f.fd < 0) return false;')
    L.append('  if (!read_full(f.fd, &f.n, 8, 0)) return false;')
    L.append('  f.off.resize((size_t)f.n);')
    L.append('  return f.n == 0 || read_full(f.fd, f.off.data(), 8 * (size_t)f.n, 8);')
    L.append('}')
    L.append('static bool inst_apply(const InstFile& f, uint64_t k, PesState& s) {')
    L.append('  if (k >= f.n) return false;')
    L.append('  uint64_t pos = f.off[(size_t)k];')
    L.append('  for (size_t i = 0; i < %d; ++i) {' % len(rnames))
    L.append('    uint64_t hdr[2];')
    L.append('    if (!read_full(f.fd, hdr, 16, pos)) return false;')
    L.append('    pos += 16;')
    L.append('    if (hdr[1] == 0) continue;')
    L.append('    if (hdr[0] + hdr[1] > s.mem.segs[i].bytes.size()) return false;')
    L.append('    std::vector<uint8_t> b((size_t)hdr[1]);')
    L.append('    if (!read_full(f.fd, b.data(), b.size(), pos)) return false;')
    L.append('    pos += hdr[1];')
    L.append('    std::memcpy(s.mem.segs[i].bytes.data() + hdr[0], b.data(), b.size());')
    L.append('  }')
    L.append('  return true;')
    L.append('}')
    L.append('int main(int argc, char** argv) {')
    L.append('  if (argc < 4) return 2;')
    L.append('  FILE* f = fopen(argv[1], "r"); if (!f) return 2;')
    L.append('  int ifd = open(argv[2], O_RDONLY); if (ifd < 0) return 2;')
    L.append('  InstFile ins; if (!inst_open(argv[3], ins)) return 2;')
    L.append('  struct stat ist; if (fstat(ifd, &ist) != 0) return 2;')
    L.append('  // The image is mapped the way run.py maps a flat dump (file offset = VA - 0x140000000),')
    L.append('  // copy-on-write, so writes persist across cases exactly as in the harness mapping.')
    L.append('  void* ip = mmap(nullptr, (size_t)ist.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, ifd, 0);')
    L.append('  if (ip == MAP_FAILED) return 2;')
    L.append('  uint8_t* const flat = (uint8_t*)ip; const uint64_t flat_n = (uint64_t)ist.st_size;')
    L.append('  char buf[1 << 22]; long nc = 0, bad = 0;')
    L.append('  while (fgets(buf, sizeof buf, f)) {')
    L.append('    if (buf[0] != \'{\') continue;')
    L.append('    JLine jl; if (!parse_line(buf, jl)) { printf("parse fail\\n"); return 2; }')
    L.append('    PesState s;')
    L.append('    s.mem.flat = flat; s.mem.flat_base = 0x140000000ULL; s.mem.flat_size = flat_n;')
    for name in rnames:
        L.append('    s.mem.segs.push_back({base_%s, std::vector<uint8_t>(size_%s, 0)});' % (ids[name], ids[name]))
    L.append('    s.mem.segs.push_back({stack_base, std::vector<uint8_t>(stack_size, 0)});')
    L.append('    if (!inst_apply(ins, (uint64_t)jl.inst, s)) { printf("instance %d unreadable\\n", (int)jl.inst); return 2; }')
    L.append('    const uint64_t stack_top = stack_base + stack_size - 0x100ULL;')
    L.append('    s.gpr[4] = stack_top - 8; s.wr(s.gpr[4], 0ULL, 8);')
    # args
    for reg, val in (sp.get("args") or {}).items():
        if isinstance(val, str) and val in regions:
            L.append('    s.gpr[%d] = base_%s;' % (GPR[reg], ids[val]))
        else:
            L.append('    s.gpr[%d] = %sULL;' % (GPR[reg], val))
    # regions_init linkage
    for dst, val in (sp.get("regions_init") or {}).items():
        dname, doff = dst.split("+", 1)
        if isinstance(val, str) and val in regions:
            L.append('    s.wr(base_%s + %s, base_%s, 8);' % (ids[dname], doff, ids[val]))
        else:
            L.append('    s.wr(base_%s + %s, (uint64_t)(%s), 4);' % (ids[dname], doff, val))
    # patch_addrs: the image VA holds a pointer the function dereferences
    # (e.g. 0x1436F3DC8 -> the IMG shadow region). run.py writes the region
    # base there before each case; the flat image view takes the write, so the
    # translated side reads it back the same way.
    for vas, rname in (sp.get("patch_addrs") or {}).items():
        L.append('    s.wr(%sULL, base_%s, 8);' % (vas, ids[rname]))
    # inputs
    inputs = sp.get("inputs") or {}
    for name, loc in inputs.items():
        enc = loc.get("enc", "u32")
        if "reg" in loc:
            reg = loc["reg"]
            if reg in XMM:
                L.append('    { auto it = jl.in.find("%s"); uint32_t _b = it == jl.in.end() ? 0 : (it->second.t == JVal::BITS ? it->second.bits : (uint32_t)it->second.n); s.set_x_f32(%d, PesState::f32(_b)); }' % (name, XMM[reg]))
            else:
                L.append('    { auto it = jl.in.find("%s"); uint64_t _v = it == jl.in.end() ? 0 : (it->second.t == JVal::BITS ? it->second.bits : (uint64_t)it->second.n); s.gpr[%d] = _v; }' % (name, GPR[reg]))
        elif "stack_offset" in loc:
            n = {"u8": 1, "u32": 4, "u64": 8, "i32": 4, "f32": 4}[enc]
            L.append('    { auto it = jl.in.find("%s"); uint64_t _v = it == jl.in.end() ? 0 : (it->second.t == JVal::BITS ? it->second.bits : (uint64_t)it->second.n); s.wr(stack_top + %s, _v, %d); }' % (name, loc["stack_offset"], n))
        elif "region" in loc:
            n = {"u8": 1, "u32": 4, "u64": 8, "i32": 4, "f32": 4}[enc]
            L.append('    { auto it = jl.in.find("%s"); uint64_t _v = it == jl.in.end() ? 0 : (it->second.t == JVal::BITS ? it->second.bits : (uint64_t)it->second.n); s.wr(base_%s + %s, _v, %d); }' % (name, ids[loc["region"]], loc["offset"], n))
        elif "absolute" in loc:
            n = {"u8": 1, "u32": 4, "u64": 8, "i32": 4, "f32": 4}[enc]
            L.append('    { auto it = jl.in.find("%s"); uint64_t _v = it == jl.in.end() ? 0 : (it->second.t == JVal::BITS ? it->second.bits : (uint64_t)it->second.n); s.wr(%sULL, _v, %d); }' % (name, loc["absolute"], n))
        elif "absfill" in loc:
            rname, off_key, typ = loc["absfill"]
            n = {"u8": 1, "u32": 4, "u64": 8, "i32": 4, "f32": 4}[typ]
            L.append('    { auto jt = jl.in.find("%s"); uint64_t _a = (uint64_t)(jt == jl.in.end() ? 0 : jt->second.n); auto it = jl.in.find("%s"); uint64_t _v = it == jl.in.end() ? 0 : (it->second.t == JVal::BITS ? it->second.bits : (uint64_t)it->second.n); s.wr(base_%s + (_a & 0xFFFFFF), _v, %d); }' % (off_key, name, ids[rname], n))
    # stub-fed inputs are provided by the harness only; translated code runs
    # callees for real, so nothing to do here.
    L.append('    pes_fn_%s(s);' % V)
    # outputs
    for name, loc in (sp.get("outputs") or {}).items():
        if "reg" in loc:
            reg = loc["reg"]
            if reg in XMM:
                L.append('    { auto it = jl.out.find("%s"); uint32_t _want = it == jl.out.end() ? 0 : it->second.bits; uint32_t _got = PesState::bits(s.x_f32(%d)); if (_want != _got) { if (bad < 10) printf("case %%ld: %s want %%08X got %%08X\\n", nc, _want, _got); ++bad; } }' % (name, XMM[reg], name))
            elif reg in ("eax", "ebx", "ecx", "edx", "r8d"):
                # 32-bit outputs (Map ret_eax): compare low 32 bits; the
                # harness decode masks the same way.
                L.append('    { auto it = jl.out.find("%s"); uint64_t _want = it == jl.out.end() ? 0 : (uint64_t)it->second.n; uint64_t _got = s.gpr[%d]; if ((_want & 0xFFFFFFFFULL) != (_got & 0xFFFFFFFFULL)) { if (bad < 10) printf("case %%ld: %s want %%llX got %%llX\\n", nc, (unsigned long long)_want, (unsigned long long)_got); ++bad; } }' % (name, GPR[reg], name))
            else:
                # Full 64-bit outputs (rax pointer returns): bitwise.
                L.append('    { auto it = jl.out.find("%s"); uint64_t _want = it == jl.out.end() ? 0 : (uint64_t)it->second.n; uint64_t _got = s.gpr[%d]; if (_want != _got) { if (bad < 10) printf("case %%ld: %s want %%llX got %%llX\\n", nc, (unsigned long long)_want, (unsigned long long)_got); ++bad; } }' % (name, GPR[reg], name))
        elif "region" in loc:
            enc = loc.get("enc", "u32")
            n = {"u8": 1, "u32": 4, "u64": 8, "i32": 4, "f32": 4}[enc]
            if enc == "f32":
                L.append('    { auto it = jl.out.find("%s"); uint32_t _want = it == jl.out.end() ? 0 : it->second.bits; float _gf = s.rdf(base_IDS + OFF); uint32_t _got = PesState::bits(_gf); if (_want != _got) { if (bad < 10) printf("case %%ld: %s want %%08X got %%08X\\n", nc, _want, _got); ++bad; } }'.replace('IDS', ids[loc["region"]]).replace('OFF', loc["offset"]) % (name, name))
            elif enc == "u64":
                L.append('    { auto it = jl.out.find("%s"); uint64_t _want = it == jl.out.end() ? 0 : (uint64_t)it->second.n; uint64_t _got = s.rd(base_IDS + OFF, 8); if (_want != _got) { if (bad < 10) printf("case %%ld: %s want %%llX got %%llX\\n", nc, (unsigned long long)_want, (unsigned long long)_got); ++bad; } }'.replace('IDS', ids[loc["region"]]).replace('OFF', loc["offset"]) % (name, name))
            else:
                L.append('    { auto it = jl.out.find("%s"); uint64_t _want = it == jl.out.end() ? 0 : (uint64_t)it->second.n; uint64_t _got = s.rd(base_IDS + OFF, %d); if (_want != _got) { if (bad < 10) printf("case %%ld: %s want %%llu got %%llu\\n", nc, (unsigned long long)_want, (unsigned long long)_got); ++bad; } }'.replace('IDS', ids[loc["region"]]).replace('OFF', loc["offset"]) % (name, n, name))
    L.append('    ++nc;')
    L.append('  }')
    L.append('  printf("%s: %ld cases, %ld mismatch\\n", argv[1], nc, bad);')
    L.append('  fclose(f);')
    L.append('  return bad != 0;')
    L.append('}')
    # Every mismatching field is printed (the report lists each case), not the first ten.
    return ("\n".join(L) + "\n").replace("if (bad < 10)", "if (true)")


def gen_snap(va, callees):
    """The snapshot diff main for one function. argv: ok_calls.bin (the replayable calls of one
    snapshot, snapcase.py), mem.bin, maps.txt (its readable runs), out.jsonl, ok_delta.bin (each call's delta pages),
    pool/pages.bin (their bytes) and the snapshot's TEB (hex, record.json "teb"): the GS base each call runs with.
    Each call starts from its recorded registers over the snapshot's own pages (SnapView, copied on first touch and
    dropped per call), with its return address at [rsp]. One output line per call in the harness corpus's
    format: rax, the full xmm0, and every byte written (address and value, in order)."""
    V = va[2:]
    fns = sorted({V} | {c[2:] for c in callees})
    L = []
    L.append('// Generated snapshot diff main for %s (build tree only).' % va)
    L.append('#include "pes_state.hpp"')
    L.append('#include "pes_cond.inl"')
    L.append('#include <cstdint>')
    L.append('#include <cstdio>')
    L.append('#include <cstdlib>')
    L.append('#include <fcntl.h>')
    L.append('#include <sys/stat.h>')
    L.append('#include <unistd.h>')
    L.append('#include <vector>')
    for f in fns:
        L.append('void pes_fn_%s(PesState& s);' % f)
    L.append('// One traced call (snapcase.py CALL_REC): 16 GPRs, 16 XMM (lo, hi), the return address.')
    L.append('struct CallRec { uint64_t v[49]; };')
    L.append('int main(int argc, char** argv) {')
    L.append('  if (argc < 8) return 2;')
    L.append('  int mfd = open(argv[2], O_RDONLY); if (mfd < 0) return 2;')
    L.append('  SnapView snap; snap.fd = mfd;')
    L.append('  int pfd = open(argv[6], O_RDONLY); if (pfd < 0) return 2;')
    L.append('  struct stat dst; if (stat(argv[5], &dst) != 0) return 2;')
    L.append('  std::vector<uint64_t> dwords(dst.st_size / 8);')
    L.append('  FILE* df = fopen(argv[5], "rb"); if (!df) return 2;')
    L.append('  if (!dwords.empty() && fread(dwords.data(), 8, dwords.size(), df) != dwords.size()) return 2;')
    L.append('  fclose(df);')
    L.append('  size_t dpos = 0;')
    L.append('  FILE* mp = fopen(argv[3], "r"); if (!mp) return 2;')
    L.append('  unsigned long long rva, rsize, roff;')
    L.append('  while (fscanf(mp, "%llx %llx %llx", &rva, &rsize, &roff) == 3)')
    L.append('    snap.runs.push_back({rva, rsize, roff});')
    L.append('  fclose(mp);')
    L.append('  struct stat st; if (stat(argv[1], &st) != 0) return 2;')
    L.append('  size_t n = (size_t)st.st_size / sizeof(CallRec);')
    L.append('  std::vector<CallRec> recs(n);')
    L.append('  FILE* cf = fopen(argv[1], "rb"); if (!cf) return 2;')
    L.append('  if (n && fread(recs.data(), sizeof(CallRec), n, cf) != n) return 2;')
    L.append('  fclose(cf);')
    L.append('  FILE* out = fopen(argv[4], "w"); if (!out) return 2;')
    L.append('  for (size_t i = 0; i < n; ++i) {')
    L.append('    snap.over.clear();')
    L.append('    uint64_t np = dwords[dpos++];')
    L.append('    for (uint64_t k = 0; k < np; ++k) {')
    L.append('      uint64_t va = dwords[dpos], off = dwords[dpos + 1]; dpos += 2;')
    L.append('      SnapView::Block b{va, std::vector<uint8_t>(4096)};')
    L.append('      if (pread(pfd, b.bytes.data(), 4096, (off_t)off) != 4096) return 2;')
    L.append('      snap.over.push_back(std::move(b));')
    L.append('    }')
    L.append('    const uint64_t* r = recs[i].v;')
    L.append('    PesState s;')
    L.append('    s.mem.snap = &snap;')
    L.append('    s.gs_base = strtoull(argv[7], nullptr, 16);  // the snapshot TEB (record.json "teb"), per run')
    L.append('    for (int g = 0; g < 16; ++g) s.gpr[g] = r[g];')
    L.append('    for (int x = 0; x < 16; ++x) { s.xmm[x][0] = r[16 + 2 * x]; s.xmm[x][1] = r[17 + 2 * x]; }')
    L.append('    s.wr(r[4], r[48], 8);  // the recorded return address, before the write log starts')
    L.append('    std::vector<std::pair<uint64_t, uint8_t>> log;')
    L.append('    s.mem.wlog = &log;')
    L.append('    pes_fn_%s(s);' % V)
    L.append(r'    fprintf(out, "{\"case\": %zu, \"ok\": true, \"rax\": \"%016llx\", \"xmm0\": \"%016llx%016llx\", \"w\": \"",')
    L.append(r'            i, (unsigned long long)s.gpr[0], (unsigned long long)s.xmm[0][1], (unsigned long long)s.xmm[0][0]);')
    L.append('    for (const auto& w : log) fprintf(out, "%016llx%02x", (unsigned long long)w.first, (unsigned)w.second);')
    L.append(r'    fprintf(out, "\"}\n");')
    L.append('    snap.reset();')
    L.append('  }')
    L.append('  fclose(out);')
    L.append('  printf("%s: %zu calls\\n", argv[1], n);')
    L.append('  return 0;')
    L.append('}')
    return "\n".join(L) + "\n"
