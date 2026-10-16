#!/usr/bin/env python3
"""Offline tests of crew_simulator.py: geometry, presentation proofs, human model."""
import math
import random
import statistics
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import crew_simulator as sim  # noqa: E402


def frames_present(runs, t):
    return {r['frameId'] for r in runs if r['presentedFromUs'] <= t <= r['presentedUntilUs']}


class GeometryTests(unittest.TestCase):
    def test_projection_round_trip_and_centre(self):
        for projection in ({'kind': 'equirectangular'}, {'kind': 'fisheye_domemaster', 'domeTiltDeg': 0},
                           {'kind': 'fisheye_domemaster', 'domeTiltDeg': -30}):
            for u in (.1, .3, .5, .77):
                for v in (.2, .5, .64, .9):
                    d = sim.direction(projection, u, v)
                    if d is None:
                        continue
                    ru, rv = sim.uv(projection, tuple(3 * c for c in d))
                    self.assertAlmostEqual(ru, u, places=9)
                    self.assertAlmostEqual(rv, v, places=9)
        self.assertEqual(sim.direction({'kind': 'fisheye_domemaster'}, .5, .5), (0.0, 0.0, 1.0))
        self.assertIsNone(sim.direction({'kind': 'fisheye_domemaster'}, 0, 0))
        self.assertAlmostEqual(sim.angle_deg((1, 0, 0), (0, 1, 0)), 90)

    def test_slerp_and_tremor_rotation_are_small_and_bounded(self):
        a, b = (0, 0, 1), (1, 0, 0)
        half = sim.slerp(a, b, .5)
        self.assertAlmostEqual(sim.angle_deg(a, half), 45, places=6)
        moved = sim.rotate((0, 0, 1), 1.0, 0.0)
        self.assertAlmostEqual(sim.angle_deg((0, 0, 1), moved), 1.0, places=2)


class PresentationTests(unittest.TestCase):
    def test_runs_follow_capture_plus_latency_and_never_rewrite(self):
        offset, latency = 5_000, 45_000
        captures = [1_000_000 + 16_667 * i for i in range(60)]
        previous = None
        proof_from = None
        for step in range(5, 60):
            view_now = captures[step - 1] + 2_000          # view generated after the newest capture
            frames = [{'id': i + 1, 'captureUs': captures[i]} for i in range(step)][-10:]
            now_host = view_now - offset + 30_000
            start, runs = sim.presentation_runs(frames, offset, latency, view_now, now_host, proof_from)
            self.assertTrue(runs)
            self.assertEqual(start, runs[0]['presentedFromUs'])
            self.assertGreaterEqual(start, now_host - sim.LOOKBACK_US)
            for run in runs:
                capture = captures[run['frameId'] - 1] - offset
                self.assertGreaterEqual(run['presentedFromUs'], capture + latency)  # never before capture
                self.assertLessEqual(run['presentedUntilUs'], now_host - 1000)
            for a, b in zip(runs, runs[1:]):
                self.assertEqual(a['presentedUntilUs'], b['presentedFromUs'])  # continuous coverage
                self.assertLess(a['frameId'], b['frameId'])
            if previous:
                prev_from, prev_runs = previous
                # Producer rule: every overlapping instant keeps the same frames,
                # and both frontiers only move forward.
                self.assertGreaterEqual(start, prev_from)
                self.assertGreaterEqual(runs[-1]['presentedUntilUs'], prev_runs[-1]['presentedUntilUs'])
                lo, hi = max(start, prev_from), min(runs[-1]['presentedUntilUs'], prev_runs[-1]['presentedUntilUs'])
                probes = set(range(lo, hi + 1, 997)) | {lo, hi}
                probes |= {r[k] for r in runs + prev_runs for k in ('presentedFromUs', 'presentedUntilUs') if lo <= r[k] <= hi}
                for t in probes:
                    self.assertEqual(frames_present(prev_runs, t), frames_present(runs, t), t)
            previous, proof_from = (start, runs), start

    def test_window_starting_on_a_frame_boundary_keeps_its_ambiguity(self):
        # Frames 10 ms apart, three listed per view: the lookback can reach the
        # first listed frame, whose predecessor (ending at that instant) is gone.
        offset, latency = 0, 45_000
        captures = [1_000_000 + 10_000 * i for i in range(40)]
        previous = None
        proof_from = None
        for step in range(4, 40):
            frames = [{'id': i + 1, 'captureUs': captures[i]} for i in range(step)][-3:]
            view_now = captures[step - 1] + 1_000
            now_host = view_now + latency + 5_000
            start, runs = sim.presentation_runs(frames, offset, latency, view_now, now_host, proof_from)
            if not runs:
                continue
            if previous:
                prev_from, prev_runs = previous
                lo, hi = max(start, prev_from), min(runs[-1]['presentedUntilUs'], prev_runs[-1]['presentedUntilUs'])
                points = {r[k] for r in runs + prev_runs for k in ('presentedFromUs', 'presentedUntilUs')} | {lo, hi}
                for t in sorted(p for p in points | {p + 1 for p in points} if lo <= p <= hi):
                    self.assertEqual(frames_present(prev_runs, t), frames_present(runs, t), t)
            previous, proof_from = (start, runs), start

    def test_no_run_before_its_frame_is_on_screen(self):
        frames = [{'id': 1, 'captureUs': 1_000_000}]
        self.assertEqual(sim.presentation_runs(frames, 0, 45_000, 1_001_000, 1_030_000), (None, []))


class HumanTests(unittest.TestCase):
    def test_pursuit_converges_and_tremor_matches_its_deviation(self):
        human = sim.Human(rng=random.Random(4), tremor_deg=.35, pursuit_tau_s=.12)
        goal = sim.direction({'kind': 'fisheye_domemaster'}, .62, .45)
        for _ in range(100):
            human.step(goal, .01)
        self.assertLess(sim.angle_deg(human.hand, goal), .01)   # one second = 8 time constants
        samples = [sim.angle_deg(goal, human.step(goal, .01)) for _ in range(6000)]
        # Two independent axes: radial offset is Rayleigh with mean sigma*sqrt(pi/2).
        self.assertAlmostEqual(statistics.mean(samples), .35 * math.sqrt(math.pi / 2), delta=.08)

    def test_no_goal_keeps_the_hand_still(self):
        human = sim.Human(rng=random.Random(1), tremor_deg=0)
        start = human.hand
        for _ in range(50):
            human.step(None, .02)
        self.assertEqual(human.hand, start)


if __name__ == '__main__':
    unittest.main()
