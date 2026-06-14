#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

JOBS="${JOBS:-8}"
RUN_BUILDS="${RUN_BUILDS:-1}"
RUN_GRAPHICS="${RUN_GRAPHICS:-1}"
RUN_RECEIVER="${RUN_RECEIVER:-1}"
RUN_BENCHMARKS="${RUN_BENCHMARKS:-1}"
RUN_GAMEPLAY="${RUN_GAMEPLAY:-1}"
DURATION="${DURATION:-60}"
WARMUP="${WARMUP:-10}"
WARMUP_SAMPLES="${WARMUP_SAMPLES:-$((WARMUP * 30))}"
RESULT_DIR="${RESULT_DIR:-/tmp/stk_phase45_$(date +%Y%m%d_%H%M%S)}"
SERVER_NAME="${SYPHON_SERVER_NAME:-SuperTuxKart Immersive Relay Domemaster}"

VANILLA_BUILD="${VANILLA_BUILD:-cmake_build_phase45_vanilla}"
RELAY_BUILD="${RELAY_BUILD:-cmake_build_phase45_relay}"
SYPHON_BUILD="${SYPHON_BUILD:-cmake_build_phase45_syphon}"

VANILLA_APP="$REPO_DIR/$VANILLA_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
RELAY_APP="$REPO_DIR/$RELAY_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
SYPHON_APP="$REPO_DIR/$SYPHON_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
PROBE="$RESULT_DIR/SyphonReceiverProbe"
CSV="$RESULT_DIR/phase45_benchmark.csv"

mkdir -p "$RESULT_DIR"
cd "$REPO_DIR"

log_step() {
    printf '\n[%s] %s\n' "$1" "$2" | tee -a "$RESULT_DIR/phase45.log"
}

resolve_syphon_path() {
    local candidate="${SYPHON_FRAMEWORK_PATH:-}"
    if [[ -n "$candidate" ]]; then
        echo "$candidate"
        return 0
    fi
    for path in \
        "$HOME/Library/Frameworks/Syphon.framework" \
        "/Library/Frameworks/Syphon.framework" \
        "/opt/homebrew/Frameworks/Syphon.framework" \
        "/usr/local/Frameworks/Syphon.framework"; do
        if [[ -d "$path" ]]; then
            echo "$path"
            return 0
        fi
    done
    return 1
}

configure_and_build() {
    local build_dir="$1"
    shift
    cmake -S "$REPO_DIR" -B "$build_dir" "$@"
    cmake --build "$build_dir" --target supertuxkart --parallel "$JOBS"
}

write_system_info() {
    {
        echo "date=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
        echo "uname=$(uname -a)"
        if command -v sw_vers >/dev/null 2>&1; then
            sw_vers
        fi
        if command -v sysctl >/dev/null 2>&1; then
            sysctl -n machdep.cpu.brand_string 2>/dev/null | sed 's/^/cpu=/'
            sysctl -n hw.memsize 2>/dev/null | awk '{printf "mem_bytes=%s\n", $1}'
        fi
        if command -v system_profiler >/dev/null 2>&1; then
            system_profiler SPHardwareDataType 2>/dev/null |
                sed -n 's/^[[:space:]]*//p' | head -40
        fi
    } > "$RESULT_DIR/system_info.txt"
}

run_cmake_failure_checks() {
    log_step CMAKE "failure mode checks"
    local fail_dir="$RESULT_DIR/cmake_fail_syphon_without_relay"
    if cmake -S "$REPO_DIR" -B "$fail_dir" \
        -DENABLE_IMMERSIVE_RELAY=OFF \
        -DENABLE_SYPHON_OUTPUT=ON \
        >"$RESULT_DIR/cmake_fail_syphon_without_relay.log" 2>&1; then
        echo "FAIL expected ENABLE_SYPHON_OUTPUT without relay to fail" >&2
        exit 1
    fi
    rg -q "requires ENABLE_IMMERSIVE_RELAY=ON" \
        "$RESULT_DIR/cmake_fail_syphon_without_relay.log"
    echo "PASS Syphon without relay fails clearly"

    if [[ "$(uname -s)" == "Darwin" ]]; then
        fail_dir="$RESULT_DIR/cmake_fail_missing_syphon"
        if cmake -S "$REPO_DIR" -B "$fail_dir" \
            -DENABLE_IMMERSIVE_RELAY=ON \
            -DENABLE_SYPHON_OUTPUT=ON \
            -DSYPHON_FRAMEWORK_PATH="$RESULT_DIR/does_not_exist/Syphon.framework" \
            >"$RESULT_DIR/cmake_fail_missing_syphon.log" 2>&1; then
            echo "FAIL expected missing Syphon.framework to fail" >&2
            exit 1
        fi
        rg -q "SYPHON_FRAMEWORK_PATH must point to Syphon.framework" \
            "$RESULT_DIR/cmake_fail_missing_syphon.log"
        echo "PASS missing Syphon.framework fails clearly"
    else
        echo "SKIPPED missing-framework check: non-macOS."
    fi
}

build_matrix() {
    if [[ "$RUN_BUILDS" != "1" ]]; then
        return 0
    fi

    log_step BUILD "vanilla"
    configure_and_build "$VANILLA_BUILD" \
        -DENABLE_IMMERSIVE_RELAY=OFF \
        -DENABLE_RELAY_LOCAL_API=OFF \
        -DENABLE_RELAY_TEST_TOOLS=OFF \
        -DENABLE_SYPHON_OUTPUT=OFF \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo

    log_step BUILD "relay without Syphon"
    configure_and_build "$RELAY_BUILD" \
        -DENABLE_IMMERSIVE_RELAY=ON \
        -DENABLE_RELAY_LOCAL_API=ON \
        -DENABLE_RELAY_TEST_TOOLS=ON \
        -DENABLE_SYPHON_OUTPUT=OFF \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo

    log_step BUILD "relay with Syphon"
    if [[ "$(uname -s)" != "Darwin" ]]; then
        echo "SKIPPED Syphon build: macOS-only." | tee "$RESULT_DIR/syphon_build.skipped"
        return 0
    fi
    local syphon_path
    if ! syphon_path="$(resolve_syphon_path)"; then
        echo "SKIPPED Syphon build: Syphon.framework not found." | tee "$RESULT_DIR/syphon_build.skipped"
        return 0
    fi
    configure_and_build "$SYPHON_BUILD" \
        -DENABLE_IMMERSIVE_RELAY=ON \
        -DENABLE_RELAY_LOCAL_API=ON \
        -DENABLE_RELAY_TEST_TOOLS=ON \
        -DENABLE_SYPHON_OUTPUT=ON \
        -DSYPHON_FRAMEWORK_PATH="$syphon_path" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo
}

check_help() {
    log_step HELP "vanilla/relay flags"
    "$VANILLA_APP" --help >"$RESULT_DIR/help_vanilla.txt" 2>&1
    if rg -q -- '--immersive-output|--immersive-relay|--relay-local-api|--syphon-server-name' \
        "$RESULT_DIR/help_vanilla.txt"; then
        echo "FAIL vanilla help exposes relay/immersive flags" >&2
        exit 1
    fi
    "$RELAY_APP" --help >"$RESULT_DIR/help_relay.txt" 2>&1
    for flag in --immersive-output --immersive-projection \
                --immersive-domemaster-size --immersive-cubemap-size \
                --immersive-camera --immersive-yaw-deg --syphon-server-name; do
        rg -q -- "$flag" "$RESULT_DIR/help_relay.txt"
    done
    echo "PASS help checks"
}

run_gameplay_regression() {
    if [[ "$RUN_GAMEPLAY" != "1" ]]; then
        echo "SKIPPED gameplay regression: RUN_GAMEPLAY=0"
        return 0
    fi
    log_step GAMEPLAY "Phase 4 regression smoke with FakeCrewPhones"
    SKIP_BUILDS=1 RUN_GRAPHICS=0 \
        VANILLA_BUILD="$VANILLA_BUILD" \
        RELAY_BUILD="$RELAY_BUILD" \
        SYPHON_BUILD="$SYPHON_BUILD" \
        "$SCRIPT_DIR/RunRelayPhase4RenderSmoke.sh" \
        >"$RESULT_DIR/phase4_regression.log" 2>&1
    echo "PASS gameplay regression smoke"
}

common_race_args() {
    local profile_time="$1"
    shift
    echo \
        --race-now \
        --profile-time="$profile_time" \
        --immersive-relay \
        --relay-test-accelerated-progress \
        --relay-player-count=6 \
        --relay-handoff-policy=lap_based_handoff \
        --track=snowtuxpeak \
        --kart=tux \
        --numkarts=6 \
        --difficulty=0 \
        --unlock-all
}

run_benchmark() {
    local label="$1"
    local app="$2"
    local output="$3"
    local domemaster="$4"
    local cubemap="$5"
    local log_file="$RESULT_DIR/${label}.log"
    local runtime=$((WARMUP + DURATION + 5))
    local profile_time=$((WARMUP + DURATION + 3))
    local extra_notes="$6"

    log_step BENCH "$label"
    "$app" \
        $(common_race_args "$profile_time") \
        --immersive-output="$output" \
        --immersive-projection=fisheye \
        --immersive-domemaster-size="$domemaster" \
        --immersive-cubemap-size="$cubemap" \
        --immersive-camera=stabilized_chase \
        --immersive-yaw-deg=0 \
        --immersive-pitch-deg=0 \
        --immersive-roll-deg=0 \
        --immersive-dome-tilt-deg=0 \
        --immersive-horizon-stabilization=on \
        --immersive-roll-limit-deg=12 \
        --immersive-debug-grid=on \
        --immersive-stats=on \
        --syphon-server-name="$SERVER_NAME" \
        >"$log_file" 2>&1 &
    local stk_pid=$!

    if [[ "$output" == "syphon" && "$RUN_RECEIVER" == "1" && -x "$PROBE" ]]; then
        sleep 4
        "$PROBE" \
            --server-name "$SERVER_NAME" \
            --expected-width "$domemaster" \
            --expected-height "$domemaster" \
            --duration "$DURATION" \
            --timeout 20 \
            --min-frames 30 \
            --sample-every 30 \
            --save-ppm "$RESULT_DIR/${label}.ppm" \
            >"$RESULT_DIR/${label}_receiver.log" 2>&1 || true
    else
        sleep "$runtime"
    fi

    kill "$stk_pid" >/dev/null 2>&1 || true
    wait "$stk_pid" >/dev/null 2>&1 || true

    "$SCRIPT_DIR/ParseImmersiveStats.py" \
        --log "$log_file" \
        --mode "$label" \
        --output "$output" \
        --duration "${DURATION}s" \
        --skip-samples "$WARMUP_SAMPLES" \
        --notes "$extra_notes" \
        --csv "$CSV" \
        ${CSV_CREATED:+--append}
    CSV_CREATED=1
}

run_none_regression_row() {
    local label="$1"
    local log_file="$RESULT_DIR/${label}.log"
    local runtime=$((WARMUP + DURATION + 5))
    log_step BENCH "$label"
    "$RELAY_APP" \
        $(common_race_args "$((WARMUP + DURATION + 3))") \
        --immersive-output=none \
        --immersive-stats=on \
        >"$log_file" 2>&1 &
    local stk_pid=$!
    sleep "$runtime"
    kill "$stk_pid" >/dev/null 2>&1 || true
    wait "$stk_pid" >/dev/null 2>&1 || true
    "$SCRIPT_DIR/ParseImmersiveStats.py" \
        --log "$log_file" \
        --mode "$label" \
        --output "main" \
        --duration "${DURATION}s" \
        --notes "run regression only; no immersive timing emitted for output=none" \
        --csv "$CSV" \
        ${CSV_CREATED:+--append}
    CSV_CREATED=1
}

run_benchmarks() {
    if [[ "$RUN_BENCHMARKS" != "1" || "$RUN_GRAPHICS" != "1" ]]; then
        echo "SKIPPED benchmarks: set RUN_GRAPHICS=1 RUN_BENCHMARKS=1."
        return 0
    fi

    log_step PROBE "build receiver"
    if [[ "$(uname -s)" == "Darwin" ]]; then
        "$SCRIPT_DIR/BuildSyphonReceiverProbe.sh" "$PROBE" >"$RESULT_DIR/build_probe.log"
    else
        echo "SKIPPED receiver probe build: non-macOS." >"$RESULT_DIR/build_probe.log"
    fi

    CSV_CREATED=""
    run_none_regression_row "relay_16x9_no_immersive"
    run_benchmark "fisheye_1024_window" "$RELAY_APP" window 1024 512 "debug preset"
    run_benchmark "fisheye_1536_window" "$RELAY_APP" window 1536 768 "safe/test preset"
    run_benchmark "fisheye_2048_window" "$RELAY_APP" window 2048 1024 "target preset"

    if [[ -x "$SYPHON_APP" ]]; then
        run_benchmark "fisheye_1024_syphon" "$SYPHON_APP" syphon 1024 512 "receiver probe expected 1024x1024"
        run_benchmark "fisheye_1536_syphon" "$SYPHON_APP" syphon 1536 768 "receiver probe expected 1536x1536"
        run_benchmark "fisheye_2048_syphon" "$SYPHON_APP" syphon 2048 1024 "receiver probe expected 2048x2048"
    else
        echo "SKIPPED Syphon benchmarks: Syphon app not built." | tee "$RESULT_DIR/syphon_bench.skipped"
    fi
}

write_system_info
run_cmake_failure_checks
build_matrix
check_help
run_gameplay_regression
run_benchmarks

log_step DONE "Phase 4.5 validation artifacts"
echo "$RESULT_DIR"
if [[ -f "$CSV" ]]; then
    cat "$CSV"
fi
