#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

TOKEN="${RELAY_API_TOKEN:-immersive-dev-token}"
BASE="${RELAY_API_BASE:-http://127.0.0.1:8890}"
JOBS="${JOBS:-8}"
FAKE_DELAY="${FAKE_DELAY:-0.7}"
RUN_GRAPHICS="${RUN_GRAPHICS:-0}"
RUN_2048="${RUN_2048:-1}"
SKIP_BUILDS="${SKIP_BUILDS:-0}"

VANILLA_BUILD="${VANILLA_BUILD:-cmake_build_phase4_vanilla}"
RELAY_BUILD="${RELAY_BUILD:-cmake_build_phase4_relay}"
SYPHON_BUILD="${SYPHON_BUILD:-cmake_build_phase4_syphon}"

VANILLA_APP="$REPO_DIR/$VANILLA_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
RELAY_APP="$REPO_DIR/$RELAY_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
SYPHON_APP="$REPO_DIR/$SYPHON_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"

cd "$REPO_DIR"

log_step() {
    printf '\n[%s] %s\n' "$1" "$2"
}

has_syphon_framework() {
    [[ -d "$HOME/Library/Frameworks/Syphon.framework" ]] ||
    [[ -d "/Library/Frameworks/Syphon.framework" ]] ||
    [[ -d "/opt/homebrew/Frameworks/Syphon.framework" ]] ||
    [[ -d "/usr/local/Frameworks/Syphon.framework" ]]
}

configure_and_build() {
    local build_dir="$1"
    shift
    cmake -S "$REPO_DIR" -B "$build_dir" "$@"
    cmake --build "$build_dir" --target supertuxkart --parallel "$JOBS"
}

if [[ "$SKIP_BUILDS" != "1" ]]; then
    log_step A "build vanilla"
    configure_and_build "$VANILLA_BUILD" \
        -DENABLE_IMMERSIVE_RELAY=OFF \
        -DENABLE_RELAY_LOCAL_API=OFF \
        -DENABLE_RELAY_TEST_TOOLS=OFF \
        -DENABLE_SYPHON_OUTPUT=OFF \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo

    log_step B "build relay without Syphon"
    configure_and_build "$RELAY_BUILD" \
        -DENABLE_IMMERSIVE_RELAY=ON \
        -DENABLE_RELAY_LOCAL_API=ON \
        -DENABLE_RELAY_TEST_TOOLS=ON \
        -DENABLE_SYPHON_OUTPUT=OFF \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo

    log_step C "build relay with Syphon"
    if [[ "$(uname -s)" != "Darwin" ]]; then
        echo "SKIPPED Syphon build: ENABLE_SYPHON_OUTPUT is macOS-only."
    elif ! has_syphon_framework; then
        echo "SKIPPED Syphon build: Syphon.framework was not found."
        echo "Manual check: install Syphon.framework, then rerun with ENABLE_SYPHON_OUTPUT=ON."
    else
        configure_and_build "$SYPHON_BUILD" \
            -DENABLE_IMMERSIVE_RELAY=ON \
            -DENABLE_RELAY_LOCAL_API=ON \
            -DENABLE_RELAY_TEST_TOOLS=ON \
            -DENABLE_SYPHON_OUTPUT=ON \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo
    fi
fi

if [[ ! -x "$VANILLA_APP" ]]; then
    echo "Vanilla app not found: $VANILLA_APP" >&2
    exit 1
fi
if [[ ! -x "$RELAY_APP" ]]; then
    echo "Relay app not found: $RELAY_APP" >&2
    exit 1
fi

log_step D "help vanilla hides relay/immersive flags"
"$VANILLA_APP" --help 2>&1 | tee /tmp/stk_phase4_vanilla_help.txt >/dev/null
if rg -q -- '--immersive-output|--immersive-relay|--relay-local-api|--syphon-server-name' /tmp/stk_phase4_vanilla_help.txt; then
    echo "FAIL: vanilla help exposes relay/immersive flags" >&2
    exit 1
fi
echo "PASS vanilla help"

log_step E "help relay shows immersive flags"
"$RELAY_APP" --help 2>&1 | tee /tmp/stk_phase4_relay_help.txt >/dev/null
for flag in --immersive-output --immersive-projection --immersive-domemaster-size \
            --immersive-cubemap-size --immersive-camera --syphon-server-name; do
    rg -q -- "$flag" /tmp/stk_phase4_relay_help.txt
done
echo "PASS relay help"

cleanup() {
    if [[ -n "${STK_PID:-}" ]]; then
        kill "$STK_PID" >/dev/null 2>&1 || true
        wait "$STK_PID" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

start_relay_api() {
    local log_file="$1"
    rm -f "$log_file"
    "$RELAY_APP" \
        --no-graphics \
        --race-now \
        --immersive-relay \
        --relay-local-api \
        --relay-test-accelerated-progress \
        --relay-api-token="$TOKEN" \
        --relay-player-count=6 \
        --relay-handoff-policy=lap_based_handoff \
        --immersive-output=none \
        --immersive-projection=fisheye \
        --immersive-domemaster-size=1024 \
        --immersive-cubemap-size=512 \
        --immersive-camera=stabilized_chase \
        --immersive-yaw-deg=11.5 \
        --immersive-pitch-deg=-4.25 \
        --immersive-roll-deg=2.0 \
        --immersive-dome-tilt-deg=12.0 \
        --immersive-horizon-stabilization=on \
        --immersive-roll-limit-deg=9.0 \
        --immersive-debug-grid=on \
        --immersive-stats=on \
        --syphon-server-name="Phase4 CLI Smoke" \
        --track=snowtuxpeak \
        --kart=tux \
        --numkarts=6 \
        --difficulty=0 \
        --unlock-all >"$log_file" 2>&1 &
    STK_PID=$!
}

wait_for_api() {
    python3 - "$BASE" "$TOKEN" <<'PY'
import json
import sys
import time
import urllib.request

base, token = sys.argv[1], sys.argv[2]
headers = {"X-STK-Relay-Token": token}
for _ in range(120):
    try:
        req = urllib.request.Request(base + "/immersive/v1/state",
                                     headers=headers, method="GET")
        with urllib.request.urlopen(req, timeout=1) as response:
            state = json.loads(response.read().decode("utf-8"))
        if state.get("ok") and state.get("relay_enabled"):
            print("PASS relay API ready")
            raise SystemExit(0)
    except Exception:
        time.sleep(0.5)
print("relay API readiness timed out", file=sys.stderr)
raise SystemExit(1)
PY
}

log_step F "relay 16:9/API gameplay and CLI parsing"
start_relay_api /tmp/stk_phase4_relay_api.log
wait_for_api
rg -q "Configured output=none projection=fisheye domemaster=1024 cubemap=512" \
    /tmp/stk_phase4_relay_api.log || \
    rg -q "Configured capture=full_sphere_cubemap output=none projection=fisheye_domemaster status=production size=1024x1024 cubemap=512" \
        /tmp/stk_phase4_relay_api.log
echo "PASS immersive CLI accepted"

for players in 2 4 6; do
    log_step M "FakeCrewPhones slow players=$players"
    "$SCRIPT_DIR/FakeCrewPhones.py" \
        --base "$BASE" \
        --token "$TOKEN" \
        --players "$players" \
        --mode slow \
        --delay "$FAKE_DELAY" \
        --session-id "phase4-fake-$players"
done
cleanup
unset STK_PID

run_window_smoke() {
    local domemaster="$1"
    local cubemap="$2"
    local log_file="/tmp/stk_phase4_window_${domemaster}.log"
    echo "RUN window fisheye domemaster=$domemaster cubemap=$cubemap"
    "$RELAY_APP" \
        --race-now \
        --profile-time=8 \
        --immersive-relay \
        --relay-local-api \
        --relay-test-accelerated-progress \
        --relay-api-token="$TOKEN" \
        --relay-player-count=6 \
        --relay-handoff-policy=lap_based_handoff \
        --immersive-output=window \
        --immersive-projection=fisheye \
        --immersive-domemaster-size="$domemaster" \
        --immersive-cubemap-size="$cubemap" \
        --immersive-camera=stabilized_chase \
        --immersive-debug-grid=on \
        --immersive-stats=on \
        --track=snowtuxpeak \
        --kart=tux \
        --numkarts=6 \
        --difficulty=0 \
        --unlock-all >"$log_file" 2>&1 &
    STK_PID=$!
    sleep 10
    cleanup
    unset STK_PID
    if rg -q "ImmersiveRenderStats: mode=window projection=fisheye size=$domemaster" "$log_file"; then
        echo "PASS window domemaster $domemaster"
        rg "ImmersiveRenderStats" "$log_file" | tail -1
    else
        echo "FAIL window domemaster $domemaster; see $log_file" >&2
        exit 1
    fi
}

if [[ "$RUN_GRAPHICS" == "1" ]]; then
    log_step G "window fisheye 1024 + debug grid"
    run_window_smoke 1024 512
    log_step H "window fisheye 2048 + debug grid"
    if [[ "$RUN_2048" == "1" ]]; then
        run_window_smoke 2048 1024
    else
        echo "SKIPPED 2048 window smoke: RUN_2048=0."
    fi

    log_step I "Syphon runtime smoke"
    if [[ "$(uname -s)" != "Darwin" ]]; then
        echo "SKIPPED Syphon runtime: not macOS."
    elif [[ ! -x "$SYPHON_APP" ]]; then
        echo "SKIPPED Syphon runtime: Syphon build app not available."
    else
        echo "Manual receiver check: open a Syphon receiver and select 'SuperTuxKart Immersive Relay Domemaster'."
        "$SYPHON_APP" --race-now --profile-time=8 --immersive-relay \
            --immersive-output=syphon --immersive-projection=fisheye \
            --immersive-domemaster-size=1024 --immersive-cubemap-size=512 \
            --immersive-debug-grid=on --immersive-stats=on \
            --track=snowtuxpeak --kart=tux --numkarts=6 --difficulty=0 \
            --unlock-all >/tmp/stk_phase4_syphon_1024.log 2>&1 &
        STK_PID=$!
        sleep 10
        cleanup
        unset STK_PID
        rg -q "Publishing immersive projection via Syphon server" /tmp/stk_phase4_syphon_1024.log
        echo "PASS Syphon publish smoke"
    fi
else
    log_step G "window/Syphon runtime smokes"
    echo "SKIPPED graphical window and Syphon runtime smokes: set RUN_GRAPHICS=1."
fi

echo
echo "[done] relay phase 4 render smoke completed"
