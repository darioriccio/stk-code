#!/usr/bin/env python3
"""Evaluate source scalar expressions against independent geometry; no native code."""
import itertools
import math
from pathlib import Path
import random
import re
import struct
import unittest

ROOT = Path(__file__).resolve().parents[3]
CORE = (ROOT/'src/modes/immersive_relay/scintilla_combat.cpp').read_text()
HEADER = (ROOT/'src/modes/immersive_relay/scintilla_combat.hpp').read_text()
ANIMATION = (ROOT/'src/karts/explosion_animation.cpp').read_text()
GEOMETRY = CORE.split('bool StaggerEnvelope::create(')[1].split('bool StaggerEnvelope::supportingFace(')[0]
ALIGNMENT = float(re.search(r'MinimumSupportAlignment=([0-9.]+)', HEADER)[1])


def split_top_level(text):
    parts, start, depth = [], 0, 0
    for i, char in enumerate(text):
        if char in '([{': depth += 1
        if char in ')]}': depth -= 1
        if depth == 0 and char in ',;':
            parts.append(text[start:i].strip()); start = i+1
    parts.append(text[start:].strip())
    return parts


def assignment(name):
    start = re.search(r'(?<![\w.])'+re.escape(name)+r'\s*=', GEOMETRY).end()
    return split_top_level(GEOMETRY[start:])[0]


def scalar(expression, scope):
    expression = expression.replace('std::', '').replace('e.', '')
    return eval(expression, {'__builtins__': {}, 'min': min, 'max': max, 'abs': abs,
                            'hypot': math.hypot, 'sin': math.sin, 'cos': math.cos}, scope)


def envelope(lower, upper):
    # Only valid fixture inputs are evaluated here. Native invalid-input behavior
    # is covered by written C++ regressions, not claimed executed by this script.
    scope = {'lower': lower, 'upper': upper, 'lift': 0.25,
             'maximumPitch': math.radians(8), 'MinimumSupportAlignment': ALIGNMENT}
    for name in ('y', 'z', 'radius', 'pitch', 'c', 's', 'ycLo', 'ycHi', 'zcLo', 'zcHi'):
        scope[name] = scalar(assignment('e.'+name if name in ('radius', 'pitch') else name), scope)
    for name in ('lo', 'hi'):
        body = re.search(r'const double '+name+r'\[3\]=\{([^;]+)\};', GEOMETRY)[1]
        scope[name] = [scalar(expr, scope) for expr in split_top_level(body)]
    center, half = [], []
    for i in range(3):
        scope['i'] = i
        center.append(scalar(assignment('e.center[i]'), scope))
        half.append(scalar(assignment('e.halfExtents[i]'), scope))
    return center, half, scope['pitch'], scope['radius']


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def rotate(v, axis, angle):
    # Independent unit-quaternion rotation, not the interval enclosure formula.
    q = tuple(x*math.sin(angle/2) for x in axis)
    t = tuple(2*x for x in cross(q, v))
    correction = cross(q, t)
    return tuple(v[i]+math.cos(angle/2)*t[i]+correction[i] for i in range(3))


def binary32(x):
    return struct.unpack('f', struct.pack('f', x))[0]


def next_float_up(x):
    return struct.unpack('f', struct.pack('I', struct.unpack('I', struct.pack('f', x))[0]+1))[0]


class StaggerClearanceChecks(unittest.TestCase):
    def test_original_ceiling_counterexample_is_inside_the_checked_sweep(self):
        center, half, pitch, radius = envelope((-0.5, 0, -1), (0.5, 1, 1))
        old = 0.5+math.sin(math.radians(8))+0.02+0.25
        self.assertLess(old, 1.15)
        initial_top = center[1]+half[1]
        self.assertLess(initial_top, 1.15)
        self.assertGreater(initial_top+0.25, 1.15)
        self.assertGreater(rotate((0, 1, -1), (1, 0, 0), pitch)[1]+0.25, 1.15)
        self.assertLess(initial_top+0.25, 1.5)  # A higher roof need not suppress the hop.
        self.assertAlmostEqual(radius, math.sqrt(2))

    def test_asymmetric_compound_bounds_fit_at_every_pitch_and_world_pose(self):
        rng = random.Random(1267)
        for _ in range(40):
            lower = tuple(rng.uniform(-8, 8) for _ in range(3))
            upper = tuple(x+rng.uniform(0.03, 6) for x in lower)
            center, half, pitch, _ = envelope(lower, upper)
            axis = [rng.uniform(-1, 1) for _ in range(3)]
            norm = math.sqrt(sum(x*x for x in axis)); axis = tuple(x/norm for x in axis)
            angle = rng.uniform(-math.pi, math.pi)
            origin = tuple(rng.uniform(-10000, 10000) for _ in range(3))
            up = rotate((0, 1, 0), axis, angle)
            for point in itertools.product(*zip(lower, upper)):
                for step in range(41):
                    w = step/40
                    posed = rotate(point, (1, 0, 0), pitch*w)
                    world = tuple(origin[i]+rotate(posed, axis, angle)[i]+up[i]*0.25*w for i in range(3))
                    box = tuple(origin[i]+rotate(center, axis, angle)[i]+up[i]*0.25*w for i in range(3))
                    local = rotate(tuple(world[i]-box[i] for i in range(3)), axis, -angle)
                    for i in range(3): self.assertLessEqual(abs(local[i]), half[i])

    def test_pitch_displacement_cannot_enter_an_admitted_supporting_plane(self):
        for lower, upper in [((-0.5, 0, -1), (0.5, 1, 1)),
                             ((-1, -2, 0.3), (1, -0.2, 4)),
                             ((1, 0.2, -4), (2, 3, -1))]:
            _, _, pitch, radius = envelope(lower, upper)
            for point in itertools.product(*zip(lower, upper)):
                for step in range(101):
                    w = step/100
                    p = rotate(point, (1, 0, 0), pitch*w)
                    rotation_distance = math.sqrt(sum((p[i]-point[i])**2 for i in range(3)))
                    self.assertLessEqual(rotation_distance, ALIGNMENT*0.25*w+1e-14)
                    for normal_up in (0.500001, 0.75, 1):
                        outward = normal_up*(p[1]-point[1]+0.25*w)-math.sqrt(1-normal_up**2)*abs(p[2]-point[2])
                        self.assertGreaterEqual(outward, -1e-14)

    def test_upward_ledges_and_invalid_contacts_are_not_support(self):
        body = CORE.split('bool StaggerEnvelope::supportingFace(')[1].split('int Runtime::slot')[0]
        expression = re.search(r'return ([^;]+);', body, re.S)[1]
        expression = expression.replace('std::isfinite', 'isfinite').replace('&&', ' and ')
        for alignment, height, bottom, allowed in [(1, 0, 0, True), (0.500001, -1, 0, True),
                (0.5, -1, 0, False), (1, 0.000001, 0, False), (-1, -1, 0, False),
                (math.nan, 0, 0, False), (1, math.inf, 0, False), (1, 0, math.nan, False)]:
            actual = eval('('+expression.strip()+')', {'__builtins__': {}, 'isfinite': math.isfinite},
                {'normalUp': alignment, 'contactHeight': height, 'bodyBottom': bottom, 'MinimumSupportAlignment': ALIGNMENT})
            self.assertEqual(actual, allowed)

    def test_float_center_and_extent_conversion_only_expands_the_envelope(self):
        for scale in (0.001, 1, 100, 10000, 1e20, 1e30):
            lower = (-0.7*scale, 0.12345*scale, -2.3*scale)
            upper = (1.2*scale, 1.91234*scale, 0.2*scale)
            center, half, _, _ = envelope(lower, upper)
            for c, h in zip(center, half):
                converted = binary32(c)
                outer = next_float_up(binary32(h+abs(converted-c)))
                self.assertLessEqual(converted-outer, c-h)
                self.assertGreaterEqual(converted+outer, c+h)

    def test_bullet_queries_use_the_actual_shape_center_and_conservative_fallback(self):
        helper = ANIMATION.split('bool relayStaggerClearance(')[1].split('ExplosionAnimation* ExplosionAnimation::createRelayStagger')[0]
        self.assertIn('shape->getAabb(identity,lower,upper)', helper)
        self.assertIn('from.setOrigin(body(center))', helper)
        self.assertIn('bounds.halfExtents[i]+std::abs(double(center[i])-bounds.center[i])', helper)
        self.assertIn('std::nextafter(float(bounds.pitch),0.0f)', helper)
        self.assertLess(helper.index('world->contactTest('), helper.index('world->convexSweepTest('))
        self.assertIn('if(overlap.blocked||clearance.hasHit())return false', helper)
        self.assertNotIn('getKartHeight()', helper)
        self.assertNotIn('getKartWidth()', helper)
        factory = ANIMATION.split('ExplosionAnimation* ExplosionAnimation::createRelayStagger')[1].split('ExplosionAnimation::ExplosionAnimation')[0]
        self.assertIn('float height=0,pitch=0', factory)
        self.assertIn('if(relayStaggerClearance(kart,physics->getPhysicsWorld(),&pitch))height=0.25f', factory)


if __name__ == '__main__':
    unittest.main(verbosity=2)
