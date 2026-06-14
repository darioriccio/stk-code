#!/usr/bin/env python3
"""Phase 5.1 venue acceptance harness for operator + fake crew tests."""

import argparse
import json
import pathlib
import subprocess
import sys
import time
import urllib.error
import urllib.request
import uuid


ROLE_ORDER = ["navigator", "engineer", "defender", "hacker", "gunner"]


def expected_laps(players):
    if players <= 3:
        return 6
    if players == 4:
        return 8
    if players == 5:
        return 10
    return 12


def expected_turns(players):
    return expected_laps(players) // players


def int_field(mapping, key, default=0):
    value = mapping.get(key, default)
    if value is None:
        return default
    return int(value)


class AcceptanceHarness:
    def __init__(self, args):
        self.args = args
        self.root = pathlib.Path(__file__).resolve().parents[2]
        self.result_dir = pathlib.Path(args.result_dir)
        self.report_dir = self.result_dir / "reports"
        self.tests = []
        self.reset_runs = []
        self.error_tests = []
        self.handoff_tests = []
        self.failures = []
        self.operator_only_report = None

    def request(self, method, path, body=None, token=None):
        data = None
        headers = {"X-STK-Relay-Token": self.args.token if token is None else token}
        if body is not None:
            data = json.dumps(body).encode("utf-8")
            headers["Content-Type"] = "application/json"
        req = urllib.request.Request(
            self.args.base + path, data=data, headers=headers, method=method)
        try:
            with urllib.request.urlopen(req, timeout=self.args.timeout) as res:
                text = res.read().decode("utf-8")
                return res.status, json.loads(text) if text else {}
        except urllib.error.HTTPError as err:
            text = err.read().decode("utf-8")
            try:
                return err.code, json.loads(text) if text else {}
            except json.JSONDecodeError:
                return err.code, {"ok": False, "error": text}

    def state(self):
        status, payload = self.request("GET", "/immersive/v1/operator/state")
        if status != 200 or not payload.get("ok"):
            raise RuntimeError(f"state failed HTTP {status}: {payload}")
        return payload

    def check(self, ok, message):
        if not ok:
            self.failures.append(message)
        return ok

    def record_test(self, **fields):
        self.tests.append(fields)
        if fields.get("result") != "PASS" and fields.get("result") != "SKIP":
            self.failures.append(f"{fields.get('test')}: {fields.get('notes')}")

    def record_reset(self, **fields):
        self.reset_runs.append(fields)
        if fields.get("result") != "PASS":
            self.failures.append(f"reset {fields.get('run')}: {fields.get('notes')}")

    def record_error(self, case, expected, observed, passed):
        row = {
            "case": case,
            "expected_error": expected,
            "observed": observed,
            "result": "PASS" if passed else "FAIL",
        }
        self.error_tests.append(row)
        if not passed:
            self.failures.append(
                f"error {case}: expected {expected}, observed {observed}")

    def session_id(self, label):
        safe = "".join(c if c.isalnum() else "-" for c in label.lower())
        return f"phase51-{self.args.suite}-{safe}-{int(time.time() * 1000)}"

    def players(self, count):
        return [
            {"player_id": f"p{i}", "display_name": f"Player {i}"}
            for i in range(1, count + 1)
        ]

    def configure(self, players, session_id, output=None,
                  require_handoff_confirm=False):
        body = {
            "session_id": session_id,
            "player_count": players,
            "track_id": self.args.track,
            "kart_id": self.args.kart,
            "difficulty": "easy",
            "ai_karts": max(0, players - 1),
            "lap_policy": "player_count_based",
            "handoff_policy": "lap_based_handoff",
            "crew_mode": "enabled",
            "encounter_script": f"{self.args.track}_relay_encounters",
            "render_profile": self.args.render_profile,
            "output": output or self.args.output,
            "projection": "fisheye_domemaster",
            "operator_mode": "arcade",
            "allow_pause": True,
            "allow_force_finish": True,
            "allow_abort": True,
            "handoff_operator_confirm": require_handoff_confirm,
            "auto_confirm_handoff": not require_handoff_confirm,
            "briefing_seconds": 0,
            "countdown_seconds": 0,
            "test_accelerated_progress": True,
            "players": self.players(players),
        }
        status, payload = self.request(
            "POST", "/immersive/v1/operator/configure", body)
        if status != 200 or not payload.get("ok"):
            raise RuntimeError(f"configure failed HTTP {status}: {payload}")
        return payload

    def start(self):
        status, payload = self.request(
            "POST", "/immersive/v1/operator/start", {})
        if status != 200 or not payload.get("ok"):
            raise RuntimeError(f"start failed HTTP {status}: {payload}")
        status, payload = self.request(
            "POST", "/immersive/v1/operator/countdown", {"skip": True})
        if status != 200 or not payload.get("ok"):
            raise RuntimeError(f"countdown failed HTTP {status}: {payload}")
        return payload

    def reset(self):
        status, payload = self.request(
            "POST", "/immersive/v1/operator/reset", {})
        if status != 200 or not payload.get("ok"):
            raise RuntimeError(f"reset failed HTTP {status}: {payload}")
        return payload

    def legacy_reset_to_attract(self):
        status, payload = self.request(
            "POST", "/immersive/v1/session/reset", {})
        if status != 200 or not payload.get("ok"):
            raise RuntimeError(f"legacy reset failed HTTP {status}: {payload}")
        return payload

    def progress(self, lap, progress):
        status, payload = self.request(
            "POST", "/immersive/v1/test/progress",
            {"lap": lap, "progress_x1000": int(progress * 1000),
             "checkpoint": -1})
        if status != 200 or not payload.get("ok"):
            raise RuntimeError(f"progress failed HTTP {status}: {payload}")
        return payload

    def payload_for_action(self, action):
        payload = {}
        if action.get("target_key"):
            payload[action["target_key"]] = action.get("target_id", "")
        if action.get("mode_key"):
            payload[action["mode_key"]] = action.get("mode", "")
        if int(action.get("amount", 0) or 0) > 0:
            payload["amount"] = int(action.get("amount", 0) or 0)
        return payload

    def one_fake_phone_action(self):
        state = self.state()
        for phone in state.get("phone_states", []):
            for action in phone.get("actions", []):
                if not action.get("available"):
                    continue
                payload = self.payload_for_action(action)
                body = {
                    "session_id": state.get("session_id", ""),
                    "action_id": str(uuid.uuid4()),
                    "idempotency_key": str(uuid.uuid4()),
                    "state_version_seen": state.get("state_version", 0),
                    "player_id": phone.get("player_id", ""),
                    "role": phone.get("role", ""),
                    "type": action.get("type", ""),
                    "payload": payload,
                }
                body.update(payload)
                status, response = self.request(
                    "POST", "/immersive/v1/crew/action", body)
                return status == 200 and response.get("ok"), response
        return False, {"error": "no_available_fake_phone_action"}

    def run_fake_phones(self, players, mode, silent_role=""):
        cmd = [
            sys.executable,
            str(self.root / "tools" / "immersive" / "FakeCrewPhones.py"),
            "--base", self.args.base,
            "--token", self.args.token,
            "--players", str(players),
            "--mode", mode,
            "--delay", str(self.args.fake_delay),
            "--no-start",
        ]
        if mode == "silent-role":
            cmd.extend(["--silent-role", silent_role or "hacker"])
        proc = subprocess.run(cmd, text=True, capture_output=True, check=False)
        try:
            payload = json.loads(proc.stdout)
        except json.JSONDecodeError:
            payload = {
                "failures": [f"non_json_fake_phones rc={proc.returncode}"],
                "stdout": proc.stdout,
                "stderr": proc.stderr,
            }
        payload["returncode"] = proc.returncode
        if proc.stderr:
            payload["stderr"] = proc.stderr
        return payload

    def report_path_exists(self, report):
        if not report or not report.get("session_id"):
            return False
        return (self.report_dir / f"{report['session_id']}.json").exists()

    def validate_reset_clean(self, players, previous_session_id=None):
        self.reset()
        state = self.state()
        arcade = state.get("arcade_session", {})
        clean = True
        checks = [
            arcade.get("state") == "ready_for_players",
            arcade.get("report") is None,
            int_field(state, "crew_score", -1) == 0,
            int_field(state, "crew_actions_accepted", -1) == 0,
            int_field(state, "encounters_started", -1) == 0,
            int_field(arcade, "current_lap", 0) == 1,
            int_field(arcade, "total_laps", 0) == expected_laps(players),
        ]
        clean = all(checks)
        return clean, arcade

    def validate_full_report(self, players, state, fake_report,
                             require_score=True, silent_role=""):
        arcade = state.get("arcade_session", {})
        report = arcade.get("report") or {}
        notes = []
        if arcade.get("state") != "results":
            notes.append(f"arcade_state={arcade.get('state')}")
        if int(arcade.get("total_laps", 0) or 0) != expected_laps(players):
            notes.append(f"total_laps={arcade.get('total_laps')}")
        if not report:
            notes.append("missing report")
            return False, report, notes
        if report.get("session_id") != arcade.get("session_id"):
            notes.append("report session_id mismatch")
        if int(report.get("completed_laps", 0) or 0) != expected_laps(players):
            notes.append(f"completed_laps={report.get('completed_laps')}")
        turns = report.get("driver_turns_by_player", {})
        for i in range(1, players + 1):
            if int(turns.get(f"p{i}", 0) or 0) != expected_turns(players):
                notes.append(f"turns p{i}={turns.get(f'p{i}')}")
        if require_score and int(report.get("score", 0) or 0) <= 0:
            notes.append("score not positive")
        if require_score and int(report.get("crew_actions_accepted", 0) or 0) <= 0:
            notes.append("crew_actions_accepted not positive")
        if require_score and not report.get("crew_actions_enabled"):
            notes.append("crew_actions_enabled false")
        if int(report.get("encounters_started", 0) or 0) <= 0:
            notes.append("encounters_started not positive")
        if not str(report.get("final_result", "")).startswith("finished_"):
            notes.append(f"final_result={report.get('final_result')}")
        if report.get("projection") != "fisheye_domemaster":
            notes.append(f"projection={report.get('projection')}")
        if report.get("output") != self.args.output:
            notes.append(f"output={report.get('output')}")
        if not self.report_path_exists(report):
            notes.append("report file missing")
        if fake_report.get("failures"):
            notes.append(f"fake phones failures={fake_report.get('failures')}")
        if fake_report.get("returncode", 0) != 0:
            notes.append(f"fake phones rc={fake_report.get('returncode')}")
        if silent_role:
            actions = report.get("actions_by_role", {})
            if int(actions.get(silent_role, 0) or 0) != 0:
                notes.append(f"silent role {silent_role} acted")
        return not notes, report, notes

    def run_integrated_full(self, label, players, mode="perfect",
                            silent_role="", require_score=True):
        session_id = self.session_id(label)
        try:
            self.configure(players, session_id)
            self.start()
            fake = self.run_fake_phones(players, mode, silent_role)
            state = self.state()
            ok, report, notes = self.validate_full_report(
                players, state, fake, require_score, silent_role)
            reset_ok, reset_state = self.validate_reset_clean(players,
                                                              session_id)
            ok = ok and reset_ok
            if not reset_ok:
                notes.append("reset state not clean")
            self.record_test(
                test=label,
                players=players,
                fake_phones_mode=mode if not silent_role else f"{mode}:{silent_role}",
                syphon=self.args.output == "syphon",
                result="PASS" if ok else "FAIL",
                score=int(report.get("score", 0) or 0) if report else 0,
                encounters_ok=int(report.get("encounters_succeeded", 0) or 0) if report else 0,
                encounters_fail=int(report.get("encounters_failed", 0) or 0) if report else 0,
                reset_ok=reset_ok,
                session_id=session_id,
                report=report,
                notes="; ".join(notes) if notes else "ok")
        except Exception as exc:
            self.record_test(
                test=label, players=players, fake_phones_mode=mode,
                syphon=self.args.output == "syphon", result="FAIL",
                score=0, encounters_ok=0, encounters_fail=0,
                reset_ok=False, notes=str(exc))

    def run_short_reset_session(self, label, players, action):
        session_id = self.session_id(label)
        changed = True
        clean = False
        notes = []
        try:
            before = self.state().get("arcade_session", {}).get("session_id")
            self.configure(players, session_id)
            self.start()
            if action == "partial_fake_phones":
                self.progress(1, 0.16)
                acted, ack = self.one_fake_phone_action()
                if not acted:
                    notes.append(f"fake phone action failed: {ack}")
                self.reset()
            elif action == "force_finish":
                status, payload = self.request(
                    "POST", "/immersive/v1/operator/force-finish", {})
                if status != 200 or not payload.get("ok"):
                    notes.append(f"force finish HTTP {status} {payload}")
                self.reset()
            elif action == "abort":
                status, payload = self.request(
                    "POST", "/immersive/v1/operator/abort", {})
                if status != 200 or not payload.get("ok"):
                    notes.append(f"abort HTTP {status} {payload}")
                self.reset()
            else:
                notes.append(f"unknown action {action}")
            state = self.state()
            arcade = state.get("arcade_session", {})
            changed = before != session_id
            clean = (
                arcade.get("report") is None and
                int_field(state, "crew_score", -1) == 0 and
                int_field(state, "crew_actions_accepted", -1) == 0 and
                int_field(arcade, "current_lap", 0) == 1 and
                int_field(arcade, "total_laps", 0) == expected_laps(players)
            )
        except Exception as exc:
            notes.append(str(exc))
        self.record_reset(
            run=label,
            players=players,
            action=action,
            result="PASS" if clean and changed and not notes else "FAIL",
            session_id_changed=changed,
            state_clean=clean,
            notes="; ".join(notes) if notes else "ok")

    def run_reset_loops(self):
        for i in range(1, 4):
            self.run_short_reset_session(
                f"reset_loop_fake_phones_{i}", 6, "partial_fake_phones")
        if self.args.suite != "syphon":
            self.run_short_reset_session(
                "mixed_reset_6p_partial_fake_phones", 6,
                "partial_fake_phones")
            self.run_integrated_full(
                "mixed_reset_6p_full_fake_phones", 6, "perfect")
            self.run_short_reset_session(
                "mixed_reset_4p_force_finish", 4, "force_finish")
            self.run_short_reset_session(
                "mixed_reset_2p_abort", 2, "abort")

    def run_handoff_test(self):
        label = "handoff_confirm_one_wheel"
        notes = []
        result = "FAIL"
        try:
            self.configure(6, self.session_id(label),
                           require_handoff_confirm=True)
            self.start()
            status, payload = self.request(
                "POST", "/immersive/v1/operator/handoff", {})
            if status != 200 or not payload.get("ok"):
                notes.append(f"handoff HTTP {status} {payload}")
            state = self.state()
            arcade = state.get("arcade_session", {})
            if not arcade.get("handoff_active"):
                notes.append("handoff_active false")
            if not arcade.get("waiting_for_operator_confirm"):
                notes.append("waiting_for_operator_confirm false")
            if not arcade.get("input_locked"):
                notes.append("input_locked false")
            if arcade.get("next_driver") != 2:
                notes.append(f"next_driver={arcade.get('next_driver')}")
            status, confirm = self.request(
                "POST", "/immersive/v1/operator/handoff/confirm", {})
            if status != 200 or not confirm.get("ok"):
                notes.append(f"confirm HTTP {status} {confirm}")
            result = "PASS" if not notes else "FAIL"
            self.reset()
        except Exception as exc:
            notes.append(str(exc))
        self.handoff_tests.append({
            "players": 6,
            "total_laps": 12,
            "handoff_count": 1 if result == "PASS" else 0,
            "confirm_mode": "operator_confirm",
            "result": result,
            "notes": "; ".join(notes) if notes else "ok",
        })
        if result != "PASS":
            self.failures.append(f"{label}: {'; '.join(notes)}")

    def expect_error(self, case, method, path, body, expected,
                     token=None, expected_status_min=400):
        status, payload = self.request(method, path, body, token=token)
        observed = payload.get("error", f"http_{status}")
        passed = status >= expected_status_min and observed == expected
        self.record_error(case, expected, observed, passed)

    def run_error_recovery(self):
        self.expect_error(
            "invalid token", "GET", "/immersive/v1/operator/state", None,
            "invalid_token", token="wrong-token")
        bad_config = {
            "session_id": self.session_id("invalid-player-count"),
            "player_count": 7,
            "track_id": self.args.track,
        }
        self.expect_error(
            "invalid player_count", "POST",
            "/immersive/v1/operator/configure", bad_config,
            "invalid_player_count")
        self.request("POST", "/immersive/v1/session/reset", {})
        self.expect_error(
            "start without configure", "POST",
            "/immersive/v1/operator/start", {}, "session_not_configured")
        self.expect_error(
            "pause outside running", "POST",
            "/immersive/v1/operator/pause", {}, "wrong_state")
        self.expect_error(
            "resume outside paused", "POST",
            "/immersive/v1/operator/resume", {}, "not_paused")
        self.configure(6, self.session_id("reset-during-running"))
        self.start()
        try:
            clean, _ = self.validate_reset_clean(6)
            self.record_error(
                "reset during running", "ok", "ok" if clean else "dirty",
                clean)
        except Exception as exc:
            self.record_error("reset during running", "ok", str(exc), False)
        self.configure(6, self.session_id("reset-during-handoff"),
                       require_handoff_confirm=True)
        self.start()
        self.request("POST", "/immersive/v1/operator/handoff", {})
        try:
            clean, _ = self.validate_reset_clean(6)
            self.record_error(
                "reset during handoff", "ok", "ok" if clean else "dirty",
                clean)
        except Exception as exc:
            self.record_error("reset during handoff", "ok", str(exc), False)
        self.configure(6, self.session_id("force-finish-running"))
        self.start()
        status, payload = self.request(
            "POST", "/immersive/v1/operator/force-finish", {})
        self.record_error(
            "force-finish during running", "ok",
            "ok" if status == 200 and payload.get("ok") else payload,
            status == 200 and payload.get("ok"))
        self.reset()
        self.configure(6, self.session_id("abort-running"))
        self.start()
        status, payload = self.request(
            "POST", "/immersive/v1/operator/abort", {})
        self.record_error(
            "abort during running", "ok",
            "ok" if status == 200 and payload.get("ok") else payload,
            status == 200 and payload.get("ok"))
        self.reset()
        self.expect_error(
            "invalid render preset", "POST",
            "/immersive/v1/operator/render-preset",
            {"render_profile": "ultra"}, "invalid_render_preset")
        self.expect_error(
            "projection unavailable", "POST",
            "/immersive/v1/render/config",
            {"projection": {"type": "equirectangular",
                            "width": 2048, "height": 1024},
             "output": {"type": "window"}},
            "immersive_projection_unavailable:equirectangular")
        if self.args.expect_syphon_unavailable:
            self.expect_error(
                "Syphon unavailable", "POST",
                "/immersive/v1/operator/render-preset",
                {"render_profile": "safe", "output": "syphon"},
                "immersive_output_unavailable:syphon")
        self.configure(6, self.session_id("fake-phones-disconnected"))
        self.start()
        status, payload = self.request(
            "POST", "/immersive/v1/operator/force-finish", {})
        state = self.state()
        report = state.get("arcade_session", {}).get("report") or {}
        disconnected_ok = (
            status == 200 and payload.get("ok") and
            int_field(report, "score", -1) == 0 and
            not report.get("crew_actions_enabled") and
            int_field(report, "crew_actions_accepted", -1) == 0)
        self.operator_only_report = report
        self.record_error(
            "FakeCrewPhones disconnected", "operator-only report",
            "operator-only report" if disconnected_ok else report,
            disconnected_ok)
        self.reset()

    def run_no_syphon_suite(self):
        self.legacy_reset_to_attract()
        self.run_integrated_full("A_2p_full_fake_phones", 2, "perfect")
        self.run_integrated_full("B_4p_full_fake_phones", 4, "perfect")
        self.run_integrated_full("C_6p_full_fake_phones", 6, "perfect")
        self.run_integrated_full("D_6p_full_fake_phones_mistakes", 6,
                                 "mistakes")
        self.run_integrated_full("E_6p_silent_role_hacker", 6,
                                 "silent-role", "hacker")
        self.run_reset_loops()
        self.run_handoff_test()
        self.run_error_recovery()

    def run_syphon_suite(self):
        self.reset()
        self.run_integrated_full("F_6p_full_fake_phones_syphon", 6,
                                 "perfect")
        self.run_reset_loops()

    def summary(self):
        real_reports = [
            t.get("report") for t in self.tests
            if t.get("result") == "PASS" and t.get("report")
        ]
        operator_only_report = None
        for row in self.error_tests:
            if row["case"] == "FakeCrewPhones disconnected":
                operator_only_report = row["observed"]
        return {
            "ok": not self.failures,
            "suite": self.args.suite,
            "output": self.args.output,
            "tests": self.tests,
            "reset_loop": self.reset_runs,
            "handoff": self.handoff_tests,
            "error_recovery": self.error_tests,
            "example_crew_report": real_reports[0] if real_reports else None,
            "example_operator_only_report": self.operator_only_report
                or (operator_only_report
                    if isinstance(operator_only_report, dict) else None),
            "failures": self.failures,
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True)
    parser.add_argument("--token", required=True)
    parser.add_argument("--result-dir", required=True)
    parser.add_argument("--suite", choices=["no-syphon", "syphon"],
                        default="no-syphon")
    parser.add_argument("--output", default="none")
    parser.add_argument("--track", default="snowtuxpeak")
    parser.add_argument("--kart", default="tux")
    parser.add_argument("--render-profile", default="fulldome_safe")
    parser.add_argument("--fake-delay", type=float, default=0.28)
    parser.add_argument("--timeout", type=float, default=6.0)
    parser.add_argument("--expect-syphon-unavailable", action="store_true")
    args = parser.parse_args()

    harness = AcceptanceHarness(args)
    try:
        if args.suite == "syphon":
            harness.run_syphon_suite()
        else:
            harness.run_no_syphon_suite()
    except Exception as exc:
        harness.failures.append(f"fatal: {exc}")
    result = harness.summary()
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
