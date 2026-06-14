#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

VANILLA_BUILD="${VANILLA_BUILD:-cmake_build_vanilla}"
RELAY_BUILD="${RELAY_BUILD:-cmake_build_relay_phase1}"

VANILLA_EXE="$REPO_DIR/$VANILLA_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
RELAY_EXE="$REPO_DIR/$RELAY_BUILD/bin/supertuxkart.app/Contents/MacOS/supertuxkart"

failures=0

pass() {
    printf '[ok] %s\n' "$1"
}

fail() {
    printf '[fail] %s\n' "$1" >&2
    failures=$((failures + 1))
}

contains_binary_string() {
    local exe="$1"
    local needle="$2"
    strings -a "$exe" | grep -Fq -- "$needle"
}

help_contains() {
    local exe="$1"
    local needle="$2"
    "$exe" --help 2>&1 | grep -Fq -- "$needle"
}

if [[ -x "$VANILLA_EXE" ]]; then
    pass "vanilla executable exists"
else
    fail "vanilla executable missing: $VANILLA_EXE"
fi

if [[ -x "$RELAY_EXE" ]]; then
    pass "relay executable exists"
else
    fail "relay executable missing: $RELAY_EXE"
fi

if [[ -x "$VANILLA_EXE" ]]; then
    if contains_binary_string "$VANILLA_EXE" "--immersive-relay"; then
        fail "vanilla binary contains --immersive-relay"
    else
        pass "vanilla binary does not contain --immersive-relay"
    fi

    if contains_binary_string "$VANILLA_EXE" "ImmersiveRelay"; then
        fail "vanilla binary contains ImmersiveRelay"
    else
        pass "vanilla binary does not contain ImmersiveRelay"
    fi
fi

if [[ -x "$RELAY_EXE" ]]; then
    if help_contains "$RELAY_EXE" "--immersive-relay"; then
        pass "relay help exposes --immersive-relay"
    else
        fail "relay help does not expose --immersive-relay"
    fi
fi

core_files=(
    "CMakeLists.txt"
    "src/main.cpp"
    "src/modes/world.cpp"
    "src/race/race_manager.cpp"
)

for file in "${core_files[@]}"; do
    path="$REPO_DIR/$file"
    if [[ ! -f "$path" ]]; then
        fail "core file missing: $file"
        continue
    fi

    if grep -Fq "THOLUS_IMMERSIVE_RELAY_HOOK_BEGIN" "$path" &&
       grep -Fq "THOLUS_IMMERSIVE_RELAY_HOOK_END" "$path"; then
        pass "THOLUS markers present in $file"
    else
        fail "THOLUS markers missing in $file"
    fi
done

if [[ -d "$REPO_DIR/src/modes/immersive_relay" ]]; then
    pass "relay modules directory exists under src/modes/immersive_relay"
else
    fail "relay modules directory missing under src/modes/immersive_relay"
fi

unexpected_paths="$(
    find "$REPO_DIR/src" -type f -path '*immersive_relay*' \
        ! -path "$REPO_DIR/src/modes/immersive_relay/*" -print
)"
if [[ -n "$unexpected_paths" ]]; then
    printf '%s\n' "$unexpected_paths" >&2
    fail "found immersive_relay files outside src/modes/immersive_relay"
else
    pass "all immersive_relay source paths stay under src/modes/immersive_relay"
fi

vanilla_metadata=()
for path in \
    "$REPO_DIR/$VANILLA_BUILD/CMakeFiles/supertuxkart.dir" \
    "$REPO_DIR/$VANILLA_BUILD/Makefile" \
    "$REPO_DIR/$VANILLA_BUILD/cmake_install.cmake"; do
    if [[ -e "$path" ]]; then
        vanilla_metadata+=("$path")
    fi
done

if [[ ${#vanilla_metadata[@]} -eq 0 ]]; then
    fail "vanilla build metadata missing; build $VANILLA_BUILD first"
elif grep -R -Fq "src/modes/immersive_relay" "${vanilla_metadata[@]}"; then
    fail "vanilla target metadata references src/modes/immersive_relay"
else
    pass "vanilla target metadata does not reference relay sources"
fi

if [[ "$failures" -ne 0 ]]; then
    printf '[fail] thin fork check failed with %d issue(s)\n' "$failures" >&2
    exit 1
fi

pass "thin fork check passed"
