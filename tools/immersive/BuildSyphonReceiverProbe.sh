#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-$SCRIPT_DIR/SyphonReceiverProbe}"

resolve_framework_dir() {
    local candidate="${SYPHON_FRAMEWORK_PATH:-}"
    if [[ -z "$candidate" ]]; then
        for path in \
            "$HOME/Library/Frameworks/Syphon.framework" \
            "/Library/Frameworks/Syphon.framework" \
            "/opt/homebrew/Frameworks/Syphon.framework" \
            "/usr/local/Frameworks/Syphon.framework"; do
            if [[ -d "$path" ]]; then
                candidate="$path"
                break
            fi
        done
    fi

    if [[ -z "$candidate" ]]; then
        return 1
    fi

    if [[ -d "$candidate/Syphon.framework" ]]; then
        dirname "$candidate/Syphon.framework"
    elif [[ -d "$candidate" && -f "$candidate/Syphon" ]]; then
        dirname "$candidate"
    else
        return 1
    fi
}

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "SKIPPED: SyphonReceiverProbe is macOS-only." >&2
    exit 2
fi

FRAMEWORK_DIR="$(resolve_framework_dir || true)"
if [[ -z "$FRAMEWORK_DIR" ]]; then
    echo "ERROR: Syphon.framework not found." >&2
    echo "Set SYPHON_FRAMEWORK_PATH=/path/to/Syphon.framework or install it in ~/Library/Frameworks." >&2
    exit 3
fi

clang++ \
    -std=c++17 \
    -fblocks \
    -DGL_SILENCE_DEPRECATION \
    -x objective-c++ \
    -F"$FRAMEWORK_DIR" \
    "$SCRIPT_DIR/SyphonReceiverProbe.mm" \
    -o "$OUT" \
    -framework Syphon \
    -framework Cocoa \
    -framework OpenGL \
    -Wl,-rpath,"$FRAMEWORK_DIR"

echo "$OUT"
