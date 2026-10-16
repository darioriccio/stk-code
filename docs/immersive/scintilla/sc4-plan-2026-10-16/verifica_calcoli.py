#!/usr/bin/env python3
"""SC4 reference calculations, not tests of the production implementation.
No external dependencies. --write writes only adjacent calcoli.json.
"""
import argparse
import itertools
import json
import math
from pathlib import Path


def ticks_min(us, hz):
    assert 0 <= us <= 10_000_000 and 1 <= hz <= 1000
    return (us * hz + 999_999) // 1_000_000


def ticks_max(us, hz):
    assert 0 <= us <= 10_000_000 and 1 <= hz <= 1000
    return us * hz // 1_000_000


def run():
    checks = []
    def check(name, condition):
        if not condition:
            raise AssertionError(name)
        checks.append(name)

    timing = []
    for hz in [60, 120, 144]:
        hold, window = ticks_min(600_000, hz), ticks_max(3_000_000, hz)
        latest = window - hold - 1
        check(f'deadline-{hz}', latest + hold < window and latest + 1 + hold == window)
        check(f'rounding-{hz}', hold / hz >= .6 and window / hz <= 3)
        timing.append(dict(hz=hz, hold_ticks=hold, window_ticks=window,
                           latest_start_tick=latest, latest_start_seconds=latest/hz,
                           hold_seconds=hold/hz, tick_ms=1000/hz))
    # Time accrues over intervals, not by counting both sampled endpoints.
    check('no-off-by-one-dwell', 72 - 0 == 72 and 71 - 0 < 72)
    def advance_clock(current, elapsed_ticks, held):
        return current if held else current + elapsed_ticks
    paused = advance_clock(120, 100000, True)
    check('pause-preserves-remaining', 360 - paused == 240)
    resumed = advance_clock(paused, 72, False)
    check('pause-resets-progress', resumed == 192 and resumed < 360)
    def apply_impact(state, serial, consumed):
        if serial <= consumed:
            return tuple(state), consumed
        after = list(state)
        if 'T' not in state and 'S' in state:
            after[max(i for i, x in enumerate(state) if x == 'S')] = 'T'
        return tuple(after), serial
    # Exhaust all valid abstract slot states; at most one pending tether.
    states = [s for s in itertools.product('EST', repeat=3) if s.count('T') <= 1]
    transitions = 0
    for state in states:
        after, receipt = apply_impact(state, 7, 6)
        check('impact-conservation-' + ''.join(state),
              after.count('S') + after.count('T') == state.count('S') + state.count('T'))
        check('impact-at-most-one-' + ''.join(state),
              0 <= state.count('S')-after.count('S') <= 1 and after.count('T') <= 1)
        # The receipt for the SAME impact must make its second delivery a no-op.
        duplicate, duplicate_receipt = apply_impact(after, 7, receipt)
        check('impact-idempotent-' + ''.join(state), duplicate == after and duplicate_receipt == receipt)
        transitions += 1
        if 'T' in state:
            for outcome in ['S', 'E']:
                next_state = list(state)
                next_state[state.index('T')] = outcome
                check('tether-' + ''.join(state) + outcome,
                      next_state.count('T') == 0 and 0 <= next_state.count('S') <= 3)
                transitions += 1
    outcomes = {}
    for first, full in itertools.product([False, True], repeat=2):
        outcome = ('mission_victory' if first and full else
                   'race_won_cargo_incomplete' if first else
                   'cargo_delivered_race_lost' if full else 'mission_failed')
        outcomes[f'{first=},{full=}'] = outcome
    check('four-natural-outcomes', len(set(outcomes.values())) == 4)
    check('tether-not-complete', not all(x == 'S' for x in ('S','S','T')))
    check('aborted-not-victory', 'not_completed' not in outcomes.values())
    # Budget is a proposed interaction budget, NOT a measured human percentile.
    budget = dict(notice=.45, primary_resolution=.25, switch_and_rearm=.20,
                  tool_resolution=.25, continuous_hold=.60, margin=.25)
    required = sum(budget.values())
    check('budget-two-seconds', math.isclose(required, 2.0))
    radius, angle, max_range, min_range, transverse = 2.0, 3.0, 75.0, 8.0, 4.0
    angular_limit = radius / math.sin(math.radians(angle/2))
    far = min(max_range, angular_limit)
    # Front approach of a straight constant-speed trajectory, b < Rnear.
    usable = math.sqrt(far**2-transverse**2)-math.sqrt(min_range**2-transverse**2)
    rows = [dict(speed_m_s=v, available_seconds=usable/v,
                 meets_2s_necessary_budget=usable/v >= required)
            for v in [15, 17, 25, 30, 35, 40]]
    check('base-speed-example', rows[0]['meets_2s_necessary_budget'])
    check('fast-speed-counterexample', not rows[-1]['meets_2s_necessary_budget'])
    # Need integrated relative motion and guaranteed projected visibility in production.
    r_needed_40 = math.sqrt((40*required+math.sqrt(min_range**2-transverse**2))**2+transverse**2)
    sphere_min_radius = r_needed_40 * math.sin(math.radians(angle/2))
    check('no-universal-range-guarantee', r_needed_40 > max_range)
    # Tether candidate in kart local coordinates, camera constructor defaults.
    tether_offset=(3.0,5.5,6.0)
    camera=(0.0,.4,-.25)
    delta=tuple(a-b for a,b in zip(tether_offset,camera))
    tether_distance=math.sqrt(sum(a*a for a in delta))
    tether_radius=.5
    pitch=math.radians(35)
    camera_x=delta[0]
    camera_y=delta[1]*math.cos(pitch)-delta[2]*math.sin(pitch)
    camera_z=delta[1]*math.sin(pitch)+delta[2]*math.cos(pitch)
    tether_angle=math.degrees(2*math.asin(tether_radius/tether_distance))
    cable_length=math.sqrt(sum(a*a for a in tether_offset))
    check('tether-camera-basis-length', math.isclose(camera_x**2+camera_y**2+camera_z**2,tether_distance**2))
    check('tether-angular-candidate', tether_angle > 3 and tether_distance<15 and tether_distance-tether_radius>.2)
    # Three readonly cargo icons and smaller beacon dots in existing isotropic HUD.
    cargo = [(x,-.16,.06) for x in [-.18,0,.18]]
    beacons = [(x,-.27,.025) for x in [-.18,0,.18]]
    for i, (x,y,r) in enumerate(cargo+beacons):
        check(f'hud-dome-contained-{i}', math.hypot(x,y)+r < 1)
        check(f'hud-below-rear-{i}', y-r > -.34)
        check(f'hud-above-badges-{i}', y+r < .025)
    for i,a in enumerate(cargo+beacons):
        for j,b in enumerate(cargo+beacons):
            if i<j:
                check(f'hud-disjoint-{i}-{j}', math.hypot(a[0]-b[0],a[1]-b[1]) > a[2]+b[2])
    # Crossing only forward continuous motion; reference progress, not native source test.
    def crossed(prev, curr, anchor, continuous=True):
        return continuous and prev < anchor <= curr
    check('cross-forward', crossed(99,101,100))
    check('cross-backward-no', not crossed(101,99,100))
    check('cross-teleport-no', not crossed(99,101,100,False))
    seen = set()
    rewards = 0
    for prev,curr in [(99,101),(101,99),(99,101)]:
        key = (1,'a')
        if crossed(prev,curr,100) and key not in seen:
            seen.add(key); rewards += 1
    check('cross-once', rewards == 1)
    # Native line time: distances before/after crossing, not progress extrapolation.
    hz, a,b,t0 = 120,.2,.1,10.0
    t1=t0+1/hz
    actual = t0*b/(a+b) + t1*a/(a+b)
    check('finish-interpolation', math.isclose(actual,10.005555555555556))
    check('finish-within-step', t0 < actual < t1)
    durations=[6,6,7,7,7,7]
    cuts=[0]
    for seconds in durations: cuts.append(cuts[-1]+seconds*30)
    check('intro-1200-frames', cuts[-1] == 1200)
    check('intro-audio-integral', 48000 % 30 == 0)
    intro=dict(seconds=sum(durations), fps=30, frame_boundaries=cuts,
               audio_rate=48000, samples_per_frame=1600,
               sample_boundaries=[x*1600 for x in cuts])
    check('intro-samples', intro['sample_boundaries'][-1] == 1_920_000)
    # Worst-case content receipt storage is fixed by validated manifest limits.
    check('bounded-receipts', 12*12*2 == 288)
    minimum_gap = min(math.hypot(a[0]-b[0],a[1]-b[1])-a[2]-b[2]
                      for i,a in enumerate(cargo+beacons) for j,b in enumerate(cargo+beacons) if i<j)
    check('hud-gap-value', math.isclose(minimum_gap,.025))
    result = dict(schema=1, evidence='reference mathematics, not production tests',
                  timing=timing, inventory=dict(valid_abstract_states=len(states), transitions=transitions),
                  outcomes=outcomes, opportunity=dict(budget_seconds=budget, required_seconds=required,
                  radius_m=radius, min_angular_diameter_deg=angle, angular_max_range_m=angular_limit,
                  range_m=far, near_m=min_range, transverse_m=transverse, usable_path_m=usable,
                  duration_by_speed=rows, constant_speed_limit_m_s=usable/required,
                  required_range_at_40m_s=r_needed_40, required_radius_at_40m_s=sphere_min_radius,
                  diameter_deg_at_75m=math.degrees(2*math.asin(radius/max_range)),
                  equirectangular_pixels_at_2048=2048*2*math.asin(radius/max_range)/(2*math.pi)),
                  tether=dict(offset=tether_offset, radius_m=tether_radius, camera_defaults=camera,
                              camera_pitch_deg=35, camera_relative=(camera_x,camera_y,camera_z),
                              distance_m=tether_distance, diameter_deg=tether_angle,
                              cable_length_from_local_origin_m=cable_length,
                              caveat='static constructor defaults, no smoothing/occlusion qualification'),
                  hud=dict(cargo=cargo,beacons=beacons,min_circle_gap=minimum_gap,
                           pixel_gap_at_min256=minimum_gap*256/2),
                  finish=dict(previous=t0,current=t1,native_interpolated=actual), intro=intro,
                  checks_passed=len(checks))
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write',action='store_true')
    args=parser.parse_args()
    result=run()
    serialized=json.dumps(result,indent=2,ensure_ascii=False)+'\n'
    if args.write:
        Path(__file__).with_name('calcoli.json').write_text(serialized)
    else:
        existing=Path(__file__).with_name('calcoli.json')
        if existing.exists():
            assert json.loads(existing.read_text()) == json.loads(serialized), 'calcoli.json differs; review before --write'
    print(serialized)

if __name__=='__main__': main()
