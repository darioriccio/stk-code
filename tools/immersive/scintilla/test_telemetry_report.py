#!/usr/bin/env python3
"""Offline tests of telemetry_report.py on synthetic sessions."""
import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import telemetry_report as report  # noqa: E402

S = 1_000_000


def session(*records, footer=True):
    lines = [{'type': 'header', 'schema': report.SCHEMA, 't': 0, 'tick': 0, 'track': 'snowtuxpeak', 'raceEpoch': 3,
              'crew': ['p1', 'p2'], 'tuningProfile': 'test_week_v1', 'tuningSha256': 'ab' * 32}]
    lines += records
    if footer:
        lines.append({'type': 'footer', 'schema': report.SCHEMA, 'accepted': len(lines), 'dropped': 0})
    return lines


def write(directory, name, records):
    path = Path(directory) / name
    path.write_text(''.join(json.dumps(r) + '\n' for r in records), encoding='utf-8')
    return path


def shot(t, result, edge, radius=1.0, distance=15.0, automatic=False, participant='p1'):
    return {'type': 'input', 't': t, 'control': 'primary', 'result': result, 'participant': participant,
            'automatic': automatic, 'poseLagDeg': 1.5,
            'nearest': {'kind': 'rival', 'distanceM': distance, 'angularRadiusDeg': radius, 'edgeDeg': edge}}


class ReportTests(unittest.TestCase):
    def summary(self, records):
        with tempfile.TemporaryDirectory() as directory:
            path = write(directory, 'session.ndjson', records)
            return report.summarise(report.read_session(path), path)

    def test_aim_hit_rate_near_misses_and_bands(self):
        s = self.summary(session(
            shot(0, 'hit', -0.5, distance=5), shot(S, 'miss', 0.5, distance=15), shot(2 * S, 'miss', 3.0, distance=25),
            shot(3 * S, 'hit', -0.1, automatic=True, distance=25),
            {'type': 'input', 't': 4 * S, 'control': 'secondary', 'result': 'hit'},
            {'type': 'input_note', 't': 4 * S, 'kind': 'rejected', 'reason': 'fire_rate_limited'},
            {'type': 'input_note', 't': 4 * S, 'kind': 'rejected', 'reason': 'fire_rate_limited'},
        ))['aim']
        self.assertEqual((s['presses'], s['automatic']), (4, 1))
        self.assertEqual(s['toolPresses'], 1)
        self.assertEqual(s['results'], {'hit': 2, 'miss': 2})
        self.assertAlmostEqual(s['hitRate'], .5)
        self.assertAlmostEqual(s['nearMissShare'], .5)  # 0.5 deg within one radius, 3 deg is not
        self.assertEqual(s['admissionRejections'], {'rejected:fire_rate_limited': 2})
        self.assertEqual(s['byDistance']['0-10 m']['hits'], 1)
        self.assertAlmostEqual(s['byDistance']['20+ m']['hitRate'], .5)
        self.assertAlmostEqual(s['poseLagDegP50'], 1.5)

    def test_combat_breaches_expiry_and_staggers(self):
        s = self.summary(session(
            {'type': 'combat', 't': 0, 'target': 2, 'phase': 'acquiring'},
            {'type': 'combat', 't': S, 'target': 2, 'phase': 'breached'},
            {'type': 'combat', 't': 2 * S, 'target': 2, 'phase': 'shielded'},
            {'type': 'combat', 't': 3 * S, 'target': 2, 'phase': 'breached'},
            {'type': 'combat_shot', 't': 4 * S, 'reason': 'stagger_pending', 'effectApplied': True},
            {'type': 'combat_shot', 't': 5 * S, 'reason': 'shield_rebound', 'effectApplied': False},
            {'type': 'sample', 't': 60 * S},
        ))['combat']
        self.assertEqual((s['breachesOpened'], s['breachesExpired'], s['staggers']), (2, 1, 1))
        # acquiring at 0 -> breached at 1 s: one hold of one second.
        self.assertEqual((s['acquisitions'], s['lockHoldSP50'], s['locksLostShort']), (1, 1.0, 0))
        self.assertAlmostEqual(s['staggersPerMinute'], 1.0)
        self.assertEqual(s['shotOutcomes'], {'stagger_pending': 1, 'shield_rebound': 1})

    def test_defense_outcomes_and_energy(self):
        s = self.summary(session(
            {'type': 'defense', 't': 0, 'phase': 'await_warning'},
            {'type': 'defense', 't': 1, 'phase': 'telegraph'},
            {'type': 'defense', 't': 2, 'phase': 'cooldown', 'reason': 'blocked'},
            {'type': 'defense', 't': 3, 'phase': 'await_warning'},
            {'type': 'defense', 't': 4, 'phase': 'cooldown', 'reason': 'opportunity_lost'},
            {'type': 'boost', 't': 5, 'energy': 70},
            {'type': 'sample', 't': 6, 'energy': 40, 'humanTransient': True},
            {'type': 'sample', 't': 7, 'energy': 80, 'humanTransient': False},
        ))['defense']
        self.assertEqual((s['threatsStarted'], s['telegraphs'], s['blocked'], s['cancelled'], s['boosts']), (2, 1, 1, 1, 1))
        self.assertEqual((s['energyMin'], s['energyMean']), (40, 60))
        self.assertAlmostEqual(s['humanTransientShare'], .5)

    def test_mission_capsules_tethers_and_result(self):
        s = self.summary(session(
            {'type': 'mission_entity', 't': 0, 'kind': 'capsule', 'id': 1, 'instance': 1, 'phase': 'shell'},
            {'type': 'mission_entity', 't': 1, 'kind': 'capsule', 'id': 1, 'instance': 1, 'phase': 'open'},
            {'type': 'mission_entity', 't': 2, 'kind': 'capsule', 'id': 1, 'instance': 1, 'phase': 'collected'},
            {'type': 'mission_entity', 't': 3, 'kind': 'capsule', 'id': 2, 'instance': 2, 'phase': 'shell'},
            {'type': 'mission_entity', 't': 4, 'kind': 'capsule', 'id': 2, 'instance': 2, 'phase': 'expired'},
            {'type': 'mission_entity', 't': 5, 'kind': 'tether', 'id': 9, 'instance': 1, 'phase': 'tether_recoverable'},
            {'type': 'cargo', 't': 5, 'slots': ['secured', 'empty', 'empty']},
            {'type': 'cargo', 't': 6, 'slots': ['tethered', 'empty', 'empty']},
            {'type': 'cargo', 't': 7, 'slots': ['secured', 'empty', 'empty']},
            {'type': 'cargo', 't': 8, 'slots': ['tethered', 'empty', 'empty']},
            {'type': 'cargo', 't': 9, 'slots': ['empty', 'empty', 'empty']},
            {'type': 'mission_result', 't': 10, 'outcome': 'mission_failed', 'position': 2, 'secured': 0,
             'natural': True, 'termination': ''},
        ))['mission']
        self.assertEqual((s['capsulesOffered'], s['capsulesOpened'], s['capsulesCollected'], s['capsulesExpired']), (2, 1, 1, 1))
        self.assertEqual((s['tethers'], s['tethersRecovered'], s['tethersLost']), (1, 1, 1))
        self.assertEqual(s['result']['outcome'], 'mission_failed')

    def test_activity_and_latency_chain(self):
        s = self.summary(session(
            {'type': 'sample', 't': 0, 'gameplay': True, 'attackable': 1, 'capsules': 0, 'defense': 'idle', 'boostReady': False},
            {'type': 'sample', 't': 1, 'gameplay': True, 'attackable': 0, 'capsules': 0, 'defense': 'cooldown', 'boostReady': False},
            {'type': 'sample', 't': 2, 'gameplay': False, 'attackable': 0},
            {'type': 'latency_flash', 't': 3},
            {'type': 'rescue', 't': 3, 'lap': 0, 'trackM': 411.6},
            {'type': 'latency_presented', 't': 4, 'poseUs': 1000, 'receivedUs': 21000, 'captureUs': 31000,
             'presentedLowUs': 71000, 'presentedHighUs': 88000},
            {'type': 'presentation', 't': 5, 'captureToPresentedP50Ms': 40, 'captureToPresentedP90Ms': 55},
            {'type': 'sample', 't': 6, 'gameplay': False, 'frameMaxMs': 17},
            {'type': 'sample', 't': 7, 'gameplay': False, 'frameMaxMs': 300},
            {'type': 'sample', 't': 8, 'gameplay': False, 'frameMaxMs': 700, 'depthSkippedPinned': 2, 'depthSkippedBudget': 1},
        ))
        activity, latency = s['activity'], s['latency']
        self.assertEqual(activity['samples'], 2)
        self.assertEqual((activity['rescues'], activity['rescueTrackM']), (1, [412]))
        self.assertAlmostEqual(activity['nothingToDoShare'], .5)
        self.assertEqual((latency['probes'], latency['presented']), (1, 1))
        self.assertEqual((latency['poseToReceivedMsP50'], latency['receivedToCaptureMsP50'],
                          latency['captureToPresentedMsP50'], latency['poseToPresentedMsP50']), (20, 10, 40, 70))
        self.assertEqual(latency['framePresentationP50Ms'], 40)
        self.assertEqual((latency['frameMaxMs'], latency['stallsOver250Ms'], latency['stallsOver600Ms']), (700, 2, 1))
        self.assertEqual(latency['depthSkipped'], 3)

    def test_stall_rows_count_even_without_samples(self):
        latency = self.summary(session({'type': 'stall', 't': 1, 'ms': 1150.5}, {'type': 'stall', 't': 2, 'ms': 300}))['latency']
        self.assertEqual((latency['frameMaxMs'], latency['stallsOver250Ms'], latency['stallsOver600Ms']), (1150.5, 2, 1))

    def test_incomplete_and_invalid_sessions(self):
        self.assertFalse(self.summary(session(footer=False))['complete'])
        with tempfile.TemporaryDirectory() as directory:
            bad = write(directory, 'bad.ndjson', [{'type': 'sample'}])
            with self.assertRaises(report.TelemetryError):
                report.read_session(bad)
            broken = Path(directory) / 'broken.ndjson'
            broken.write_text('{"type":"header","schema":"%s"}\n{not json\n' % report.SCHEMA)
            with self.assertRaisesRegex(report.TelemetryError, 'broken.ndjson:2'):
                report.read_session(broken)

    def test_command_line_writes_markdown_and_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = write(directory, 'a.ndjson', session(shot(0, 'hit', -1)))
            out = Path(directory) / 'report.json'
            buffer = io.StringIO()
            with contextlib.redirect_stdout(buffer):
                self.assertEqual(report.main([str(path), '--json', str(out)]), 0)
            self.assertIn('Missione Scintilla', buffer.getvalue())
            self.assertIn('test_week_v1', buffer.getvalue())
            data = json.loads(out.read_text())
            self.assertEqual(data['sessions'][0]['aim']['presses'], 1)
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(report.main([str(Path(directory) / 'missing.ndjson')]), 1)


if __name__ == '__main__':
    unittest.main()
