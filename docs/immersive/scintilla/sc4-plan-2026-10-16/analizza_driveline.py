#!/usr/bin/env python3
"""Offline candidates only: no mesh, occlusion, native camera or usability proof."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET


def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def add(a,b): return tuple(x+y for x,y in zip(a,b))
def mul(a,s): return tuple(x*s for x in a)
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def length(a): return math.sqrt(dot(a,a))
def unit(a):
    n=length(a)
    assert n>1e-8
    return mul(a,1/n)
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])


def analyze(track):
    quads=[]
    for q in ET.parse(track/'quads.xml').getroot().findall('quad'):
        points=[]
        for i in range(4):
            value=q.attrib[f'p{i}']
            if ':' in value:
                qi,pi=map(int,value.split(':'))
                assert qi<len(quads) and 0<=pi<4
                point=quads[qi][pi]
            else: point=tuple(map(float,value.split()))
            assert len(point)==3 and all(math.isfinite(x) for x in point)
            points.append(point)
        quads.append(points)
    graph=ET.parse(track/'graph.xml').getroot()
    edges=graph.findall('.//edge-loop')
    assert len(edges)==1 and int(edges[0].attrib['from'])==0 and int(edges[0].attrib['to'])==len(quads)-1
    assert not graph.findall('.//edge'), 'Only the verified simple cycle is supported'
    lower=[mul(add(q[0],q[1]),.5) for q in quads]
    upper=[mul(add(q[2],q[3]),.5) for q in quads]
    step=[length(sub(lower[(i+1)%len(quads)],p)) for i,p in enumerate(lower)]
    cumulative=[0.]
    for distance in step: cumulative.append(cumulative[-1]+distance)
    lap_length=cumulative[-1]
    rows=[]
    for i,q in enumerate(quads):
        forward=unit(sub(upper[i],lower[i]))
        across=sub(q[1],q[0])
        right=unit(sub(across,mul(forward,dot(across,forward))))
        up=unit(cross(forward,right))
        assert up[1]>0
        width=(length(sub(q[1],q[0]))+length(sub(q[2],q[3])))/2
        before=unit(sub(lower[i],lower[(i-1)%len(quads)]))
        after=unit(sub(lower[(i+1)%len(quads)],lower[i]))
        turn=math.degrees(math.acos(max(-1,min(1,dot(before,after)))))
        center=mul(add(lower[i],upper[i]),.5)
        rows.append(dict(node=i,s_lower_m=cumulative[i],fraction=cumulative[i]/lap_length,
                         width_m=width,local_turn_deg=turn,center=center,forward=forward,right=right,up=up))
    # One initial candidate in each of 6 route bands. This selects local straightness,
    # not 75 metres of visible corridor, which cannot be inferred from the driveline.
    chosen=[]
    for lo,hi in [(0.05,.18),(.20,.33),(.36,.49),(.52,.65),(.68,.81),(.84,.94)]:
        pool=[r for r in rows if lo<=r['fraction']<=hi and r['width_m']>=6]
        assert pool
        candidate=min(pool,key=lambda r:(r['local_turn_deg'], -r['width_m'],r['node']))
        entry=dict(candidate)
        lateral=min(1.8,entry['width_m']*.20)
        entry['provisional_capsule_centers']=[add(add(entry['center'],mul(entry['right'],side*lateral)),mul(entry['up'],3.5)) for side in [-1,1]]
        entry['placement_note']='These are alternatives, not a qualified simultaneous pair.'
        entry['qualified']=False
        chosen.append(entry)
    return dict(schema=1, status='authoring_candidates_not_runtime_configuration',
                track=track.name, quads=len(quads),
                lower_center_cycle_length_m=lap_length,
                length_caveat='double precision reconstruction; native DriveGraph/float and direction are authoritative',
                source_hashes={p:hashlib.sha256((track/p).read_bytes()).hexdigest() for p in ['quads.xml','graph.xml']},
                candidates=chosen)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--track',type=Path,required=True)
    p.add_argument('--write',action='store_true')
    a=p.parse_args()
    result=analyze(a.track)
    destination=Path(__file__).with_name('ancoraggi-candidati.json')
    if a.write: destination.write_text(json.dumps(result,indent=2)+'\n')
    elif destination.exists(): assert json.loads(destination.read_text())==json.loads(json.dumps(result))
    print(json.dumps(dict(quads=result['quads'],length_m=result['lower_center_cycle_length_m'],
                         candidates=[{k:r[k] for k in ['node','fraction','width_m','local_turn_deg']} for r in result['candidates']]),indent=2))

if __name__=='__main__': main()
