# Immersive Relay Upstream Sync Notes

This fork keeps Immersive Relay isolated behind build flags so upstream
SuperTuxKart rebases stay small and reviewable.

## Core Files Touched

The only upstream/core files expected to carry relay hooks are:

```text
CMakeLists.txt
src/graphics/fixed_pipeline_renderer.cpp
src/graphics/render_target.cpp
src/graphics/render_target.hpp
src/graphics/shader_based_renderer.cpp
src/main.cpp
src/main_loop.cpp
src/modes/world.cpp
src/race/race_manager.cpp
src/states_screens/kart_selection.cpp
src/states_screens/track_info_screen.cpp
src/states_screens/track_info_screen.hpp
```

Each relay hook in those files is bracketed with:

```text
THOLUS_IMMERSIVE_RELAY_HOOK_BEGIN
THOLUS_IMMERSIVE_RELAY_HOOK_END
```

## Why They Are Touched

`CMakeLists.txt`

Defines the relay feature flags, fails early for invalid flag combinations, and
filters relay sources out of the vanilla target when `ENABLE_IMMERSIVE_RELAY`
is off. Phase 4 also links `Syphon.framework` only when
`ENABLE_SYPHON_OUTPUT=ON`.

`src/graphics/shader_based_renderer.cpp` and
`src/graphics/fixed_pipeline_renderer.cpp`

Call `ImmersiveRenderPipeline::renderFrame(dt)` after the normal scene/UI render
and before the final swap. This is the only render-loop hook for domemaster
output.

`src/graphics/render_target.cpp` and `.hpp`

Expose the RTT color texture to relay builds and preserve the outer RTT during
nested cubemap face renders. The additional methods are behind
`ENABLE_IMMERSIVE_RELAY`.

`src/main.cpp`

Adds command-line help and delegates relay command-line parsing, race defaults,
local API startup and shutdown to `RelayMainHooks`.

`src/main_loop.cpp`

Drains authenticated operator jobs between frames when `ENABLE_RELAY_LOCAL_API`
is enabled. Scene changes never run on the socket thread or inside World::update.

`src/modes/world.cpp`

Installs `RelayPlayerController` only when the relay runtime is enabled.

`src/race/race_manager.cpp`

Creates `RelayRaceMode` only when the relay runtime is enabled. Unsupported
minor modes are folded back to normal race mode without changing the handoff
policy.

`src/states_screens/kart_selection.cpp`

When `--relay-arcade-ui` is active, bypasses the vanilla race setup screen
after kart selection and sends players directly to track selection with normal
race mode selected.

`src/states_screens/track_info_screen.cpp` and `.hpp`

When the relay runtime is active, locks lap count and AI count to values derived
from relay configuration. When `--relay-arcade-ui` is active, hides the vanilla
race-options box so players see track champions/high scores and Start Race, not
venue-controlled settings.

Important wording: track records/champions/high scores are intentionally kept
visible for arcade motivation. The hidden "record" control is only the vanilla
technical checkbox for recording a ghost replay, not the leaderboard.

## Launcher Rule For Tests

A relay-capable binary is not the same thing as a relay session. Opening the
relay `.app` directly, or opening a vanilla build, does not enable Immersive
Relay by itself. Without `--immersive-relay` the game intentionally behaves like
vanilla SuperTuxKart, including the normal menus and track settings.

For relay or sala giochi tests, always use a launcher/command that passes the
relay runtime flags:

```bash
--immersive-relay --relay-local-api --relay-player-count=N
```

For the arcade/venue UI path, also pass:

```bash
--relay-arcade-ui
```

Current local launchers:

```text
../Apri Immersive Relay Phase 2.command
../Apri Immersive Relay Sala Giochi.command
```

Use the first for direct keyboard race tests. Use the second for the player
flow: choose kart, choose track, see champions/high scores, then play. Those
track records should remain visible in sala giochi mode. If a test shows
vanilla options such as editable laps, AI karts, reverse, or ghost replay
recording, first verify that the session was launched through one of these relay
launchers and not by opening the `.app` directly.

## After A Rebase

Check the following:

```bash
rg -n "THOLUS_IMMERSIVE_RELAY_HOOK" \
  CMakeLists.txt src/main.cpp src/modes/world.cpp src/race/race_manager.cpp
```

Make sure:

- every relay hook is still inside `#ifdef ENABLE_IMMERSIVE_RELAY` or a CMake
  feature flag block;
- vanilla builds do not mention relay command-line flags;
- relay builds still expose `--immersive-relay` and `--relay-arcade-ui`;
- relay Phase 4 builds expose `--immersive-output`,
  `--immersive-projection`, and `--syphon-server-name`;
- relay sources remain under `src/modes/immersive_relay/`;
- Syphon remains isolated under `src/modes/immersive_relay/output/syphon_mac/`
  and is not compiled unless `ENABLE_SYPHON_OUTPUT=ON`;
- no Spout, equirectangular, PWA, or future venue-management work leaks into
  Phase 4.

## Build Commands

Vanilla:

```bash
cmake --build cmake_build_vanilla --parallel 8
```

Relay:

```bash
cmake --build cmake_build_relay_phase1 --parallel 8
```

## Vanilla Has No Relay Flags

```bash
cmake_build_vanilla/bin/supertuxkart.app/Contents/MacOS/supertuxkart \
  --help 2>&1 | rg -- '--immersive-relay|--relay-local-api|Immersive Relay'
```

Expected result: no output.

Binary string check:

```bash
strings -a cmake_build_vanilla/bin/supertuxkart.app/Contents/MacOS/supertuxkart \
  | rg -- '--immersive-relay|ImmersiveRelay|relay-local-api'
```

Expected result: no output.

## Relay Exposes Relay Flags

```bash
cmake_build_relay_phase1/bin/supertuxkart.app/Contents/MacOS/supertuxkart \
  --help 2>&1 | rg -- '--immersive-relay|--relay-arcade-ui|--relay-local-api'
```

Expected result: all three flags are shown.

## Relay Sources Stay Out Of Vanilla Target

```bash
rg -n "src/modes/immersive_relay" \
  cmake_build_vanilla/CMakeFiles/supertuxkart.dir \
  cmake_build_vanilla/Makefile \
  cmake_build_vanilla/cmake_install.cmake
```

Expected result: no output.

Relay target should include those sources:

```bash
rg -n "src/modes/immersive_relay" \
  cmake_build_relay_phase1/CMakeFiles/supertuxkart.dir | head
```

## Phase 4 Render Smoke

```bash
tools/immersive/RunRelayPhase4RenderSmoke.sh
```

For local graphical preview and domemaster stats:

```bash
SKIP_BUILDS=1 RUN_GRAPHICS=1 tools/immersive/RunRelayPhase4RenderSmoke.sh
```

If `Syphon.framework` is not installed, the script should report `SKIPPED`
rather than a fake pass.

## One-Shot Thin Fork Check

```bash
tools/immersive/check_thin_fork.sh
```

## Known Fragile Points

- `CMakeLists.txt`: feature flags and source filtering can drift during
  upstream build-system changes.
- `src/main.cpp`: command-line help, parsing order and local API lifecycle are
  sensitive to upstream startup/shutdown changes.
- `src/modes/world.cpp`: player-controller creation is a narrow hook and should
  remain runtime-gated.
- `src/race/race_manager.cpp`: world selection is a narrow hook and should
  remain runtime-gated.
- `src/states_screens/kart_selection.cpp`: arcade UI bypass should remain
  runtime-gated behind `RelaySessionController::isArcadeUIEnabled()`.
- `src/states_screens/track_info_screen.cpp`: hidden/locked race settings must
  apply only when relay runtime or arcade UI is enabled.
