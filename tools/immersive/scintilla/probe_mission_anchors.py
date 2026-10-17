#!/usr/bin/env python3
"""Explore SC4 capsule anchors with native probes, then place them.

Geometry alone cannot see terrain: anchors placed only from camera paths
ended behind snow banks and rock walls. This tool works in two steps.

`candidates` keeps, on a grid around the driveline, the centres whose
geometry (capsule range, projection, every reserved HUD area of layout v3)
admits them long enough in most recorded laps, and writes them for
STK_RELAY_SCINTILLA_PROBES. A measurement recorded with that variable
measures four of them per sample with the native queries, masks and depth.

`place` reads the probe traces. Visibility depends on where the camera is,
so probe samples are looked up by track fraction. For every candidate and
recorded lap, a window starts at the activation crossing, needs the capsule
admissible and seen just before it (the runtime decides there with a live
proof), and lasts while geometry, race speed and every nearby probe hold.
The worst lap must exceed the qualification window with a margin. Probes
sample each centre only every few hundred milliseconds, so a placement is
a proposal: the native qualification (qualify_mission_autopilot.py) decides.
"""
from __future__ import annotations

import argparse
import bisect
import json
import os
import sys
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import place_mission_anchors as geo  # noqa: E402
import validate_mission_qualification as v  # noqa: E402

LATERALS = [s * d for d in (6, 10, 14, 18, 24, 30) for s in (-1, 1)]
HEIGHTS = [3, 6, 9, 12, 16]
SCREEN_STEP_US = 100000      # geometry screening at 10 Hz
REACH = .004                 # lap fraction a probe sample speaks for (~4 m)
MARGIN_S = .5                # extra time demanded from a placement


def grid(nodes, laterals=LATERALS, heights=HEIGHTS, span=(0., 1.)):
    for n, node in enumerate(nodes):
        if not span[0] <= node['fraction'] < span[1]:
            continue
        for lateral in laterals:
            for height in heights:
                center = geo.add(node['center'], geo.add(geo.mul(node['right'], lateral), geo.mul(node['up'], height)))
                yield n, lateral, height, center


def longest_run_us(frames, center, fingerprint, speed):
    best, start, last = 0, None, None
    for time, _, minimum, continuous, camera in frames:
        if last is not None and time - last < SCREEN_STEP_US:
            continue
        ok = continuous and minimum >= speed and geo.admissible(center, camera, fingerprint)
        if ok and start is None:
            start = time
        if not ok:
            start = None
        if ok:
            best = max(best, time - start)
        last = time
    return best


def candidates(args):
    nodes = geo.driveline(args.track)
    sequences, fingerprint = geo.laps_of(args.trace)
    fingerprint = dict(fingerprint, evaluatedLayoutRevision=3)
    speed = geo.SPEED_SHARE * fingerprint['nativeBaseSpeed']
    import numpy
    positions = {key: numpy.array([f[4]['position'] for f in frames]) for key, frames in sequences.items()}
    kept = []
    for n, lateral, height, center in grid(nodes, args.laterals or LATERALS, args.heights or HEIGHTS, (args.start, args.end)):
        good = 0
        for key, frames in sequences.items():
            close = numpy.nonzero(numpy.linalg.norm(positions[key] - numpy.array(center), axis=1) <= geo.RANGE_M[1] + 5)[0]
            near = [frames[i] for i in close]
            good += longest_run_us(near, center, fingerprint, speed) >= args.min_run * 1e6
        if good >= args.min_share * len(sequences):
            kept.append(dict(node=n, lateralM=lateral, heightM=height, center=center))
    with open(args.output, 'w') as stream:
        for entry in kept:
            stream.write('%.6f %.6f %.6f\n' % tuple(entry['center']))
    Path(str(args.output) + '.json').write_text(json.dumps(kept))
    print(f'{len(kept)} centres pass geometry in most laps', file=sys.stderr)
    return 0


def probe_samples(traces):
    """Per candidate index: (fraction, visible) native samples, by fraction."""
    samples = {}
    for path in traces:
        with open(path) as stream:
            stream.readline()
            for line in stream:
                row = json.loads(line)
                if row.get('type') != 'frame' or row['officialLap'] < 0:
                    continue
                for obj in row['objects']:
                    if obj['kind'] != 'capsule' or obj['group'] != -2 or obj['status'] != 'ready':
                        continue
                    visible = obj['range'] and obj['rectangleFree'] and obj['idealPixels'] > 0 and \
                        obj['idealPixels'] == obj['admissiblePixels'] == obj['visiblePixels']
                    samples.setdefault(obj['capsule'], []).append((row['fraction'], visible))
    for value in samples.values():
        value.sort()
    return samples


class Seen:
    def __init__(self, samples):
        self.samples = samples; self.fractions = [s[0] for s in samples]

    def near(self, fraction):
        lo = bisect.bisect_left(self.fractions, fraction - REACH); hi = bisect.bisect_right(self.fractions, fraction + REACH)
        return self.samples[lo:hi]


def admissible(center, camera, fingerprint, inflate):
    """Capsule range, and its silhouette grown by `inflate` clear of the HUD:
    a centre near a reserved edge flips with small camera differences."""
    distance = geo.norm(geo.sub(center, camera['position']))
    return geo.RANGE_M[0] <= distance <= geo.RANGE_M[1] and \
        v.rectangle_free(center, geo.CAPSULE_RADIUS * inflate, camera, fingerprint)


def lap_profile(frames, center, fingerprint, speed, seen, inflate=1.):
    """Per frame: admissible at race speed, every nearby probe visible, probe count."""
    profile = []
    for time, fraction, minimum, continuous, camera in frames:
        near = seen.near(fraction)
        profile.append((time, fraction, continuous and minimum >= speed and admissible(center, camera, fingerprint, inflate),
                        all(s[1] for s in near), len(near)))
    return profile


def window(profile, activation, required):
    """(seconds, probes, fraction where `required` seconds were reached) from
    the activation crossing in one lap. The group's expiry must come later."""
    crossing = next((i for i, p in enumerate(profile) if p[1] >= activation), None)
    if not crossing:
        return 0., 0, None
    # The runtime decides at the crossing with the proof read back just before.
    # Profiles keep only frames near the capsule: a gap here means the capsule
    # was still out of range at the crossing, so nothing could prove it.
    before = profile[crossing - 1]
    if profile[crossing][0] - before[0] > v.MAX_GAP_US or not (before[2] and before[3] and before[4]):
        return 0., 0, None
    last, probes, reached = None, before[4], None
    for i in range(crossing, len(profile)):
        time, fraction, admissible_now, seen, count = profile[i]
        if last is not None and time - profile[last][0] > v.MAX_GAP_US:
            break
        if not (admissible_now and seen):
            break
        probes += count; last = i
        if reached is None and time - profile[crossing][0] >= required * 1e6:
            reached = fraction
    return (0., 0, None) if last is None else ((profile[last][0] - profile[crossing][0]) / 1e6, probes, reached)


def place(args):
    """Feasible capsule pairs anywhere on the lap, then non-overlapping groups."""
    import copy
    import numpy
    nodes = geo.driveline(args.track)
    sequences, fingerprint = geo.laps_of(args.geometry + args.probe)
    fingerprint = dict(fingerprint, evaluatedLayoutRevision=3)
    speed = geo.SPEED_SHARE * fingerprint['nativeBaseSpeed']
    listed = json.loads(Path(str(args.candidates) + '.json').read_text())
    probed = probe_samples(args.probe)
    positions = {key: numpy.array([f[4]['position'] for f in frames]) for key, frames in sequences.items()}
    last_fraction = nodes[-2]['fraction']
    manifest = json.loads(args.anchors.read_text())
    kept = [g for g in manifest['groups'] if g['id'] in args.keep]
    singles = {}  # activation node -> [(worst, candidate index, probes)]
    for index, entry in enumerate(listed):
        node = nodes[entry['node']]
        if index not in probed or not .01 <= node['fraction'] <= last_fraction or \
                any(g['activationFraction'] - args.spacing < node['fraction'] < g['expiryFraction'] + args.spacing for g in kept):
            continue
        seen = Seen(probed[index])
        profiles = []
        for key, frames in sequences.items():
            close = numpy.nonzero(numpy.linalg.norm(positions[key] - numpy.array(entry['center']), axis=1) <= geo.RANGE_M[1] + 5)[0]
            profiles.append((key[1], lap_profile([frames[i] for i in close], entry['center'], fingerprint, speed, seen, args.inflate)))
        for activation in nodes:
            if not node['fraction'] - args.lead <= activation['fraction'] < node['fraction']:
                continue
            per_lap, probes = {}, 0
            for lap, profile in profiles:
                seconds, count, reached = window(profile, activation['fraction'], geo.WINDOW_S + args.margin)
                if seconds > per_lap.get(lap, (0., None))[0]:
                    per_lap[lap] = (seconds, reached)
                probes += count
            worst = min(w[0] for w in per_lap.values()) if per_lap else 0.
            if worst >= geo.WINDOW_S + args.margin and probes >= args.min_probes:
                # Each lap's best window must fit before the expiry.
                need = max(w[1] for w in per_lap.values())
                singles.setdefault(activation['node'], []).append((worst, index, probes, need))
    # Every pair among an activation's ten best capsules: an earlier expiry
    # can leave room for the next group.
    pairs = []
    for activation, options in singles.items():
        options.sort(key=lambda o: (-o[0], abs(listed[o[1]]['lateralM'])))
        options = options[:10]
        for i, first in enumerate(options):
            for second in options[i + 1:]:
                if geo.norm(geo.sub(listed[first[1]]['center'], listed[second[1]]['center'])) < 2 * geo.CAPSULE_RADIUS + .5:
                    continue
                # Capsules live until the kart has passed them (more time to
                # play), and at least until every lap's window is complete.
                need = max(first[3], second[3]) + .002
                passed = nodes[min(max(listed[first[1]]['node'], listed[second[1]]['node']) + 1, len(nodes) - 1)]['fraction']
                expiry = next((n['node'] for n in nodes if n['fraction'] >= max(need, passed)), None)
                if expiry is None or nodes[expiry]['fraction'] > last_fraction:
                    continue
                pairs.append((min(first[0], second[0]), activation, expiry, first, second))
    # Most groups first (the qualification needs four per lap and each one
    # is an opportunity), then the best worst window; windows stay apart so
    # opportunities spread over the lap.
    # Groups verified natively stay; new ones fit around them.
    pairs = [p for p in pairs if all(nodes[p[2]]['fraction'] + args.spacing <= g['activationFraction'] or
                                     g['expiryFraction'] + args.spacing <= nodes[p[1]]['fraction'] for g in kept)]
    pairs.sort(key=lambda p: nodes[p[2]]['fraction'])
    for pair in pairs:
        print(f"pair: activation {nodes[pair[1]]['fraction']:.3f} expiry {nodes[pair[2]]['fraction']:.3f} worst {pair[0]:.2f} s, "
              + ', '.join(f"{listed[c[1]]['lateralM']:+} x {listed[c[1]]['heightM']} at node {listed[c[1]]['node']}" for c in pair[3:]), file=sys.stderr)
    best = []  # per pair: best chain ending with it, as (count, worst, chain)
    for i, pair in enumerate(pairs):
        start = nodes[pair[1]]['fraction']
        chain = (1, pair[0], [pair])
        for j in range(i):
            if nodes[pairs[j][2]]['fraction'] + args.spacing <= start and best[j][0] < args.groups - len(kept):
                candidate = (best[j][0] + 1, min(best[j][1], pair[0]), best[j][2] + [pair])
                if candidate[:2] > chain[:2]:
                    chain = candidate
        best.append(chain)
    chosen = max(best, key=lambda c: c[:2])[2] if best else []
    template = manifest['groups'][0]
    groups = []
    for quality, activation, expiry, first, second in chosen:
        group = copy.deepcopy(template)
        group['activationNode'], group['activationFraction'] = activation, nodes[activation]['fraction']
        group['expiryNode'], group['expiryFraction'] = expiry, nodes[expiry]['fraction']
        for capsule, pick in zip(group['capsules'], (first, second)):
            capsule['center'] = [round(x, 8) for x in listed[pick[1]]['center']]
        groups.append(group)
        print(f"new group: activation node {activation} ({nodes[activation]['fraction']:.3f}), expiry {expiry}, worst windows "
              f"{first[0]:.2f}/{second[0]:.2f} s, " + ', '.join(f"{listed[p[1]]['lateralM']:+} m x {listed[p[1]]['heightM']} m at node "
              f"{listed[p[1]]['node']} ({p[2]} probes)" for p in (first, second)), file=sys.stderr)
    groups = sorted(groups + copy.deepcopy(kept), key=lambda g: g['activationFraction'])
    for number, group in enumerate(groups, 1):
        group['id'] = f'snow_supply_{number}'
    manifest['groups'] = groups
    manifest['author'] = manifest['author'].split(';')[0] + '; capsules placed from native probe visibility'
    args.output.write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'{len(pairs)} feasible pairs, {len(groups)} groups placed', file=sys.stderr)
    return 0 if len(groups) >= 4 else 1


def record(args):
    """Natively driven races measuring the candidates instead of the anchors."""
    import qualify_mission_autopilot as qualify
    import crew_simulator as sim
    from types import SimpleNamespace
    os.environ['STK_RELAY_SCINTILLA_PROBES'] = str(args.candidates.resolve())
    staging = args.runtime_dir.resolve() / 'probes'
    staging.mkdir(parents=True, exist_ok=False)
    launch = SimpleNamespace(runtime_dir=args.runtime_dir, build_dir=args.build_dir, assets_dir=args.assets_dir,
                             crew=args.players - 1, projection='fisheye_domemaster', tuning=None,
                             telemetry_dir=args.runtime_dir / 'telemetry')
    token, port = 'probe-' + uuid.uuid4().hex, sim.free_port()
    process, log = sim.launch(launch, token, port)
    api = sim.Api(f'http://127.0.0.1:{port}/immersive/v1', token)
    try:
        qualify.until(lambda: process.poll() is None and qualify.sim_state(api), 300, 'producer API unavailable', interval=1)
        for race in range(1, args.races + 1):
            exported = qualify.record(api, args.players, 'shell', staging / f'probe-{race}.ndjson', args.race_timeout)
            qualify.log(f'probe-{race}.ndjson: {exported["byteCount"]} bytes')
        return 0
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=15)
            except Exception:
                process.kill()
        log.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest='command', required=True)
    c = sub.add_parser('candidates', help='write centres worth probing')
    c.add_argument('--track', type=Path, required=True, help='stk-assets/tracks/snowtuxpeak')
    c.add_argument('--trace', type=Path, action='append', required=True, help='measurement traces for camera paths')
    c.add_argument('--output', type=Path, required=True)
    c.add_argument('--laterals', type=float, nargs='+', help='metres right of the driveline (default: a side grid)')
    c.add_argument('--heights', type=float, nargs='+', help='metres above the driveline')
    c.add_argument('--min-run', type=float, default=geo.WINDOW_S + MARGIN_S, help='seconds of admissible geometry')
    c.add_argument('--min-share', type=float, default=.5, help='share of recorded laps with such a run')
    c.add_argument('--start', type=float, default=0., help='first lap fraction of the nodes')
    c.add_argument('--end', type=float, default=1., help='lap fraction after the last node')
    p = sub.add_parser('place', help='place capsules from probe traces')
    p.add_argument('--track', type=Path, required=True)
    p.add_argument('--anchors', type=Path, required=True)
    p.add_argument('--candidates', type=Path, required=True)
    p.add_argument('--geometry', type=Path, action='append', default=[], help='more traces for camera paths')
    p.add_argument('--probe', type=Path, action='append', required=True, help='probe traces')
    p.add_argument('--min-probes', type=int, default=8, help='probe samples over all laps inside a window')
    p.add_argument('--lead', type=float, default=.12, help='longest lap fraction from activation to a capsule')
    p.add_argument('--groups', type=int, default=6, choices=range(4, 13))
    p.add_argument('--spacing', type=float, default=.04, help='lap fraction between two groups')
    p.add_argument('--keep', nargs='*', default=[], help='group ids of --anchors that stay as they are')
    p.add_argument('--margin', type=float, default=MARGIN_S, help='seconds a worst lap must exceed the window by')
    p.add_argument('--inflate', type=float, default=1.5, help='capsule radius factor for the HUD clearance')
    p.add_argument('--output', type=Path, required=True)
    r = sub.add_parser('record', help='record natively driven races that measure the candidates')
    r.add_argument('--build-dir', type=Path, required=True)
    r.add_argument('--assets-dir', type=Path)
    r.add_argument('--runtime-dir', type=Path, required=True, help='new directory on the external SSD')
    r.add_argument('--candidates', type=Path, required=True)
    r.add_argument('--players', type=int, default=3, choices=range(2, 7))
    r.add_argument('--races', type=int, default=2)
    r.add_argument('--race-timeout', type=float, default=900)
    args = parser.parse_args(argv)
    return {'candidates': candidates, 'place': place, 'record': record}[args.command](args)


if __name__ == '__main__':
    sys.exit(main())
