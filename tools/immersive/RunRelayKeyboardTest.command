#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="${0:A:h}"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
APP="$REPO_DIR/cmake_build_relay_phase1/bin/supertuxkart.app/Contents/MacOS/supertuxkart"

if [[ ! -x "$APP" ]]; then
    echo "Relay build not found at:"
    echo "$APP"
    echo
    echo "Build it first with:"
    echo "cmake --build \"$REPO_DIR/cmake_build_relay_phase1\" --parallel 8"
    exit 1
fi

cd "$REPO_DIR"

PLAYER_COUNT="${RELAY_PLAYER_COUNT:-}"
if [[ -z "$PLAYER_COUNT" ]]; then
    PLAYER_COUNT="$(osascript <<'APPLESCRIPT'
set playerChoices to {"1 - Solo, nessun cambio", "2 - 2 piloti", "3 - 3 piloti", "4 - 4 piloti", "5 - 5 piloti", "6 - 6 piloti"}
set picked to choose from list playerChoices with title "Immersive Relay" with prompt "Quanti piloti umani giocano?" default items {"1 - Solo, nessun cambio"} OK button name "Avvia" cancel button name "Annulla"
if picked is false then
    return "cancel"
end if
set pickedText to item 1 of picked
return first character of pickedText
APPLESCRIPT
)"
fi

if [[ "$PLAYER_COUNT" == "cancel" ]]; then
    exit 0
fi

if [[ ! "$PLAYER_COUNT" =~ '^[1-6]$' ]]; then
    echo "Invalid relay player count: $PLAYER_COUNT"
    exit 1
fi

exec "$APP" \
    --windowed \
    --screensize=1280x720 \
    -R \
    --immersive-relay \
    --relay-arcade-ui \
    --relay-local-api \
    --relay-api-token=immersive-dev-token \
    --relay-player-count="$PLAYER_COUNT" \
    --relay-handoff-policy=lap_based_handoff \
    --mode=0 \
    --track=snowtuxpeak \
    --kart=tux \
    --numkarts=6 \
    --difficulty=0 \
    --unlock-all
