#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_DIR"

TOKEN="${TOKEN:-immersive-dev-token}"
PORT="${PORT:-8890}"
BASE="${BASE:-http://127.0.0.1:$PORT}"
JOBS="${JOBS:-8}"
SKIP_BUILDS="${SKIP_BUILDS:-0}"
RUN_SYPHON="${RUN_SYPHON:-0}"
RUN_SYPHON_RECEIVER="${RUN_SYPHON_RECEIVER:-1}"
RESULT_DIR="${RESULT_DIR:-/tmp/stk_tholustos_integration_smoke}"
RELAY_BUILD="${RELAY_BUILD:-cmake_build_relay_phase1}"
SYPHON_BUILD="${SYPHON_BUILD:-cmake_build_phase45_syphon}"
SERVER_NAME="${SERVER_NAME:-SuperTuxKart Immersive Relay Domemaster}"
if [[ -z "${SAFE_OUTPUT:-}" ]]; then
    SAFE_OUTPUT="window"
    if [[ "$RUN_SYPHON" == "1" ]]; then
        SAFE_OUTPUT="syphon"
    fi
fi

mkdir -p "$RESULT_DIR"

log() {
    echo "== $*"
}

build_dir() {
    local dir="$1"
    shift
    if [[ ! -f "$dir/CMakeCache.txt" ]]; then
        cmake -S "$REPO_DIR" -B "$dir" "$@"
    else
        cmake -S "$REPO_DIR" -B "$dir" "$@" >/dev/null
    fi
    if [[ "$SKIP_BUILDS" != "1" ]]; then
        cmake --build "$dir" --target supertuxkart --parallel "$JOBS"
    else
        echo "SKIPPED build $dir"
    fi
}

cleanup_pid() {
    local pid="${1:-}"
    if [[ -n "$pid" ]] && kill -0 "$pid" >/dev/null 2>&1; then
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
    fi
}

cleanup() {
    cleanup_pid "${STK_PID:-}"
}
trap cleanup EXIT

curl_get() {
    local path="$1"
    local out="$2"
    curl -fsS -H "X-STK-Relay-Token: $TOKEN" \
        "$BASE$path" >"$out"
}

curl_post() {
    local path="$1"
    local body="$2"
    local out="$3"
    curl -fsS -X POST "$BASE$path" \
        -H "X-STK-Relay-Token: $TOKEN" \
        -H "Content-Type: application/json" \
        --data-binary "@$body" >"$out"
}

curl_post_code() {
    local path="$1"
    local body="$2"
    local out="$3"
    curl -sS -o "$out" -w '%{http_code}' -X POST "$BASE$path" \
        -H "X-STK-Relay-Token: $TOKEN" \
        -H "Content-Type: application/json" \
        --data-binary "@$body"
}

wait_for_api() {
    for _ in $(seq 1 120); do
        if curl_get "/immersive/v1/capabilities" "$RESULT_DIR/capabilities.json" \
            >/dev/null 2>&1; then
            python3 - "$RESULT_DIR/capabilities.json" <<'PY'
import json
import sys

data = json.load(open(sys.argv[1]))
raise SystemExit(0 if data.get("ok") and data.get("status") == "ready" else 1)
PY
            return 0
        fi
        sleep 0.5
    done
    echo "FAIL TholusOS smoke API readiness timed out for $BASE" >&2
    return 1
}

log "build relay target"
if [[ "$RUN_SYPHON" == "1" ]]; then
    if [[ "$(uname -s)" != "Darwin" || \
          ( ! -d "$HOME/Library/Frameworks/Syphon.framework" && \
            ! -d "/Library/Frameworks/Syphon.framework" ) ]]; then
        echo "FAIL RUN_SYPHON=1 but Syphon.framework is not available." >&2
        exit 1
    fi
    build_dir "$SYPHON_BUILD" \
        -DENABLE_IMMERSIVE_RELAY=ON \
        -DENABLE_RELAY_LOCAL_API=ON \
        -DENABLE_RELAY_TEST_TOOLS=ON \
        -DENABLE_SYPHON_OUTPUT=ON
    APP="$SYPHON_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
    SAFE_OUTPUT="syphon"
else
    build_dir "$RELAY_BUILD" \
        -DENABLE_IMMERSIVE_RELAY=ON \
        -DENABLE_RELAY_LOCAL_API=ON \
        -DENABLE_RELAY_TEST_TOOLS=ON \
        -DENABLE_SYPHON_OUTPUT=OFF
    APP="$RELAY_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
fi

if [[ ! -x "$APP" ]]; then
    echo "FAIL STK app not found: $APP" >&2
    exit 1
fi

log "write TholusOS smoke requests"
python3 - "$RESULT_DIR" "$SAFE_OUTPUT" "$SERVER_NAME" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
safe_output = sys.argv[2]
server_name = sys.argv[3]

render = {
    "render_profile": "fulldome_safe",
    "output": safe_output,
    "projection": "fisheye_domemaster",
    "domemaster_size": 1536,
    "cubemap_size": 768,
    "camera": {
        "mode": "stabilized_chase",
        "pitch_deg": 35,
        "forward_offset": 0.25,
        "vertical_offset": 0.40,
        "smoothing_factor": 0.16,
        "horizon_stabilization": True,
        "roll_limit_deg": 12,
    },
    "syphon_server_name": server_name,
}
configure = {
    "player_count": 6,
    "track_id": "snowtuxpeak",
    "kart_id": "tux",
    "difficulty": "easy",
    "ai_karts": 5,
    "lap_policy": "player_count_based",
    "handoff_policy": "lap_based_handoff",
    "crew_mode": "enabled",
    "encounter_script": "snowtuxpeak_relay_encounters",
    "render_profile": "fulldome_safe",
    "output": "syphon",
    "projection": "fisheye_domemaster",
    "operator_mode": "arcade",
    "allow_pause": True,
    "allow_force_finish": True,
    "allow_abort": True,
}
(root / "render_config_safe_request.json").write_text(
    json.dumps(render, indent=2, sort_keys=True) + "\n", encoding="utf-8")
(root / "operator_configure_6p_request.json").write_text(
    json.dumps(configure, indent=2, sort_keys=True) + "\n", encoding="utf-8")
(root / "operator_start_request.json").write_text("{}\n", encoding="utf-8")
(root / "operator_countdown_skip_request.json").write_text(
    json.dumps({"skip": True}, indent=2, sort_keys=True) + "\n",
    encoding="utf-8")
(root / "operator_reset_request.json").write_text("{}\n", encoding="utf-8")
(root / "invalid_payload_request.json").write_text("[not-object]\n",
    encoding="utf-8")
PY

log "start STK for TholusOS integration smoke"
if [[ "$RUN_SYPHON" == "1" ]]; then
    STK_RELAY_REPORT_DIR="$RESULT_DIR/reports" "$APP" \
        --race-now \
        --profile-time=420 \
        --immersive-relay \
        --relay-local-api \
        --relay-test-accelerated-progress \
        --relay-api-port="$PORT" \
        --relay-api-token="$TOKEN" \
        --relay-player-count=6 \
        --relay-handoff-policy=lap_based_handoff \
        --immersive-output=syphon \
        --immersive-projection=fisheye_domemaster \
        --immersive-domemaster-size=1536 \
        --immersive-cubemap-size=768 \
        --immersive-camera=stabilized_chase \
        --immersive-pitch-deg=35 \
        --immersive-forward-offset=0.25 \
        --immersive-vertical-offset=0.40 \
        --immersive-smoothing-factor=0.16 \
        --immersive-horizon-stabilization=on \
        --immersive-roll-limit-deg=12 \
        --immersive-stats=on \
        --syphon-server-name="$SERVER_NAME" \
        --track=snowtuxpeak \
        --kart=tux \
        --numkarts=6 \
        --difficulty=0 \
        --unlock-all >"$RESULT_DIR/stk.log" 2>&1 &
else
    STK_RELAY_REPORT_DIR="$RESULT_DIR/reports" "$APP" \
        --no-graphics \
        -N \
        --immersive-relay \
        --relay-local-api \
        --relay-test-accelerated-progress \
        --relay-api-port="$PORT" \
        --relay-api-token="$TOKEN" \
        --relay-player-count=6 \
        --relay-handoff-policy=lap_based_handoff \
        --immersive-output=none \
        --track=snowtuxpeak \
        --kart=tux \
        --numkarts=6 \
        --difficulty=0 \
        --unlock-all >"$RESULT_DIR/stk.log" 2>&1 &
fi
STK_PID=$!
wait_for_api

log "normalize startup with operator reset"
curl_post "/immersive/v1/operator/reset" \
    "$RESULT_DIR/operator_reset_request.json" \
    "$RESULT_DIR/operator_reset_bootstrap_response.json"

log "GET /state idle"
curl_get "/immersive/v1/state" "$RESULT_DIR/state_idle.json"
curl_get "/immersive/v1/operator/state" "$RESULT_DIR/operator_state_ready.json"

log "POST /render/config safe"
curl_post "/immersive/v1/render/config" \
    "$RESULT_DIR/render_config_safe_request.json" \
    "$RESULT_DIR/render_config_safe_response.json"
curl_get "/immersive/v1/state" "$RESULT_DIR/state_after_render_config.json"
python3 - "$RESULT_DIR/state_after_render_config.json" "$SAFE_OUTPUT" "$SERVER_NAME" <<'PY'
import json
import sys

state = json.load(open(sys.argv[1]))
safe_output = sys.argv[2]
server_name = sys.argv[3]
render = state["immersive_render"]
assert render["projection"]["type"] == "fisheye_domemaster"
assert render["projection"]["width"] == 1536
assert render["projection"]["height"] == 1536
assert render["capture"]["cubemap_size"] == 768
assert render["output"]["type"] == safe_output
assert render["output"]["server_name"] == server_name
PY

log "POST /operator/configure 6p"
curl_post "/immersive/v1/operator/configure" \
    "$RESULT_DIR/operator_configure_6p_request.json" \
    "$RESULT_DIR/operator_configure_6p_response.json"
curl_get "/immersive/v1/state" "$RESULT_DIR/state_configured.json"
python3 - "$RESULT_DIR/state_configured.json" <<'PY'
import json
import sys

state = json.load(open(sys.argv[1]))
arcade = state["arcade_session"]
config = arcade["config"]
assert arcade["configured"] is True
assert arcade["player_count"] == 6
assert arcade["total_laps"] == 12
assert arcade["handoff_policy"] == "lap_based_handoff"
assert config["render_profile"] == "fulldome_safe"
assert config["projection"] == "fisheye_domemaster"
assert config["output"] == "syphon"
PY

log "POST /operator/start and /operator/countdown"
curl_post "/immersive/v1/operator/start" \
    "$RESULT_DIR/operator_start_request.json" \
    "$RESULT_DIR/operator_start_response.json"
curl_post "/immersive/v1/operator/countdown" \
    "$RESULT_DIR/operator_countdown_skip_request.json" \
    "$RESULT_DIR/operator_countdown_response.json"
curl_get "/immersive/v1/state" "$RESULT_DIR/state_running_6p.json"
curl_get "/immersive/v1/operator/state" "$RESULT_DIR/operator_state_running.json"

log "crew invalid_payload sanity"
INVALID_CODE="$(curl_post_code "/immersive/v1/crew/action" \
    "$RESULT_DIR/invalid_payload_request.json" \
    "$RESULT_DIR/crew_action_invalid_payload.json")"
if [[ "$INVALID_CODE" != "400" ]]; then
    echo "FAIL invalid_payload expected HTTP 400, got $INVALID_CODE" >&2
    exit 1
fi

if [[ "$RUN_SYPHON" == "1" && "$RUN_SYPHON_RECEIVER" == "1" ]]; then
    log "Syphon receiver probe"
    PROBE="$RESULT_DIR/SyphonReceiverProbe"
    "$SCRIPT_DIR/BuildSyphonReceiverProbe.sh" "$PROBE" \
        >"$RESULT_DIR/build_probe.log" 2>&1
    "$PROBE" --server-name "$SERVER_NAME" \
        --expected-width 1536 \
        --expected-height 1536 \
        --orientation-grid off \
        --duration 6 >"$RESULT_DIR/syphon_probe.log" 2>&1
    rg -q "SYPHON_PROBE PASS" "$RESULT_DIR/syphon_probe.log"
else
    echo '{"ok":true,"skipped":true,"reason":"RUN_SYPHON=0 or receiver disabled"}' \
        >"$RESULT_DIR/syphon_probe.json"
fi

log "FakeCrewPhones forwarded actions"
set +e
"$SCRIPT_DIR/FakeCrewPhones.py" \
    --base "$BASE" \
    --token "$TOKEN" \
    --players 6 \
    --mode perfect \
    --delay 0.05 \
    --no-start >"$RESULT_DIR/fake_crew_phones_report.json"
FAKE_CREW_STATUS=$?
set -e
echo "$FAKE_CREW_STATUS" >"$RESULT_DIR/fake_crew_phones_exit_code.txt"
curl_get "/immersive/v1/state" "$RESULT_DIR/state_results_crew_active.json"
curl_get "/immersive/v1/operator/state" "$RESULT_DIR/operator_state_results.json"
python3 - "$RESULT_DIR/fake_crew_phones_report.json" "$RESULT_DIR/state_results_crew_active.json" <<'PY'
import json
import sys

report = json.load(open(sys.argv[1]))
state = json.load(open(sys.argv[2]))
assert int(report.get("accepted_count", 0) or 0) > 0
assert int(state.get("crew_score", 0) or 0) > 0
assert int(state.get("crew_actions_accepted", 0) or 0) > 0
PY

log "operator reset"
curl_post "/immersive/v1/operator/reset" \
    "$RESULT_DIR/operator_reset_request.json" \
    "$RESULT_DIR/operator_reset_response.json"
curl_get "/immersive/v1/operator/state" "$RESULT_DIR/operator_state_after_reset.json"
python3 - "$RESULT_DIR/operator_state_after_reset.json" <<'PY'
import json
import sys

state = json.load(open(sys.argv[1]))
arcade = state["arcade_session"]
assert arcade["state"] in ("ready_for_players", "attract", "idle")
assert arcade["can_reset"] is True
PY

log "write final smoke report"
python3 - "$RESULT_DIR" "$BASE" "$RUN_SYPHON" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
base = sys.argv[2]
run_syphon = sys.argv[3] == "1"

def load(name):
    return json.load(open(root / name))

state = load("state_results_crew_active.json")
operator = load("operator_state_results.json")
render = load("state_after_render_config.json")["immersive_render"]
fake = load("fake_crew_phones_report.json")
report = {
    "ok": True,
    "suite": "tholustos_integration",
    "base": base,
    "run_syphon": run_syphon,
    "capabilities_status": load("capabilities.json").get("status"),
    "render": {
        "output": render["output"]["type"],
        "projection": render["projection"]["type"],
        "width": render["projection"]["width"],
        "height": render["projection"]["height"],
        "cubemap_size": render["capture"]["cubemap_size"],
        "server_name": render["output"]["server_name"],
    },
    "session": {
        "state": operator["arcade_session"]["state"],
        "configured": operator["arcade_session"]["configured"],
        "player_count": operator["arcade_session"]["player_count"],
        "total_laps": operator["arcade_session"]["total_laps"],
        "handoff_policy": operator["arcade_session"]["handoff_policy"],
    },
    "crew": {
        "score": state.get("crew_score"),
        "accepted": state.get("crew_actions_accepted"),
        "combo_count": state.get("combo_count"),
        "fake_crew_exit_code": int((root / "fake_crew_phones_exit_code.txt").read_text().strip() or 0),
        "fake_crew_harness_failures": fake.get("failures", []),
    },
}
(root / "tholustos_integration_report.json").write_text(
    json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
print(json.dumps(report, indent=2, sort_keys=True))
PY

log "TholusOS integration smoke passed"
