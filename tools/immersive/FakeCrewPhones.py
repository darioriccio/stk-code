#!/usr/bin/env python3
"""Phase 3 Immersive Relay fake crew phones.

This remains a local smoke-test harness. Production ownership stays:

    Phone -> Swift app/session server -> STK local API
"""

import argparse
import json
import random
import sys
import time
import urllib.error
import urllib.request
import uuid
from collections import Counter


DEFAULT_BASE_URL = "http://127.0.0.1:8890"
DEFAULT_TOKEN = "immersive-dev-token"
DEFAULT_SESSION_ID = "tholus-fake-crew-001"
ROLE_ORDER = ["navigator", "engineer", "defender", "hacker", "gunner"]
FULL_SESSION_MODES = {"perfect", "slow", "mistakes", "random", "silent-role"}


def expected_laps(players):
    if players <= 3:
        return 6
    if players == 4:
        return 8
    if players == 5:
        return 10
    return 12


class Harness:
    def __init__(self, args):
        self.args = args
        self.session_id = args.session_id
        self.state_version = 0
        self.state_versions_seen = set()
        self.accepted_count = 0
        self.rejected_count = 0
        self.rejected_by_reason = Counter()
        self.score_start = 0
        self.score_end = 0
        self.actions_by_role = Counter()
        self.no_op_accepted_count = 0
        self.duplicate_ignored_count = 0
        self.max_actions_visible_per_phone = 0
        self.max_actions_visible_per_player = 0
        self.visible_action_samples = 0
        self.visible_action_sample_count = 0
        self.combined_roles_by_player = {}
        self.failures = []
        self.last_state = {}
        self.random = random.Random(args.seed)

    def request(self, method, path, body=None, token=None):
        data = None
        headers = {"X-STK-Relay-Token": self.args.token if token is None else token}
        if body is not None:
            data = json.dumps(body).encode("utf-8")
            headers["Content-Type"] = "application/json"
        req = urllib.request.Request(
            self.args.base + path, data=data, headers=headers, method=method)
        try:
            with urllib.request.urlopen(req, timeout=self.args.timeout) as response:
                text = response.read().decode("utf-8")
                return response.status, json.loads(text) if text else {}
        except urllib.error.HTTPError as error:
            text = error.read().decode("utf-8")
            try:
                return error.code, json.loads(text) if text else {}
            except json.JSONDecodeError:
                return error.code, {"ok": False, "error": text}

    def note_state_version(self, payload):
        version = int(payload.get("state_version", 0) or 0)
        if version:
            self.state_version = version
            self.state_versions_seen.add(version)

    def start_body(self):
        players = []
        for index in range(1, self.args.players + 1):
            players.append({
                "player_id": f"p{index}",
                "display_name": f"Player {index}",
            })
        return {
            "session_id": self.session_id,
            "preset": "standard_arcade",
            "track_id": "snowtuxpeak",
            "handoff_policy": "lap_based_handoff",
            "player_count": self.args.players,
            "test_accelerated_progress": True,
            "players": players,
        }

    def state(self, label):
        status, payload = self.request("GET", "/immersive/v1/state")
        if status != 200 or not payload.get("ok"):
            self.failures.append(
                f"{label}: expected state HTTP 200, got {status} {payload}")
        self.note_state_version(payload)
        self.note_visible_load(payload)
        self.session_id = payload.get("session_id", self.session_id)
        self.last_state = payload
        return payload

    def note_visible_load(self, state):
        by_player = Counter()
        total_available = 0
        for phone_state in state.get("phone_states", []):
            available = sum(1 for action in phone_state.get("actions", [])
                            if action.get("available", False))
            total_available += available
            by_player[phone_state.get("player_id", "")] += available
            self.max_actions_visible_per_phone = max(
                self.max_actions_visible_per_phone, available)
        if by_player:
            self.max_actions_visible_per_player = max(
                self.max_actions_visible_per_player, max(by_player.values()))
        self.visible_action_samples += total_available
        self.visible_action_sample_count += 1

        for assignment in state.get("role_assignments", []):
            player_id = assignment.get("player_id", "")
            roles = assignment.get("roles", [])
            if roles:
                self.combined_roles_by_player[player_id] = roles

    def start(self):
        status, payload = self.request("GET", "/immersive/v1/health")
        if status != 200 or not payload.get("ok"):
            self.failures.append(f"health: expected HTTP 200, got {status} {payload}")
            return
        if not self.args.no_start:
            status, payload = self.request(
                "POST", "/immersive/v1/session/start", self.start_body())
            if status != 200 or not payload.get("ok"):
                self.failures.append(f"start: expected HTTP 200, got {status} {payload}")
            self.note_state_version(payload)
        state = self.state("state-before")
        self.score_start = int(state.get("crew_score", 0) or 0)
        self.score_end = self.score_start

    def progress(self, lap, progress, checkpoint=-1):
        body = {
            "lap": lap,
            "progress_x1000": int(max(0.0, min(1.0, progress)) * 1000),
            "checkpoint": checkpoint,
        }
        status, payload = self.request("POST", "/immersive/v1/test/progress", body)
        self.note_state_version(payload)
        if status != 200 or not payload.get("ok"):
            self.failures.append(
                f"progress lap={lap} progress={progress}: HTTP {status} {payload}")
        return self.state(f"progress-{lap}-{progress}")

    def payload_for_action(self, action):
        payload = {}
        if action.get("target_key"):
            payload[action["target_key"]] = action.get("target_id", "")
        if action.get("mode_key"):
            payload[action["mode_key"]] = action.get("mode", "")
        amount = int(action.get("amount", 0) or 0)
        if amount > 0:
            payload["amount"] = amount
        return payload

    def action_body(self, phone_state, action, idempotency_key=None):
        payload = self.payload_for_action(action)
        body = {
            "session_id": self.session_id,
            "action_id": str(uuid.uuid4()),
            "idempotency_key": idempotency_key or str(uuid.uuid4()),
            "state_version_seen": self.state_version,
            "player_id": phone_state["player_id"],
            "role": phone_state["role"],
            "type": action["type"],
            "payload": payload,
        }
        body.update(payload)
        return body

    def observable_signature(self, state):
        return {
            "crew_score": state.get("crew_score"),
            "crew_multiplier_x100": state.get("crew_multiplier_x100"),
            "crew_heat": state.get("crew_heat"),
            "crew_armor": state.get("crew_armor"),
            "boost_charge": state.get("boost_charge"),
            "active_route_id": state.get("active_route_id"),
            "track_effects": state.get("track_effects", []),
            "last_crew_event": state.get("last_crew_event"),
            "last_crew_effect": state.get("last_crew_effect"),
            "active_encounter": state.get("active_encounter", {}),
        }

    def available_phone_actions(self, state, include_unavailable=False):
        entries = []
        for phone_state in state.get("phone_states", []):
            role = phone_state.get("role", "")
            if self.args.silent_role and role == self.args.silent_role:
                continue
            actions = phone_state.get("actions", [])
            if not include_unavailable:
                actions = [a for a in actions if a.get("available", False)]
            if actions:
                entries.append((phone_state, actions))
        entries.sort(key=lambda item: ROLE_ORDER.index(item[0].get("role", "gunner"))
                     if item[0].get("role") in ROLE_ORDER else 99)
        return entries

    def choose_action(self, actions):
        if self.args.mode == "random":
            return self.random.choice(actions)
        if self.args.mode == "mistakes" and len(actions) > 1 and self.random.random() < 0.35:
            return actions[-1]
        return actions[0]

    def record_response(self, role, before_state, status, payload):
        self.note_state_version(payload)
        if status == 200 and payload.get("ok"):
            if payload.get("duplicate"):
                self.duplicate_ignored_count += 1
                return
            self.accepted_count += 1
            self.actions_by_role[role] += 1
            after_state = self.state("state-after-action")
            self.score_end = int(after_state.get("crew_score", self.score_end) or 0)
            if self.observable_signature(before_state) == self.observable_signature(after_state):
                self.no_op_accepted_count += 1
            return
        self.rejected_count += 1
        self.rejected_by_reason[payload.get("error", f"http_{status}")] += 1

    def perform_available_actions(self, state):
        acted = 0
        current = state
        attempted = set()
        for _ in range(12):
            entries = self.available_phone_actions(current)
            if not entries:
                break
            phone_state, actions = entries[0]
            role = phone_state.get("role", "")
            encounter_id = phone_state.get("encounter_id", "")
            action = self.choose_action(actions)
            key = (
                phone_state.get("player_id", ""), role, encounter_id,
                action.get("type", ""), action.get("target_id", ""),
                action.get("mode", "")
            )
            if key in attempted:
                break
            attempted.add(key)
            before = self.state(f"before-{role}")
            status, payload = self.request(
                "POST", "/immersive/v1/crew/action",
                self.action_body(phone_state, action))
            self.record_response(role, before, status, payload)
            acted += 1
            time.sleep(self.args.delay)
            current = self.last_state
        return acted

    def run_full_session(self):
        self.start()
        total_laps = expected_laps(self.args.players)
        points = [
            (0.10, -1), (0.16, -1), (0.28, 1), (0.38, -1),
            (0.52, -1), (0.64, -1), (0.66, -1), (0.88, -1)
        ]
        for lap in range(1, total_laps + 1):
            lap_points = points
            if lap == total_laps:
                lap_points = [
                    (0.10, -1), (0.16, -1), (0.28, 1), (0.38, -1),
                    (0.52, -1), (0.64, -1), (0.66, -1),
                    (0.79, -1), (0.86, -1), (0.88, -1)
                ]
            for progress, checkpoint in lap_points:
                if self.args.mode == "slow":
                    time.sleep(max(self.args.delay, 0.6))
                state = self.progress(lap, progress, checkpoint)
                if self.args.mode == "mistakes" and self.random.random() < 0.12:
                    continue
                self.perform_available_actions(state)
        self.progress(total_laps, 1.0, -1)

    def run_wrong_role(self):
        self.start()
        state = self.progress(1, 0.16, -1)
        entries = self.available_phone_actions(state)
        if not entries:
            self.failures.append("wrong-role: no available action")
            return
        phone_state, actions = entries[0]
        body = self.action_body(phone_state, actions[0])
        body["role"] = next(r for r in ROLE_ORDER if r != phone_state["role"])
        status, payload = self.request("POST", "/immersive/v1/crew/action", body)
        self.record_response(body["role"], state, status, payload)

    def run_wrong_window(self):
        self.start()
        state = self.progress(1, 0.10, -1)
        entries = self.available_phone_actions(state, include_unavailable=True)
        entries = [(p, a) for p, a in entries if a]
        if not entries:
            self.failures.append("wrong-window: no preview action")
            return
        phone_state, actions = entries[0]
        status, payload = self.request(
            "POST", "/immersive/v1/crew/action",
            self.action_body(phone_state, actions[0]))
        self.record_response(phone_state.get("role", ""), state, status, payload)

    def run_cooldown(self):
        self.start()
        state = self.progress(1, 0.16, -1)
        entries = self.available_phone_actions(state)
        if not entries:
            self.failures.append("cooldown: no available action")
            return
        phone_state, actions = entries[0]
        body = self.action_body(phone_state, actions[0])
        status, payload = self.request("POST", "/immersive/v1/crew/action", body)
        self.record_response(phone_state["role"], state, status, payload)
        before = self.state("cooldown-before-second")
        second = self.action_body(phone_state, actions[0])
        status, payload = self.request("POST", "/immersive/v1/crew/action", second)
        self.record_response(phone_state["role"], before, status, payload)

    def run_already_used(self):
        self.start()
        state = self.progress(1, 0.16, -1)
        entries = self.available_phone_actions(state)
        if not entries or len(entries[0][1]) < 2:
            self.failures.append("already-used: need two route actions")
            return
        phone_state, actions = entries[0]
        status, payload = self.request(
            "POST", "/immersive/v1/crew/action",
            self.action_body(phone_state, actions[0]))
        self.record_response(phone_state["role"], state, status, payload)
        self.progress(1, 0.18, -1)
        time.sleep(max(self.args.delay, 0.32))
        before = self.state("already-before-second")
        status, payload = self.request(
            "POST", "/immersive/v1/crew/action",
            self.action_body(phone_state, actions[1]))
        self.record_response(phone_state["role"], before, status, payload)

    def run_expired(self):
        self.start()
        state = self.progress(1, 0.50, -1)
        phone = {
            "player_id": "p2",
            "role": "navigator",
        }
        action = {
            "type": "nav_select_route",
            "target_key": "route_id",
            "target_id": "ridge_left",
        }
        status, payload = self.request(
            "POST", "/immersive/v1/crew/action",
            self.action_body(phone, action))
        self.record_response("navigator", state, status, payload)

    def run_stale(self):
        self.start()
        state = self.progress(1, 0.16, -1)
        entries = self.available_phone_actions(state)
        if not entries:
            self.failures.append("stale: no available action")
            return
        phone_state, actions = entries[0]
        body = self.action_body(phone_state, actions[0])
        body["state_version_seen"] = 1
        status, payload = self.request("POST", "/immersive/v1/crew/action", body)
        self.record_response(phone_state["role"], state, status, payload)

    def run_duplicate(self):
        self.start()
        state = self.progress(1, 0.16, -1)
        entries = self.available_phone_actions(state)
        if not entries:
            self.failures.append("duplicate: no available action")
            return
        phone_state, actions = entries[0]
        key = str(uuid.uuid4())
        body = self.action_body(phone_state, actions[0], key)
        status, payload = self.request("POST", "/immersive/v1/crew/action", body)
        self.record_response(phone_state["role"], state, status, payload)
        before = self.state("duplicate-before")
        status, payload = self.request("POST", "/immersive/v1/crew/action", body)
        self.record_response(phone_state["role"], before, status, payload)

    def run_invalid_token(self):
        status, payload = self.request(
            "GET", "/immersive/v1/state", token="definitely-wrong-token")
        if status != 401:
            self.failures.append(f"invalid-token: expected HTTP 401, got {status} {payload}")
            return
        self.rejected_count += 1
        self.rejected_by_reason[payload.get("error", "invalid_or_missing_token")] += 1

    def validate_expectations(self):
        state = self.state("final-state") if self.args.mode != "invalid-token" else self.last_state
        mode = self.args.mode
        if mode in FULL_SESSION_MODES:
            total_laps = int(state.get("total_laps", 0) or 0)
            if total_laps != expected_laps(self.args.players):
                self.failures.append(f"{mode}: expected total_laps={expected_laps(self.args.players)}, got {total_laps}")
            turns = state.get("pilot_turns_by_player", {})
            for index in range(1, self.args.players + 1):
                player_id = f"p{index}"
                if int(turns.get(player_id, 0) or 0) < 2:
                    self.failures.append(f"{mode}: {player_id} drove fewer than 2 laps")
            if self.accepted_count <= 0:
                self.failures.append(f"{mode}: expected accepted actions")
            if mode == "perfect" and self.rejected_count:
                self.failures.append("perfect: expected zero rejections")
            if mode == "perfect" and int(state.get("combo_count", 0) or 0) < 3:
                self.failures.append("perfect: expected three combo types")
            if not str(state.get("state", "")).startswith("finished_"):
                self.failures.append(f"{mode}: expected finished state, got {state.get('state')}")
        elif mode == "wrong-role":
            if not self.rejected_by_reason.get("wrong_role"):
                self.failures.append("wrong-role: expected wrong_role")
        elif mode == "wrong-window":
            if not self.rejected_by_reason.get("wrong_window"):
                self.failures.append("wrong-window: expected wrong_window")
        elif mode == "cooldown":
            if not self.rejected_by_reason.get("cooldown"):
                self.failures.append("cooldown: expected cooldown rejection")
        elif mode == "already-used":
            if not self.rejected_by_reason.get("already_used"):
                self.failures.append("already-used: expected already_used")
        elif mode == "expired":
            if not self.rejected_by_reason.get("encounter_expired"):
                self.failures.append("expired: expected encounter_expired")
        elif mode == "stale":
            if not self.rejected_by_reason.get("stale_state"):
                self.failures.append("stale: expected stale_state")
        elif mode == "duplicate":
            if self.duplicate_ignored_count <= 0:
                self.failures.append("duplicate: expected duplicate_ignored")
        elif mode == "invalid-token":
            if not (self.rejected_by_reason.get("invalid_token") or
                    self.rejected_by_reason.get("invalid_or_missing_token")):
                self.failures.append("invalid-token: expected invalid_token")

    def report(self):
        state = self.last_state or {}
        total_laps = int(state.get("total_laps", 0) or 0)
        final_result = state.get("state", "")
        role_idle = 0.0
        phone_states = state.get("phone_states", [])
        if phone_states:
            idle = sum(1 for p in phone_states if not p.get("actions"))
            role_idle = idle / float(len(phone_states))
        return {
            "mode": self.args.mode,
            "player_count": self.args.players,
            "total_laps": total_laps,
            "laps_completed": total_laps if str(final_result).startswith("finished_") else max(0, int(state.get("lap", 1) or 1) - 1),
            "pilot_turns_by_player": state.get("pilot_turns_by_player", {}),
            "encounters_started": state.get("encounters_started", 0),
            "encounters_succeeded": state.get("encounters_succeeded", 0),
            "encounters_failed": state.get("encounters_failed", 0),
            "actions_accepted_by_role": state.get("actions_accepted_by_role", dict(self.actions_by_role)),
            "actions_rejected_by_reason": dict(self.rejected_by_reason),
            "combo_count": state.get("combo_count", 0),
            "score_start": self.score_start,
            "score_end": int(state.get("crew_score", self.score_end) or 0),
            "no_op_accepted_count": self.no_op_accepted_count,
            "average_role_idle_estimate": role_idle,
            "max_actions_visible_per_phone": self.max_actions_visible_per_phone,
            "max_actions_visible_per_player": self.max_actions_visible_per_player,
            "average_visible_actions_per_state": (
                float(self.visible_action_samples) / self.visible_action_sample_count
                if self.visible_action_sample_count else 0.0),
            "average_actions_per_lap": (
                float(self.accepted_count + self.rejected_count) / total_laps
                if total_laps else 0.0),
            "combined_roles_by_player": self.combined_roles_by_player,
            "final_result": final_result,
            "accepted_count": self.accepted_count,
            "rejected_count": self.rejected_count,
            "state_versions_seen": sorted(self.state_versions_seen),
            "failures": self.failures,
        }


def main():
    parser = argparse.ArgumentParser(
        description="Phase 3 test harness for Immersive Relay contextual crew encounters.")
    parser.add_argument("--base", default=DEFAULT_BASE_URL)
    parser.add_argument("--token", default=DEFAULT_TOKEN)
    parser.add_argument("--session-id", default=DEFAULT_SESSION_ID)
    parser.add_argument("--players", type=int, choices=range(2, 7), default=6)
    parser.add_argument("--mode", choices=[
        "perfect", "slow", "mistakes", "random",
        "silent-role", "wrong-role", "wrong-window", "cooldown",
        "already-used", "expired", "stale", "duplicate", "invalid-token",
        "disconnect-reconnect", "spam"
    ], default="perfect")
    parser.add_argument("--silent-role", choices=ROLE_ORDER, default="")
    parser.add_argument("--no-start", action="store_true")
    parser.add_argument("--delay", type=float, default=0.35)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--seed", type=int, default=3)
    args = parser.parse_args()

    if args.mode == "silent-role" and not args.silent_role:
        parser.error("--mode silent-role requires --silent-role")

    harness = Harness(args)
    try:
        if args.mode == "invalid-token":
            harness.run_invalid_token()
        elif args.mode in FULL_SESSION_MODES:
            harness.run_full_session()
        elif args.mode == "wrong-role":
            harness.run_wrong_role()
        elif args.mode == "wrong-window":
            harness.run_wrong_window()
        elif args.mode == "cooldown" or args.mode == "spam":
            harness.run_cooldown()
        elif args.mode == "already-used":
            harness.run_already_used()
        elif args.mode == "expired":
            harness.run_expired()
        elif args.mode == "stale":
            harness.run_stale()
        elif args.mode == "duplicate":
            harness.run_duplicate()
        elif args.mode == "disconnect-reconnect":
            harness.start()
            time.sleep(max(args.delay, 1.0))
            harness.run_full_session()
        harness.validate_expectations()
    except Exception as exc:
        harness.failures.append(f"exception: {exc}")

    report = harness.report()
    print(json.dumps(report, indent=2, sort_keys=True))
    return 1 if report["failures"] else 0


if __name__ == "__main__":
    sys.exit(main())
