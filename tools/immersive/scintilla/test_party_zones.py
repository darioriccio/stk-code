"""Synthetic validator fixtures only; no operating qualification is installed."""
import copy
import json
from pathlib import Path
import unittest
import tempfile
import validate_party_zones as zones


class PartyZoneValidationTests(unittest.TestCase):
    def setUp(self):
        self.m = json.loads((Path(__file__).resolve().parents[3] / "data/immersive_relay/scintilla/snowtuxpeak_party_zones_v1.json").read_text())
        self.m["authoringStatus"] = "authored"
        self.m["zones"] = [{"id": "synthetic.zone", "startNode": 5, "endNode": 10, "startMeters": 10, "endMeters": 70, "minimumWidthMeters": 6}]
        self.capture = b"SYNTHETIC UNIT FIXTURE: never install as native evidence"
        self.proof = {"passed": True, "nativeRunId": "12345678-1234-4234-8234-123456789abc", "artifactSHA256": zones.digest(self.capture)}
        self.e = {"schemaVersion": 1, "contract": "tholus.stk-scintilla-party-zones-evidence/1", "sourceKind": "native_runtime", "synthetic": False,
                  "producerUUID": self.proof["nativeRunId"], "operatorId": "synthetic.operator", "manifestSHA256": zones.digest(self.encode(self.m)),
                  "trackHash": self.m["trackHash"], "nativeBuildSHA256": "a" * 64, "trackAssetsSHA256": "b" * 64, "physicsFPS": 120,
                  "zones": [{"zoneId": "synthetic.zone", "geometryClear": True, "noRampOrJump": True, "observedSpeedBoundMetersPerSecond": 30,
                             "observedPositionErrorMeters": .5, "checks": {k: copy.deepcopy(self.proof) for k in zones.CHECKS}}]}

    @staticmethod
    def encode(value):
        return json.dumps(value, sort_keys=True).encode()

    def qualify(self):
        return zones.qualify(self.encode(self.m), self.encode(self.e), "a" * 64, "b" * 64, 120, lambda _: self.capture)

    def test_shipping_candidates_cannot_become_operating_zones(self):
        self.m["authoringStatus"] = "development_unqualified"
        with self.assertRaises(ValueError):
            self.qualify()
        self.m["authoringStatus"] = "authored"; self.m["zones"] = []
        with self.assertRaises(ValueError):
            self.qualify()

    def test_complete_declared_native_format_produces_exact_hash_bound_report(self):
        # Validates format and hash math, not the truth of this synthetic fixture.
        report, captures = self.qualify()
        self.assertEqual(report["evidenceSHA256"], zones.digest(self.encode(self.e)))
        self.assertEqual(report["manifestSHA256"], zones.digest(self.encode(self.m)))
        self.assertEqual(len(captures), 1)
        self.assertEqual(set(report["zones"][0]), set("zoneId geometryClear noRampOrJump observedSpeedBoundMetersPerSecond observedPositionErrorMeters physicsFPS".split()) | set(zones.CHECKS))

    def test_synthetic_and_offline_origins_fail_closed(self):
        for change in ({"synthetic": True}, {"sourceKind": "offline_math"}, {"sourceKind": "native_fixture"}):
            with self.subTest(change=change):
                e = self.e; self.e = e | change
                with self.assertRaises(ValueError): self.qualify()
                self.e = e

    def test_every_identity_dimension_and_actual_fps_are_required(self):
        for field, value in (("manifestSHA256", "c" * 64), ("trackHash", "c" * 64), ("nativeBuildSHA256", "c" * 64),
                             ("trackAssetsSHA256", "c" * 64), ("physicsFPS", 61), ("physicsFPS", True), ("producerUUID", "INVALID")):
            with self.subTest(field=field, value=value):
                e = self.e; self.e = e | {field: value}
                with self.assertRaises(ValueError): self.qualify()
                self.e = e

    def test_missing_failed_unknown_controller_checks_cannot_pass(self):
        for check in zones.CHECKS:
            with self.subTest(check=check):
                saved = self.e["zones"][0]["checks"][check]
                self.e["zones"][0]["checks"][check] = saved | {"passed": False}
                with self.assertRaises(ValueError): self.qualify()
                del self.e["zones"][0]["checks"][check]
                with self.assertRaises(ValueError): self.qualify()
                self.e["zones"][0]["checks"][check] = saved
        self.e["zones"][0]["checks"]["invented"] = self.proof
        with self.assertRaises(ValueError): self.qualify()

    def test_capture_mutation_and_size_are_rejected(self):
        for capture in (b"", b"changed", b"x" * (1024 * 1024 + 1)):
            self.capture = capture
            with self.assertRaises(ValueError): self.qualify()

    def test_geometry_envelope_and_exact_zone_set_are_required(self):
        row = copy.deepcopy(self.e["zones"][0])
        for change in ({"geometryClear": False}, {"noRampOrJump": False}, {"observedSpeedBoundMetersPerSecond": 29.99},
                       {"observedPositionErrorMeters": .501}, {"zoneId": "other"}):
            self.e["zones"] = [row | change]
            with self.assertRaises(ValueError): self.qualify()
        self.e["zones"] = [row, row]
        with self.assertRaises(ValueError): self.qualify()

    def test_closed_schema_duplicates_types_and_nonfinite_values_are_rejected(self):
        for data in (b'{"a":1,"a":2}', b'{"a":1,"\\u0061":2}', b'{"a":NaN}', b'{"a":Infinity}'):
            with self.assertRaises(ValueError): zones.read_json(data, 16384)
        self.e["extra"] = True
        with self.assertRaises(ValueError): self.qualify()
        for mutation in ({"candidateNodes": [True]}, {"reverse": 0}, {"marginSeconds": float("inf")}, {"authoringStatus": "native_qualified"}):
            with self.assertRaises(ValueError): zones.manifest(self.encode(self.m | mutation))

    def test_cli_file_reader_is_bounded_before_json_or_capture_hashing(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as directory:
            path = Path(directory) / "fixture.proof"
            path.write_bytes(b"12345678")
            self.assertEqual(zones.read_bounded(path, 8), b"12345678")
            for data in (b"", b"123456789"):
                path.write_bytes(data)
                with self.assertRaises(ValueError): zones.read_bounded(path, 8)
            with self.assertRaises(ValueError): zones.read_bounded(Path(directory), 8)


if __name__ == "__main__":
    unittest.main()
