#!/usr/bin/env python3
"""Propose SC4 capsule anchors from measured camera trajectories.

Native measurement traces record, at 30 Hz and race speed, where the camera
was on every lap. For each anchor group this tool searches capsule positions
(driveline node, lateral offset, height) and an activation node such that,
in every recorded lap, the whole capsule silhouette stays geometrically
admissible from just before activation for longer than the qualification
window: inside the projection, outside every reserved HUD area of layout v3
(a superset of v2), within the capsule range, while the kart holds the
qualified share of its native base speed.

Geometry only. Occlusion by scenery and karts needs depth, so the proposal
must be measured natively again (qualify_mission_autopilot.py); a proposal is
never a qualification. The output manifest stays development_unqualified.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import validate_mission_qualification as v  # noqa: E402

SPEED_SHARE = .95          # windows must hold this share of the native base speed
WINDOW_S = 2.0             # qualification window per capsule phase
MARGIN_S = .5              # extra time demanded from the proposal
PREFETCH_S = .15           # admitted before the activation crossing (readback)
CAPSULE_RADIUS = 2.0
RANGE_M = (8.0, 75.0)


def sub(a, b): return [x - y for x, y in zip(a, b)]
def add(a, b): return [x + y for x, y in zip(a, b)]
def mul(a, s): return [x * s for x in a]
def dot(a, b): return sum(x * y for x, y in zip(a, b))
def norm(a): return math.sqrt(dot(a, a))
def unit(a): return mul(a, 1 / norm(a))
def cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def driveline(track: Path):
    """Nodes with lap fraction and an orthonormal basis, as the SC4 plan computed them."""
    quads = []
    for q in ET.parse(track / 'quads.xml').getroot().findall('quad'):
        points = []
        for i in range(4):
            value = q.attrib[f'p{i}']
            if ':' in value:
                qi, pi = map(int, value.split(':'))
                points.append(quads[qi][pi])
            else:
                points.append([float(x) for x in value.split()])
        quads.append(points)
    lower = [mul(add(q[0], q[1]), .5) for q in quads]
    upper = [mul(add(q[2], q[3]), .5) for q in quads]
    steps = [norm(sub(lower[(i + 1) % len(quads)], p)) for i, p in enumerate(lower)]
    cumulative = [0.]
    for step in steps:
        cumulative.append(cumulative[-1] + step)
    nodes = []
    for i, q in enumerate(quads):
        forward = unit(sub(upper[i], lower[i]))
        across = sub(q[1], q[0])
        right = unit(sub(across, mul(forward, dot(across, forward))))
        up = unit(cross(forward, right))
        nodes.append(dict(node=i, fraction=cumulative[i] / cumulative[-1], center=mul(add(lower[i], upper[i]), .5),
                          forward=forward, right=right, up=up, width=(norm(sub(q[1], q[0])) + norm(sub(q[2], q[3]))) / 2))
    return nodes


def laps_of(traces):
    """Per (trace, lap): frames in time order, with what admission needs."""
    sequences, fingerprint = {}, None
    for index, path in enumerate(traces):
        with open(path) as stream:
            header = json.loads(stream.readline())
            fingerprint = header['fingerprint']
            for line in stream:
                row = json.loads(line)
                if row['type'] != 'frame' or row['officialLap'] < 0:
                    continue
                sequences.setdefault((index, row['officialLap']), []).append(
                    (int(row['captureUs']), row['fraction'], row['minimumSpeed'], bool(row['continuous']), row['camera']))
    return sequences, fingerprint


def admissible(center, camera, fingerprint):
    distance = norm(sub(center, camera['position']))
    return RANGE_M[0] <= distance <= RANGE_M[1] and v.rectangle_free(center, CAPSULE_RADIUS, camera, fingerprint)


def window_from(frames, activation, center, fingerprint, speed):
    """Seconds the capsule stays admissible from PREFETCH_S before the crossing."""
    crossing = next((i for i, f in enumerate(frames) if f[1] >= activation), None)
    if crossing is None:
        return 0.
    start_us = frames[crossing][0] - PREFETCH_S * 1e6
    first = crossing
    while first > 0 and frames[first - 1][0] >= start_us:
        first -= 1
    last = None
    for i in range(first, len(frames)):
        time, _, minimum, continuous, camera = frames[i]
        if last is not None and time - frames[last][0] > 50000:
            break
        if minimum < speed or not continuous or not admissible(center, camera, fingerprint):
            break
        last = i
    if last is None or last < crossing:
        return 0.
    return (frames[last][0] - frames[crossing][0]) / 1e6


def candidates_for(nodes, around, laterals, heights):
    for n in around:
        node = nodes[n % len(nodes)]
        for lateral in laterals:
            for height in heights:
                yield n % len(nodes), lateral, height, add(node['center'], add(mul(node['right'], lateral), mul(node['up'], height)))


def place(nodes, sequences, fingerprint, group, lo, hi, laterals, heights):
    """Best activation node and capsule pair for one group within [lo, hi) of the lap."""
    speed = SPEED_SHARE * fingerprint['nativeBaseSpeed']
    relevant = {key: [f for f in frames if lo - .02 <= f[1] <= hi + .04] for key, frames in sequences.items()}
    activations = [node for node in nodes if lo <= node['fraction'] < hi - .02]
    best = []
    # Capsules and expiry stay within the lap: a window crossing the finish
    # line would belong to two laps.
    for anchor_node in range(len(nodes) - 1):
        if not lo + .01 <= nodes[anchor_node]['fraction'] <= min(hi + .03, nodes[-2]['fraction']):
            continue
        for node, lateral, height, center in candidates_for(nodes, [anchor_node], laterals, heights):
            for activation in activations:
                if activation['fraction'] >= nodes[node]['fraction']:
                    continue
                # The validator keeps, per lap, the best window among the
                # traces of a phase; geometry does not depend on the phase, so
                # every recorded race of a lap may serve it. Worst over laps.
                per_lap = {}
                for (trace, lap), frames in relevant.items():
                    if frames:
                        window = window_from(frames, activation['fraction'], center, fingerprint, speed)
                        per_lap[lap] = max(per_lap.get(lap, 0.), window)
                worst = min(per_lap.values())
                if worst >= WINDOW_S + MARGIN_S:
                    best.append((worst, activation['node'], node, lateral, height, center))
    best.sort(key=lambda b: (-b[0], abs(b[3]), -b[4]))
    # Two opportunities per group (C >= 2): same activation, not overlapping.
    for first in best:
        for second in best:
            if second[1] == first[1] and norm(sub(first[5], second[5])) >= 2 * CAPSULE_RADIUS + .5 and \
                    (second[3] > 0) != (first[3] > 0):
                return first, second
    for first in best:
        for second in best:
            if second[1] == first[1] and norm(sub(first[5], second[5])) >= 2 * CAPSULE_RADIUS + .5:
                return first, second
    return None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--track', type=Path, required=True, help='stk-assets/tracks/snowtuxpeak')
    parser.add_argument('--anchors', type=Path, required=True)
    parser.add_argument('--trace', type=Path, action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    nodes = driveline(args.track)
    sequences, fingerprint = laps_of(args.trace)
    fingerprint = dict(fingerprint, evaluatedLayoutRevision=3)  # v3 masks contain v2's
    manifest = json.loads(args.anchors.read_text())
    groups = manifest['groups']
    bounds = [(g['activationFraction'] - .03, (groups[i + 1]['activationFraction'] if i + 1 < len(groups) else 1.0) - .005)
              for i, g in enumerate(groups)]
    laterals = [s * d for d in (8, 12, 16, 20, 25, 30, 35, 40) for s in (-1, 1)]
    heights = [2, 4, 6, 8, 10, 12]
    report = []
    for group, (lo, hi) in zip(groups, bounds):
        lo = max(lo, groups[groups.index(group) - 1]['expiryFraction'] if groups.index(group) else 0.0)
        result = place(nodes, sequences, fingerprint, group, lo, hi, laterals, heights)
        if result is None:
            report.append(dict(group=group['id'], placed=False))
            print(f"{group['id']}: no admissible pair in [{lo:.3f}, {hi:.3f})", file=sys.stderr)
            continue
        first, second = result
        activation = nodes[first[1]]
        expiry = nodes[min(max(first[2], second[2]) + 1, len(nodes) - 1)]
        group['activationNode'], group['activationFraction'] = activation['node'], activation['fraction']
        group['expiryNode'], group['expiryFraction'] = expiry['node'], expiry['fraction']
        for capsule, chosen in zip(group['capsules'], result):
            capsule['center'] = [round(x, 8) for x in chosen[5]]
        report.append(dict(group=group['id'], placed=True, activationNode=activation['node'], expiryNode=expiry['node'],
                           capsules=[dict(node=c[2], lateralM=c[3], heightM=c[4], worstWindowS=round(c[0], 2)) for c in result]))
        print(f"{group['id']}: activation node {activation['node']}, windows "
              f"{round(first[0], 2)} / {round(second[0], 2)} s, offsets {first[3]:+} m x {first[4]} m, "
              f"{second[3]:+} m x {second[4]} m", file=sys.stderr)
    manifest['author'] = manifest['author'].split(';')[0] + '; capsules placed from measured camera paths'
    args.output.write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(report, indent=1))
    return 0 if all(r['placed'] for r in report) else 1


if __name__ == '__main__':
    sys.exit(main())
