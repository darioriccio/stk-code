#!/bin/zsh
set -euo pipefail
# Resolve Finder/workspace symlinks before locating the repository.
SCRIPT_DIR="${0:A:h}"
REPO_DIR="${SCRIPT_DIR:h:h}"
BUILD_DIR="${RELAY_BUILD_DIR:-$REPO_DIR/cmake_build_mvp_syphon}"
APP="$BUILD_DIR/bin/supertuxkart.app/Contents/MacOS/supertuxkart"
export SUPERTUXKART_ASSETS_DIR="${SUPERTUXKART_ASSETS_DIR:-${REPO_DIR:h}/stk-assets}"
export SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS="${SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS:-1}"
PLAYER_COUNT="${RELAY_PLAYER_COUNT:-1}"
API_PORT="${RELAY_API_PORT:-8890}"
if [[ ! "$PLAYER_COUNT" =~ '^[1-6]$' ]] || \
   [[ ! "$API_PORT" =~ '^[0-9]{1,5}$' ]] || (( API_PORT < 1 || API_PORT > 65535 )); then
    print -u2 'RELAY_PLAYER_COUNT deve essere 1–6 e RELAY_API_PORT 1–65535.'
    exit 2
fi
PROJECTION="${RELAY_PROJECTION:-fisheye_domemaster}"
CUBEMAP_SIZE="${RELAY_CUBEMAP_SIZE:-768}"
PROJECTION_ARGS=()
PRESENTATION_ARGS=()
case "${RELAY_PRESENTATION_REQUIRED:-0}" in
    0) ;;
    1) PRESENTATION_ARGS+=(--relay-presentation-required) ;;
    *) print -u2 'RELAY_PRESENTATION_REQUIRED deve essere 0 o 1.'; exit 2 ;;
esac
PITCH=35
case "$PROJECTION" in
    fisheye|fisheye_domemaster|domemaster)
        PROJECTION_WIDTH="${RELAY_PROJECTION_WIDTH:-${RELAY_PROJECTION_HEIGHT:-1536}}"
        PROJECTION_HEIGHT="${RELAY_PROJECTION_HEIGHT:-$PROJECTION_WIDTH}"
        case "$PROJECTION_WIDTH:$PROJECTION_HEIGHT" in
            1024:1024|1536:1536|2048:2048) ;;
            *) print -u2 'Dimensioni domemaster non valide: usare 1024x1024, 1536x1536 o 2048x2048.'; exit 2 ;;
        esac
        PROJECTION_ARGS+=(--immersive-domemaster-size="$PROJECTION_WIDTH")
        ;;
    equirect|equirectangular)
        PROJECTION_WIDTH="${RELAY_PROJECTION_WIDTH:-2048}"
        PROJECTION_HEIGHT="${RELAY_PROJECTION_HEIGHT:-1024}"
        case "$PROJECTION_WIDTH:$PROJECTION_HEIGHT" in
            2048:1024|4096:2048|8192:4096) ;;
            *) print -u2 'Dimensioni equirettangolari non valide: usare 2048x1024, 4096x2048 o 8192x4096.'; exit 2 ;;
        esac
        PROJECTION_ARGS+=(--immersive-projection-width="$PROJECTION_WIDTH" --immersive-projection-height="$PROJECTION_HEIGHT")
        PITCH=0
        ;;
    *) print -u2 "Proiezione non disponibile: $PROJECTION"; exit 2 ;;
esac
case "$CUBEMAP_SIZE" in
    512|768|1024|1536) ;;
    *) print -u2 'RELAY_CUBEMAP_SIZE deve essere 512, 768, 1024 o 1536.'; exit 2 ;;
esac
if [[ ! -x "$APP" ]]; then
    print -u2 "Build MVP assente: $APP"
    print -u2 "Esegui: $SCRIPT_DIR/BuildRelayMVP.sh"
    exit 1
fi
if [[ ! -f "$SUPERTUXKART_ASSETS_DIR/tracks/snowtuxpeak/track.xml" ]]; then
    print -u2 "Asset SuperTuxKart assenti: $SUPERTUXKART_ASSETS_DIR"
    exit 1
fi
cd "$REPO_DIR"
# exec preserves ownership: TholusOS can stop only the process it started.
exec "$APP" --render-driver=opengl --windowed --screensize=1280x720 -R \
    --immersive-relay --relay-arcade-ui --relay-operator-controlled "${PRESENTATION_ARGS[@]}" \
    --relay-local-api --relay-api-bind=127.0.0.1 \
    --relay-api-port="$API_PORT" --relay-api-token="${RELAY_API_TOKEN:-immersive-dev-token}" \
    --relay-player-count="$PLAYER_COUNT" --relay-handoff-policy=lap_based_handoff \
    --mode=0 --track=snowtuxpeak --kart=tux --numkarts=4 --difficulty=0 --unlock-all \
    --immersive-output=syphon --immersive-projection="$PROJECTION" \
    "${PROJECTION_ARGS[@]}" --immersive-cubemap-size="$CUBEMAP_SIZE" \
    --immersive-camera=stabilized_chase --immersive-pitch-deg="$PITCH" \
    --immersive-forward-offset=0.25 --immersive-vertical-offset=0.4 \
    --immersive-smoothing-factor=0.16 --immersive-horizon-stabilization=on \
    --immersive-roll-limit-deg=12 "$@"
