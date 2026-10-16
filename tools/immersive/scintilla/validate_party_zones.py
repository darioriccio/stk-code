#!/usr/bin/env python3
"""Audit SC5 authoring or install a hash-bound, declared-native qualification.

This does not run STK, infer safe intervals, or turn track curvature into proof.
Native/operator evidence and its captures must already exist. The native loader
rechecks identity, completeness, capture hashes and actual graph intervals.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import tempfile
import uuid

CHECKS = ("brakeBefore", "brakeDuring", "brakeAccelOrders", "analogBrake", "laterNativePad")
SSD = Path("/Volumes/SanDisk 2TB")


def require(value, reason):
    if not value:
        raise ValueError(reason)


def closed(value, fields):
    require(isinstance(value, dict) and set(value) == set(fields.split()), "closed schema mismatch")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def hash_value(value):
    return isinstance(value, str) and re.fullmatch(r"[a-f0-9]{64}", value) is not None


def token(value):
    return isinstance(value, str) and re.fullmatch(r"[A-Za-z0-9_.:-]{1,128}", value) is not None


def uuid_value(value):
    try:
        return str(uuid.UUID(value)) == value
    except (ValueError, TypeError, AttributeError):
        return False


def number(value, low, high):
    return type(value) in (int, float) and low <= value <= high and math.isfinite(value)


def read_json(data, maximum):
    require(0 < len(data) <= maximum, "document size")

    def pairs(rows):
        value = {}
        for key, item in rows:
            require(key not in value, "duplicate JSON key")
            value[key] = item
        return value

    def constant(_):
        raise ValueError("nonfinite JSON number")

    return json.loads(data.decode("utf-8"), object_pairs_hook=pairs, parse_constant=constant)


def read_bounded(path, maximum):
    require(path.is_file(), "capture/document must be a regular file")
    with path.open("rb") as stream:
        data = stream.read(maximum + 1)
    require(0 < len(data) <= maximum, "document size")
    return data


def manifest(data):
    m = read_json(data, 16384)
    closed(m, "schemaVersion profileId crewRulesRevision trackId trackHash reverse authoringStatus author license speedBoundMetersPerSecond marginSeconds positionErrorMeters finalExclusionMeters candidateNodes zones")
    require(type(m["schemaVersion"]) is int and m["schemaVersion"] == 1 and m["profileId"] == "sc5_crew_party_v1" and m["crewRulesRevision"] == "1", "manifest profile")
    require(token(m["trackId"]) and hash_value(m["trackHash"]) and type(m["reverse"]) is bool, "manifest identity")
    require(m["authoringStatus"] in ("development_unqualified", "authored"), "authoring status")
    require(isinstance(m["author"], str) and 0 < len(m["author"].encode()) <= 256 and isinstance(m["license"], str) and 0 < len(m["license"].encode()) <= 128, "provenance")
    for field, low, high in (("speedBoundMetersPerSecond", 1, 100), ("marginSeconds", 0, 10), ("positionErrorMeters", 0, 5), ("finalExclusionMeters", 1, 500)):
        require(number(m[field], low, high), field)
    nodes = m["candidateNodes"]
    require(isinstance(nodes, list) and len(nodes) <= 6 and all(type(n) is int and 0 <= n <= 4095 for n in nodes) and len(set(nodes)) == len(nodes), "candidate nodes")
    require(isinstance(m["zones"], list) and len(m["zones"]) <= 6, "zone count")
    seen = set()
    for row in m["zones"]:
        closed(row, "id startNode endNode startMeters endMeters minimumWidthMeters")
        require(token(row["id"]) and row["id"] not in seen, "zone id")
        seen.add(row["id"])
        require(all(type(row[k]) is int and 0 <= row[k] <= 4095 for k in ("startNode", "endNode")) and row["startNode"] != row["endNode"], "zone node")
        require(number(row["startMeters"], 0, 100000) and number(row["endMeters"], 0, 100000) and row["startMeters"] < row["endMeters"] and number(row["minimumWidthMeters"], 6, 100), "zone interval")
    ordered = sorted(m["zones"], key=lambda z: z["startMeters"])
    require(all(a["endMeters"] <= b["startMeters"] for a, b in zip(ordered, ordered[1:])), "overlapping zones")
    require(m["authoringStatus"] != "authored" or bool(ordered), "empty authored manifest")
    return m


def qualify(manifest_bytes, evidence_bytes, build_hash, assets_hash, fps, capture_reader):
    m = manifest(manifest_bytes)
    require(m["authoringStatus"] == "authored" and m["zones"], "authoring candidates are not operating zones")
    require(hash_value(build_hash) and hash_value(assets_hash) and type(fps) is int and 60 <= fps <= 1000, "expected native identity")
    evidence = read_json(evidence_bytes, 65536)
    closed(evidence, "schemaVersion contract sourceKind synthetic producerUUID operatorId manifestSHA256 trackHash nativeBuildSHA256 trackAssetsSHA256 physicsFPS zones")
    require(type(evidence["schemaVersion"]) is int and evidence["schemaVersion"] == 1 and evidence["contract"] == "tholus.stk-scintilla-party-zones-evidence/1", "evidence contract")
    require(evidence["sourceKind"] == "native_runtime" and evidence["synthetic"] is False and uuid_value(evidence["producerUUID"]) and token(evidence["operatorId"]), "native provenance required")
    require(evidence["manifestSHA256"] == digest(manifest_bytes) and evidence["trackHash"] == m["trackHash"] and evidence["nativeBuildSHA256"] == build_hash and evidence["trackAssetsSHA256"] == assets_hash and type(evidence["physicsFPS"]) is int and evidence["physicsFPS"] == fps, "evidence identity mismatch")
    rows = evidence["zones"]
    require(isinstance(rows, list) and len(rows) == len(m["zones"]), "evidence zone set")
    expected = {z["id"] for z in m["zones"]}
    seen, captures, result_rows = set(), {}, []
    for row in rows:
        closed(row, "zoneId geometryClear noRampOrJump observedSpeedBoundMetersPerSecond observedPositionErrorMeters checks")
        require(token(row["zoneId"]) and row["zoneId"] in expected and row["zoneId"] not in seen, "evidence zone identity")
        seen.add(row["zoneId"])
        require(row["geometryClear"] is True and row["noRampOrJump"] is True, "native geometry inspection required")
        require(number(row["observedSpeedBoundMetersPerSecond"], m["speedBoundMetersPerSecond"], 100) and number(row["observedPositionErrorMeters"], 0, m["positionErrorMeters"]), "measured envelope")
        closed(row["checks"], " ".join(CHECKS))
        for name in CHECKS:
            check = row["checks"][name]
            closed(check, "passed nativeRunId artifactSHA256")
            h = check["artifactSHA256"]
            require(check["passed"] is True and uuid_value(check["nativeRunId"]) and hash_value(h), "native controller check required")
            if h not in captures:
                capture = capture_reader(h)
                require(isinstance(capture, bytes) and 0 < len(capture) <= 1024 * 1024 and digest(capture) == h, "missing or changed native capture")
                captures[h] = capture
        result_rows.append({k: row[k] for k in ("zoneId", "geometryClear", "noRampOrJump", "observedSpeedBoundMetersPerSecond", "observedPositionErrorMeters")} | {k: True for k in CHECKS} | {"physicsFPS": fps})
    report = {"schemaVersion": 1, "contract": "tholus.stk-scintilla-party-zones/1", "profileId": "sc5_crew_party_v1", "crewRulesRevision": "1", "manifestSHA256": digest(manifest_bytes), "trackHash": m["trackHash"], "nativeBuildSHA256": build_hash, "trackAssetsSHA256": assets_hash, "evidenceSHA256": digest(evidence_bytes), "zones": result_rows}
    return report, captures


def ssd_path(path):
    require(SSD.is_mount() and os.access(SSD, os.W_OK), "SSD must be mounted and writable")
    resolved = path.resolve()
    require(resolved.is_relative_to(SSD), "output must remain on SSD")
    return resolved


def atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix=".party-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
        os.replace(name, path)
    finally:
        Path(name).unlink(missing_ok=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--manifest", required=True, type=Path)
    p.add_argument("--evidence", type=Path)
    p.add_argument("--proofs-dir", type=Path)
    p.add_argument("--native-build-sha256")
    p.add_argument("--track-assets-sha256")
    p.add_argument("--physics-fps", type=int)
    p.add_argument("--output-dir", type=Path)
    args = p.parse_args()
    try:
        data = read_bounded(args.manifest, 16384)
        m = manifest(data)
        if args.evidence is None:
            require(not any(v is not None for v in (args.proofs_dir, args.native_build_sha256, args.track_assets_sha256, args.physics_fps, args.output_dir)), "qualification arguments require native evidence")
            print(json.dumps({"manifestValid": True, "qualified": False, "authoringStatus": m["authoringStatus"], "candidateNodes": m["candidateNodes"], "operatingIntervals": len(m["zones"])}))
            return 0
        require(args.proofs_dir is not None and args.output_dir is not None, "proofs directory and output directory required")
        output = ssd_path(args.output_dir)
        evidence = read_bounded(args.evidence, 65536)
        report, captures = qualify(data, evidence, args.native_build_sha256, args.track_assets_sha256, args.physics_fps, lambda h: read_bounded(args.proofs_dir / (h + ".proof"), 1024 * 1024))
        for h, capture in captures.items():
            atomic_write(ssd_path(output / "party_zone_proofs" / (h + ".proof")), capture)
        atomic_write(ssd_path(output / "party_zones_evidence_v1.json"), evidence)
        # Commit the descriptor last; incomplete installation always fails closed.
        atomic_write(ssd_path(output / "party_zones_v1.json"), (json.dumps(report, indent=2) + "\n").encode())
        print(json.dumps({"qualificationInstalled": True, "nativeEvidenceVerifiedByTool": True, "captures": len(captures), "zones": len(report["zones"]), "hardwareQualifiedByThisCommand": False}))
        return 0
    except (ValueError, OSError, TypeError, KeyError) as error:
        print(json.dumps({"qualificationInstalled": False, "error": str(error)}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
