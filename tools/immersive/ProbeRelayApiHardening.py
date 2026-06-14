#!/usr/bin/env python3
"""Negative probes for the Phase 2 localhost relay API."""

import argparse
import json
import sys
import urllib.error
import urllib.request
import uuid


def request(base, token, method, path, body=None, raw=None, timeout=5.0):
    data = raw
    headers = {"X-STK-Relay-Token": token}
    if body is not None:
        data = json.dumps(body).encode("utf-8")
        headers["Content-Type"] = "application/json"
    elif raw is not None:
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(
        base + path, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as response:
            text = response.read().decode("utf-8")
            return response.status, json.loads(text) if text else {}
    except urllib.error.HTTPError as error:
        text = error.read().decode("utf-8")
        try:
            return error.code, json.loads(text) if text else {}
        except json.JSONDecodeError:
            return error.code, {"ok": False, "error": text}


def start_body(players):
    roster = []
    for index in range(1, players + 1):
        roster.append({
            "player_id": f"p{index}",
            "display_name": "Pilot 1" if index == 1 else f"Crew Phone {index - 1}",
        })
    return {
        "session_id": "tholus-hardening-probe",
        "preset": "standard_arcade",
        "track_id": "snowtuxpeak",
        "handoff_policy": "lap_based_handoff",
        "player_count": players,
        "arcade_ui": True,
        "test_accelerated_progress": True,
        "players": roster,
    }


def stable_signature(state):
    cooldown_keys = []
    for cooldown in state.get("cooldowns", []):
        cooldown_keys.append({
            "player_id": cooldown.get("player_id"),
            "role": cooldown.get("role"),
            "action_type": cooldown.get("action_type"),
        })
    cooldown_keys.sort(key=lambda item: (
        item.get("player_id") or "",
        item.get("role") or "",
        item.get("action_type") or ""))
    return {
        "crew_score": state.get("crew_score"),
        "crew_multiplier_x100": state.get("crew_multiplier_x100"),
        "crew_heat": state.get("crew_heat"),
        "crew_armor": state.get("crew_armor"),
        "boost_charge": state.get("boost_charge"),
        "active_route_id": state.get("active_route_id"),
        "track_effects": state.get("track_effects", []),
        "cooldown_keys": cooldown_keys,
    }


def available_action(state):
    for phone in state.get("phone_states", []):
        actions = [
            action for action in phone.get("actions", [])
            if action.get("available", True)
        ]
        if actions:
            return phone, actions[0]
    raise RuntimeError("no available phone action")


def state_with_available_action(base, token):
    status, state = request(base, token, "GET", "/immersive/v1/state")
    if status == 200 and state.get("ok"):
        try:
            available_action(state)
            return status, state
        except RuntimeError:
            pass

    for progress in (160, 280, 520, 640, 880):
        progress_body = {
            "lap": 1,
            "progress_x1000": progress,
            "checkpoint": -1,
        }
        request(base, token, "POST", "/immersive/v1/test/progress",
                body=progress_body)
        status, state = request(base, token, "GET", "/immersive/v1/state")
        if status != 200 or not state.get("ok"):
            continue
        try:
            available_action(state)
            return status, state
        except RuntimeError:
            continue

    return status, state


def payload_for_action(action):
    payload = {}
    if action.get("target_key"):
        payload[action["target_key"]] = action.get("target_id", "")
    if action.get("mode_key"):
        payload[action["mode_key"]] = action.get("mode", "")
    amount = int(action.get("amount", 0) or 0)
    if amount > 0:
        payload["amount"] = amount
    return payload


def action_body(session_id, state_version, phone, action, key=None):
    payload = payload_for_action(action)
    body = {
        "session_id": session_id,
        "action_id": str(uuid.uuid4()),
        "idempotency_key": key or str(uuid.uuid4()),
        "state_version_seen": state_version,
        "player_id": phone["player_id"],
        "role": phone["role"],
        "type": action["type"],
        "payload": payload,
    }
    body.update(payload)
    return body


def mutate_signature(body):
    mutated = dict(body)
    payload = dict(mutated.get("payload", {}))
    if "amount" in payload or "amount" in mutated:
        amount = int(payload.get("amount", mutated.get("amount", 0)) or 0) + 1
        payload["amount"] = amount
        mutated["amount"] = amount
    elif "duration_ms" in payload or "duration_ms" in mutated:
        duration = int(
            payload.get("duration_ms", mutated.get("duration_ms", 0)) or 0) + 100
        payload["duration_ms"] = duration
        mutated["duration_ms"] = duration
    elif "target_id" in payload or "target_id" in mutated:
        target = payload.get("target_id", mutated.get("target_id", "target"))
        payload["target_id"] = str(target) + "_reused"
        mutated["target_id"] = payload["target_id"]
    else:
        amount = int(payload.get("amount", mutated.get("amount", 0)) or 0) + 1
        payload["amount"] = amount
        mutated["amount"] = amount
    mutated["payload"] = payload
    mutated["action_id"] = str(uuid.uuid4())
    return mutated


def expect_rejection(base, token, case, body=None, raw=None,
                     expected_status=400, expected_error=""):
    before_status, before_state = request(
        base, token, "GET", "/immersive/v1/state")
    if before_status != 200:
        return {
            "case": case,
            "ok": False,
            "error": f"state before returned HTTP {before_status}",
        }
    before_signature = stable_signature(before_state)
    if body is not None:
        body = dict(body)
        if "state_version_seen" in body:
            body["state_version_seen"] = int(
                before_state.get("state_version", 0) or 0)
        if "session_id" in body:
            body["session_id"] = before_state.get("session_id", body["session_id"])

    status, payload = request(
        base, token, "POST", "/immersive/v1/crew/action",
        body=body, raw=raw)

    after_status, after_state = request(
        base, token, "GET", "/immersive/v1/state")
    after_signature = stable_signature(after_state) if after_status == 200 else {}
    actual_error = payload.get("error", "")
    ok = (
        status == expected_status and
        (not expected_error or actual_error == expected_error or
         actual_error.startswith(expected_error)) and
        before_signature == after_signature
    )
    return {
        "case": case,
        "ok": ok,
        "http_status": status,
        "expected_status": expected_status,
        "error": actual_error,
        "expected_error": expected_error,
        "stable_state_unchanged": before_signature == after_signature,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Probe Phase 2 relay API negative cases.")
    parser.add_argument("--base", default="http://127.0.0.1:8890")
    parser.add_argument("--token", default="immersive-dev-token")
    parser.add_argument("--players", type=int, choices=range(2, 7), default=6)
    args = parser.parse_args()

    failures = []
    cases = []

    status, payload = request(
        args.base, args.token, "POST", "/immersive/v1/session/start",
        body=start_body(args.players))
    if status != 200 or not payload.get("ok"):
        failures.append(f"session/start failed: HTTP {status} {payload}")
    status, state = state_with_available_action(args.base, args.token)
    if status != 200 or not state.get("ok"):
        failures.append(f"state failed: HTTP {status} {state}")

    if not failures:
        phone, action = available_action(state)
        session_id = state.get("session_id", "tholus-hardening-probe")
        state_version = int(state.get("state_version", 0) or 0)
        valid = action_body(session_id, state_version, phone, action)

        missing_key = dict(valid)
        missing_key.pop("idempotency_key", None)
        cases.append(expect_rejection(
            args.base, args.token, "missing_idempotency_key",
            body=missing_key, expected_error="missing_idempotency_key"))

        missing_player = dict(valid)
        missing_player.pop("player_id", None)
        missing_player["idempotency_key"] = str(uuid.uuid4())
        cases.append(expect_rejection(
            args.base, args.token, "missing_player_id",
            body=missing_player, expected_error="missing_player_id"))

        unknown_action = dict(valid)
        unknown_action["idempotency_key"] = str(uuid.uuid4())
        unknown_action["type"] = "not_a_phase_2_action"
        cases.append(expect_rejection(
            args.base, args.token, "unknown_crew_action",
            body=unknown_action, expected_error="unknown_crew_action"))

        unknown_role = dict(valid)
        unknown_role["idempotency_key"] = str(uuid.uuid4())
        unknown_role["role"] = "not_a_role"
        cases.append(expect_rejection(
            args.base, args.token, "unknown_crew_role",
            body=unknown_role, expected_error="unknown_crew_role"))

        cases.append(expect_rejection(
            args.base, args.token, "malformed_json",
            raw=b"{", expected_error="malformed_json:"))

        reuse_key = str(uuid.uuid4())
        status, fresh_state = state_with_available_action(args.base, args.token)
        if status != 200 or not fresh_state.get("ok"):
            failures.append(f"idempotency setup state failed: HTTP {status} {fresh_state}")
            fresh_state = state
        phone, action = available_action(fresh_state)
        session_id = fresh_state.get("session_id", session_id)
        state_version = int(fresh_state.get("state_version", state_version) or 0)
        first = action_body(session_id, state_version, phone, action, reuse_key)
        status, accepted = request(
            args.base, args.token, "POST", "/immersive/v1/crew/action",
            body=first)
        if status != 200 or not accepted.get("ok") or not accepted.get("accepted"):
            failures.append(f"idempotency setup action failed: HTTP {status} {accepted}")
        else:
            state_status, fresh_state = request(
                args.base, args.token, "GET", "/immersive/v1/state")
            reused = mutate_signature(first)
            reused["session_id"] = session_id
            reused["state_version_seen"] = int(
                fresh_state.get("state_version", state_version) or state_version)
            cases.append(expect_rejection(
                args.base, args.token, "idempotency_key_reused",
                body=reused, expected_error="idempotency_key_reused"))

    for case in cases:
        if not case.get("ok"):
            failures.append(
                f"{case.get('case')}: HTTP {case.get('http_status')} "
                f"error={case.get('error')} "
                f"stable={case.get('stable_state_unchanged')}")

    report = {
        "cases": cases,
        "failures": failures,
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
