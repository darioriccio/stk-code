#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

RESULT_DIR="${RESULT_DIR:-/tmp/stk_phase46_projection_smoke}"
VANILLA_BUILD="${VANILLA_BUILD:-cmake_build_phase45_vanilla}"
RELAY_BUILD="${RELAY_BUILD:-cmake_build_phase45_relay}"
SYPHON_BUILD="${SYPHON_BUILD:-cmake_build_phase45_syphon}"
PORT="${PORT:-27646}"
TOKEN="${TOKEN:-phase46-smoke}"
SERVER_NAME="${SERVER_NAME:-SuperTuxKart Immersive Relay Domemaster}"
SKIP_BUILDS="${SKIP_BUILDS:-0}"
RUN_GRAPHICS="${RUN_GRAPHICS:-1}"
RUN_SYPHON="${RUN_SYPHON:-1}"

mkdir -p "$RESULT_DIR"

step() {
    echo "== $*"
}

build_target() {
    local dir="$1"
    if [[ "$SKIP_BUILDS" == "1" ]]; then
        echo "SKIPPED build $dir"
        return
    fi
    cmake --build "$dir" --target supertuxkart --parallel "${BUILD_JOBS:-8}"
}

wait_for_api() {
    local log="$1"
    for _ in $(seq 1 90); do
        if rg -q "Relay API listening" "$log"; then
            return 0
        fi
        sleep 1
    done
    echo "FAIL API did not start; see $log" >&2
    return 1
}

cleanup_pid() {
    local pid="${1:-}"
    if [[ -n "$pid" ]] && kill -0 "$pid" >/dev/null 2>&1; then
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
    fi
}

step "build vanilla"
build_target "$VANILLA_BUILD"

step "build relay without Syphon"
build_target "$RELAY_BUILD"

step "build relay with Syphon"
if [[ "$(uname -s)" == "Darwin" && -d "$HOME/Library/Frameworks/Syphon.framework" ]]; then
    build_target "$SYPHON_BUILD"
else
    echo "SKIPPED Syphon build: macOS/Syphon.framework not available."
fi

if [[ "$RUN_GRAPHICS" != "1" ]]; then
    echo "SKIPPED graphics/API smoke: RUN_GRAPHICS=0"
    exit 0
fi

APP="$RELAY_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
step "nested render config and projection stub rejection"
"$APP" \
    --race-now \
    --profile-time=45 \
    --immersive-relay \
    --relay-local-api \
    --relay-api-port="$PORT" \
    --relay-api-token="$TOKEN" \
    --relay-player-count=6 \
    --relay-handoff-policy=lap_based_handoff \
    --immersive-output=window \
    --immersive-projection=fisheye_domemaster \
    --immersive-domemaster-size=1024 \
    --immersive-cubemap-size=512 \
    --immersive-stats=on \
    --track=snowtuxpeak \
    --kart=tux \
    --numkarts=6 \
    --difficulty=0 \
    --unlock-all >"$RESULT_DIR/api.log" 2>&1 &
STK_PID=$!
trap 'cleanup_pid "${STK_PID:-}"' EXIT
wait_for_api "$RESULT_DIR/api.log"

curl -fsS -X POST "http://127.0.0.1:$PORT/immersive/v1/render/config" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    --data '{"capture":{"type":"full_sphere_cubemap","cubemap_size":512},"projection":{"type":"fisheye_domemaster","width":1024,"height":1024,"debug_grid":true},"output":{"type":"window"},"camera":{"mode":"stabilized_chase","pitch_deg":35,"forward_offset":0.25,"vertical_offset":0.40,"smoothing_factor":0.16,"horizon_stabilization":true,"roll_limit_deg":12}}' \
    >"$RESULT_DIR/render_config_response.json"

HTTP_CODE="$(curl -sS -o "$RESULT_DIR/equirect_response.json" -w '%{http_code}' \
    -X POST "http://127.0.0.1:$PORT/immersive/v1/render/config" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    --data '{"projection":{"type":"equirectangular","width":2048,"height":1024},"output":{"type":"window"}}')"
if [[ "$HTTP_CODE" != "400" ]]; then
    echo "FAIL equirectangular stub should return HTTP 400, got $HTTP_CODE" >&2
    exit 1
fi

curl -fsS -H "Authorization: Bearer $TOKEN" \
    "http://127.0.0.1:$PORT/immersive/v1/state" \
    >"$RESULT_DIR/state.json"
python3 - <<'PY' "$RESULT_DIR/state.json" "$RESULT_DIR/equirect_response.json"
import json, sys
state = json.load(open(sys.argv[1]))["immersive_render"]
err = json.load(open(sys.argv[2])).get("error", "")
assert state["capture"]["type"] == "full_sphere_cubemap"
assert state["projection"]["type"] == "fisheye_domemaster"
assert state["projection"]["status"] == "production"
assert "immersive_projection_unavailable:equirectangular" == err
print("PASS nested API/state and projection stub rejection")
PY
cleanup_pid "$STK_PID"
trap - EXIT

step "window fisheye smoke"
STK_IMMERSIVE_DOMEMASTER_DUMP="$RESULT_DIR/domemaster.ppm" "$APP" \
    --race-now \
    --profile-time=12 \
    --immersive-relay \
    --relay-player-count=6 \
    --relay-handoff-policy=lap_based_handoff \
    --immersive-output=window \
    --immersive-projection=fisheye_domemaster \
    --immersive-domemaster-size=1024 \
    --immersive-cubemap-size=512 \
    --immersive-stats=on \
    --track=snowtuxpeak \
    --kart=tux \
    --numkarts=6 \
    --difficulty=0 \
    --unlock-all >"$RESULT_DIR/window.log" 2>&1 &
STK_PID=$!
sleep 14
cleanup_pid "$STK_PID"
rg -q "ImmersiveRenderStats: mode=window projection=fisheye size=1024" \
    "$RESULT_DIR/window.log"
test -s "$RESULT_DIR/domemaster.ppm"
echo "PASS window fisheye smoke"

if [[ "$RUN_SYPHON" == "1" && "$(uname -s)" == "Darwin" && \
      -x "$SYPHON_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart" ]]; then
    step "Syphon receiver smoke"
    PROBE="$RESULT_DIR/SyphonReceiverProbe"
    tools/immersive/BuildSyphonReceiverProbe.sh "$PROBE" >"$RESULT_DIR/build_probe.log" 2>&1
    "$SYPHON_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart" \
        --race-now \
        --profile-time=20 \
        --immersive-relay \
        --immersive-output=syphon \
        --immersive-projection=fisheye_domemaster \
        --immersive-domemaster-size=1024 \
        --immersive-cubemap-size=512 \
        --immersive-debug-grid=on \
        --immersive-stats=on \
        --syphon-server-name="$SERVER_NAME" \
        --track=snowtuxpeak \
        --kart=tux \
        --numkarts=6 \
        --difficulty=0 \
        --unlock-all >"$RESULT_DIR/syphon.log" 2>&1 &
    STK_PID=$!
    sleep 6
    "$PROBE" --server-name "$SERVER_NAME" \
        --expected-width 1024 \
        --expected-height 1024 \
        --duration 6 >"$RESULT_DIR/probe.log" 2>&1
    cleanup_pid "$STK_PID"
    rg -q "SYPHON_PROBE PASS" "$RESULT_DIR/probe.log"
    echo "PASS Syphon receiver smoke"
else
    echo "SKIPPED Syphon receiver smoke"
fi

echo "PASS Phase 4.6 projection smoke"
