#!/usr/bin/env python3
"""synth_trapvirt_specs.py: writes synth_specs/trap_virt_f10.json and trap_virt_f90.json, the SYNTHETIC cases of the anime interface's
two virtual slots 0x1406E9F10 and 0x1406E9F90 (vtable data ref 0x1425E35E0; their caller is 0x1407C1F40). P is the rdx of the real
act4 call pid615595/seq7805; both slots take this = P+0xAC0. Every threshold is read from the image (PES_EXE). Usage: PES_EXE=<pes_full.bin>
python3 synth_trapvirt_specs.py; then synth.py over the capture and prove_bounded.py (see docs/ports/pes-port-RestingBall6.md)."""
import json
import os
import struct
IMG = os.environ["PES_EXE"]
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "synth_specs")
def f32(va):
    with open(IMG,'rb') as f: f.seek(va-0x140000000); return struct.unpack('<f',f.read(4))[0]
def step(x,d): return struct.unpack('<f',struct.pack('<I',struct.unpack('<I',struct.pack('<f',x))[0]+d))[0]
P=0x41c774f0           # P of the capture's man (rdx of the act4 call, seq7805): rcx of both leaves is its second interface P+0xAC0
RCX=P+0xAC0
# --- 0x1406E9F10: float f(this=P+0xAC0, x); reads [[[P+0xA08]+0xF0]+0xC0]
a,b=f32(0x1425ddf78),f32(0x1425ddf80)
rates=sorted({-1.0,1e9,a,step(a,1),step(a,-1),(a+b)/2,b,step(b,1)})
xs=[0.5,-2.0]
cases=[{"name":"r%08x_x%s"%(struct.unpack('<I',struct.pack('<f',r))[0],i),"rate":r,"x":x} for r in rates for i,x in enumerate(xs)]
spec={"label":"trapvirtf10","seed":1,"as":"0x1406E9F10",
 "fields":[{"name":"this","at":"rcx","type":"u64","values":[RCX]},
           {"name":"x","at":"xmm1","type":"f32","values":[0.0]},
           {"name":"rate","at":"mem:0x%X+0xF0+0xC0"%(P+0xA08),"type":"f32","values":[0.0]}],
 "cases":[dict(c,this=RCX) for c in cases]}
json.dump(spec,open(os.path.join(OUT, 'trap_virt_f10.json'), 'w'))
print('f10',len(cases))
# --- 0x1406E9F90: this=P+0xAC0, dl flag; id = byte [P+0x2458]; the slot record of man 15: stat bytes at SR+0x70+0x2B / +0x15
lo,hi=f32(0x14259be2c),f32(0x1425c48cc)
G25F8=0x25F8
def stat_at(i,off): return "mem:0x1436F3DC8+0x%X+0x%X"%(G25F8+0x28*i,0x70+off)
cases=[]
stats=sorted({0,int(lo),int(hi),255})
for flag in (0,1):
    for s in stats:
        cases.append({"name":"m15_f%d_s%d"%(flag,s),"id":15,"flag":flag,"s2b":s,"s15":s})
for i in (0,11,21):
    cases.append({"name":"id%d"%i,"id":i,"flag":0,"s2b":None,"s15":None})
for i in (0x16,0xFF):
    cases.append({"name":"id%x"%i,"id":i,"flag":0,"s2b":None,"s15":None})
# a case that does not poke a stat keeps the capture's value: write it back as the capture's own byte (read at run time is not possible
# in a static spec), so unpoked cases carry the stat the poked 15-man cases do not touch: man 15 only is poked; other men are read as is.
cases=[c for c in cases if c['s2b'] is not None or c['id']!=15]
fields=[{"name":"this","at":"rcx","type":"u64","values":[RCX]},
        {"name":"flag","at":"edx","type":"u32","values":[0]},
        {"name":"id","at":"va:0x%X"%(P+0x2458),"type":"u8","values":[15]},
        {"name":"s2b","at":stat_at(15,0x2B),"type":"u8","values":[0]},
        {"name":"s15","at":stat_at(15,0x15),"type":"u8","values":[0]}]
# cases without stat pokes: leave the capture's bytes (omitted keys take the field's first value in a static spec, which would poke 0):
# so every case pokes stats; non-15 cases poke man 15 harmlessly.
for c in cases:
    if c['s2b'] is None: c['s2b']=0; c['s15']=0
spec={"label":"trapvirtf90","seed":1,"as":"0x1406E9F90","fields":fields,"cases":[dict(c,this=RCX) for c in cases]}
json.dump(spec,open(os.path.join(OUT, 'trap_virt_f90.json'), 'w'))
print('f90',len(cases),lo,hi)
