#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_DIR"

TOKEN="${TOKEN:-phase51-venue-acceptance}"
PORT="${PORT:-27581}"
SYPHON_PORT="${SYPHON_PORT:-27582}"
BASE="http://127.0.0.1:$PORT"
SYPHON_BASE="http://127.0.0.1:$SYPHON_PORT"
JOBS="${JOBS:-8}"
SKIP_BUILDS="${SKIP_BUILDS:-0}"
RUN_SYPHON="${RUN_SYPHON:-1}"
RUN_SYPHON_RECEIVER="${RUN_SYPHON_RECEIVER:-1}"
VANILLA_BUILD="${VANILLA_BUILD:-cmake_build_vanilla}"
RELAY_BUILD="${RELAY_BUILD:-cmake_build_relay_phase1}"
SYPHON_BUILD="${SYPHON_BUILD:-cmake_build_phase45_syphon}"
RESULT_DIR="${RESULT_DIR:-/tmp/stk_phase51_venue_acceptance}"
SERVER_NAME="${SERVER_NAME:-SuperTuxKart Immersive Relay Domemaster}"
FAKE_DELAY="${FAKE_DELAY:-0.28}"
SYPHON_DOMEMASTER_SIZE="${SYPHON_DOMEMASTER_SIZE:-1536}"
SYPHON_CUBEMAP_SIZE="${SYPHON_CUBEMAP_SIZE:-768}"

mkdir -p "$RESULT_DIR/reports"

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
    cleanup_pid "${SYPHON_PID:-}"
}
trap cleanup EXIT

wait_for_api() {
    local base="$1"
    local out="$2"
    for _ in $(seq 1 120); do
        if curl -fsS -H "X-STK-Relay-Token: $TOKEN" \
            "$base/immersive/v1/health" >"$out" 2>/dev/null; then
            python3 - "$out" <<'PY'
import json, sys
data = json.load(open(sys.argv[1]))
raise SystemExit(0 if data.get("ok") and data.get("api_active") else 1)
PY
            return 0
        fi
        sleep 0.5
    done
    echo "API readiness timed out for $base" >&2
    return 1
}

collect_hardware() {
    log "hardware target inventory"
    {
        echo "date=$(date -Iseconds)"
        sw_vers
        echo
        system_profiler SPHardwareDataType SPDisplaysDataType
        echo
        if [[ -d "$HOME/Library/Frameworks/Syphon.framework" ]]; then
            echo "syphon_framework=$HOME/Library/Frameworks/Syphon.framework"
        elif [[ -d "/Library/Frameworks/Syphon.framework" ]]; then
            echo "syphon_framework=/Library/Frameworks/Syphon.framework"
        else
            echo "syphon_framework=not_found"
        fi
    } >"$RESULT_DIR/hardware.txt"
    system_profiler SPHardwareDataType SPDisplaysDataType -json \
        >"$RESULT_DIR/hardware.json" || true
}

collect_hardware

log "build vanilla"
build_dir "$VANILLA_BUILD" \
    -DENABLE_IMMERSIVE_RELAY=OFF \
    -DENABLE_RELAY_LOCAL_API=OFF \
    -DENABLE_RELAY_TEST_TOOLS=OFF

log "build relay without Syphon"
build_dir "$RELAY_BUILD" \
    -DENABLE_IMMERSIVE_RELAY=ON \
    -DENABLE_RELAY_LOCAL_API=ON \
    -DENABLE_RELAY_TEST_TOOLS=ON \
    -DENABLE_SYPHON_OUTPUT=OFF

log "build relay with Syphon if available"
SYPHON_AVAILABLE=0
if [[ "$(uname -s)" == "Darwin" && \
      ( -d "$HOME/Library/Frameworks/Syphon.framework" || \
        -d "/Library/Frameworks/Syphon.framework" ) ]]; then
    build_dir "$SYPHON_BUILD" \
        -DENABLE_IMMERSIVE_RELAY=ON \
        -DENABLE_RELAY_LOCAL_API=ON \
        -DENABLE_RELAY_TEST_TOOLS=ON \
        -DENABLE_SYPHON_OUTPUT=ON
    SYPHON_AVAILABLE=1
else
    echo "SKIPPED Syphon build: Syphon.framework not available."
fi

APP="$RELAY_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
if [[ ! -x "$APP" ]]; then
    echo "Relay app not found: $APP" >&2
    exit 1
fi

log "start relay API process without Syphon"
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
    --unlock-all >"$RESULT_DIR/stk_no_syphon.log" 2>&1 &
STK_PID=$!
wait_for_api "$BASE" "$RESULT_DIR/health_no_syphon.json"

log "run no-Syphon integrated acceptance"
"$SCRIPT_DIR/Phase51VenueAcceptance.py" \
    --base "$BASE" \
    --token "$TOKEN" \
    --result-dir "$RESULT_DIR" \
    --suite no-syphon \
    --output none \
    --fake-delay "$FAKE_DELAY" \
    --expect-syphon-unavailable \
    >"$RESULT_DIR/phase51_no_syphon.json"

cleanup_pid "$STK_PID"
STK_PID=""

if [[ "$RUN_SYPHON" == "1" && "$SYPHON_AVAILABLE" == "1" && \
      -x "$SYPHON_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart" ]]; then
    log "start Syphon relay API process"
    STK_RELAY_REPORT_DIR="$RESULT_DIR/reports" \
        "$SYPHON_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart" \
        --race-now \
        --profile-time=240 \
        --immersive-relay \
        --relay-local-api \
        --relay-test-accelerated-progress \
        --relay-api-port="$SYPHON_PORT" \
        --relay-api-token="$TOKEN" \
        --relay-player-count=6 \
        --relay-handoff-policy=lap_based_handoff \
        --immersive-output=syphon \
        --immersive-projection=fisheye_domemaster \
        --immersive-domemaster-size="$SYPHON_DOMEMASTER_SIZE" \
        --immersive-cubemap-size="$SYPHON_CUBEMAP_SIZE" \
        --immersive-stats=on \
        --syphon-server-name="$SERVER_NAME" \
        --track=snowtuxpeak \
        --kart=tux \
        --numkarts=6 \
        --difficulty=0 \
        --unlock-all >"$RESULT_DIR/stk_syphon.log" 2>&1 &
    SYPHON_PID=$!
    wait_for_api "$SYPHON_BASE" "$RESULT_DIR/health_syphon.json"

    if [[ "$RUN_SYPHON_RECEIVER" == "1" ]]; then
        log "run Syphon receiver probe"
        PROBE="$RESULT_DIR/SyphonReceiverProbe"
        "$SCRIPT_DIR/BuildSyphonReceiverProbe.sh" "$PROBE" \
            >"$RESULT_DIR/build_probe.log" 2>&1
        "$PROBE" --server-name "$SERVER_NAME" \
            --expected-width "$SYPHON_DOMEMASTER_SIZE" \
            --expected-height "$SYPHON_DOMEMASTER_SIZE" \
            --orientation-grid off \
            --duration 6 >"$RESULT_DIR/probe.log" 2>&1
        rg -q "SYPHON_PROBE PASS" "$RESULT_DIR/probe.log"
    fi

    log "run Syphon integrated acceptance"
    "$SCRIPT_DIR/Phase51VenueAcceptance.py" \
        --base "$SYPHON_BASE" \
        --token "$TOKEN" \
        --result-dir "$RESULT_DIR" \
        --suite syphon \
        --output syphon \
        --fake-delay "$FAKE_DELAY" \
        >"$RESULT_DIR/phase51_syphon.json"
    cleanup_pid "$SYPHON_PID"
    SYPHON_PID=""
else
    echo '{"ok":true,"suite":"syphon","skipped":true,"reason":"Syphon unavailable or RUN_SYPHON=0"}' \
        >"$RESULT_DIR/phase51_syphon.json"
fi

python3 - "$RESULT_DIR" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
failed = []
for name in ("phase51_no_syphon.json", "phase51_syphon.json"):
    data = json.load(open(root / name))
    if not data.get("ok"):
        failed.append((name, data.get("failures", [])))
if failed:
    for name, failures in failed:
        print(f"FAIL {name}: {failures}")
    raise SystemExit(1)
print("PASS Phase 5.1 venue acceptance package")
PY

log "done; results in $RESULT_DIR"
