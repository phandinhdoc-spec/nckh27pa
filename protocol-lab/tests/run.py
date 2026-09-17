#!/usr/bin/env python3
"""Independent wire oracle + shared transcripts; stdlib only, no network."""
import json, pathlib, struct, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parents[1]
CMDS = [[str(ROOT/'build/cpp-lab')], [str(ROOT/'build/java'), '-jar', str(ROOT/'build/kotlin-lab.jar')]]

def wire(kind=1, mid=1, index=0, count=2, total=8, offset=0, data=b'abcd', version=1, magic=b'FS'):
    return (magic + struct.pack('<BBIHHHH', version, kind, mid, index, count, total, offset) + data).hex()

def recv(h=None, t=0, gen=0, kind=1):
    return f'R {t} {gen} {kind} {h or wire()}'

def run(cmd, lines):
    p = subprocess.run(cmd, input='\n'.join(lines)+'\n', text=True, capture_output=True)
    assert p.returncode == 0, (cmd, p.returncode, p.stderr)
    return p.stdout.splitlines()

def main():
    vectors = json.loads((ROOT/'fixtures/golden.json').read_text())
    cases = []
    def case(name, lines, want): cases.append((name, lines, want))
    for v in vectors:
        case('golden encoder '+v['name'], [f"E {v['mtu']} {v['kind']} {v['id']} {v['payload']}"], ['FRAMES '+','.join(v['frames'])])
        lines = [recv(h, kind=v['kind']) for h in v['frames']]
        case('golden decoder '+v['name'], lines, ['PENDING 1 0']*(len(lines)-1)+['COMPLETE '+v['payload']+' 0 0'])
    a, b = wire(), wire(index=1, offset=4, data=b'efgh')
    done = 'COMPLETE 6162636465666768 0 0'
    case('out of order', [recv(b), recv(a)], ['PENDING 1 0', done])
    case('duplicate identical', [recv(a), recv(a, 1999), recv(b,1999)], ['PENDING 1 0','DUPLICATE 1 0',done])
    for label, h in [('bytes',wire(data=b'abce')), ('offset',wire(offset=1)), ('length',wire(data=b'abc')), ('count',wire(count=3)), ('total',wire(total=9))]:
        case('conflict '+label,[recv(a),recv(h)],['PENDING 1 0','CONFLICT 0 0'])
    for label,h in [('short','4653'),('magic',wire(magic=b'XX')),('version',wire(version=2)),('kind zero',wire(kind=0)),('kind six',wire(kind=6)),('characteristic',wire(kind=2)),('id zero',wire(mid=0)),('count zero',wire(count=0)),('count 257',wire(count=257)),('count > total',wire(count=9)),('index',wire(index=2)),('total zero',wire(total=0)),('oversize',wire(total=1025)),('empty data',wire(data=b'')),('offset',wire(offset=65535)),('end',wire(offset=5)),('long frame',wire(data=b'x'*1025,total=1024))]:
        case('invalid '+label,[recv(h)],['INVALID 0 0'])
    case('invalid preserves active', [recv(a),recv(wire(version=2)),recv(b)],['PENDING 1 0','INVALID 1 0',done])
    case('overlap',[recv(a),recv(wire(index=1,offset=3))],['PENDING 1 0','CONFLICT 0 0'])
    case('gap',[recv(wire(data=b'abc')),recv(b)],['PENDING 1 0','CONFLICT 0 0'])
    case('noncontiguous index',[recv(wire(offset=4)),recv(wire(index=1,offset=0,data=b'efgh'))],['PENDING 1 0','CONFLICT 0 0'])
    case('timeout boundary',[recv(a), 'T 1999','T 2000'],['PENDING 1 0','TICK 1 0','TICK 0 0'])
    case('duplicate no extension',[recv(a),recv(a,1999),recv(b,2000),recv(a,2001)],['PENDING 1 0','DUPLICATE 1 0','PENDING 1 0',done])
    case('missing expires before invalid',[recv(a),recv('00',2000)],['PENDING 1 0','INVALID 0 0'])
    case('clock rollback',[recv(a,100),'T 99','T 2100'],['PENDING 1 0','CLOCK 1 0','TICK 0 0'])
    case('disconnect stale',[recv(a),'D',recv(b,gen=0),recv(b,gen=1),recv(a,gen=1)],['PENDING 1 0','DISCONNECTED 0 1','STALE 0 1','PENDING 1 1','COMPLETE 6162636465666768 0 1'])
    case('capacity retains',[recv(a),recv(wire(mid=2)),recv(wire(mid=3)),recv(b)],['PENDING 1 0','PENDING 2 0','CAPACITY 2 0','COMPLETE 6162636465666768 1 0'])
    case('capacity expires',[recv(a),recv(wire(mid=2),1),recv(wire(mid=3),2000)],['PENDING 1 0','PENDING 2 0','PENDING 2 0'])
    case('kind key',[recv(a),recv(wire(kind=2),kind=2),recv(b)],['PENDING 1 0','PENDING 2 0','COMPLETE 6162636465666768 1 0'])
    case('independent deadlines',[recv(a),recv(wire(mid=2),1000),'T 2000'],['PENDING 1 0','PENDING 2 0','TICK 1 0'])
    case('no completion cache',[recv(a),recv(b),recv(a),recv(b)],['PENDING 1 0',done,'PENDING 1 0',done])
    case('bytes not JSON validation',[recv(wire(count=1,total=1,data=b'\xff'))],['COMPLETE ff 0 0'])
    for mtu,kind,mid,payload in [(22,1,1,'00'),(23,0,1,'00'),(23,6,1,'00'),(23,1,0,'00'),(23,1,4294967296,'00'),(23,1,1,'-'),(23,1,1,'aa'*1025)]:
        case('encoder invalid',[f'E {mtu} {kind} {mid} {payload}'],['INVALID'])
    failures = []
    for name,lines,want in cases:
        for cmd in CMDS:
            got=run(cmd,lines)
            if got != want: failures.append((name, pathlib.Path(cmd[0]).name))
    if failures:
        for failure in failures: print("FAIL", *failure)
        raise AssertionError(f"{len(failures)} shared behavior checks failed")
    # Both directions, multiple MTUs/kinds, binary payloads, reversed order.
    for mtu in [23,24,185,517]:
        for length in [1,4,5,166,167,169,1024]:
            payload=bytes((i*37)%256 for i in range(length)).hex()
            for source,target in [(CMDS[0],CMDS[1]),(CMDS[1],CMDS[0])]:
                frames=run(source,[f'E {mtu} 5 4294967295 {payload}'])[0].removeprefix('FRAMES ').split(',')
                assert all(17<=len(bytes.fromhex(h))<=mtu-3 for h in frames)
                expected=[wire(kind=5,mid=4294967295,index=i,count=len(frames),total=length,offset=i*(mtu-19),data=bytes.fromhex(payload)[i*(mtu-19):(i+1)*(mtu-19)]) for i in range(len(frames))]
                assert frames==expected
                got=run(target,[recv(h,kind=5) for h in reversed(frames)])
                assert got==['PENDING 1 0']*(len(frames)-1)+[f'COMPLETE {payload} 0 0']
    print(f'PASS {len(cases)} shared scenarios x 2 languages; 56 cross-language exchanges')

if __name__=='__main__': main()
