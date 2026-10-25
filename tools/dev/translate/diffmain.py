#!/usr/bin/env python3
"""diffmain.py: generate the C++ diff-test main for one function.

Feeds the harness corpus into the translated C++ and compares outputs bit
for bit. Memory setup mirrors run.py set_inputs + regions_init/patch_addrs.
Floats compare by IEEE bits.
"""
import json

GPR = {"rcx": 1, "rdx": 2, "r8": 8, "r9": 9, "rax": 0, "rbx": 3,
       "rbp": 5, "rsp": 4, "eax": 0, "ebx": 3, "r8d": 8, "edx": 2,
       "ecx": 1}
XMM = {"xmm0": 0, "xmm1": 1, "xmm2": 2, "xmm3": 3}

def ident(name):
    # Region names carry dots (IMG.1436F3DC8, A1.28): not C identifiers.
    return name.replace(".", "_").replace("-", "_")


def gen(spec, va, callees, seed=None, bank=None):
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
    # seed.py artifact (live state): region bytes copied into the per-case
    # segments below, before the args and linkage writes (run.py applies the
    # same bytes before the case inputs). A bank (run.py --seed-bank) holds one
    # artifact per instance; each case copies the instance its corpus line names
    # ("inst", 0 when unbanked). The segments are fresh per case, so unseeded
    # regions are zero, as in run.py's bank mode.
    def seed_bytes(art):
        seeded = {r["name"]: r for r in (art or {}).get("regions") or []}
        out = {}
        for name in rnames:
            if name in seeded:
                r = seeded[name]
                blob = bytes.fromhex(r["hex"])
                if int(r["off"]) + len(blob) > int(regions[name]["size"], 16):
                    raise ValueError("seed for %s exceeds its region" % name)
                out[name] = (int(r["off"]), blob)
        return out
    seeded_k = [seed_bytes(a) for a in (([seed]) if bank is None else list(bank))]
    for k, sb in enumerate(seeded_k):
        for name, (off, blob) in sb.items():
            L.append('static const uint8_t seed_%s_%d[] = {%s};' % (ids[name], k, ",".join(str(b) for b in blob)))
    L.append('int main(int argc, char** argv) {')
    L.append('  if (argc < 3) return 2;')
    L.append('  FILE* f = fopen(argv[1], "r"); if (!f) return 2;')
    L.append('  int ifd = open(argv[2], O_RDONLY); if (ifd < 0) return 2;')
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
    for k, sb in enumerate(seeded_k):
        L.append('    if (jl.inst == %d) {' % k)
        for i, name in enumerate(rnames):
            if name in sb:
                L.append('      std::memcpy(s.mem.segs[%d].bytes.data() + %d, seed_%s_%d, sizeof seed_%s_%d);' % (i, sb[name][0], ids[name], k, ids[name], k))
        L.append('    }')
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
    return "\n".join(L) + "\n"
