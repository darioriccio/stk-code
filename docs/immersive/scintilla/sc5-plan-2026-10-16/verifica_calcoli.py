#!/usr/bin/env python3
"""SC5 planning oracle; Python stdlib, no game/build/runtime. Default: compare only."""
import argparse
from collections import Counter
from fractions import Fraction as Q
import hashlib
import itertools
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET

CHECKS = 0

def check(condition):
    global CHECKS
    CHECKS += 1
    if not condition:
        raise AssertionError(f'calculation {CHECKS} failed')

def ceil_ticks(us, fps): return (us * fps + 999999) // 1000000

def floor_ticks(us, fps): return us * fps // 1000000

def number(q): return float(q)

def rotation(p):
    laps = 6 if p <= 3 else 2 * p
    rows, counts, driving = [], [Counter() for _ in range(p)], Counter()
    for turn in range(laps):
        pilot = turn % p
        driving[pilot] += 1
        crew = [(pilot + j + 1) % p for j in range(p-1)]
        roles = [None if p <= 3 else (j + (p-1)*(turn//p)) % 5 for j in range(p-1)]
        check(len(set(crew)) == p-1 and pilot not in crew)
        check(p <= 3 or len(set(roles)) == p-1)
        for participant, role in zip(crew, roles): counts[participant][role] += 1
        rows.append(dict(turn=turn, pilot=pilot, crew=crew, roles=roles))
    check(max(driving.values())-min(driving.values()) <= 1)
    for c in counts:
        check(sum(c.values()) == laps-laps//p)
        if p >= 4:
            check(set(c) == set(range(5)))
            check(max(c.values())-min(c.values()) <= 1)
    return dict(P=p, C=p-1, laps=laps, pilot_turns=dict(driving),
                role_counts=[dict((str(k),v) for k,v in sorted(c.items(),key=lambda v:str(v[0]))) for c in counts], schedule=rows)

def geometry():
    x,y,r=.22,.85,.06
    check(math.hypot(x,y)+r < 1)
    check(math.hypot(x,y-.85) > r+.06)
    defense=[(-.6+.4*i,.62,.07) for i in range(4)]
    check(all(math.hypot(x-a,y-b)>r+c for a,b,c in defense))
    # All remaining v2 HUD primitives are above normalized y=.52.
    check(y-r>.52)
    sizes=[]
    for w,h in [(256,256),(768,768),(1536,1536),(2048,1024),(8192,8192)]:
        s=min(w,h);u=.5+x*s/(2*w);v=.5+y*s/(2*h);rp=r*s/2
        check(0 < u-rp/w < u+rp/w < 1 and 0 < v-rp/h < v+rp/h < 1)
        sizes.append(dict(width=w,height=h,u=u,v=v,radius_pixels=rp))
    return dict(node=dict(x=x,y=y,r=r),circle_edge=math.hypot(x,y)+r,
                gap_to_service=.22-.12,sizes=sizes)

def clocks_energy():
    table=[]
    for f in range(60,1001):
        for c in range(1,6):
            q=1000*f; warning=ceil_ticks(3000000 if c==1 else 2500000,f); margin=ceil_ticks(250000,f)
            reserve=5*q+20000*(warning+margin); price=15*q
            check(0 < reserve < 100*q and reserve+price<=100*q)
            for us in [600000,750000,3000000,250000]:
                lo,hi=ceil_ticks(us,f),floor_ticks(us,f)
                check(Q(lo,f)>=Q(us,1000000) and Q(lo,f)-Q(us,1000000)<Q(1,f))
                check(Q(hi,f)<=Q(us,1000000) and Q(us,1000000)-Q(hi,f)<Q(1,f))
            hold=ceil_ticks(750000,f); window=floor_ticks(3000000,f)
            check(window>hold and floor_ticks(500000,f)>0 and floor_ticks(250000,f)>0)
            # Two confirmations can qualify before the half-open deadline.
            check(2*hold<window)
            # Critical lower boundary and integer fixed point budget.
            check((reserve+price)-price==reserve and reserve+price-1-price<reserve)
            if f in [60,61,120,144,1000] and c in [1,2]:
                table.append(dict(F=f,C=c,Q=q,reserve=number(Q(reserve,q)),party_threshold=number(Q(reserve+price,q)),
                    party_hold=hold,consensus_window=window,burst=floor_ticks(500000,f),fade=floor_ticks(250000,f)))
    return table

def consensus_cases():
    # Pure specification model: completed holds vote; a quorum is committed only
    # before deadline and all voters still held/visible in the same authority.
    def admitted(c,t,deadline,voters,current,energy,reserve,applied):
        return t<deadline and len(set(voters)&set(current))>=min(c,2) and energy-reserve>=15 and applied
    checks=[(1,89,90,['a'],['a'],85,70,True,True),
            (2,359,360,['a','b'],['a','b'],75,60,True,True),
            (2,360,360,['a','b'],['a','b'],75,60,True,False),
            (2,100,360,['a','a'],['a'],75,60,True,False),
            (2,100,360,['a','b'],['a'],75,60,True,False),
            (2,100,360,['a','b'],['a','b'],74,60,True,False),
            (2,100,360,['a','b'],['a','b'],75,60,False,False)]
    for args in checks: check(admitted(*args[:-1]) == args[-1])
    # Quota keyed by committed driver turn, not by input epoch or device.
    consumed={};race=1;turn=0
    for input_epoch in range(1,201):
        key=(race,turn)
        if not consumed.get(key): consumed[key]=1
        check(sum(consumed.values())==1)
    turn+=1;consumed[race,turn]=1;check(sum(consumed.values())==2)
    return dict(cases=len(checks),rebind_retries=200,deadline='half_open')

def assignment():
    # Independent upper bound and resource feasibility, not runtime scheduler proof.
    bounds=[]
    for c in range(1,6):
        offense=1 if c==1 else 2 if c==2 else 3
        costs=[offense,1,1,1,1,1,min(c,2)]
        feasible=[]
        for bits in itertools.product((0,1),repeat=7):
            used=sum(b*n for b,n in zip(bits,costs))
            if used<=c:
                permutations=math.factorial(c)//math.factorial(c-used)
                feasible.append(permutations)
                check(used<=c)
        bound=128*max(math.factorial(c)//math.factorial(c-k) for k in range(c+1))
        check(sum(feasible)<=bound<=15360)
        check((offense+1<=c)==(c>=4))
        check((offense+2<=c)==(c>=5))
        bounds.append(dict(C=c,offense_hands=offense,defense_and_offense=c>=4,
                          defense_offense_collection=c>=5,assignment_upper_bound=bound,feasible_assignments=sum(feasible)))
    # Equal persistent eligibility + one slot + waiting first: bounded round robin.
    for c in range(1,6):
        waiting=[0]*c;counts=[0]*c
        for step in range(c*20):
            who=max(range(c),key=lambda i:(waiting[i],-i));counts[who]+=1
            waiting=[0 if i==who else w+1 for i,w in enumerate(waiting)]
            check(max(waiting)<=c-1)
        check(len(set(counts))==1)
    return bounds

def motion():
    # Only reference models: constant tangential direction, no collisions or slope.
    dv=Q(1);duration=Q(1,2);fade=Q(1,4)
    extra_cap_area=dv*(duration+fade/2)
    check(extra_cap_area==Q(5,8))
    rows=[]
    for v in [10,15,20,25]:
        delta_specific_energy=Q((v+1)**2-v*v,2)
        a=Q(6);reaction=Q(1,4)
        old=Q(v)*reaction+Q(v*v)/(2*a)
        new=Q(v+1)*reaction+Q((v+1)**2)/(2*a)
        delta=reaction+Q(2*v+1)/(2*a)
        check(new-old==delta)
        rows.append(dict(v=v,delta_v=1,delta_specific_energy_J_per_kg=number(delta_specific_energy),
                         illustrative_braking_a=6,illustrative_delay_s=.25,
                         stopping_before_m=number(old),stopping_after_m=number(new),extra_stopping_m=number(delta)))
    bounded=[]
    for t in [0.1,1,4,10,20,27.7]:
        for vn in [0,0.1,3]:
            for f in [.5,.8,1]:
                base=28.;cap=(base+1)*f
                next_t=min(t+1,math.sqrt(max(0,cap*cap-vn*vn)))
                allowed=next_t>t+1e-6
                if allowed:
                    check(0<next_t-t<=1+1e-12)
                    check(math.hypot(next_t,vn)<=cap+1e-12)
                    bounded.append(dict(t=t,normal=vn,slowdown=f,next_t=next_t,cap=cap))
    check(math.hypot(4,3)+1-4==2)  # blind native floor counterexample
    check(min(4+1,math.sqrt(29**2-3**2))==5)
    return dict(cap_area_m=number(extra_cap_area),not_a_real_displacement_bound=True,reference_cases=rows,
                bounded_tangent_cases=bounded,native_floor_counterexample=dict(tangent=4,normal=3,total=5,blind_floor=6,blind_gain=2,bounded_gain=1))

def track_geometry(track):
    quads=[]
    for e in ET.parse(track/'quads.xml').getroot().findall('quad'):
        pts=[]
        for i in range(4):
            raw=e.attrib[f'p{i}']
            if ':' in raw:
                a,b=map(int,raw.split(':'));check(0<=a<len(quads) and 0<=b<4);p=quads[a][b]
            else:p=tuple(map(float,raw.split()))
            check(len(p)==3 and all(math.isfinite(v) for v in p));pts.append(p)
        quads.append(pts)
    graph=ET.parse(track/'graph.xml').getroot();loops=graph.findall('.//edge-loop')
    check(len(loops)==1 and int(loops[0].attrib['from'])==0 and int(loops[0].attrib['to'])==len(quads)-1 and not graph.findall('.//edge'))
    sub=lambda a,b:tuple(x-y for x,y in zip(a,b))
    norm=lambda a:math.sqrt(sum(x*x for x in a))
    centers=[tuple((a+b)/2 for a,b in zip(q[0],q[1])) for q in quads]
    edges=[norm(sub(centers[(i+1)%len(quads)],p)) for i,p in enumerate(centers)]
    total=sum(edges);s=0;rows=[]
    for i,p in enumerate(centers):
        a=centers[i-1];b=p;c=centers[(i+1)%len(quads)]
        u=sub(b,a);v=sub(c,b);chord=sub(c,a)
        cross=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
        # 4 triangle area/(a*b*c) = 2|u cross v|/(a*b*c).
        curvature=2*norm(cross)/(norm(u)*norm(v)*norm(chord))
        width=min(norm(sub(quads[i][1],quads[i][0])),norm(sub(quads[i][2],quads[i][3])))
        slope=math.degrees(math.atan2(v[1],math.hypot(v[0],v[2])))
        rows.append(dict(node=i,fraction=s/total,width_m=width,curvature_per_m=curvature,
                         slope_deg=slope,reference_v_for_3ms2=math.sqrt(3/curvature) if curvature>1e-12 else None))
        s+=edges[i]
    candidates=[]
    for lo,hi in [(.05,.18),(.20,.33),(.36,.49),(.52,.65),(.68,.81),(.84,.94)]:
        pool=[r for r in rows if lo<=r['fraction']<=hi and r['width_m']>=6]
        check(bool(pool));candidates.append(min(pool,key=lambda r:(r['curvature_per_m'],r['node'])))
    return dict(status='authoring_points_only_not_allowed_zones',qualified=False,
                source_hashes={n:hashlib.sha256((track/n).read_bytes()).hexdigest() for n in ['quads.xml','graph.xml']},
                quads=len(quads),length_m=total,candidates=candidates,
                caveat='Sparse driveline curvature is not a bound for mesh, obstacles, braking, grip, comfort or a full corridor.')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--track',type=Path,default=Path('/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-assets/tracks/snowtuxpeak'))
    parser.add_argument('--write',action='store_true');args=parser.parse_args()
    result=dict(schema=1,scope='SC5 mathematical planning only',roles=[rotation(p) for p in range(2,7)],
                timings_energy=clocks_energy(),consensus=consensus_cases(),assignment=assignment(),hud=geometry(),motion=motion(),track=track_geometry(args.track))
    result['checks']=CHECKS
    path=Path(__file__).with_name('calcoli.json')
    encoded=json.dumps(result,ensure_ascii=False,indent=2,allow_nan=False)+'\n'
    if args.write:
        ssd=Path('/Volumes/SanDisk 2TB');check(ssd.is_mount() and path.resolve().is_relative_to(ssd.resolve()))
        path.write_text(encoded)
    else:check(json.loads(path.read_text())==json.loads(encoded))
    print(json.dumps(dict(checks=result['checks'],comparison='written' if args.write else 'passed',native_runtime=False,track_quads=result['track']['quads'],candidate_nodes=[r['node'] for r in result['track']['candidates']])))

if __name__=='__main__':main()
