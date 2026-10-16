#!/usr/bin/env python3
"""Validate bounded native SC4 geometry captures; never infer optical qualification.

The report is a descriptor for the original measurements. The native reader
checks their hashes and repeats the admission calculation; a report status is
not an instruction to trust a manually edited qualification flag.
"""
from __future__ import annotations

import argparse
from collections import deque
from dataclasses import dataclass, field
import hashlib
import json
import math
from pathlib import Path
import re
import sys
from typing import BinaryIO

TRACE_CONTRACT = "tholus.stk-scintilla-measurement/1"
REPORT_CONTRACT = "tholus.stk-scintilla-qualification/1"
CREW_TRACE_CONTRACT = "tholus.stk-scintilla-measurement/2"
CREW_REPORT_CONTRACT = "tholus.stk-scintilla-qualification/2"
PROFILE = "sc4_cargo_three_slots_v1"
CREW_PROFILES = ("sc5_crew_standard_v1", "sc5_crew_party_v1")
PHASE_POLICY = "diagnostic_constant_per_trace"
MAX_LINE = 65536
MAX_BYTES = 64 * 1024 * 1024
MAX_FRAMES = 18000
MAX_TRACES = 8
MAX_GAP_US = 50000
LIVE_PROOF_MAX_AGE_US = 50000
UINT64_MAX = (1 << 64) - 1
INT64_MAX = (1 << 63) - 1
HEX = re.compile(r"[0-9a-f]{64}\Z")
TOKEN = re.compile(r"[A-Za-z0-9_.:-]{1,128}\Z")
UUID = re.compile(r"[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}\Z")


class Invalid(ValueError):
    """A bounded, machine-readable validation failure."""


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise Invalid(reason)


def closed(value: object, required: set[str], optional: set[str] = frozenset()) -> dict:
    require(isinstance(value, dict) and required <= value.keys() <= required | optional, "invalid_fields")
    return value


def integer(value: object, low: int, high: int) -> int:
    require(type(value) is int and low <= value <= high, "invalid_integer")
    return value


def number(value: object, low: float = -1e9, high: float = 1e9) -> float:
    require(type(value) in (int, float) and math.isfinite(value) and low <= value <= high, "invalid_number")
    return value


def boolean(value: object) -> bool:
    require(type(value) is bool, "invalid_boolean")
    return value


def uint(value: object, maximum: int = UINT64_MAX) -> int:
    require(isinstance(value, str) and re.fullmatch(r"0|[1-9][0-9]{0,19}", value) is not None, "invalid_decimal")
    result = int(value)
    require(result <= maximum, "invalid_decimal")
    return result


def vec(value: object) -> tuple[float, float, float]:
    require(isinstance(value, list) and len(value) == 3, "invalid_vector")
    return tuple(number(x, -1e6, 1e6) for x in value)


def unique(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        require(key not in result, "duplicate_json_key")
        result[key] = value
    return result


def json_value(raw: bytes | str) -> object:
    try:
        value = json.loads(raw, object_pairs_hook=unique,
                           parse_constant=lambda _: (_ for _ in ()).throw(Invalid("nonfinite_json")))
        # Limit nesting before subsequent recursive comparisons. JSON is bounded
        # in bytes as well; never raise Python's recursion limit for a capture.
        pending = [(value, 0)]
        while pending:
            node, depth = pending.pop()
            require(depth <= 32, "json_depth")
            if isinstance(node, dict):
                pending.extend((item, depth + 1) for item in node.values())
            elif isinstance(node, list):
                pending.extend((item, depth + 1) for item in node)
        return value
    except (UnicodeError, RecursionError, json.JSONDecodeError) as error:
        raise Invalid("invalid_json") from error


def digest(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def norm(v: tuple[float, float, float]) -> float:
    return math.hypot(*v)


def dot(a, b) -> float:
    return sum(x * y for x, y in zip(a, b))


def angular_diameter(radius: float, distance: float) -> float:
    require(math.isfinite(radius) and math.isfinite(distance) and 0 < radius < distance, "invalid_sphere")
    return math.degrees(2 * math.asin(radius / distance))


def world_rect_free(left: float, top: float, right: float, bottom: float, width: int, height: int, revision: int = 2) -> bool:
    """Independent transcription of the closed Layout v2/v3 world masks."""
    require(revision in (2, 3), "invalid_layout")
    if not (0 <= left <= right < 1 and 0 <= top <= bottom < 1):
        return False
    scale = min(width, height)

    def intersects(l, t, r, b):
        return left <= r and right >= l and top <= b and bottom >= t

    def circle(x, y, radius):
        cx = max(left, min(right, x)); cy = max(top, min(bottom, y))
        return math.hypot((x - cx) * width, (y - cy) * height) <= radius

    # C++ round is half-away-from-zero. These pixel coordinates are positive.
    rear = [math.floor(width * .5 - .18 * scale + .5) / width,
            math.floor(height * .5 - .35 * scale + .5) / height,
            math.floor(width * .5 + .18 * scale + .5) / width,
            math.floor(height * .5 - .17 * scale + .5) / height]
    if intersects(*rear):
        return False
    for i in range(5):
        x = (width * .5 + (-.6 + .4 * i if i < 4 else 0) * scale * .5) / width
        y = (height * .5 + (.62 if i < 4 else .85) * scale * .5) / height
        if circle(x, y, (.07 if i < 4 else .06) * scale * .5):
            return False
    for i in range(3):
        for y, radius in ((-.16, .06), (-.27, .025)):
            if circle(.5 + (-.18 + .18 * i) * scale * .5 / width,
                      .5 + y * scale * .5 / height, radius * scale * .5):
                return False
    if revision == 3 and circle(.5 + .22 * scale * .5 / width, .5 + .85 * scale * .5 / height, .06 * scale * .5):
        return False
    for i in range(2):
        if intersects(.5 - .21 * scale / width, .5 + (.21 + .03 * i) * scale / height,
                      .5 + .21 * scale / width, .5 + (.23 + .03 * i) * scale / height):
            return False
    for i in range(5):
        for y in (.08, .25):
            if intersects(.5 + (-.32 + .16 * i - .055) * scale * .5 / width,
                          .5 + (y - .055) * scale * .5 / height,
                          .5 + (-.32 + .16 * i + .055) * scale * .5 / width,
                          .5 + (y + .055) * scale * .5 / height):
                return False
    return True


def rectangle_free(center, radius: float, camera: dict, fingerprint: dict) -> bool:
    delta = tuple(x - y for x, y in zip(center, camera["position"]))
    distance = norm(delta)
    if distance <= radius:
        return False
    direction = [dot(delta, camera[axis]) / distance for axis in ("right", "up", "forward")]
    alpha = math.asin(radius / distance)
    if fingerprint["projection"] == "equirectangular":
        latitude = math.asin(max(-1., min(1., direction[1])))
        if abs(latitude) + alpha >= math.pi * .5:
            return False
        du = alpha / (2 * math.pi * math.cos(abs(latitude) + alpha)); dv = alpha / math.pi
        u = .5 + math.atan2(direction[0], direction[2]) / (2 * math.pi)
        v = .5 - latitude / math.pi
    else:
        angle = math.radians(-fingerprint["domeTiltDeg"]); c = math.cos(angle); s = math.sin(angle)
        x, y, z = direction[0], c * direction[1] - s * direction[2], s * direction[1] + c * direction[2]
        theta = math.acos(max(-1., min(1., z))); xy = math.hypot(x, y)
        if theta + alpha >= math.pi * .5:
            return False
        du = dv = alpha
        u = .5 + (theta / (math.pi * .5) * x / xy if xy else 0) * .5
        v = .5 - (theta / (math.pi * .5) * y / xy if xy else 0) * .5
    return world_rect_free(u - du, v - dv, u + du, v + dv, fingerprint["width"], fingerprint["height"], fingerprint["evaluatedLayoutRevision"])


def capsule_proof_available(capture: int, readback: int, at: int) -> bool:
    return 0 <= capture <= readback <= at and at - capture <= LIVE_PROOF_MAX_AGE_US


def capsule_proofs_overlap(previous_capture: int, readback: int) -> bool:
    return 0 <= readback - previous_capture <= LIVE_PROOF_MAX_AGE_US


@dataclass
class Window:
    """Maximin sustained speed, using intervals rather than sample counts.

    For each end, the latest start that still spans the full duration is optimal:
    any earlier start can only lower the minimum speed. Two monotone queues make
    the calculation linear and bound storage by the recording's frame limit.
    """
    fps: int
    duration_us: int
    strict: bool
    points: deque = field(default_factory=deque)
    minima: deque = field(default_factory=deque)
    best: float | None = None
    next_serial: int = 0

    def reset_run(self):
        self.points.clear(); self.minima.clear()

    def enough(self, first, last) -> bool:
        wall = last[1] - first[3]
        ticks = (last[2] - first[4]) * 1000000
        required_ticks = self.duration_us * self.fps
        return (wall > self.duration_us and ticks > required_ticks) if self.strict else (wall >= self.duration_us and ticks >= required_ticks)

    def add(self, capture_us: int, tick: int, minimum_speed: float, readback_us: int | None = None, readback_tick: int | None = None):
        # Tether windows retain their capture-based 3s geometric contract.
        available_us = capture_us if readback_us is None or not self.strict else readback_us
        available_tick = tick if readback_tick is None or not self.strict else readback_tick
        point = (self.next_serial, capture_us, tick, available_us, available_tick); self.next_serial += 1
        self.points.append(point)
        while self.minima and self.minima[-1][1] >= minimum_speed:
            self.minima.pop()
        self.minima.append((point[0], minimum_speed))
        while len(self.points) > 1 and self.enough(self.points[1], point):
            self.points.popleft()
        while self.minima and self.minima[0][0] < self.points[0][0]:
            self.minima.popleft()
        if self.enough(self.points[0], point):
            bound = self.minima[0][1]
            self.best = bound if self.best is None else max(self.best, bound)


FINGERPRINT_KEYS = {"anchorHash", "trackHash", "trackAssetsHash", "shaderHash", "executableHash", "profileId", "profileRevision",
                    "P", "C", "laps", "F", "width", "height", "cubemapSize", "projection", "output", "near", "far", "faceFovDeg",
                    "domeTiltDeg", "cameraMode", "camera", "horizonStabilization", "capsuleRadius", "tetherRadius", "tetherOffsets",
                    "evaluatedLayoutRevision", "nativeBaseSpeed"}
CREW_FINGERPRINT_KEYS = {"crewRulesRevision", "zoneManifestHash", "partyEnabled", "partyCost", "partyGain", "partyHoldTicks",
                         "partyWindowTicks", "partyDurationTicks", "partyFadeTicks", "partyCooldownTicks"}


def validate_fingerprint(value: object) -> dict:
    require(isinstance(value, dict), "invalid_fields")
    crew = value.get("profileId") in CREW_PROFILES
    f = closed(value, FINGERPRINT_KEYS | (CREW_FINGERPRINT_KEYS if crew else set()))
    for key in ("anchorHash", "trackHash", "trackAssetsHash", "shaderHash", "executableHash"):
        require(isinstance(f[key], str) and HEX.fullmatch(f[key]) is not None, "invalid_hash")
    require(f["profileId"] in (PROFILE, *CREW_PROFILES) and f["profileRevision"] == "1", "invalid_profile")
    integer(f["P"], 2, 6); integer(f["C"], 1, 5); require(f["P"] == f["C"] + 1, "invalid_crew")
    integer(f["laps"], 1, 12); integer(f["F"], 60 if crew else 30, 1000)
    integer(f["width"], 256, 8192); integer(f["height"], 256, 8192); integer(f["cubemapSize"], 64, 8192)
    require(f["projection"] in ("equirectangular", "fisheye_domemaster"), "invalid_projection")
    for key in ("output", "cameraMode"):
        require(isinstance(f[key], str) and TOKEN.fullmatch(f[key]) is not None, "invalid_token")
    near = number(f["near"], 1e-6, 1000); number(f["far"], near, 1e7); require(f["far"] > near, "invalid_clip")
    require(number(f["faceFovDeg"]) == 90, "invalid_face_fov"); number(f["domeTiltDeg"], -90, 90)
    require(isinstance(f["camera"], list) and len(f["camera"]) == 8, "invalid_camera")
    for v in f["camera"]:
        number(v, -1e6, 1e6)
    boolean(f["horizonStabilization"])
    require(number(f["capsuleRadius"]) == 2 and number(f["tetherRadius"]) == .5, "invalid_radius")
    require(f["tetherOffsets"] == [[-3, 5.5, 6], [3, 5.5, 6]], "invalid_tether_offsets")
    require(type(f["evaluatedLayoutRevision"]) is int and f["evaluatedLayoutRevision"] == (3 if crew else 2), "invalid_layout")
    if crew:
        require(f["crewRulesRevision"] == "1" and isinstance(f["zoneManifestHash"], str) and HEX.fullmatch(f["zoneManifestHash"]), "invalid_crew_identity")
        require(boolean(f["partyEnabled"]) == (f["profileId"] == "sc5_crew_party_v1"), "invalid_party_profile")
        expected = {"partyCost": 15, "partyGain": 1, "partyHoldTicks": (3 * f["F"] + 3) // 4,
                    "partyWindowTicks": 3 * f["F"], "partyDurationTicks": f["F"] // 2,
                    "partyFadeTicks": f["F"] // 4, "partyCooldownTicks": 6 * f["F"]}
        require(all(type(f[key]) is int and f[key] == v for key, v in expected.items()), "invalid_crew_parameters")
    number(f["nativeBaseSpeed"], .01, 200)
    return f


def validate_groups(value: object, manifest: dict) -> list:
    require(isinstance(value, list) and 1 <= len(value) <= 12, "invalid_groups")
    require(len(value) == len(manifest["groups"]), "manifest_groups_changed")
    seen = set()
    for group, original in zip(value, manifest["groups"]):
        closed(group, {"id", "activationFraction", "expiryFraction", "laps", "capsules"})
        require(isinstance(group["id"], str) and TOKEN.fullmatch(group["id"]) is not None and group["id"] not in seen, "invalid_group_id")
        seen.add(group["id"])
        require(0 <= number(group["activationFraction"]) < number(group["expiryFraction"]) <= 1, "invalid_group_window")
        require(isinstance(group["laps"], list) and 1 <= len(group["laps"]) <= 12 and len(set(group["laps"])) == len(group["laps"]), "invalid_group_laps")
        for lap in group["laps"]:
            integer(lap, 0, 11)
        for key in ("id", "activationFraction", "expiryFraction", "laps"):
            require(group[key] == original[key], "manifest_groups_changed")
        require(isinstance(group["capsules"], list) and 1 <= len(group["capsules"]) <= 2 and len(group["capsules"]) == len(original["capsules"]), "invalid_capsules")
        for capsule, source in zip(group["capsules"], original["capsules"]):
            closed(capsule, {"center", "radius"}); vec(capsule["center"])
            require(number(capsule["radius"]) == 2 and capsule["center"] == source["center"] and capsule["radius"] == source["radius"], "manifest_capsule_changed")
    return value


def validate_camera(value: object) -> dict:
    c = closed(value, {"position", "right", "up", "forward"})
    vectors = {key: vec(c[key]) for key in c}
    for key in ("right", "up", "forward"):
        require(abs(norm(vectors[key]) - 1) <= .001, "invalid_camera_basis")
    for a, b in (("right", "up"), ("right", "forward"), ("up", "forward")):
        require(abs(dot(vectors[a], vectors[b])) <= .001, "invalid_camera_basis")
    return vectors


@dataclass
class Evidence:
    fingerprint_json: str | None = None
    fingerprint_hash: str | None = None
    fingerprint: dict | None = None
    groups: list | None = None
    bounds: dict = field(default_factory=dict)
    reasons: set[str] = field(default_factory=set)
    scenarios: set[str] = field(default_factory=set)
    natural_finish: bool = False
    frames: int = 0
    activations: set = field(default_factory=set)

    def read(self, stream: BinaryIO, manifest: dict, manifest_hash: str) -> tuple[str, int]:
        hasher = hashlib.sha256(); size = 0; header = None; footer = None; windows = {}; previous = None; count = 0
        while True:
            raw = stream.readline(MAX_LINE + 1)
            if not raw:
                break
            size += len(raw); hasher.update(raw)
            require(len(raw) <= MAX_LINE and size <= MAX_BYTES, "trace_size_limit")
            require(raw.endswith(b"\n") and raw.strip(), "invalid_ndjson")
            row = json_value(raw)
            require(isinstance(row, dict), "invalid_record")
            if header is None:
                header = self._header(row, manifest, manifest_hash)
                continue
            require(footer is None, "data_after_footer")
            if row.get("type") == "footer":
                closed(row, {"type", "frameCount", "complete", "reason", "naturalFinish", "officialLap"})
                require(uint(row["frameCount"]) == count and boolean(row["complete"]), "incomplete_capture")
                require(row["reason"] == "", "invalid_footer_reason")
                integer(row["officialLap"], -1, self.fingerprint["laps"])
                if boolean(row["naturalFinish"]):
                    require(row["officialLap"] >= self.fingerprint["laps"], "premature_natural_finish")
                    self.natural_finish = True
                footer = row
                continue
            count += 1; require(count <= MAX_FRAMES, "frame_limit")
            previous = self._frame(row, header, windows, previous)
        require(header is not None and footer is not None and count > 0, "incomplete_capture")
        self.frames += count
        for key, window in windows.items():
            if window.best is not None:
                self.bounds[key] = max(self.bounds.get(key, -1), window.best)
        return hasher.hexdigest(), size

    def _header(self, row: dict, manifest: dict, manifest_hash: str) -> dict:
        closed(row, {"type", "contract", "schemaVersion", "recordingID", "producerUUID", "raceEpoch", "provenance", "fingerprint", "fingerprintJSON", "fingerprintHash", "groups", "limits", "phasePolicy", "phase", "actualLayoutRevision"})
        crew = isinstance(row.get("fingerprint"), dict) and row["fingerprint"].get("profileId") in CREW_PROFILES
        require(row["type"] == "header" and row["contract"] == (CREW_TRACE_CONTRACT if crew else TRACE_CONTRACT) and type(row["schemaVersion"]) is int and row["schemaVersion"] == (2 if crew else 1), "invalid_trace_contract")
        for key in ("recordingID", "producerUUID"):
            require(isinstance(row[key], str) and UUID.fullmatch(row[key]) is not None, "invalid_uuid")
        require(uint(row["raceEpoch"]) > 0, "invalid_race")
        if row["provenance"] != "native_render_depth":
            self.reasons.add("non_native_provenance")
        require(row["phasePolicy"] == PHASE_POLICY and row["phase"] in ("shell", "open"), "invalid_phase_policy")
        if crew:
            require(integer(row["actualLayoutRevision"], 3, 3) == 3, "invalid_actual_layout")
        else:
            integer(row["actualLayoutRevision"], 0, 1)
        f = validate_fingerprint(row["fingerprint"])
        require(isinstance(row["fingerprintJSON"], str) and len(row["fingerprintJSON"].encode()) <= 8192, "invalid_fingerprint_json")
        require(json_value(row["fingerprintJSON"]) == f and row["fingerprintHash"] == digest(row["fingerprintJSON"].encode()), "fingerprint_hash_mismatch")
        require(f["anchorHash"] == manifest_hash and f["trackHash"] == manifest["trackHash"], "anchor_hash_mismatch")
        groups = validate_groups(row["groups"], manifest)
        limits = closed(row["limits"], {"maxFrames", "maxPending", "maxGapUs", "liveProofMaxAgeUs"})
        require(limits == {"maxFrames": MAX_FRAMES, "maxPending": 8, "maxGapUs": MAX_GAP_US, "liveProofMaxAgeUs": LIVE_PROOF_MAX_AGE_US}, "invalid_limits")
        if self.fingerprint_json is not None:
            require(self.fingerprint_json == row["fingerprintJSON"] and self.groups == groups, "mixed_fingerprint")
        else:
            self.fingerprint_json = row["fingerprintJSON"]; self.fingerprint_hash = row["fingerprintHash"]
            self.fingerprint = f; self.groups = groups
        return row

    def _frame(self, row: dict, header: dict, windows: dict, previous):
        closed(row, {"type", "index", "frameID", "captureUs", "readbackUs", "readbackTick", "tick", "officialLap", "fraction", "speed", "minimumSpeed", "maxSpeed", "scenario", "speedIncreaseTicks", "active", "continuous", "published", "camera", "kartPosition", "tetherCandidates", "objects"})
        require(row["type"] == "frame", "invalid_record_type")
        index = uint(row["index"]); frame_id = uint(row["frameID"])
        capture = uint(row["captureUs"], INT64_MAX); tick = uint(row["tick"], INT64_MAX)
        readback = uint(row["readbackUs"], INT64_MAX); readback_tick = uint(row["readbackTick"], INT64_MAX)
        require(readback >= capture and readback_tick >= tick, "invalid_readback_clock")
        if previous is not None:
            require(readback >= previous[8] and readback_tick >= previous[9], "readback_order")
        f = self.fingerprint; lap = integer(row["officialLap"], -1, f["laps"])
        fraction = number(row["fraction"], 0, 1)
        speed = number(row["speed"], -200, 200); minimum = number(row["minimumSpeed"], 0, 200)
        number(row["maxSpeed"], .01, 200); require(minimum <= abs(speed) + 1e-5, "invalid_interval_speed")
        flags = row["speedIncreaseTicks"]
        require(isinstance(flags, list) and len(flags) == 6, "invalid_speed_flags")
        for flag in flags:
            integer(flag, 0, 10000000)
        scenario = "nitro" if flags[2] > 0 else "zipper" if flags[0] > 0 else "other" if any(flags) else "normal"
        require(row["scenario"] == scenario, "scenario_flag_mismatch")
        active = boolean(row["active"]); continuous = boolean(row["continuous"]); published = boolean(row["published"])
        camera = validate_camera(row["camera"]); kart = vec(row["kartPosition"])
        require(isinstance(row["tetherCandidates"], list) and len(row["tetherCandidates"]) == 2, "invalid_tether_candidates")
        candidates = [vec(candidate) for candidate in row["tetherCandidates"]]
        require(isinstance(row["objects"], list) and len(row["objects"]) <= 4, "object_limit")
        if previous is None:
            require(index == 1 and frame_id > 0, "invalid_first_index")
            contiguous = False
        else:
            old_index, old_frame, old_capture, old_tick = previous[:4]
            require(index == old_index + 1 and frame_id > old_frame and capture > old_capture and tick >= old_tick, "trace_order")
            contiguous = capture - old_capture <= MAX_GAP_US
            if previous[6] >= 0:
                traveled = abs(lap + fraction - previous[5])
                if lap + fraction < previous[5] or traveled > .1 or norm(tuple(a - b for a, b in zip(kart, previous[7]))) > 20:
                    contiguous = False
        repeated = previous is not None and tick == previous[3] and contiguous and active and continuous and published and 0 <= lap < f["laps"]
        if not contiguous or not active or not continuous or not published:
            for window in windows.values():
                window.reset_run()
        if active and continuous and published:
            self.scenarios.add(scenario)
        present = set()
        prefetch = {}
        for obj in row["objects"]:
            key, geometry, in_window, activation = self._object(obj, header, camera, candidates, lap, fraction)
            require(key not in present, "duplicate_object")
            present.add(key)
            if key not in windows:
                windows[key] = Window(f["F"], 2000000 if obj["kind"] == "capsule" else 3000000, obj["kind"] == "capsule")
            window = windows[key]
            valid = contiguous and active and continuous and published and geometry
            if obj["kind"] == "capsule":
                prefetch[key] = valid and not in_window and capsule_proof_available(capture, readback, readback)
                if activation:
                    require(previous is not None and previous[2] < uint(obj["activationUs"], INT64_MAX) <= capture, "invalid_activation_clock")
                if activation and valid and contiguous and previous is not None and previous[4].get(key, False) and capsule_proof_available(previous[2], previous[8], uint(obj["activationUs"], INT64_MAX)):
                    definition = self.groups[obj["group"]]
                    crossing = lap + definition["activationFraction"]
                    if previous[5] < crossing <= lap + fraction:
                        if obj["phase"] == "shell":
                            self.activations.add(key[:-1])
            if obj["kind"] == "capsule":
                available = capsule_proof_available(capture, readback, readback)
                overlap = previous is not None and capsule_proofs_overlap(previous[2], readback)
                if not available or not overlap:
                    window.reset_run()
                valid = valid and available
            if valid and in_window and not repeated:
                window.add(capture, tick, minimum, readback, readback_tick)
            elif not repeated or not geometry or not in_window:
                window.reset_run()
        for key, window in windows.items():
            if key not in present:
                window.reset_run()
        return index, frame_id, capture, tick, prefetch, lap + fraction, lap, kart, readback, readback_tick

    def _object(self, obj: object, header: dict, camera: dict, candidates: list, lap: int, fraction: float):
        closed(obj, {"kind", "group", "capsule", "side", "phase", "center", "radius", "range", "rectangleFree", "activation", "activationUs", "inWindow", "idealPixels", "admissiblePixels", "visiblePixels", "status"})
        f = self.fingerprint; center = vec(obj["center"]); radius = number(obj["radius"], .01, 5)
        group = integer(obj["group"], -1, len(self.groups) - 1); capsule = integer(obj["capsule"], -1, 1); side = integer(obj["side"], -1, 1)
        require(obj["kind"] in ("capsule", "tether"), "invalid_object_kind")
        if obj["kind"] == "capsule":
            require(group >= 0 and side == -1 and 0 <= capsule < (1 if f["C"] == 1 else 2), "invalid_capsule_identity")
            definition = self.groups[group]
            require(capsule < len(definition["capsules"]) and lap in definition["laps"] and fraction < definition["expiryFraction"], "object_outside_occurrence")
            in_window = definition["activationFraction"] <= fraction < definition["expiryFraction"]
            expected = definition["capsules"][capsule]
            require(norm(tuple(a - b for a, b in zip(center, expected["center"]))) <= 1e-5 and radius == expected["radius"], "object_pose_mismatch")
            require(obj["phase"] == header["phase"], "object_phase_mismatch")
            key = ("capsule", lap, group, capsule, obj["phase"]); low, high = 8, 75
        else:
            require(group == -1 and capsule == -1 and side in (0, 1), "invalid_tether_identity")
            require(norm(tuple(a - b for a, b in zip(center, candidates[side]))) <= 1e-5 and radius == .5, "object_pose_mismatch")
            require(obj["phase"] == ("awaiting_presentation" if header["phase"] == "shell" else "recoverable"), "object_phase_mismatch")
            key = ("tether", lap, side, obj["phase"]); low, high = .75, 15
            in_window = 0 <= lap < f["laps"]
        require(boolean(obj["inWindow"]) == in_window, "occurrence_window_mismatch")
        activation = boolean(obj["activation"])
        require((uint(obj["activationUs"], INT64_MAX) != 0) == activation, "invalid_activation_clock")
        require(not activation or (obj["kind"] == "capsule" and in_window), "invalid_activation")
        distance = norm(tuple(a - b for a, b in zip(center, camera["position"])))
        in_range = distance > radius and low <= distance <= high and angular_diameter(radius, distance) >= 3
        require(boolean(obj["range"]) == in_range, "range_mismatch")
        require(boolean(obj["rectangleFree"]) == rectangle_free(center, radius, camera, f), "rectangle_mismatch")
        pixels = integer(obj["idealPixels"], 0, f["width"] * f["height"])
        admitted = integer(obj["admissiblePixels"], 0, pixels); visible = integer(obj["visiblePixels"], 0, admitted)
        require(obj["status"] in ("ready", "pending_timeout", "unavailable", "overflow"), "invalid_query_status")
        return key, 0 <= lap < f["laps"] and obj["status"] == "ready" and in_range and obj["rectangleFree"] and pixels > 0 and visible == admitted == pixels, in_window, activation

    def finish(self):
        require(self.fingerprint is not None, "no_captures")
        f = self.fingerprint; base = f["nativeBaseSpeed"] + (1 if f["profileId"] == "sc5_crew_party_v1" else 0)
        if not self.natural_finish:
            self.reasons.add("natural_finish_missing")
        if "normal" not in self.scenarios or not ({"zipper", "nitro"} & self.scenarios):
            self.reasons.add("native_stress_missing")
        for lap in range(f["laps"]):
            groups = [i for i, group in enumerate(self.groups) if lap in group["laps"]]
            if len(groups) < 4:
                self.reasons.add("replacement_opportunities_missing")
            for group in groups:
                for capsule in range(1 if f["C"] == 1 else 2):
                    if ("capsule", lap, group, capsule) not in self.activations:
                        self.reasons.add("activation_not_proven")
                    for phase in ("shell", "open"):
                        if self.bounds.get(("capsule", lap, group, capsule, phase), -1) < base:
                            self.reasons.add("capsule_window_below_base_speed")
            for side in (0, 1):
                for phase in ("awaiting_presentation", "recoverable"):
                    if self.bounds.get(("tether", lap, side, phase), -1) < base:
                        self.reasons.add("tether_window_below_base_speed")


def report_for(paths: list[Path], anchors: Path, report_path: Path) -> tuple[dict, Evidence]:
    require(1 <= len(paths) <= MAX_TRACES and len({p.resolve() for p in paths}) == len(paths), "trace_file_limit")
    require(anchors.is_file() and 0 < anchors.stat().st_size <= 65536, "anchor_size_limit")
    with anchors.open("rb") as stream:
        raw_manifest = stream.read(65537)
    require(0 < len(raw_manifest) <= 65536, "anchor_size_limit")
    manifest = json_value(raw_manifest)
    require(isinstance(manifest, dict) and manifest.get("profileId") == PROFILE and isinstance(manifest.get("groups"), list), "invalid_anchor_manifest")
    evidence = Evidence(); descriptors = []; root = report_path.parent.resolve(strict=True)
    for path in paths:
        absolute = path.absolute()
        require(absolute.is_relative_to(root), "trace_outside_report_directory")
        relative = absolute.relative_to(root).as_posix()
        require(len(relative) <= 240 and re.fullmatch(r"[A-Za-z0-9_./-]+", relative) is not None and
                all(part not in ("", ".", "..") for part in relative.split("/")), "unsafe_trace_path")
        component = root
        for part in absolute.relative_to(root).parts:
            component /= part
            require(not component.is_symlink(), "trace_symlink")
        resolved = path.resolve(strict=True)
        require(resolved.is_file() and resolved.is_relative_to(root), "trace_outside_report_directory")
        require(0 < resolved.stat().st_size <= MAX_BYTES, "trace_size_limit")
        with resolved.open("rb") as stream:
            sha, size = evidence.read(stream, manifest, digest(raw_manifest))
        descriptors.append({"path": relative, "sha256": sha, "bytes": size})
    evidence.finish()
    # Synthetic captures are useful to test rejection and mathematics, but must
    # never be written into a descriptor claiming native-render provenance.
    require("non_native_provenance" not in evidence.reasons, "non_native_provenance")
    crew = evidence.fingerprint["profileId"] in CREW_PROFILES
    return {"contract": CREW_REPORT_CONTRACT if crew else REPORT_CONTRACT, "schemaVersion": 2 if crew else 1, "scope": "native_render_geometry",
            "status": "unqualified" if evidence.reasons else "qualified_geometry", "provenance": "native_render_depth",
            "fingerprintJSON": evidence.fingerprint_json, "fingerprintHash": evidence.fingerprint_hash,
            "traceFiles": descriptors, "reasons": sorted(evidence.reasons)}, evidence


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace", type=Path, action="append", required=True, help="Native NDJSON; repeat at most eight times")
    parser.add_argument("--anchors", type=Path, required=True, help="Exact loaded anchor manifest")
    parser.add_argument("--report", type=Path, required=True, help="Descriptor beside or above its trace files, on the external SSD")
    args = parser.parse_args(argv)
    try:
        ssd = Path("/Volumes/SanDisk 2TB")
        require(ssd.is_mount() and args.report.parent.resolve(strict=True).is_relative_to(ssd.resolve()) and
                args.report.resolve().is_relative_to(ssd.resolve()) and not args.report.is_symlink(), "report_requires_ssd")
        require(args.report.resolve() not in {p.resolve() for p in [*args.trace, args.anchors]}, "report_overwrites_input")
        report, evidence = report_for(args.trace, args.anchors, args.report)
        args.report.write_text(json.dumps(report, indent=2, ensure_ascii=True, allow_nan=False) + "\n")
        print(json.dumps({"status": report["status"], "reasons": report["reasons"], "frames": evidence.frames,
                          "scope": report["scope"], "opticalQualification": False, "hardwareQualification": False}))
        return 0 if report["status"] == "qualified_geometry" else 1
    except (Invalid, OSError, TypeError, KeyError, OverflowError, ValueError) as error:
        print(json.dumps({"status": "unqualified", "error": str(error)}), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
