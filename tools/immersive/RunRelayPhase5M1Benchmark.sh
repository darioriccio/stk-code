#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_DIR"

RELAY_BUILD="${RELAY_BUILD:-cmake_build_relay_phase1}"
SYPHON_BUILD="${SYPHON_BUILD:-cmake_build_phase45_syphon}"
RESULT_DIR="${RESULT_DIR:-/tmp/stk_phase5_m1_benchmark}"
DURATION="${DURATION:-60}"
TOKEN="${TOKEN:-phase5-benchmark}"
PORT="${PORT:-27576}"
SERVER_NAME="${SERVER_NAME:-SuperTuxKart Immersive Relay Domemaster}"
RUN_SYPHON="${RUN_SYPHON:-1}"
APP="$RELAY_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
SYPHON_APP="$SYPHON_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
CSV="$RESULT_DIR/phase5_m1_benchmark.csv"
META="$RESULT_DIR/machine.json"
HARDWARE_TXT="$RESULT_DIR/hardware.txt"

mkdir -p "$RESULT_DIR"

machine_model="$(sysctl -n hw.model 2>/dev/null || echo unknown)"
cpu_brand="$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo Apple Silicon)"
ram_bytes="$(sysctl -n hw.memsize 2>/dev/null || echo 0)"
macos_version="$(sw_vers -productVersion 2>/dev/null || echo unknown)"
uname_s="$(uname -a)"
if [[ -d "$HOME/Library/Frameworks/Syphon.framework" ]]; then
    syphon_framework="$HOME/Library/Frameworks/Syphon.framework"
elif [[ -d "/Library/Frameworks/Syphon.framework" ]]; then
    syphon_framework="/Library/Frameworks/Syphon.framework"
else
    syphon_framework="not_found"
fi
is_mac_mini_m1_base="no"
if [[ "$machine_model" == "Macmini9,1" && "$ram_bytes" =~ ^[0-9]+$ && "$ram_bytes" -le 9000000000 ]]; then
    is_mac_mini_m1_base="yes"
elif [[ "$machine_model" == "Macmini9,1" ]]; then
    is_mac_mini_m1_base="mac_mini_m1_non_base_ram"
fi
cat >"$META" <<JSON
{
  "machine_model": "$machine_model",
  "cpu_gpu": "$cpu_brand",
  "ram_bytes": $ram_bytes,
  "macos_version": "$macos_version",
  "uname": "$uname_s",
  "syphon_framework": "$syphon_framework",
  "target_machine": "Mac mini M1 base",
  "is_mac_mini_m1_base": "$is_mac_mini_m1_base",
  "hardware_acceptance": "pending_until_target_run",
  "duration_seconds": $DURATION
}
JSON
{
    echo "date=$(date -Iseconds)"
    sw_vers || true
    echo
    echo "machine_model=$machine_model"
    echo "cpu_gpu=$cpu_brand"
    echo "ram_bytes=$ram_bytes"
    echo "syphon_framework=$syphon_framework"
    echo "is_mac_mini_m1_base=$is_mac_mini_m1_base"
    echo
    system_profiler SPHardwareDataType SPDisplaysDataType || true
} >"$HARDWARE_TXT"

run_case() {
    local mode="$1"
    local app="$2"
    local output="$3"
    local domemaster="$4"
    local cubemap="$5"
    local extra_notes="$6"
    local log="$RESULT_DIR/$mode.log"

    echo "== $mode output=$output domemaster=$domemaster cubemap=$cubemap"
    "$app" \
        --race-now \
        --profile-time="$DURATION" \
        --immersive-relay \
        --relay-local-api \
        --relay-api-port="$PORT" \
        --relay-api-token="$TOKEN" \
        --relay-player-count=6 \
        --relay-handoff-policy=lap_based_handoff \
        --immersive-output="$output" \
        --immersive-projection=fisheye_domemaster \
        --immersive-domemaster-size="$domemaster" \
        --immersive-cubemap-size="$cubemap" \
        --immersive-camera=stabilized_chase \
        --immersive-stats=on \
        --syphon-server-name="$SERVER_NAME" \
        --track=snowtuxpeak \
        --kart=tux \
        --numkarts=6 \
        --difficulty=0 \
        --unlock-all >"$log" 2>&1 &
    local pid=$!
    sleep "$((DURATION + 4))"
    if kill -0 "$pid" >/dev/null 2>&1; then
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
    fi
    "$SCRIPT_DIR/ParseImmersiveStats.py" \
        --log "$log" \
        --mode "$mode" \
        --output "$output" \
        --duration "${DURATION}s" \
        --skip-samples 2 \
        --notes "$machine_model $macos_version $extra_notes" \
        --csv "$CSV" \
        ${CSV_CREATED:+--append}
    CSV_CREATED=1
}

run_16x9_case() {
    local log="$RESULT_DIR/relay_16x9_no_immersive.log"
    echo "== relay_16x9_no_immersive"
    "$APP" \
        --race-now \
        --profile-time="$DURATION" \
        --immersive-relay \
        --relay-player-count=6 \
        --relay-handoff-policy=lap_based_handoff \
        --immersive-output=none \
        --track=snowtuxpeak \
        --kart=tux \
        --numkarts=6 \
        --difficulty=0 \
        --unlock-all >"$log" 2>&1 &
    local pid=$!
    sleep "$((DURATION + 4))"
    if kill -0 "$pid" >/dev/null 2>&1; then
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
    fi
    if [[ -z "${CSV_CREATED:-}" ]]; then
        printf "mode,cubemap,domemaster,output,duration,avg_fps,avg_ms,p50_ms,p95_ms,p99_ms,min_fps,max_fps,cubemap_ms,projection_ms,syphon_ms,capture_ms,output_ms,dropped,samples,notes\n" >"$CSV"
        CSV_CREATED=1
    fi
    printf "relay_16x9_no_immersive,,,main,%ss,not measured,not measured,not measured,not measured,not measured,not measured,not measured,not measured,not measured,not measured,not measured,not measured,not measured,0,%s %s no immersive stats\n" \
        "$DURATION" "$machine_model" "$macos_version" >>"$CSV"
}

if [[ ! -x "$APP" ]]; then
    echo "Missing relay app: $APP" >&2
    exit 1
fi

run_16x9_case
run_case fisheye_1024_window "$APP" window 1024 512 "debug preset"
run_case fisheye_1536_window "$APP" window 1536 768 "safe preset"
run_case fisheye_2048_window "$APP" window 2048 1024 "target preset"

if [[ "$RUN_SYPHON" == "1" && -x "$SYPHON_APP" ]]; then
    run_case fisheye_1024_syphon "$SYPHON_APP" syphon 1024 512 "debug preset syphon"
    run_case fisheye_1536_syphon "$SYPHON_APP" syphon 1536 768 "safe preset syphon"
    run_case fisheye_2048_syphon "$SYPHON_APP" syphon 2048 1024 "target preset syphon"
else
    echo "SKIPPED Syphon benchmark: RUN_SYPHON=$RUN_SYPHON or Syphon app missing."
fi

echo "Benchmark CSV: $CSV"
echo "Machine metadata: $META"
echo "Hardware inventory: $HARDWARE_TXT"
