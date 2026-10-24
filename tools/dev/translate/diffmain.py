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
    L.append('int main(int argc, char** argv) {')
    L.append('  if (argc < 2) return 2;')
    L.append('  FILE* f = fopen(argv[1], "r"); if (!f) return 2;')
    L.append('  char buf[1 << 22]; long nc = 0, bad = 0;')
    L.append('  while (fgets(buf, sizeof buf, f)) {')
    L.append('    if (buf[0] != \'{\') continue;')
    L.append('    JLine jl; if (!parse_line(buf, jl)) { printf("parse fail\\n"); return 2; }')
    L.append('    PesState s;')
    for name in rnames:
        L.append('    s.mem.segs.push_back({base_%s, std::vector<uint8_t>(size_%s, 0)});' % (ids[name], ids[name]))
    L.append('    s.mem.segs.push_back({stack_base, std::vector<uint8_t>(stack_size, 0)});')
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
    # (e.g. 0x1436F3DC8 -> the IMG shadow region). Model it like run.py:
    # write the region base into memory at the image VA. PesMem.find checks
    # image runs first, so shadow image VAs in a writable seg that sorts
    # before any baked image run covering them (segs are checked after img
    # runs; baked runs never cover these writable-global VAs because the
    # translated code writes them, not reads constants from them).
    for vas, rname in (sp.get("patch_addrs") or {}).items():
        L.append('    { bool _covered = false; for (auto& sg : s.mem.segs) if (sg.base <= %sULL && %sULL + 8 <= sg.base + sg.bytes.size()) { uint64_t _b = base_%s; memcpy(sg.bytes.data() + (%sULL - sg.base), &_b, 8); _covered = true; break; } if (!_covered) { PesMem::Seg sg{%sULL, std::vector<uint8_t>(8, 0)}; uint64_t _b = base_%s; memcpy(sg.bytes.data(), &_b, 8); s.mem.segs.push_back(std::move(sg)); } }' % (vas, vas, ids[rname], vas, vas, ids[rname]))
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
