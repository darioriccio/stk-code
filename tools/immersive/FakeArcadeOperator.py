#!/usr/bin/env python3
"""Phase 5 fake arcade operator for local venue-session smoke tests."""

import argparse
import json
import subprocess
import sys
import time
import urllib.error
import urllib.request


DEFAULT_BASE = "http://127.0.0.1:8890"
DEFAULT_TOKEN = "immersive-dev-token"


def expected_laps(players):
    if players <= 3:
        return 6
    if players == 4:
        return 8
    if players == 5:
        return 10
    return 12


class Operator:
    def __init__(self, args):
        self.args = args
        self.failures = []

    def request(self, method, path, body=None, token=None):
        data = None
        headers = {"X-STK-Relay-Token": token or self.args.token}
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

    def expect_ok(self, label, method, path, body=None):
        status, payload = self.request(method, path, body)
        if status != 200 or not payload.get("ok"):
            self.failures.append(f"{label}: HTTP {status} {payload}")
        return payload

    def expect_error(self, label, method, path, body, error):
        status, payload = self.request(method, path, body)
        if status < 400 or payload.get("error") != error:
            self.failures.append(
                f"{label}: expected {error}, got HTTP {status} {payload}")
        return payload

    def players(self):
        return [
            {"player_id": f"p{i}", "display_name": f"Player {i}"}
            for i in range(1, self.args.players + 1)
        ]

    def config_body(self, session_id=None):
        return {
            "session_id": session_id or f"phase5-arcade-{int(time.time() * 1000)}",
            "player_count": self.args.players,
            "track_id": self.args.track,
            "kart_id": self.args.kart,
            "difficulty": "easy",
            "ai_karts": max(0, self.args.players - 1),
            "lap_policy": "player_count_based",
            "handoff_policy": "lap_based_handoff",
            "crew_mode": "enabled",
            "encounter_script": f"{self.args.track}_relay_encounters",
            "render_profile": self.args.render_profile,
            "output": self.args.output,
            "projection": "fisheye_domemaster",
            "operator_mode": "arcade",
            "allow_pause": True,
            "allow_force_finish": True,
            "allow_abort": True,
            "handoff_operator_confirm": self.args.operator_confirm_handoff,
            "auto_confirm_handoff": not self.args.operator_confirm_handoff,
            "briefing_seconds": 0 if self.args.fast else 6,
            "countdown_seconds": 0 if self.args.fast else 3,
            "test_accelerated_progress": True,
            "players": self.players(),
        }

    def state(self):
        return self.expect_ok("state", "GET", "/immersive/v1/operator/state")

    def configure(self, session_id=None):
        return self.expect_ok(
            "configure", "POST", "/immersive/v1/operator/configure",
            self.config_body(session_id))

    def start(self):
        self.expect_ok("start", "POST", "/immersive/v1/operator/start", {})
        self.expect_ok(
            "countdown", "POST", "/immersive/v1/operator/countdown",
            {"skip": True})

    def reset(self):
        return self.expect_ok("reset", "POST", "/immersive/v1/operator/reset", {})

    def progress(self, lap, progress):
        return self.expect_ok(
            "progress", "POST", "/immersive/v1/test/progress",
            {
                "lap": lap,
                "progress_x1000": int(progress * 1000),
                "checkpoint": -1,
            })

    def finish_session(self):
        total = expected_laps(self.args.players)
        for lap in range(1, total + 1):
            for progress in (0.16, 0.52, 0.88):
                self.progress(lap, progress)
        self.progress(total, 1.0)
        state = self.state()
        arcade = state.get("arcade_session", {})
        if arcade.get("state") != "results":
            self.failures.append(
                f"finish: expected results, got {arcade.get('state')}")
        if arcade.get("total_laps") != total:
            self.failures.append(
                f"finish: expected total_laps={total}, got {arcade.get('total_laps')}")
        return state

    def run_session(self):
        self.configure()
        self.start()
        if self.args.with_fake_phones:
            cmd = [
                sys.executable,
                __file__.replace("FakeArcadeOperator.py", "FakeCrewPhones.py"),
                "--base", self.args.base,
                "--token", self.args.token,
                "--players", str(self.args.players),
                "--mode", "perfect",
                "--delay", str(self.args.fake_phone_delay),
                "--no-start",
            ]
            subprocess.run(cmd, check=False)
        return self.finish_session()

    def reset_loop(self, count):
        sessions = []
        for i in range(count):
            self.configure(f"phase5-reset-loop-{i + 1}-{int(time.time() * 1000)}")
            self.start()
            state = self.finish_session()
            sessions.append(state.get("arcade_session", {}).get("session_id"))
            self.reset()
        if len(set(sessions)) != len(sessions):
            self.failures.append(f"reset-loop: duplicate session ids {sessions}")
        return sessions

    def pause_resume_test(self):
        self.configure()
        self.start()
        self.expect_ok("pause", "POST", "/immersive/v1/operator/pause", {})
        paused = self.state().get("arcade_session", {}).get("state")
        if paused != "paused":
            self.failures.append(f"pause: expected paused, got {paused}")
        self.expect_ok("resume", "POST", "/immersive/v1/operator/resume", {})
        resumed = self.state().get("arcade_session", {}).get("state")
        if resumed not in ("running", "final_lap", "lap_handoff"):
            self.failures.append(f"resume: unexpected state {resumed}")
        self.reset()

    def abort_test(self):
        self.configure()
        self.start()
        self.expect_ok("abort", "POST", "/immersive/v1/operator/abort", {})
        state = self.state().get("arcade_session", {})
        if state.get("state") != "results":
            self.failures.append(f"abort: expected results, got {state.get('state')}")
        self.reset()

    def force_finish_test(self):
        self.configure()
        self.start()
        self.progress(1, 0.25)
        self.expect_ok(
            "force-finish", "POST", "/immersive/v1/operator/force-finish", {})
        state = self.state().get("arcade_session", {})
        if state.get("state") != "results":
            self.failures.append(
                f"force-finish: expected results, got {state.get('state')}")
        self.reset()

    def handoff_confirm_test(self):
        self.configure()
        self.start()
        self.expect_ok("force-handoff", "POST", "/immersive/v1/operator/handoff", {})
        active = self.state().get("arcade_session", {})
        if not active.get("handoff_active"):
            self.failures.append("handoff: expected handoff_active")
        self.expect_ok(
            "handoff-confirm", "POST",
            "/immersive/v1/operator/handoff/confirm", {})
        self.reset()

    def reset_during_handoff_test(self):
        self.configure()
        self.start()
        self.expect_ok("force-handoff", "POST", "/immersive/v1/operator/handoff", {})
        self.reset()
        state = self.state().get("arcade_session", {})
        if state.get("state") != "ready_for_players":
            self.failures.append(
                f"reset-during-handoff: expected ready_for_players, got {state.get('state')}")

    def invalid_test(self):
        self.expect_error(
            "invalid-player-count", "POST",
            "/immersive/v1/operator/configure",
            dict(self.config_body(), player_count=7),
            "invalid_player_count")
        self.expect_error(
            "resume-not-paused", "POST",
            "/immersive/v1/operator/resume", {}, "not_paused")

    def report(self, extra=None):
        state = self.state()
        return {
            "ok": not self.failures,
            "mode": extra or "operator",
            "player_count": self.args.players,
            "arcade_session": state.get("arcade_session", {}),
            "failures": self.failures,
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default=DEFAULT_BASE)
    parser.add_argument("--token", default=DEFAULT_TOKEN)
    parser.add_argument("--players", type=int, choices=range(2, 7), default=6)
    parser.add_argument("--track", default="snowtuxpeak")
    parser.add_argument("--kart", default="tux")
    parser.add_argument("--render-profile", default="fulldome_safe")
    parser.add_argument("--output", default="window")
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--fake-phone-delay", type=float, default=0.05)
    parser.add_argument("--fast", action="store_true")
    parser.add_argument("--operator-confirm-handoff", action="store_true")
    parser.add_argument("--configure", action="store_true")
    parser.add_argument("--start", action="store_true")
    parser.add_argument("--reset", action="store_true")
    parser.add_argument("--poll-state", action="store_true")
    parser.add_argument("--run-session", action="store_true")
    parser.add_argument("--with-fake-phones", action="store_true")
    parser.add_argument("--pause-resume-test", action="store_true")
    parser.add_argument("--abort-test", action="store_true")
    parser.add_argument("--force-finish-test", action="store_true")
    parser.add_argument("--handoff-test", action="store_true")
    parser.add_argument("--reset-during-handoff-test", action="store_true")
    parser.add_argument("--invalid-test", action="store_true")
    parser.add_argument("--reset-loop", type=int, default=0)
    args = parser.parse_args()

    op = Operator(args)
    try:
        if args.poll_state:
            print(json.dumps(op.state(), indent=2, sort_keys=True))
            return 0
        if args.configure:
            op.configure()
        if args.start:
            op.start()
        if args.reset:
            op.reset()
        mode = "operator"
        if args.run_session:
            op.run_session()
            mode = "run-session"
        if args.reset_loop:
            op.reset_loop(args.reset_loop)
            mode = f"reset-loop-{args.reset_loop}"
        if args.pause_resume_test:
            op.pause_resume_test()
            mode = "pause-resume"
        if args.abort_test:
            op.abort_test()
            mode = "abort"
        if args.force_finish_test:
            op.force_finish_test()
            mode = "force-finish"
        if args.handoff_test:
            op.handoff_confirm_test()
            mode = "handoff"
        if args.reset_during_handoff_test:
            op.reset_during_handoff_test()
            mode = "reset-during-handoff"
        if args.invalid_test:
            op.invalid_test()
            mode = "invalid"
        print(json.dumps(op.report(mode), indent=2, sort_keys=True))
    except Exception as exc:
        op.failures.append(f"exception: {exc}")
        print(json.dumps(op.report("exception"), indent=2, sort_keys=True))
    return 1 if op.failures else 0


if __name__ == "__main__":
    sys.exit(main())
