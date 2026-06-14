#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOKEN="${RELAY_API_TOKEN:-immersive-dev-token}"
BASE="${RELAY_API_BASE:-http://127.0.0.1:8890}"
PORT="${RELAY_API_PORT:-8890}"
VANILLA_BUILD="${VANILLA_BUILD:-cmake_build_vanilla}"
RELAY_BUILD="${RELAY_BUILD:-cmake_build_relay_phase1}"
RELAY_APP="${RELAY_APP:-$REPO_DIR/$RELAY_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart}"

cd "$REPO_DIR"

echo "[A] build vanilla"
cmake --build "$VANILLA_BUILD" --parallel "${JOBS:-8}"

echo "[B] build relay"
cmake --build "$RELAY_BUILD" --parallel "${JOBS:-8}"

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

echo "[server] starting relay API"
"$RELAY_APP" \
    --no-graphics \
    -N \
    --immersive-relay \
    --relay-local-api \
    --relay-api-token="$TOKEN" \
    --relay-api-port="$PORT" \
    --relay-player-count=6 \
    --relay-handoff-policy=lap_based_handoff \
    --track=snowtuxpeak \
    --kart=tux \
    --numkarts=6 \
    --difficulty=0 \
    --unlock-all >"$REPO_DIR/relay_phase2_smoke_stk.log" 2>&1 &
STK_PID=$!

python3 - "$BASE" "$TOKEN" <<'PY'
import json
import sys
import time
import urllib.request

base, token = sys.argv[1], sys.argv[2]
for _ in range(120):
    try:
        health_req = urllib.request.Request(
            base + "/immersive/v1/health",
            headers={"X-STK-Relay-Token": token},
            method="GET")
        with urllib.request.urlopen(health_req, timeout=1) as response:
            health = json.loads(response.read().decode("utf-8"))

        state_req = urllib.request.Request(
            base + "/immersive/v1/state",
            headers={"X-STK-Relay-Token": token},
            method="GET")
        with urllib.request.urlopen(state_req, timeout=1) as response:
            state = json.loads(response.read().decode("utf-8"))

        if (health.get("ok") and state.get("ok") and
                state.get("relay_enabled") and state.get("race_attached")):
            print("[server] health/state/race ok")
            raise SystemExit(0)
    except Exception:
        time.sleep(0.5)
print("[server] readiness check timed out", file=sys.stderr)
raise SystemExit(1)
PY

echo "[state] minimum contract"
"$SCRIPT_DIR/ValidateRelayState.py" --base "$BASE" --token "$TOKEN"

echo "[C] FakeCrewPhones perfect 6 players"
"$SCRIPT_DIR/FakeCrewPhones.py" --base "$BASE" --token "$TOKEN" --players 6 --mode perfect

echo "[state] filtered contract current phone"
read -r FILTER_PLAYER FILTER_ROLE < <(python3 - "$BASE" "$TOKEN" <<'PY'
import json
import sys
import urllib.request

base, token = sys.argv[1], sys.argv[2]
req = urllib.request.Request(
    base + "/immersive/v1/state",
    headers={"X-STK-Relay-Token": token},
    method="GET")
with urllib.request.urlopen(req, timeout=5) as response:
    state = json.loads(response.read().decode("utf-8"))

for phone in state.get("phone_states", []):
    player = phone.get("player_id", "")
    role = phone.get("role", "")
    if player and role:
        print(player, role)
        raise SystemExit(0)

raise SystemExit("no current phone_state available for filtered contract")
PY
)
"$SCRIPT_DIR/ValidateRelayState.py" \
    --base "$BASE" --token "$TOKEN" \
    --player-id "$FILTER_PLAYER" --role "$FILTER_ROLE"

echo "[D] FakeCrewPhones perfect 5 players"
"$SCRIPT_DIR/FakeCrewPhones.py" --base "$BASE" --token "$TOKEN" --players 5 --mode perfect

echo "[E] FakeCrewPhones perfect 4 players"
"$SCRIPT_DIR/FakeCrewPhones.py" --base "$BASE" --token "$TOKEN" --players 4 --mode perfect

echo "[F] FakeCrewPhones perfect 3 players"
"$SCRIPT_DIR/FakeCrewPhones.py" --base "$BASE" --token "$TOKEN" --players 3 --mode perfect

echo "[G] FakeCrewPhones perfect 2 players"
"$SCRIPT_DIR/FakeCrewPhones.py" --base "$BASE" --token "$TOKEN" --players 2 --mode perfect

echo "[H] wrong-role rejection"
"$SCRIPT_DIR/FakeCrewPhones.py" --base "$BASE" --token "$TOKEN" --players 6 --mode wrong-role

echo "[I] duplicate idempotency"
"$SCRIPT_DIR/FakeCrewPhones.py" --base "$BASE" --token "$TOKEN" --players 6 --mode duplicate

echo "[J] stale state rejection"
"$SCRIPT_DIR/FakeCrewPhones.py" --base "$BASE" --token "$TOKEN" --players 6 --mode stale

echo "[K] spam/rate-limit rejection"
"$SCRIPT_DIR/FakeCrewPhones.py" --base "$BASE" --token "$TOKEN" --players 6 --mode spam

echo "[L] invalid token rejection"
"$SCRIPT_DIR/FakeCrewPhones.py" --base "$BASE" --token "$TOKEN" --players 6 --mode invalid-token

echo "[M] hardening negative probes"
"$SCRIPT_DIR/ProbeRelayApiHardening.py" --base "$BASE" --token "$TOKEN" --players 6

echo "[N] disconnect/reconnect"
"$SCRIPT_DIR/FakeCrewPhones.py" --base "$BASE" --token "$TOKEN" --players 6 --mode disconnect-reconnect

echo "[O] accepted action not no-op"
"$SCRIPT_DIR/FakeCrewPhones.py" --base "$BASE" --token "$TOKEN" --players 6 --mode slow

echo "[done] relay phase 2 smoke passed"
