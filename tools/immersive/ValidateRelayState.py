#!/usr/bin/env python3
import argparse
import json
import math
import sys
import urllib.error
import urllib.parse
import urllib.request


REQUIRED_FIELDS = [
    "state_version",
    "relay_enabled",
    "race_attached",
    "active_driver",
    "handoff_policy",
    "crew_score",
    "crew_multiplier",
    "crew_heat",
    "crew_armor",
    "boost_charge",
    "role_assignments",
    "phone_states",
    "track_effects",
    "cooldowns",
    "last_accepted_actions",
    "last_rejected_actions",
    "adaptive_difficulty_enabled",
    "adaptive_difficulty_profile",
    "adaptive_difficulty_intensity",
    "adaptive_pressure",
    "human_gap_ms",
    "human_position",
    "assist_level",
    "ai_pace_level",
    "leaderboard_bucket",
    "adaptive_difficulty",
]


def fetch_state(base_url, token, player_id="", role=""):
    query = []
    if player_id:
        query.append(("player_id", player_id))
    if role:
        query.append(("role", role))
    path = "/immersive/v1/state"
    if query:
        path += "?" + urllib.parse.urlencode(query)
    req = urllib.request.Request(
        base_url + path, headers={"X-STK-Relay-Token": token}, method="GET")
    with urllib.request.urlopen(req, timeout=5) as response:
        return response.status, json.loads(response.read().decode("utf-8"))


def find_bad_numbers(value, path="$"):
    errors = []
    if isinstance(value, float) and (math.isnan(value) or math.isinf(value)):
        return [path]
    if isinstance(value, dict):
        for key, child in value.items():
            errors.extend(find_bad_numbers(child, f"{path}.{key}"))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            errors.extend(find_bad_numbers(child, f"{path}[{index}]"))
    return errors


def validate_state(state, player_id="", role=""):
    errors = []
    if not state.get("ok"):
        errors.append("state ok=false")
    for field in REQUIRED_FIELDS:
        if field not in state:
            errors.append(f"missing field: {field}")
    if not isinstance(state.get("phone_states", []), list):
        errors.append("phone_states must be an array")
    adaptive = state.get("adaptive_difficulty", {})
    if not isinstance(adaptive, dict):
        errors.append("adaptive_difficulty must be an object")
    else:
        for field in ("enabled", "profile", "intensity", "pressure",
                      "assist_level", "ai_pace_level", "leaderboard_bucket"):
            if field not in adaptive:
                errors.append(f"adaptive_difficulty missing {field}")
        if adaptive.get("enabled") is True and \
                state.get("leaderboard_bucket") != "arcade_adaptive":
            errors.append("adaptive leaderboard bucket mismatch")
    for phone in state.get("phone_states", []):
        for field in ("player_id", "role", "actions"):
            if field not in phone:
                errors.append(f"phone_state missing {field}")
        if player_id and phone.get("player_id") != player_id:
            errors.append(
                f"phone_state player filter mismatch: {phone.get('player_id')}")
        if role and phone.get("role") != role:
            errors.append(f"phone_state role filter mismatch: {phone.get('role')}")
        for action in phone.get("actions", []):
            for field in ("type", "available", "cooldown_ms", "unavailable_reason"):
                if field not in action:
                    errors.append(f"phone action missing {field}")
    if (player_id or role) and not state.get("phone_states", []):
        errors.append("filtered phone_states is empty")
    errors.extend(f"bad number: {path}" for path in find_bad_numbers(state))
    return errors


def main():
    parser = argparse.ArgumentParser(
        description="Validate Immersive Relay /state minimum contract.")
    parser.add_argument("--base", default="http://127.0.0.1:8890")
    parser.add_argument("--token", default="immersive-dev-token")
    parser.add_argument("--player-id", default="")
    parser.add_argument("--role", default="")
    args = parser.parse_args()

    try:
        status, state = fetch_state(args.base, args.token, args.player_id, args.role)
    except urllib.error.HTTPError as error:
        print(error.read().decode("utf-8"))
        return 1
    except Exception as exc:
        print(f"state fetch failed: {exc}")
        return 1

    errors = validate_state(state, args.player_id, args.role)
    print(json.dumps({
        "http_status": status,
        "state_version": state.get("state_version"),
        "phone_states": len(state.get("phone_states", [])),
        "errors": errors,
    }, indent=2, sort_keys=True))
    return 1 if status != 200 or errors else 0


if __name__ == "__main__":
    sys.exit(main())
