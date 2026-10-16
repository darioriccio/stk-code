#!/usr/bin/env python3
"""Pure mathematics and EXPLICITLY SYNTHETIC negative SC4 capture regressions.

No game, GPU, compiler, optical source or hardware grant is created. No test
writes a qualification report: every capture fixture declares synthetic_fixture
and remains inadmissible even when its mathematical coverage is complete.
"""
import copy
import io
import json
import math
import random
from pathlib import Path
import tempfile
import unittest

import validate_mission_qualification as q


def encoded(value):
    return json.dumps(value, separators=(",", ":"), allow_nan=False).encode()


def synthetic_manifest(laps=1):
    center = [20 * math.sin(math.radians(72)), 0, 20 * math.cos(math.radians(72))]
    return {"schemaVersion": 1, "profileId": q.PROFILE, "profileRevision": 1, "trackId": "snowtuxpeak",
            "direction": "forward", "trackHash": "a" * 64, "authoringStatus": "development_unqualified",
            "groups": [{"id": "synthetic_" + str(i), "activationFraction": .1 + .2 * i,
                        "expiryFraction": .22 + .2 * i, "laps": list(range(laps)),
                        "capsules": [{"center": center, "radius": 2, "asset": "synthetic_sphere"},
                                     {"center": [center[0] + 6.6, 0, center[2]], "radius": 2, "asset": "synthetic_sphere"}]}
                       for i in range(4)]}


def synthetic_header(manifest, crew=1, laps=1, phase="shell"):
    fingerprint = {"anchorHash": q.digest(encoded(manifest)), "trackHash": manifest["trackHash"], "trackAssetsHash": "b" * 64,
                   "shaderHash": "c" * 64, "executableHash": "d" * 64, "profileId": q.PROFILE, "profileRevision": "1",
                   "P": crew + 1, "C": crew, "laps": laps, "F": 120, "width": 2048, "height": 1024, "cubemapSize": 1024,
                   "projection": "equirectangular", "output": "syphon", "near": .2, "far": 1000, "faceFovDeg": 90,
                   "domeTiltDeg": 0, "cameraMode": "first_person", "camera": [0, 35, 0, -.25, .4, 0, .1, 0],
                   "horizonStabilization": True, "capsuleRadius": 2, "tetherRadius": .5,
                   "tetherOffsets": [[-3, 5.5, 6], [3, 5.5, 6]], "evaluatedLayoutRevision": 2, "nativeBaseSpeed": 15}
    raw = encoded(fingerprint).decode()
    groups = [{key: copy.deepcopy(group[key]) for key in ("id", "activationFraction", "expiryFraction", "laps", "capsules")}
              for group in manifest["groups"]]
    for group in groups:
        for capsule in group["capsules"]:
            del capsule["asset"]
    return {"type": "header", "contract": q.TRACE_CONTRACT, "schemaVersion": 1,
            "recordingID": "00000000-0000-4000-8000-000000000001", "producerUUID": "00000000-0000-4000-8000-000000000002",
            "raceEpoch": "9007199254740993", "provenance": "synthetic_fixture", "fingerprint": fingerprint,
            "fingerprintJSON": raw, "fingerprintHash": q.digest(raw.encode()), "groups": groups,
            "limits": {"maxFrames": 18000, "maxPending": 8, "maxGapUs": 50000, "liveProofMaxAgeUs": 50000},
            "phasePolicy": q.PHASE_POLICY, "phase": phase, "actualLayoutRevision": 0}


def synthetic_rows(header, manifest, *, frames_per_group=140, minimum_speed=15, snapshot_speed=None, laps=None):
    """A deliberately fictitious geometry sequence, never native evidence."""
    rows = [header]; sequence = 0
    center = [8 * math.sin(math.radians(72)), 0, 8 * math.cos(math.radians(72))]
    candidates = [center, [center[0] + 6, center[1], center[2]]]
    camera = {"position": [0, 0, 0], "right": [1, 0, 0], "up": [0, 1, 0], "forward": [0, 0, 1]}
    for lap in (range(header["fingerprint"]["laps"]) if laps is None else laps):
        for group_index, group in enumerate(manifest["groups"]):
            for local in range(frames_per_group):
                sequence += 1
                fraction = group["activationFraction"] - .003 + local * .001
                # Remain inside this fictitious occurrence for long test runs.
                fraction = min(fraction, group["expiryFraction"] - .001)
                flags = [0] * 6
                if group_index == 1:
                    flags[0] = 10
                row = {"type": "frame", "index": str(sequence), "frameID": str(sequence),
                       "captureUs": str(1000000 + sequence * 25000), "readbackUs": str(1005000 + sequence * 25000), "readbackTick": str(sequence * 3), "tick": str(sequence * 3),
                       "officialLap": lap, "fraction": fraction, "speed": minimum_speed if snapshot_speed is None else snapshot_speed,
                       "minimumSpeed": minimum_speed, "maxSpeed": 25, "scenario": "zipper" if any(flags) else "normal",
                       "speedIncreaseTicks": flags, "active": True, "continuous": True, "published": True,
                       "camera": camera, "kartPosition": [sequence * .375, 0, 0], "tetherCandidates": candidates, "objects": []}
                for capsule in range(1 if header["fingerprint"]["C"] == 1 else 2):
                    shape = group["capsules"][capsule]
                    row["objects"].append({"kind": "capsule", "group": group_index, "capsule": capsule, "side": -1,
                                           "phase": header["phase"], "center": shape["center"], "radius": 2, "range": True,
                                           "rectangleFree": True, "activation": local == 3, "activationUs": str(1000000 + sequence * 25000 - 10000) if local == 3 else "0", "inWindow": local >= 3,
                                           "idealPixels": 100, "admissiblePixels": 100, "visiblePixels": 100, "status": "ready"})
                for side in (0, 1):
                    row["objects"].append({"kind": "tether", "group": -1, "capsule": -1, "side": side,
                                           "phase": "awaiting_presentation" if header["phase"] == "shell" else "recoverable",
                                           "center": candidates[side], "radius": .5, "range": True, "rectangleFree": True,
                                           "activation": False, "activationUs": "0", "inWindow": True, "idealPixels": 20,
                                           "admissiblePixels": 20, "visiblePixels": 20, "status": "ready"})
                rows.append(row)
    rows.append({"type": "footer", "frameCount": str(sequence), "complete": True, "reason": "",
                 "naturalFinish": True, "officialLap": header["fingerprint"]["laps"]})
    return rows


def read_rows(evidence, rows, manifest):
    raw = b"".join(encoded(row) + b"\n" for row in rows)
    return evidence.read(io.BytesIO(raw), manifest, q.digest(encoded(manifest)))


def synthetic_pair(*, crew=1, frames_per_group=140, minimum_speed=15, snapshot_speed=None, laps=1, recorded_laps=None):
    manifest = synthetic_manifest(laps); evidence = q.Evidence()
    for phase in ("shell", "open"):
        header = synthetic_header(manifest, crew, laps, phase)
        rows = synthetic_rows(header, manifest, frames_per_group=frames_per_group, minimum_speed=minimum_speed,
                              snapshot_speed=snapshot_speed, laps=recorded_laps)
        read_rows(evidence, rows, manifest)
    evidence.finish()
    return evidence


class QualificationMathematics(unittest.TestCase):
    def test_capsule_angular_limit_and_ideal_straight_line_budget(self):
        # Frozen independent reference values in the SC4 plan's calcoli.json.
        self.assertAlmostEqual(2 / math.sin(math.radians(1.5)), 76.40310002822089, places=10)
        self.assertAlmostEqual(q.angular_diameter(2, 75), 3.0561371892215092, places=10)
        length = math.sqrt(75 ** 2 - 4 ** 2) - math.sqrt(8 ** 2 - 4 ** 2)
        self.assertAlmostEqual(length, 67.965054, places=6)
        self.assertGreater(length / 30, 2); self.assertLess(length / 35, 2)


    def test_half_open_capsule_budget_counts_intervals_not_endpoints(self):
        window = q.Window(120, 2000000, True)
        for i in range(81):
            window.add(i * 25000, i * 3, 15)
        self.assertIsNone(window.best)  # Exactly 2 seconds is not greater than 2.
        window.add(2025000, 243, 15)
        self.assertEqual(window.best, 15)

    def test_tether_three_seconds_is_inclusive(self):
        window = q.Window(120, 3000000, False)
        for i in range(121):
            window.add(i * 25000, i * 3, 15)
        self.assertEqual(window.best, 15)

    def test_peak_speed_cannot_promote_sustained_minimum(self):
        window = q.Window(120, 2000000, True)
        for i in range(161):
            window.add(i * 25000, i * 3, 40 if i == 160 else 10)
        self.assertEqual(window.best, 10)

    def test_no_wall_time_credit_without_native_tick_progress(self):
        window = q.Window(120, 2000000, True)
        for i in range(130):
            window.add(i * 25000, 0, 15)
        self.assertIsNone(window.best)

    def test_reset_cannot_stitch_two_short_windows(self):
        window = q.Window(120, 2000000, True)
        for i in range(50):
            window.add(i * 25000, i * 3, 15)
        window.reset_run()
        for i in range(50, 100):
            window.add(i * 25000, i * 3, 15)
        self.assertIsNone(window.best)

    def test_linear_maximin_matches_independent_quadratic_oracle(self):
        rng = random.Random(610)
        for strict, duration in ((True, 2000000), (False, 3000000)):
            for _ in range(10):
                points = []; us = tick = 0
                window = q.Window(120, duration, strict)
                for i in range(190):
                    us += rng.randint(20000, 35000); tick += rng.randint(2, 4)
                    speed = rng.randint(4, 45); points.append((us, tick, speed)); window.add(us, tick, speed)
                expected = None
                for start in range(len(points)):
                    minimum = math.inf
                    for end in range(start, len(points)):
                        minimum = min(minimum, points[end][2])
                        wall = points[end][0] - points[start][0]
                        ticks = (points[end][1] - points[start][1]) * 1000000
                        good = wall > duration and ticks > duration * 120 if strict else wall >= duration and ticks >= duration * 120
                        if good:
                            expected = minimum if expected is None else max(expected, minimum)
                self.assertEqual(window.best, expected)

    def test_capsule_proof_interval_exact_boundary_and_activation_availability(self):
        self.assertTrue(q.capsule_proof_available(100000, 115000, 150000))
        self.assertFalse(q.capsule_proof_available(100000, 115000, 150001))
        self.assertFalse(q.capsule_proof_available(100000, 115000, 114999))
        self.assertTrue(q.capsule_proofs_overlap(100000, 150000))
        self.assertFalse(q.capsule_proofs_overlap(100000, 150001))

    def test_capsule_duration_starts_at_actual_readback_in_both_clocks(self):
        window = q.Window(120, 2000000, True)
        for i in range(82):
            window.add(i * 25000, i * 3, 15, i * 25000 + 25000, i * 3 + 3)
        self.assertIsNone(window.best)  # Capture span2.025s, available span2s.
        window.add(2050000, 246, 15, 2075000, 249)
        self.assertEqual(window.best, 15)
        window = q.Window(120, 2000000, True)
        for i in range(82):
            window.add(i * 25000, i * 3, 15, i * 25000 + 1, i * 3 + 3)
        self.assertIsNone(window.best)  # Native ticks independently constrain it.

    def test_exact_cargo_circle_boundary_is_reserved(self):
        width, height = 2048, 1024
        # Right edge of the central r=.06 circle, in UV coordinates.
        u = .5 + .06 * .5 * height / width; v = .5 - .16 * .5
        # The decimal edge .515 is not exactly representable. Check the two
        # neighbouring doubles rather than assuming rounded UV equals real .515.
        inside = math.nextafter(u, 0); outside = math.nextafter(u, 1)
        self.assertFalse(q.world_rect_free(inside, v, inside, v, width, height))
        self.assertTrue(q.world_rect_free(outside, v, outside, v, width, height))
        self.assertTrue(q.world_rect_free(.7, .49, .71, .50, width, height))


class SyntheticNegativeCaptures(unittest.TestCase):
    def test_synthetic_complete_geometry_never_becomes_real_qualification(self):
        evidence = synthetic_pair()
        self.assertEqual(evidence.reasons, {"non_native_provenance"})
        self.assertEqual(len(evidence.activations), 4)
        self.assertEqual(evidence.bounds[("capsule", 0, 0, 0, "shell")], 15)

    def test_readback_spacing_jointly_controls_capsule_admission(self):
        # At30Hz, delay20ms is individually younger than50ms but leaves
        # 3.333ms holes. Delay16.666ms overlaps;90ms is unusable altogether.
        for delay, expected in ((15000, True), (16666, True), (20000, False), (49999, False), (90000, False)):
            with self.subTest(delay=delay):
                manifest = synthetic_manifest(); evidence = q.Evidence()
                for phase in ("shell", "open"):
                    rows = synthetic_rows(synthetic_header(manifest, phase=phase), manifest)
                    for row in rows[1:-1]:
                        index = int(row["index"]); capture = 1000000 + index * 33333
                        row["captureUs"] = str(capture); row["tick"] = str(index * 4)
                        row["readbackUs"] = str(capture + delay)
                        row["readbackTick"] = str(index * 4 + math.ceil(delay * 120 / 1000000))
                        for obj in row["objects"]:
                            if obj["activation"]: obj["activationUs"] = str(capture - 10000)
                    read_rows(evidence, rows, manifest)
                evidence.finish()
                self.assertEqual("capsule_window_below_base_speed" not in evidence.reasons, expected)
                # Slowreadback never imposes an unintended freshness limit on
                # the tether's separate3s geometric-window contract.
                self.assertNotIn("tether_window_below_base_speed", evidence.reasons)

    def test_readback_after_crossing_cannot_retroactively_prove_activation(self):
        manifest = synthetic_manifest(); evidence = q.Evidence()
        for phase in ("shell", "open"):
            rows = synthetic_rows(synthetic_header(manifest, phase=phase), manifest)
            for row in rows[1:-1]:
                for obj in row["objects"]:
                    if obj["activation"]: obj["activationUs"] = str(int(row["captureUs"]) - 24000)
            read_rows(evidence, rows, manifest)
        evidence.finish()
        self.assertIn("activation_not_proven", evidence.reasons)
        self.assertNotIn("capsule_window_below_base_speed", evidence.reasons)

    def test_batch_readback_same_timestamp_is_valid_without_time_credit(self):
        manifest = synthetic_manifest(); evidence = q.Evidence()
        for phase in ("shell", "open"):
            rows = synthetic_rows(synthetic_header(manifest, phase=phase), manifest)
            for i in range(1, len(rows) - 1, 2):
                batch = rows[i:i + 2]; available = int(batch[-1]["captureUs"])
                for row in batch:
                    row["readbackUs"] = str(available); row["readbackTick"] = batch[-1]["tick"]
            read_rows(evidence, rows, manifest)
        evidence.finish()
        self.assertNotIn("capsule_window_below_base_speed", evidence.reasons)
        self.assertIn("activation_not_proven", evidence.reasons)  # Batch completes after crossing.

    def test_c2_objects_are_independent_and_missing_second_is_rejected(self):
        self.assertEqual(synthetic_pair(crew=2).reasons, {"non_native_provenance"})
        manifest = synthetic_manifest(); evidence = q.Evidence()
        for phase in ("shell", "open"):
            rows = synthetic_rows(synthetic_header(manifest, 2, phase=phase), manifest)
            for row in rows[1:-1]:
                row["objects"] = [obj for obj in row["objects"] if obj["kind"] != "capsule" or obj["capsule"] == 0]
            read_rows(evidence, rows, manifest)
        evidence.finish()
        self.assertIn("capsule_window_below_base_speed", evidence.reasons)

    def test_two_short_phase_runs_are_not_pooled(self):
        evidence = synthetic_pair(frames_per_group=60)
        self.assertIn("capsule_window_below_base_speed", evidence.reasons)
        self.assertIn("tether_window_below_base_speed", evidence.reasons)

    def test_snapshot_peak_and_max_speed_do_not_cover_native_base(self):
        evidence = synthetic_pair(minimum_speed=12, snapshot_speed=40)
        self.assertIn("capsule_window_below_base_speed", evidence.reasons)
        self.assertIn("tether_window_below_base_speed", evidence.reasons)

    def test_long_later_window_cannot_repair_bad_activation(self):
        manifest = synthetic_manifest(); evidence = q.Evidence()
        for phase in ("shell", "open"):
            rows = synthetic_rows(synthetic_header(manifest, phase=phase), manifest)
            for row in rows[1:-1]:
                if row["objects"][0]["activation"]:
                    row["objects"][0]["visiblePixels"] = 99
            read_rows(evidence, rows, manifest)
        evidence.finish()
        self.assertIn("activation_not_proven", evidence.reasons)
        self.assertNotIn("capsule_window_below_base_speed", evidence.reasons)

    def test_open_activation_cannot_substitute_shell_activation(self):
        manifest = synthetic_manifest(); evidence = q.Evidence()
        for phase in ("shell", "open"):
            rows = synthetic_rows(synthetic_header(manifest, phase=phase), manifest)
            if phase == "shell":
                for row in rows[1:-1]:
                    for obj in row["objects"]:
                        obj["activation"] = False; obj["activationUs"] = "0"
            read_rows(evidence, rows, manifest)
        evidence.finish(); self.assertIn("activation_not_proven", evidence.reasons)

    def test_one_unseen_lap_cannot_be_certified(self):
        evidence = synthetic_pair(laps=2, recorded_laps=[0])
        self.assertIn("capsule_window_below_base_speed", evidence.reasons)
        self.assertIn("tether_window_below_base_speed", evidence.reasons)

    def test_occlusion_and_pending_queries_break_the_entire_window(self):
        for mutation in ("occluded", "pending", "held", "unpublished", "discontinuous", "gap"):
            with self.subTest(mutation=mutation):
                manifest = synthetic_manifest(); evidence = q.Evidence()
                for phase in ("shell", "open"):
                    rows = synthetic_rows(synthetic_header(manifest, phase=phase), manifest)
                    for row in rows[1:-1]:
                        if mutation == "occluded":
                            row["objects"][0]["visiblePixels"] = 99
                        elif mutation == "pending":
                            row["objects"][0]["status"] = "pending_timeout"
                        elif mutation == "held": row["active"] = False
                        elif mutation == "unpublished": row["published"] = False
                        elif mutation == "discontinuous": row["continuous"] = False
                        elif mutation == "gap":
                            row["captureUs"] = str(1000000 + int(row["index"]) * 51000)
                            row["readbackUs"] = str(int(row["captureUs"]) + 5000)
                            for obj in row["objects"]:
                                if obj["activation"]: obj["activationUs"] = str(int(row["captureUs"]) - 10000)
                    read_rows(evidence, rows, manifest)
                evidence.finish(); self.assertIn("capsule_window_below_base_speed", evidence.reasons)

    def test_interleaved_duplicate_ticks_preserve_runs_without_adding_credit(self):
        manifest = synthetic_manifest(); evidence = q.Evidence()
        for phase in ("shell", "open"):
            original = synthetic_rows(synthetic_header(manifest, phase=phase), manifest)
            rows = [original[0]]
            for row in original[1:-1]:
                duplicate = copy.deepcopy(row)
                duplicate["captureUs"] = str(int(row["captureUs"]) + 5000)
                duplicate["readbackUs"] = str(int(row["readbackUs"]) + 5000)
                # Activation is a consumed native crossing, not repeated render data.
                for obj in duplicate["objects"]: obj["activation"] = False; obj["activationUs"] = "0"
                rows.extend([row, duplicate])
            for i, row in enumerate(rows[1:], 1): row["index"] = row["frameID"] = str(i)
            footer = original[-1]; footer["frameCount"] = str(len(rows) - 1); rows.append(footer)
            read_rows(evidence, rows, manifest)
        evidence.finish(); self.assertEqual(evidence.reasons, {"non_native_provenance"})

    def test_held_chunks_do_not_concatenate_into_a_window(self):
        manifest = synthetic_manifest(); evidence = q.Evidence()
        for phase in ("shell", "open"):
            for _ in range(3):
                rows = synthetic_rows(synthetic_header(manifest, phase=phase), manifest, frames_per_group=60)
                read_rows(evidence, rows, manifest)
        evidence.finish(); self.assertIn("capsule_window_below_base_speed", evidence.reasons)

    def test_explicit_discontinuity_on_duplicate_tick_breaks_the_window(self):
        manifest = synthetic_manifest(); evidence = q.Evidence()
        for phase in ("shell", "open"):
            original = synthetic_rows(synthetic_header(manifest, phase=phase), manifest)
            rows = [original[0]]
            for row in original[1:-1]:
                duplicate = copy.deepcopy(row); duplicate["continuous"] = False
                duplicate["captureUs"] = str(int(row["captureUs"]) + 5000)
                duplicate["readbackUs"] = str(int(row["readbackUs"]) + 5000)
                for obj in duplicate["objects"]: obj["activation"] = False; obj["activationUs"] = "0"
                rows.extend([row, duplicate])
            for i, row in enumerate(rows[1:], 1): row["index"] = row["frameID"] = str(i)
            footer = original[-1]; footer["frameCount"] = str(len(rows) - 1); rows.append(footer)
            read_rows(evidence, rows, manifest)
        evidence.finish(); self.assertIn("capsule_window_below_base_speed", evidence.reasons)

    def test_reverse_progress_and_teleports_cannot_qualify_forward_windows(self):
        for mutation in ("reverse", "teleport"):
            with self.subTest(mutation=mutation):
                manifest = synthetic_manifest(); evidence = q.Evidence()
                for phase in ("shell", "open"):
                    rows = synthetic_rows(synthetic_header(manifest, phase=phase), manifest)
                    for row in rows[1:-1]:
                        local = (int(row["index"]) - 1) % 140
                        if mutation == "teleport": row["kartPosition"] = [int(row["index"]) * 30, 0, 0]
                        else:
                            group = manifest["groups"][row["objects"][0]["group"]]
                            row["fraction"] = group["activationFraction"] + .1 - local * .0005
                            for obj in row["objects"]:
                                obj["activation"] = False; obj["activationUs"] = "0"
                                if obj["kind"] == "capsule": obj["inWindow"] = True
                    read_rows(evidence, rows, manifest)
                evidence.finish(); self.assertIn("capsule_window_below_base_speed", evidence.reasons)


class BoundedProtocol(unittest.TestCase):
    def test_legacy_missing_or_invalid_availability_fields_fail_closed(self):
        manifest = synthetic_manifest()
        for mutation in ("missing_readback", "missing_tick", "missing_policy", "missing_activation", "early_readback", "early_tick", "reverse_readback", "zero_activation"):
            with self.subTest(mutation=mutation):
                rows = synthetic_rows(synthetic_header(manifest), manifest)
                if mutation == "missing_readback": del rows[1]["readbackUs"]
                elif mutation == "missing_tick": del rows[1]["readbackTick"]
                elif mutation == "missing_policy": del rows[0]["limits"]["liveProofMaxAgeUs"]
                elif mutation == "missing_activation": del rows[1]["objects"][0]["activationUs"]
                elif mutation == "early_readback": rows[1]["readbackUs"] = str(int(rows[1]["captureUs"]) - 1)
                elif mutation == "early_tick": rows[1]["readbackTick"] = "0"
                elif mutation == "reverse_readback": rows[1]["readbackUs"] = rows[3]["readbackUs"]
                else: rows[4]["objects"][0]["activationUs"] = "0"
                with self.assertRaises(q.Invalid): read_rows(q.Evidence(), rows, manifest)

    def test_duplicate_escaped_keys_and_nonfinite_numbers_are_rejected(self):
        for raw in (b'{"phase":"shell","ph\\u0061se":"open"}', b'{"x":NaN}', b'{"x":Infinity}'):
            with self.assertRaises(q.Invalid): q.json_value(raw)

    def test_canonical_wide_identities_never_pass_through_float(self):
        self.assertEqual(q.uint("18446744073709551615"), (1 << 64) - 1)
        self.assertEqual(q.uint("9007199254740993"), 9007199254740993)
        for value in (True, 1, "01", "-1", "1e0", "18446744073709551616"):
            with self.assertRaises(q.Invalid): q.uint(value)

    def test_line_limit_duplicate_frame_and_incomplete_footer_fail_closed(self):
        with self.assertRaises(q.Invalid):
            q.Evidence().read(io.BytesIO(b" " * (q.MAX_LINE + 1)), synthetic_manifest(), "a" * 64)
        manifest = synthetic_manifest()
        for mutation in ("index", "complete", "natural", "fingerprint", "scenario", "activation"):
            with self.subTest(mutation=mutation):
                rows = synthetic_rows(synthetic_header(manifest), manifest)
                if mutation == "index": rows[2]["index"] = rows[1]["index"]
                elif mutation == "complete": rows[-1]["complete"] = False
                elif mutation == "natural": rows[-1]["officialLap"] = 0
                elif mutation == "fingerprint": rows[0]["fingerprintJSON"] += " "
                elif mutation == "scenario": rows[1]["scenario"] = "nitro"
                else: rows[4]["objects"][0]["inWindow"] = False
                with self.assertRaises(q.Invalid): read_rows(q.Evidence(), rows, manifest)

    def test_mixed_configuration_and_manually_changed_range_cannot_admit(self):
        manifest = synthetic_manifest(); evidence = q.Evidence()
        read_rows(evidence, synthetic_rows(synthetic_header(manifest), manifest), manifest)
        rows = synthetic_rows(synthetic_header(manifest, phase="open"), manifest)
        rows[0]["fingerprint"]["width"] = 1536
        rows[0]["fingerprintJSON"] = encoded(rows[0]["fingerprint"]).decode()
        rows[0]["fingerprintHash"] = q.digest(rows[0]["fingerprintJSON"].encode())
        with self.assertRaises(q.Invalid): read_rows(evidence, rows, manifest)
        rows = synthetic_rows(synthetic_header(manifest), manifest)
        rows[1]["objects"][0]["range"] = False
        with self.assertRaises(q.Invalid): read_rows(q.Evidence(), rows, manifest)

    def test_trace_symlinks_and_oversized_anchor_are_rejected_before_reading(self):
        # Keep the generic CTest fixture beside its source. In this task that
        # checkout is on the verified SSD; CI need not reproduce a Mac mount.
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as temporary:
            root = Path(temporary); anchors = root / "anchors.json"
            anchors.write_bytes(encoded(synthetic_manifest()))
            target = root / "real"; target.mkdir(); (target / "capture.ndjson").write_bytes(b"{}\n")
            (root / "link").symlink_to(target, target_is_directory=True)
            (root / "capture.ndjson").symlink_to(target / "capture.ndjson")
            for linked in (root / "link/capture.ndjson", root / "capture.ndjson"):
                with self.assertRaisesRegex(q.Invalid, "trace_symlink"):
                    q.report_for([linked], anchors, root / "report.json")
            anchors.write_bytes(b" " * 65537)
            with self.assertRaisesRegex(q.Invalid, "anchor_size_limit"):
                q.report_for([target / "capture.ndjson"], anchors, root / "report.json")


class CrewQualificationV3(unittest.TestCase):
    def header(self, party=False, fps=120):
        manifest = synthetic_manifest(); header = synthetic_header(manifest)
        f = header["fingerprint"]
        f.update(profileId=q.CREW_PROFILES[1 if party else 0], evaluatedLayoutRevision=3,
                 crewRulesRevision="1", zoneManifestHash="e" * 64, partyEnabled=party,
                 partyCost=15, partyGain=1, partyHoldTicks=(3 * fps + 3) // 4,
                 partyWindowTicks=3 * fps, partyDurationTicks=fps // 2, partyFadeTicks=fps // 4,
                 partyCooldownTicks=6 * fps, F=fps)
        header.update(contract=q.CREW_TRACE_CONTRACT, schemaVersion=2, actualLayoutRevision=3)
        raw=encoded(f).decode();header.update(fingerprintJSON=raw, fingerprintHash=q.digest(raw.encode()))
        return manifest, header

    def test_v3_has_its_own_trace_contract_and_keeps_synthetic_rejected(self):
        manifest, header=self.header(); evidence=q.Evidence()
        evidence._header(header, manifest, q.digest(encoded(manifest)))
        self.assertIn("non_native_provenance", evidence.reasons)
        for contract, revision in [(q.TRACE_CONTRACT, 2), (q.CREW_TRACE_CONTRACT, 1)]:
            changed=copy.deepcopy(header);changed.update(contract=contract, schemaVersion=revision)
            with self.assertRaises(q.Invalid): q.Evidence()._header(changed,manifest,q.digest(encoded(manifest)))

    def test_profiles_rules_zone_and_closed_fields_are_part_of_identity(self):
        _, header=self.header();f=header["fingerprint"]
        for key,value in [("partyEnabled", True), ("crewRulesRevision", "2"), ("zoneManifestHash", ""),
                          ("evaluatedLayoutRevision", 2), ("partyCost", 14), ("newField", 0)]:
            changed=copy.deepcopy(f);changed[key]=value
            with self.assertRaises(q.Invalid): q.validate_fingerprint(changed)
        old=synthetic_header(synthetic_manifest())["fingerprint"];old["evaluatedLayoutRevision"]=3
        with self.assertRaises(q.Invalid): q.validate_fingerprint(old)

    def test_minimum_and_maximum_ticks_for_every_admitted_physics_rate(self):
        for fps in range(60,1001):
            _,header=self.header(True,fps);f=q.validate_fingerprint(header["fingerprint"])
            self.assertGreaterEqual(f["partyHoldTicks"] / fps, .75)
            self.assertLessEqual((f["partyDurationTicks"] + f["partyFadeTicks"]) / fps, .75)

    def test_party_reserved_in_both_profiles_at_all_required_viewports(self):
        for width,height in [(256,256),(768,768),(1536,1536),(2048,1024),(8192,8192)]:
            scale=min(width,height);u=.5+.22*scale*.5/width;v=.5+.85*scale*.5/height
            self.assertTrue(q.world_rect_free(u,v,u,v,width,height,2))
            self.assertFalse(q.world_rect_free(u,v,u,v,width,height,3))
            radius=.06*scale*.5/width
            # Stay deliberately inside/outside after UV conversion; an exact
            # mathematical boundary need not be representable as a binary UV.
            inside=u-radius*(1-1e-12);outside=u-radius*(1+1e-9)
            self.assertFalse(q.world_rect_free(inside,v,inside,v,width,height,3))
            self.assertTrue(q.world_rect_free(outside,v,outside,v,width,height,3))

    def test_v3_requires_actual_v3_mask_and_retains_fifty_ms_availability(self):
        manifest,header=self.header();header["actualLayoutRevision"]=0
        with self.assertRaises(q.Invalid): q.Evidence()._header(header,manifest,q.digest(encoded(manifest)))
        self.assertTrue(q.capsule_proof_available(100000,149999,150000))
        self.assertFalse(q.capsule_proof_available(100000,150001,150001))

    def test_party_requires_measured_windows_at_the_extra_native_speed(self):
        manifest,header=self.header(True);evidence=q.Evidence()
        evidence._header(header,manifest,q.digest(encoded(manifest)))
        evidence.natural_finish=True;evidence.scenarios={'normal','zipper'}
        keys=[]
        for group in range(4):
            evidence.activations.add(('capsule',0,group,0))
            keys.extend(('capsule',0,group,0,phase) for phase in ('shell','open'))
        keys.extend(('tether',0,side,phase) for side in (0,1) for phase in ('awaiting_presentation','recoverable'))
        evidence.bounds=dict.fromkeys(keys,15);evidence.finish()
        self.assertIn('capsule_window_below_base_speed',evidence.reasons)
        self.assertIn('tether_window_below_base_speed',evidence.reasons)
        evidence.reasons={'non_native_provenance'};evidence.bounds=dict.fromkeys(keys,16);evidence.finish()
        self.assertEqual(evidence.reasons,{'non_native_provenance'})


if __name__ == "__main__":
    unittest.main()
