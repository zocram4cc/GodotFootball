#!/usr/bin/env python3
"""Derive per-function entry live-in regs + memory footprint from asm text.

The translator driver cross-checks this against each Map spec's inputs:
an arg register read-before-write with no region binding is a spec gap
(the driver fills it with a derived region and reports the delta).

Usage: livein.py <asm-dir> <out.json>
out: {va: {"live": [regs], "mem": [{base,index,scale,disp,absolute}],
           "writes_rax": bool, "writes_xmm0": bool, "stack_args": [offs],
           "tailjump": addr-or-null, "calls": [vas], "switches": n}}
"""
import re, glob, json, sys, os

REGFILE = (['rax', 'rcx', 'rdx', 'rbx', 'rsp', 'rbp', 'rsi', 'rdi',
            'r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15'] +
           ['eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp', 'esp',
            'r8d', 'r9d', 'r10d', 'r11d', 'r12d', 'r13d', 'r14d', 'r15d'] +
           ['ax', 'bx', 'cx', 'dx', 'si', 'di', 'bp', 'sp',
            'al', 'bl', 'cl', 'dl', 'sil', 'dil', 'bpl', 'spl',
            'r8b', 'r9b', 'r10b', 'r11b', 'r12b', 'r13b', 'r14b', 'r15b',
            'r8w', 'r9w', 'ah', 'bh', 'ch', 'dh'] +
           ['xmm%d' % i for i in range(16)])
REGSET = set(REGFILE)
PARENT = {}
for r in REGFILE:
    p = r
    if r in ('eax', 'ax', 'al', 'ah'):
        p = 'rax'
    elif r in ('ebx', 'bx', 'bl', 'bh'):
        p = 'rbx'
    elif r in ('ecx', 'cx', 'cl', 'ch'):
        p = 'rcx'
    elif r in ('edx', 'dx', 'dl', 'dh'):
        p = 'rdx'
    elif r in ('esi', 'si', 'sil'):
        p = 'rsi'
    elif r in ('edi', 'di', 'dil'):
        p = 'rdi'
    elif r in ('ebp', 'bp', 'bpl'):
        p = 'rbp'
    elif r in ('esp', 'sp', 'spl'):
        p = 'rsp'
    elif r.startswith('r8'):
        p = 'r8'
    elif r.startswith('r9'):
        p = 'r9'
    elif r.startswith('r10'):
        p = 'r10'
    elif r.startswith('r11'):
        p = 'r11'
    elif r.startswith('r12'):
        p = 'r12'
    elif r.startswith('r13'):
        p = 'r13'
    elif r.startswith('r14'):
        p = 'r14'
    elif r.startswith('r15'):
        p = 'r15'
    PARENT[r] = p


def regs_in(s):
    sl = s.lower()
    return [m.group(1) for m in
            re.finditer(r'\b([a-z][a-z0-9]*)\b', sl)
            if m.group(1) in REGSET]

def mem_parts(inner):
    parts = [p.strip().lower() for p in inner.split('+')]
    base = None
    index = None
    scale = 1
    nums = []
    ok = True
    for p in parts:
        pl = p.strip()
        mm = re.fullmatch(r'([a-z][a-z0-9]*)\*0x([0-9a-f]+)', pl)
        if mm and mm.group(1) in REGSET:
            index = mm.group(1)
            scale = int(mm.group(2), 16)
            continue
        mm = re.fullmatch(r'([a-z][a-z0-9]*)\*(\d+)', pl)
        if mm and mm.group(1) in REGSET:
            index = mm.group(1)
            scale = int(mm.group(2))
            continue
        if pl in REGSET:
            if base is None:
                base = pl
            elif index is None:
                index = pl
                scale = 1
            else:
                ok = False
            continue
        try:
            nums.append(int(pl, 16) if pl.startswith('0x') else int(pl))
            continue
        except ValueError:
            ok = False
    if not ok:
        return None
    disp = sum(nums)
    absolute = nums[0] if (base is None and index is None and nums) else None
    return {"base": base, "index": index, "scale": scale, "disp": disp,
            "absolute": ("0x%X" % absolute) if absolute is not None else None}


def analyze(lines):
    written = set()
    live = set()
    mems = []
    seen_mem = set()
    calls = []
    tailjump = None
    switches = 0
    stack_args = set()
    writes_rax = False
    writes_xmm0 = False
    addrs = []
    for l in lines:
        m = re.match(r'^([0-9a-f]+):\s*([A-Z0-9]+)\s*(.*)$', l)
        if not m:
            continue
        addr = int(m.group(1), 16)
        addrs.append(addr)
        op = m.group(2).lower()
        rest = m.group(3)
        ops = [o.strip() for o in rest.split(',')] if rest else []
        # memory shapes
        for mm in re.finditer(r'\[([^\]]+)\]', rest):
            mp = mem_parts(mm.group(1))
            if mp is None:
                continue
            key = json.dumps(mp, sort_keys=True)
            if key not in seen_mem:
                seen_mem.add(key)
                mems.append(mp)
            b = mp["base"]
            if b == "rsp":
                # stack slot read; arg spill zone identification comes later
                pass
            if b in ("rsp", "rbp") and mp["index"] is None and mp["disp"] >= 0x28:
                stack_args.add(mp["disp"])
        # crude read/write
        reads = set()
        writes = set()
        if op in ('mov', 'movsx', 'movsxd', 'movzx', 'lea',
                  'movss', 'movsd', 'movd', 'movaps', 'movups',
                  'movapd', 'movupd'):
            if len(ops) >= 2:
                reads.update(regs_in(ops[1]))
                writes.update(regs_in(ops[0]))
        elif op == 'push':
            reads.update(regs_in(ops[0]))
        elif op == 'pop':
            writes.update(regs_in(ops[0]))
        elif op == 'ret':
            reads.add('rax')
            reads.add('xmm0')
        elif op in ('cmp', 'test'):
            for o in ops:
                reads.update(regs_in(o))
        elif op in ('add', 'sub', 'xor', 'and', 'or', 'imul', 'shl', 'shr',
                    'sar', 'adc', 'sbb', 'bt', 'btc', 'btr', 'bts', 'neg',
                    'not', 'inc', 'dec', 'mul', 'imul', 'div', 'idiv'):
            if ops:
                reads.update(regs_in(ops[0]))
                writes.update(regs_in(ops[0]))
            for o in ops[1:]:
                reads.update(regs_in(o))
        elif op in ('addss', 'subss', 'mulss', 'divss', 'addps', 'subps',
                    'mulps', 'divps', 'andps', 'orps', 'xorps', 'maxss',
                    'minss', 'comiss', 'ucomiss', 'movs', 'blendvps'):
            if ops:
                reads.update(regs_in(ops[0]))
                if op not in ('comiss', 'ucomiss'):
                    writes.update(regs_in(ops[0]))
            for o in ops[1:]:
                reads.update(regs_in(o))
        elif op in ('cvtps2pd', 'cvtpd2ps', 'cvtdq2ps', 'cvtss2sd',
                    'cvtsd2ss', 'cvttss2si', 'cvtsi2ss', 'cvtss2si',
                    'cvtdq2pd', 'cvtpd2dq'):
            if len(ops) >= 2:
                reads.update(regs_in(ops[1]))
                writes.update(regs_in(ops[0]))
        elif op.startswith('cmov') or op.startswith('set'):
            # cmov reads both; set writes its byte reg
            for o in ops:
                reads.update(regs_in(o))
            if ops:
                writes.update(regs_in(ops[0]))
        elif op == 'call':
            # Calls are all intra-set (closed). The call reads the argument
            # registers and kills the caller-saved set; the callee's own
            # live set decides which arg regs are real (see fix_calls()).
            t = re.search(r'0x([0-9a-f]+)', rest)
            if t:
                calls.append("0x" + t.group(1).upper())
            reads.update(['rcx', 'rdx', 'r8', 'r9',
                          'xmm0', 'xmm1', 'xmm2', 'xmm3'])
            writes.update(['rax', 'rcx', 'rdx', 'r8', 'r9', 'r10', 'r11',
                           'xmm0', 'xmm1', 'xmm2', 'xmm3', 'xmm4', 'xmm5'])
        elif op in ('cdqe', 'cwd', 'cdq', 'cqo'):
            reads.update(['rax'])
            writes.update(['rax'])
        elif op == 'xchg':
            for o in ops:
                reads.update(regs_in(o))
                writes.update(regs_in(o))
        elif op == 'lea':
            if len(ops) >= 2:
                reads.update(regs_in(ops[1]))
                writes.update(regs_in(ops[0]))
        elif op in ('jmp', 'jz', 'jnz', 'jc', 'jnc', 'ja', 'jbe', 'jl',
                    'jge', 'jle', 'jg', 'js', 'jns', 'jp', 'jnp',
                    'jo', 'jno'):
            for o in ops:
                reads.update(regs_in(o))
            if op == 'jmp' and ops:
                if re.search(r'0x([0-9a-f]+)', ops[0]):
                    tgt = int(re.search(r'0x([0-9a-f]+)',
                                        ops[0]).group(0), 16)
                    lo = min(addrs)
                    hi = max(addrs)
                    if not (lo <= tgt <= hi + 16):
                        tailjump = "0x%X" % tgt
                else:
                    switches += 1
        else:
            for o in ops:
                reads.update(regs_in(o))
        for r in reads:
            if PARENT.get(r, r) not in written:
                live.add(PARENT.get(r, r))
        for r in writes:
            written.add(PARENT.get(r, r))
            if PARENT.get(r, r) == 'rax':
                writes_rax = True
            if r == 'xmm0':
                writes_xmm0 = True
    # entry RSP/RBP are ambient (harness provides stack); drop unless indexed
    live.discard('rsp')
    live.discard('rbp')
    return {"live": sorted(live), "mem": mems, "writes_rax": writes_rax,
            "writes_xmm0": writes_xmm0, "stack_args": sorted(stack_args),
            "tailjump": tailjump, "calls": sorted(set(calls)),
            "switches": switches}

def fix_calls(result):
    """Refine CALL reads: a call passes only the callee's live arg regs."""
    ARGS = {'rcx', 'rdx', 'r8', 'r9',
            'xmm0', 'xmm1', 'xmm2', 'xmm3'}
    changed = True
    while changed:
        changed = False
        for va, d in result.items():
            if d.get('empty') or not d.get('calls'):
                continue
            drop = set(ARGS)
            for c in d['calls']:
                cal = result.get(c, {})
                if cal.get('empty'):
                    drop = set()
                    break
                drop &= ARGS - set(cal.get('live', []))
            before = set(d['live'])
            d['live'] = sorted(r for r in d['live'] if r not in drop)
            if set(d['live']) != before:
                changed = True


def main():
    asm_dir, out = sys.argv[1], sys.argv[2]
    result = {}
    for p in sorted(glob.glob(os.path.join(asm_dir, '*.json'))):
        va = '0x' + os.path.basename(p).split('.')[0].upper()
        raw = open(p, encoding='utf-8', errors='replace').read()
        lines = [l.strip() for l in raw.splitlines()
                 if l.strip() and re.match(r'^[0-9a-f]+:', l.strip())]
        if not lines:
            result[va] = {"empty": True}
            continue
        result[va] = analyze(lines)
    fix_calls(result)
    json.dump(result, open(out, 'w'), indent=1, sort_keys=True)
    print("wrote %s: %d functions" % (out, len(result)))


if __name__ == '__main__':
    main()
