#!/usr/bin/env python3
"""Read-only live poller of PES's cutscene timeline: logs one JSONL row whenever a tracked field changes.
usage: poll.py PID OUT.jsonl [seconds]"""
import sys,os,subprocess,struct,time,json
pid=int(sys.argv[1]); out=open(sys.argv[2],'a'); secs=float(sys.argv[3]) if len(sys.argv)>3 else 3600
srv=subprocess.Popen(['sudo','-n','python3',os.path.join(os.path.dirname(os.path.abspath(__file__)),'live_srv.py')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,bufsize=1)
def rd(*rs):
    srv.stdin.write('%d %s\n'%(pid,','.join('%x:%d'%r for r in rs))); srv.stdin.flush()
    l=srv.stdout.readline().rstrip('\n')
    if l=='E': raise SystemExit('no pid')
    return [None if x=='x' else bytes.fromhex(x) for x in l.split(' ')]
U64=lambda b:struct.unpack('<Q',b)[0]
GVA=0x1436F3DC8; ROOT=0x1436F3F88
last=None; lastf=None; t0=time.time(); n=0
while time.time()-t0<secs:
    g,root=rd((GVA,8),(ROOT,8))
    G=U64(g) if g else 0; D=U64(root) if root else 0
    if not G: time.sleep(0.05); continue
    mi=U64(rd((G+0x270,8))[0])
    row={}
    if mi:
        b=rd((mi+0x1198,4),(mi+0x1168,8),(mi+0x1554,8),(mi+0x1574,4))
        row['f']=struct.unpack('<i',b[0])[0]; per,play=struct.unpack('<ii',b[1]); row['per']=per; row['play']=play
        row['rs']=struct.unpack('<ii',b[2]); row['why']=struct.unpack('<i',b[3])[0]
    if D:
        d=rd((D,0x910),)[0]
        if d:
            row['D']=dict(scene=struct.unpack('<I',d[0:4])[0],kind=struct.unpack('<I',d[8:12])[0],st=struct.unpack('<I',d[0x1d8:0x1dc])[0],w74=struct.unpack('<I',d[0x1d0:0x1d4])[0],b75=d[0x1d4],b77=d[0x1dc],s8f2=d[0x8f2],c8f3=d[0x8f3],ctr=struct.unpack('<I',d[0x900:0x904])[0],v868=struct.unpack('<i',d[0x868:0x86c])[0],b870=d[0x870],sel=d[0x8f0],k878=struct.unpack('<4I',d[0x878:0x888]))
            a=d[0x1e8:0x320]  # A0 = the goal run object at DemoInfo+0x1E8 (FUN_140472640 sets A0+0xEC, the byte FUN_14047B460 copies to DemoInfo+0x1D4)
            I=lambda o:struct.unpack('<i',a[o:o+4])[0]; F=lambda o:round(struct.unpack('<f',a[o:o+4])[0],4)
            row['A']=dict(sc=I(8),rad=F(0xc),f10=F(0x10),t14=I(0x14),t18=I(0x18),i24=I(0x24),b2c=a[0x2c],T=[F(0x30),F(0x34),F(0x38)],bc=I(0xbc),c8=I(0xc8),b=[a[0xea],a[0xeb],a[0xec],a[0xed]],b131=a[0x131])
            if mi:
                sl=rd((mi+0x1588,4),(mi+0x1590,4),(mi+0x15a5,1),(mi+0x15c8,8))
                row['A'].update(m1588=struct.unpack('<I',sl[0])[0],m1590=struct.unpack('<I',sl[1])[0],m15a5=sl[2][0],m15c8=struct.unpack('<2I',sl[3]))
                if I(8) and D:
                    eps=rd(*[(G+(i+0xbf)*0x28,8) for i in range(22)])
                    ps=rd(*[(U64(e)+0x55c,16) for e in eps if e])  # x at +0x55c, z at +0x564
                    row['A']['P']=[[round(struct.unpack('<f',p[0:4])[0],2),round(struct.unpack('<f',p[8:12])[0],2)] if p else None for p in ps]
                    # the gate 0x140472640 reads the scorer's facing (man+0x554, degrees) and run record (G+(i+0xDD)*0x28: +1 type byte, +0x24 u16, +0x54 f32)
                    fs=rd(*[(U64(e)+0x554,4) for e in eps if e])
                    row['A']['Fc']=[round(struct.unpack('<f',f)[0],3) if f else None for f in fs]
                    rps=rd(*[(G+(i+0xa5)*0x28,8) for i in range(22)])  # the anime table G+0x19C8 (FUN_140A48910)
                    rr=rd(*[(U64(r),0x58) for r in rps if r])
                    row['A']['R']=[[x[1],struct.unpack('<H',x[0x24:0x26])[0],round(struct.unpack('<f',x[0x54:0x58])[0],3)] if x else None for x in rr]
            o=d[0x6d8:0x6ec]  # the FOUL run object (kind 5/9 handlers 0x140446960 / 0x1404466F0): +0 scene, +4/+5 bytes, +8 countdown, +0xC ready (FUN_14047EB00)
            if struct.unpack('<I',o[0:4])[0]>>16==2 and mi:
                fo=dict(sc=struct.unpack('<I',o[0:4])[0],b4=o[4],b5=o[5],cd=struct.unpack('<i',o[8:12])[0],rdy=o[0xc])
                sl=rd((mi+0x1590,8),(mi+0x1588,4))
                fo.update(m1590=struct.unpack('<2I',sl[0]),m1588=struct.unpack('<I',sl[1])[0])
                eps=rd(*[(G+(i+0xbf)*0x28,8) for i in range(23)])  # slot 0x16 = the referee's man record
                ps=rd(*[(U64(e)+0x55c,16) for e in eps if e])
                fo['P']=[[round(struct.unpack('<f',p[0:4])[0],2),round(struct.unpack('<f',p[8:12])[0],2)] if p else None for p in ps]
                rps=rd(*[(G+(i+0xa5)*0x28,8) for i in range(22)])
                rr=rd(*[(U64(r)+8,8) for r in rps if r])  # run record +8: bit 10 is read for the player in MI+0x1594
                fo['R8']=[struct.unpack('<Q',x)[0] if x else None for x in rr]
                row['Fo']=fo
            M=U64(d[0x1e0:0x1e8])
            if M:
                m=rd((M,0x270),)[0]
                if m:
                    f=lambda o:struct.unpack('<f',m[o:o+4])[0]; u=lambda o:struct.unpack('<I',m[o:o+4])[0]
                    mm=dict(vt=U64(m[0:8]),p1d4=u(0x1d4),p1d8=u(0x1d8),f54=m[0x54],f55=m[0x55],f4e=m[0x4e],step=m[0xe8],tot=round(f(0x100),4),w21c=u(0x21c),nst=(U64(m[0x28:0x30])-U64(m[0x20:0x28]))//0x1e0,w254=round(f(0x254),1))
                    P=U64(m[0xe0:0xe8])
                    if P:
                        p=rd((P,0xb00),)[0]
                        if p: mm.update(ps=struct.unpack('<I',p[0x58:0x5c])[0],pt=round(struct.unpack('<f',p[0x60:0x64])[0],3),plen=round(struct.unpack('<f',p[0x64:0x68])[0],3),pstate5c=struct.unpack('<I',p[0x5c:0x60])[0],pstart=round(struct.unpack('<f',p[0x70:0x74])[0],3),pade=p[0xade],pad8=p[0xad8],pada=p[0xada])
                    row['M']=mm
    if mi and 'M' in row: row['f1']=struct.unpack('<i',rd((mi+0x1198,4),)[0])[0]  # frame re-read after the objects: f != f1 marks a row that straddles a frame
    key=json.dumps({k:v for k,v in row.items() if k not in('f','f1')},sort_keys=True)
    fkey=row.get('f') if ('D' in row or 'M' in row) else None
    if key!=last or (fkey is not None and fkey!=lastf):
        row['t']=round(time.time()-t0,3); out.write(json.dumps(row)+'\n'); out.flush(); last=key; lastf=fkey; n+=1
    time.sleep(0.004)
