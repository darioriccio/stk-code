#!/usr/bin/env python3
"""Offline transport/export regressions; these are not native measurements."""
import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import collect_mission_measurement as collector


class ExportTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="sc4-collector-", dir=Path(__file__).resolve().parent)
        self.addCleanup(self.directory.cleanup)
        self.output = Path(self.directory.name) / "trace.ndjson"
        # A synthetic mount boundary makes the unit tests portable. The actual
        # fixtures live beside this script (on the task SSD in this session).
        # The production CLI still checks its real mount before every export.
        self.addCleanup(patch.stopall)
        patch.object(collector, "SSD", self.output.parent).start()
        patch.object(Path, "is_mount", return_value=True).start()
        self.raw = '{"synthetic_test_only":"caffè"}\n'.encode()
        self.state = dict(ok=True, contract=collector.CONTRACT, recording=False, closed=True,
                          recordingID="12345678-1234-1234-1234-123456789abc", producerUUID="producer",
                          raceEpoch="9007199254740993", byteCount=str(len(self.raw)), sha256=hashlib.sha256(self.raw).hexdigest())

    def client(self, change=None):
        test = self
        class Fake:
            count = 0
            def call(self, path):
                self.count += 1
                if path == "/state":
                    result = dict(test.state)
                else:
                    result = dict(ok=True, recordingID=test.state["recordingID"], offset="0",
                                  nextOffset=str(len(test.raw)), eof=True, data=test.raw.decode())
                return change(path, result, self.count) if change else result
        return Fake()

    def test_exact_unicode_bytes_and_exclusive_publish(self):
        result = collector.export(self.client(), self.output)
        self.assertEqual(self.output.read_bytes(), self.raw)
        self.assertEqual(result["sha256"], hashlib.sha256(self.raw).hexdigest())
        with self.assertRaises(collector.Invalid):
            collector.export(self.client(), self.output)

    def rejected(self, change):
        with self.assertRaises(collector.Invalid):
            collector.export(self.client(change), self.output)
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.output.parent.glob("*.partial-*")), [])

    def test_stale_recording_is_rejected_and_partial_removed(self):
        self.rejected(lambda path, result, count: dict(result, recordingID="changed") if path.startswith("/trace") else result)

    def test_char_offsets_are_not_byte_offsets(self):
        self.rejected(lambda path, result, count: dict(result, nextOffset=str(len(self.raw.decode()))) if path.startswith("/trace") else result)

    def test_no_progress_chunk_rejected(self):
        self.rejected(lambda path, result, count: dict(result, nextOffset="0", data="") if path.startswith("/trace") else result)

    def test_metadata_change_after_export_rejected(self):
        self.rejected(lambda path, result, count: dict(result, raceEpoch="2") if count == 3 else result)

    def test_corrupt_payload_rejected(self):
        self.rejected(lambda path, result, count: dict(result, data="X" + result["data"][1:]) if path.startswith("/trace") else result)

    def test_recording_live_cannot_export(self):
        self.rejected(lambda path, result, count: dict(result, recording=True))

    def test_internal_disk_and_symlink_refused(self):
        with self.assertRaises(collector.Invalid):
            collector.safe_output(self.output.parent.parent / "sc4-test-never-created.ndjson")
        self.output.symlink_to(self.output.parent / "missing")
        with self.assertRaises(collector.Invalid):
            collector.safe_output(self.output)

    def test_unmounted_volume_refused(self):
        with patch.object(Path, "is_mount", return_value=False):
            with self.assertRaises(collector.Invalid):
                collector.safe_output(self.output)

    def test_credentials_and_redirect_boundary(self):
        for url in ("https://127.0.0.1/immersive/v1", "http://example.com/immersive/v1",
                    "http://user@127.0.0.1/immersive/v1", "http://127.0.0.1/other"):
            with self.assertRaises(collector.Invalid):
                collector.Client(url, "synthetic-secret")
        with self.assertRaises(collector.Invalid):
            collector.NoRedirect().redirect_request(None, None, 302, None, None, "http://example.com")


if __name__ == "__main__":
    unittest.main()
