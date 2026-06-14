#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOKEN="${RELAY_API_TOKEN:-immersive-dev-token}"
BASE="${RELAY_API_BASE:-http://127.0.0.1:8890}"
JOBS="${JOBS:-8}"
FAKE_DELAY="${FAKE_DELAY:-0.35}"
VANILLA_BUILD="${VANILLA_BUILD:-cmake_build_vanilla}"
RELAY_BUILD="${RELAY_BUILD:-cmake_build_relay_phase1}"
RELAY_APP="$REPO_DIR/$RELAY_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
LOG_FILE="$REPO_DIR/relay_phase3_smoke_stk.log"

cd "$REPO_DIR"

if [[ ! -f "$VANILLA_BUILD/CMakeCache.txt" ]]; then
    cmake -S "$REPO_DIR" -B "$VANILLA_BUILD" \
        -DENABLE_IMMERSIVE_RELAY=OFF \
        -DENABLE_RELAY_LOCAL_API=OFF \
        -DENABLE_RELAY_TEST_TOOLS=OFF
fi

if [[ ! -f "$RELAY_BUILD/CMakeCache.txt" ]]; then
    cmake -S "$REPO_DIR" -B "$RELAY_BUILD" \
        -DENABLE_IMMERSIVE_RELAY=ON \
        -DENABLE_RELAY_LOCAL_API=ON \
        -DENABLE_RELAY_TEST_TOOLS=ON
else
    cmake -S "$REPO_DIR" -B "$RELAY_BUILD" \
        -DENABLE_IMMERSIVE_RELAY=ON \
        -DENABLE_RELAY_LOCAL_API=ON \
        -DENABLE_RELAY_TEST_TOOLS=ON >/dev/null
fi

echo "[A] build vanilla"
cmake --build "$VANILLA_BUILD" --parallel "$JOBS"

echo "[B] build relay"
cmake --build "$RELAY_BUILD" --parallel "$JOBS"

if [[ ! -x "$RELAY_APP" ]]; then
    echo "Relay app not found: $RELAY_APP" >&2
    exit 1
fi

cleanup() {
    if [[ -n "${STK_PID:-}" ]]; then
        kill "$STK_PID" >/dev/null 2>&1 || true
        wait "$STK_PID" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

echo "[server] starting relay API with accelerated Phase 3 progress"
"$RELAY_APP" \
    --no-graphics \
    -N \
    --immersive-relay \
    --relay-local-api \
    --relay-test-accelerated-progress \
    --relay-api-token="$TOKEN" \
    --relay-player-count=6 \
    --relay-handoff-policy=lap_based_handoff \
    --track=snowtuxpeak \
    --kart=tux \
    --numkarts=6 \
    --difficulty=0 \
    --unlock-all >"$LOG_FILE" 2>&1 &
STK_PID=$!

python3 - "$BASE" "$TOKEN" <<'PY'
import json
import sys
import time
import urllib.request

base, token = sys.argv[1], sys.argv[2]
headers = {"X-STK-Relay-Token": token}
for _ in range(120):
    try:
        health_req = urllib.request.Request(
            base + "/immersive/v1/health", headers=headers, method="GET")
        with urllib.request.urlopen(health_req, timeout=1) as response:
            health = json.loads(response.read().decode("utf-8"))

        state_req = urllib.request.Request(
            base + "/immersive/v1/state", headers=headers, method="GET")
        with urllib.request.urlopen(state_req, timeout=1) as response:
            state = json.loads(response.read().decode("utf-8"))

        if (health.get("ok") and state.get("ok") and
                state.get("relay_enabled") and
                state.get("test_accelerated_progress")):
            print("[server] health/state/test-progress ok")
            raise SystemExit(0)
    except Exception:
        time.sleep(0.5)
print("[server] readiness check timed out", file=sys.stderr)
raise SystemExit(1)
PY

echo "[state] Phase 3 state contract"
"$SCRIPT_DIR/ValidateRelayState.py" --base "$BASE" --token "$TOKEN"

for players in 2 3 4 5 6; do
    echo "[session] perfect players=$players"
    "$SCRIPT_DIR/FakeCrewPhones.py" \
        --base "$BASE" \
        --token "$TOKEN" \
        --players "$players" \
        --mode perfect \
        --delay "$FAKE_DELAY"
done

for mode in wrong-role wrong-window cooldown already-used expired stale; do
    echo "[api] rejection mode=$mode"
    "$SCRIPT_DIR/FakeCrewPhones.py" \
        --base "$BASE" \
        --token "$TOKEN" \
        --players 6 \
        --mode "$mode" \
        --delay "$FAKE_DELAY"
done

echo "[mode] slow players=6"
"$SCRIPT_DIR/FakeCrewPhones.py" \
    --base "$BASE" \
    --token "$TOKEN" \
    --players 6 \
    --mode slow \
    --delay "$FAKE_DELAY"

echo "[mode] mistakes players=6"
"$SCRIPT_DIR/FakeCrewPhones.py" \
    --base "$BASE" \
    --token "$TOKEN" \
    --players 6 \
    --mode mistakes \
    --delay "$FAKE_DELAY"

echo "[mode] silent-role navigator players=6"
"$SCRIPT_DIR/FakeCrewPhones.py" \
    --base "$BASE" \
    --token "$TOKEN" \
    --players 6 \
    --mode silent-role \
    --silent-role navigator \
    --delay "$FAKE_DELAY"

echo "[done] relay phase 3 smoke passed"
