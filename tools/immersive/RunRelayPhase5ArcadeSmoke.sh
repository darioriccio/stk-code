#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_DIR"

TOKEN="${TOKEN:-phase5-arcade-smoke}"
PORT="${PORT:-27575}"
BASE="http://127.0.0.1:$PORT"
JOBS="${JOBS:-8}"
SKIP_BUILDS="${SKIP_BUILDS:-0}"
VANILLA_BUILD="${VANILLA_BUILD:-cmake_build_vanilla}"
RELAY_BUILD="${RELAY_BUILD:-cmake_build_relay_phase1}"
SYPHON_BUILD="${SYPHON_BUILD:-cmake_build_phase45_syphon}"
RESULT_DIR="${RESULT_DIR:-/tmp/stk_phase5_arcade_smoke}"
APP="$RELAY_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"

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

cleanup() {
    if [[ -n "${STK_PID:-}" ]] && kill -0 "$STK_PID" >/dev/null 2>&1; then
        kill "$STK_PID" >/dev/null 2>&1 || true
        wait "$STK_PID" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

wait_for_api() {
    for _ in $(seq 1 120); do
        if curl -fsS -H "X-STK-Relay-Token: $TOKEN" \
            "$BASE/immersive/v1/health" >"$RESULT_DIR/health.json" 2>/dev/null; then
            python3 - "$RESULT_DIR/health.json" <<'PY'
import json, sys
data = json.load(open(sys.argv[1]))
raise SystemExit(0 if data.get("ok") and data.get("api_active") else 1)
PY
            return 0
        fi
        sleep 0.5
    done
    echo "API readiness timed out; see $RESULT_DIR/stk.log" >&2
    return 1
}

run_operator() {
    local name="$1"
    shift
    log "$name"
    "$SCRIPT_DIR/FakeArcadeOperator.py" \
        --base "$BASE" \
        --token "$TOKEN" \
        --fast \
        "$@" >"$RESULT_DIR/$name.json"
}

log "A build vanilla"
build_dir "$VANILLA_BUILD" \
    -DENABLE_IMMERSIVE_RELAY=OFF \
    -DENABLE_RELAY_LOCAL_API=OFF \
    -DENABLE_RELAY_TEST_TOOLS=OFF

log "B build relay without Syphon"
build_dir "$RELAY_BUILD" \
    -DENABLE_IMMERSIVE_RELAY=ON \
    -DENABLE_RELAY_LOCAL_API=ON \
    -DENABLE_RELAY_TEST_TOOLS=ON \
    -DENABLE_SYPHON_OUTPUT=OFF

log "C build relay with Syphon if available"
if [[ "$(uname -s)" == "Darwin" && -d "$HOME/Library/Frameworks/Syphon.framework" ]]; then
    build_dir "$SYPHON_BUILD" \
        -DENABLE_IMMERSIVE_RELAY=ON \
        -DENABLE_RELAY_LOCAL_API=ON \
        -DENABLE_RELAY_TEST_TOOLS=ON \
        -DENABLE_SYPHON_OUTPUT=ON
else
    echo "SKIPPED Syphon build: Syphon.framework not available."
fi

if [[ ! -x "$APP" ]]; then
    echo "Relay app not found: $APP" >&2
    exit 1
fi

log "start relay API process"
"$APP" \
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
STK_PID=$!
wait_for_api

run_operator "D_start_finish_reset_2p" --players 2 --run-session --reset
run_operator "H_start_finish_reset_4p" --players 4 --run-session --reset
run_operator "L_start_finish_reset_6p" --players 6 --run-session --reset
run_operator "P_pause_resume" --players 6 --pause-resume-test
run_operator "Q_abort" --players 6 --abort-test
run_operator "R_force_finish" --players 6 --force-finish-test
run_operator "S_handoff_confirm" --players 6 --handoff-test
run_operator "T_invalid_operator_command" --players 6 --invalid-test
run_operator "U_reset_during_handoff" --players 6 --reset-during-handoff-test
run_operator "V_reset_loop_3_sessions" --players 6 --reset-loop 3

python3 - "$RESULT_DIR" <<'PY'
import json
import pathlib
import sys
root = pathlib.Path(sys.argv[1])
failed = []
for path in sorted(root.glob("*.json")):
    if path.name == "health.json":
        continue
    data = json.load(open(path))
    if data.get("ok") is False or data.get("failures"):
        failed.append((path.name, data.get("failures")))
if failed:
    for name, failures in failed:
        print(f"FAIL {name}: {failures}")
    raise SystemExit(1)
print("PASS Phase 5 arcade smoke")
PY

log "done"
