#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${RELAY_BUILD_DIR:-$REPO_DIR/cmake_build_mvp_syphon}"
cmake -S "$REPO_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=STKRelease \
    -DCMAKE_PREFIX_PATH=/opt/homebrew/opt \
    -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@1.1 \
    -DENABLE_IMMERSIVE_RELAY=ON -DENABLE_RELAY_LOCAL_API=ON \
    -DENABLE_SYPHON_OUTPUT=ON -DENABLE_RELAY_TEST_TOOLS=OFF \
    -DSYPHON_FRAMEWORK_PATH="${SYPHON_FRAMEWORK_PATH:-$HOME/Library/Frameworks/Syphon.framework}"
cmake --build "$BUILD_DIR" --parallel "${RELAY_BUILD_JOBS:-6}"
