#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOKEN="${RELAY_API_TOKEN:-immersive-dev-token}"
BASE="${RELAY_API_BASE:-http://127.0.0.1:8890}"
JOBS="${JOBS:-8}"
FAKE_DELAY="${FAKE_DELAY:-0.35}"
OUT_DIR="${OUT_DIR:-/tmp/relay_phase35}"
VANILLA_BUILD="${VANILLA_BUILD:-cmake_build_vanilla}"
RELAY_BUILD="${RELAY_BUILD:-cmake_build_relay_phase1}"
RELAY_APP="$REPO_DIR/$RELAY_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"

cd "$REPO_DIR"
mkdir -p "$OUT_DIR"

if [[ "${SKIP_BUILDS:-0}" != "1" ]]; then
    echo "[A] build vanilla"
    cmake --build "$VANILLA_BUILD" --parallel "$JOBS"

    echo "[B] build relay"
    cmake -S "$REPO_DIR" -B "$RELAY_BUILD" \
        -DENABLE_IMMERSIVE_RELAY=ON \
        -DENABLE_RELAY_LOCAL_API=ON \
        -DENABLE_RELAY_TEST_TOOLS=ON >/dev/null
    cmake --build "$RELAY_BUILD" --parallel "$JOBS"
fi

if [[ ! -x "$RELAY_APP" ]]; then
    echo "Relay app not found: $RELAY_APP" >&2
    exit 1
fi

STK_PID=""
cleanup_server() {
    if [[ -n "$STK_PID" ]]; then
        kill "$STK_PID" >/dev/null 2>&1 || true
        wait "$STK_PID" >/dev/null 2>&1 || true
        STK_PID=""
    fi
}
trap cleanup_server EXIT

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
        req = urllib.request.Request(
            base + "/immersive/v1/state", headers=headers, method="GET")
        with urllib.request.urlopen(req, timeout=1) as response:
            state = json.loads(response.read().decode("utf-8"))
        if (state.get("ok") and state.get("relay_enabled") and
                state.get("test_accelerated_progress")):
            raise SystemExit(0)
    except Exception:
        time.sleep(0.5)
print("relay API readiness timed out", file=sys.stderr)
raise SystemExit(1)
PY
}

start_server() {
    local name="$1"
    cleanup_server
    pkill -f "$RELAY_APP" >/dev/null 2>&1 || true
    sleep 0.3
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
        --unlock-all >"$OUT_DIR/stk_${name}.log" 2>&1 &
    STK_PID=$!
    wait_for_api
}

run_case() {
    local name="$1"
    shift
    echo "[case] $name $*"
    start_server "$name"
    "$SCRIPT_DIR/FakeCrewPhones.py" \
        --base "$BASE" \
        --token "$TOKEN" \
        --session-id "phase35-$name" \
        --delay "$FAKE_DELAY" \
        "$@" | tee "$OUT_DIR/${name}.json"
    cleanup_server
}

run_case 2_perfect --players 2 --mode perfect
run_case 2_mistakes --players 2 --mode mistakes
run_case 3_perfect --players 3 --mode perfect
run_case 3_slow --players 3 --mode slow
run_case 4_perfect --players 4 --mode perfect
run_case 4_random --players 4 --mode random
run_case 5_perfect --players 5 --mode perfect
run_case 5_silent_navigator --players 5 --mode silent-role --silent-role navigator
run_case 6_perfect --players 6 --mode perfect
run_case 6_mistakes --players 6 --mode mistakes
run_case 6_slow --players 6 --mode slow
run_case 6_random --players 6 --mode random
run_case 6_silent_navigator --players 6 --mode silent-role --silent-role navigator
run_case 6_silent_engineer --players 6 --mode silent-role --silent-role engineer
run_case 6_silent_defender --players 6 --mode silent-role --silent-role defender
run_case 6_silent_hacker --players 6 --mode silent-role --silent-role hacker
run_case 6_silent_gunner --players 6 --mode silent-role --silent-role gunner

python3 - "$OUT_DIR" <<'PY'
import glob
import json
import os
import sys

out_dir = sys.argv[1]
print("[summary]")
print("case | players | mode | final | score | started/succeeded/failed | combo | accepted/rejected | max_phone/max_player")
for path in sorted(glob.glob(os.path.join(out_dir, "*.json"))):
    name = os.path.splitext(os.path.basename(path))[0]
    with open(path) as handle:
        data = json.load(handle)
    print("{} | {} | {} | {} | {} | {}/{}/{} | {} | {}/{} | {}/{}".format(
        name,
        data.get("player_count"),
        data.get("mode"),
        data.get("final_result"),
        data.get("score_end"),
        data.get("encounters_started"),
        data.get("encounters_succeeded"),
        data.get("encounters_failed"),
        data.get("combo_count"),
        data.get("accepted_count"),
        data.get("rejected_count"),
        data.get("max_actions_visible_per_phone"),
        data.get("max_actions_visible_per_player"),
    ))
PY

echo "[done] relay phase 3.5 audit passed; reports in $OUT_DIR"
