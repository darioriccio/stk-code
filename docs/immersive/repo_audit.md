# SuperTuxKart Immersive Relay - Phase 0 Repo Audit

Date: 2026-06-08

> **Checkpoint 2026-10-13:** the [Missione Scintilla SC0 baseline](scintilla/sc0-2026-10-13/README.md)
> maps the current local checkout, shared Spatial Input, native equirectangular
> source implementation, proposed powerup policy and open frame/hardware gates.
> The audit below remains historical; its build results do not qualify that
> checkpoint. SC1 had not started at that historical checkpoint.
>
> **Checkpoint 2026-10-14:** [SC1 source implementation](scintilla/sc1-2026-10-14/README.md)
> adds canonical input, calibrated host mapping, independent reticles and
> historical presented-frame/depth resolution. No build or runtime qualification
> was executed; SC2 has not started.

## Scope

This audit was performed for the SuperTuxKart Immersive Relay MVP. The
workspace initially contained only the agent instructions and the master spec,
not a SuperTuxKart source checkout. To unblock Phase 0, upstream vanilla
SuperTuxKart was cloned into:

```text
stk-code/
```

Source baseline:

```text
remote: https://github.com/supertuxkart/stk-code.git
branch: master
commit: c1ab979
```

Release alignment update:

```text
release: SuperTuxKart 1.5
branch: immersive-relay-1.5
tag: 1.5
commit: 1fb491f
official release tarball: downloads/SuperTuxKart-1.5-src.tar.gz
release tarball sha256: 33cf8841e4ff4082d80b9248014295bbbea61d14683e86dff100e3ab8f7b27cb
asset layout: ../stk-assets sibling directory
```

The original Phase 0 audit started from a shallow upstream `master` clone
because the workspace did not contain a source checkout. The local working tree
has now been moved to the latest stable release tag, `1.5`, on branch
`immersive-relay-1.5`. The release source tarball was downloaded from the
official GitHub release, verified by SHA-256, extracted into `release-cache/`,
and its complete `data/` tree was copied into `../stk-assets`.

The master spec was copied into the repository as required:

```text
docs/supertuxkart_immersive_relay_master_spec.md
```

Required files read before inspection:

```text
docs/supertuxkart_immersive_relay_master_spec.md
README.md
INSTALL.md
CMakeLists.txt
sources.cmake
```

## Initial vanilla build result

Configure command:

```bash
cmake -S . -B cmake_build_vanilla \
  -DCHECK_ASSETS=off \
  -DBUILD_RECORDER=off \
  -DNO_SHADERC=on \
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt \
  -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@1.1
```

Build command:

```bash
cmake --build cmake_build_vanilla --parallel 8
```

Result:

```text
[100%] Built target supertuxkart
```

Produced executable:

```text
cmake_build_vanilla/bin/supertuxkart.app/Contents/MacOS/supertuxkart
```

Smoke command:

```bash
cmake_build_vanilla/bin/supertuxkart.app/Contents/MacOS/supertuxkart --help
```

Result:

```text
exit code 0
usage/help printed successfully
```

Important limit of the initial build: a full vanilla race runtime smoke test was
not completed at first because the checkout did not include the full asset tree.
That limit is resolved by the 1.5 release alignment below.

## Release 1.5 complete local build

Official release verification:

```text
GitHub latest non-prerelease: 1.5
published: 2025-10-20T16:48:01Z
tarball: SuperTuxKart-1.5-src.tar.gz
expected sha256: 33cf8841e4ff4082d80b9248014295bbbea61d14683e86dff100e3ab8f7b27cb
actual sha256:   33cf8841e4ff4082d80b9248014295bbbea61d14683e86dff100e3ab8f7b27cb
```

Local release directories:

```text
downloads/SuperTuxKart-1.5-src.tar.gz
release-cache/SuperTuxKart-1.5-src/
stk-assets/
stk-code/
```

Critical asset directories present:

```text
stk-assets/karts
stk-assets/library
stk-assets/models
stk-assets/music
stk-assets/sfx
stk-assets/textures
stk-assets/tracks
```

Configure command:

```bash
cmake -S . -B cmake_build_1_5 \
  -DBUILD_RECORDER=off \
  -DNO_SHADERC=on \
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt \
  -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@1.1
```

Configure result:

```text
Assets found
Build files written to cmake_build_1_5
```

Build command:

```bash
cmake --build cmake_build_1_5 --parallel 8
```

Build result:

```text
[100%] Built target supertuxkart
```

Produced executable:

```text
cmake_build_1_5/bin/supertuxkart.app/Contents/MacOS/supertuxkart
```

Version smoke:

```bash
cmake_build_1_5/bin/supertuxkart.app/Contents/MacOS/supertuxkart --version
```

Result:

```text
SuperTuxKart, 1.5.
```

Headless vanilla race smoke:

```bash
cmake_build_1_5/bin/supertuxkart.app/Contents/MacOS/supertuxkart \
  --no-graphics \
  --profile-time=3 \
  --track=snowtuxpeak \
  --kart=tux \
  --numkarts=1 \
  --difficulty=0 \
  --laps=1 \
  --unlock-all
```

Result:

```text
exit code 0
track loaded: snowtuxpeak
kart loaded: tux
asset roots loaded from build app data plus ../../stk-assets
profile output completed
```

## Build system observations

Relevant files:

```text
CMakeLists.txt
sources.cmake
```

Findings:

- `sources.cmake` recursively collects `src/*.hpp` and `src/*.cpp`, so new
  source files under `src/immersive`, `src/modes/immersive_relay`, and
  `src/remote` will be discovered automatically.
- Build flags still need explicit CMake options and compile definitions in
  `CMakeLists.txt`. Source discovery alone is not enough to preserve vanilla
  behavior.
- macOS dependency discovery worked with explicit Homebrew hints. Without the
  explicit `CMAKE_PREFIX_PATH` and `OPENSSL_ROOT_DIR`, this machine may not
  configure reproducibly.

Recommended initial feature options:

```text
ENABLE_IMMERSIVE_RELAY
ENABLE_RELAY_LOCAL_API
ENABLE_SYphon_OUTPUT
ENABLE_SPOUT_OUTPUT
ENABLE_RELAY_TEST_TOOLS
```

Default all new options to off except test-only options in local developer
presets. With all options off, the binary should remain vanilla.

## Camera map

Core files:

```text
src/graphics/camera/camera.hpp
src/graphics/camera/camera.cpp
src/graphics/camera/camera_normal.hpp
src/graphics/camera/camera_normal.cpp
```

Key points:

- `src/graphics/camera/camera.hpp:42` defines base `Camera`.
- `src/graphics/camera/camera.hpp:51` defines `CameraType`.
- `src/graphics/camera/camera.hpp:65` defines normal/reverse/spectator modes.
- `src/graphics/camera/camera.hpp:122` stores the global camera list.
- `src/graphics/camera/camera.hpp:127` stores the Irrlicht camera scene node.
- `src/graphics/camera/camera.hpp:133` stores the attached kart.
- `src/graphics/camera/camera.hpp:195` exposes `setKart`.
- `src/graphics/camera/camera.hpp:202` exposes `setInitialTransform`.
- `src/graphics/camera/camera.hpp:212` exposes `activate`.
- `src/graphics/camera/camera.hpp:232` exposes `update`.
- `src/graphics/camera/camera.hpp:239` exposes `getCameraSceneNode`.
- `src/graphics/camera/camera.hpp:247` exposes `setupCamera`.
- `src/graphics/camera/camera.cpp:56` creates cameras through
  `Camera::createCamera(kart, index)`.
- `src/graphics/camera/camera.cpp:122` creates the Irrlicht camera node and
  applies initial FOV.
- `src/graphics/camera/camera.cpp:179` computes viewport, aspect ratio and far
  plane in `setupCamera`.
- `src/graphics/camera/camera.cpp:195` handles camera mode changes.
- `src/graphics/camera/camera.cpp:252` sets initial position, target, rotation
  and FOV.
- `src/graphics/camera/camera.cpp:276` performs base camera update and audio
  listener handling.
- `src/graphics/camera/camera.cpp:304` activates the camera and viewport.
- `src/graphics/camera/camera_normal.cpp:51` initializes normal camera
  smoothing and configuration.
- `src/graphics/camera/camera_normal.cpp:87` moves the chase camera from kart
  transform, speed and skid state.
- `src/graphics/camera/camera_normal.cpp:202` computes per-mode camera
  settings.
- `src/graphics/camera/camera_normal.cpp:323` updates the normal camera.
- `src/graphics/camera/camera_normal.cpp:380` positions the camera from wanted
  target and wanted position.

Camera hook plan:

- Do not modify `CameraNormal` for dome output.
- Add `ImmersiveCameraRig` under `src/immersive`.
- Reuse the active player kart transform, but apply relay-specific yaw, pitch,
  roll clamp, look-ahead, height offset and horizon stabilization in the new
  rig.
- Create offscreen render cameras or temporary camera transforms for projection
  passes. Keep player HUD camera behavior unchanged.

## Render map

Core files:

```text
src/graphics/abstract_renderer.hpp
src/graphics/irr_driver.hpp
src/graphics/irr_driver.cpp
src/graphics/shader_based_renderer.cpp
src/graphics/render_target.hpp
src/graphics/render_target.cpp
```

Key points:

- `src/graphics/abstract_renderer.hpp:101` defines
  `AbstractRenderer::render(float dt, bool is_loading)`.
- `src/graphics/abstract_renderer.hpp:110` defines renderer-owned render target
  creation.
- `src/graphics/irr_driver.hpp:99` owns `AbstractRenderer* m_renderer`.
- `src/graphics/irr_driver.hpp:267` exposes render target creation.
- `src/graphics/irr_driver.hpp:303` exposes the Irrlicht video driver.
- `src/graphics/irr_driver.hpp:311` exposes the scene manager.
- `src/graphics/irr_driver.cpp:2165` starts `IrrDriver::update`.
- `src/graphics/irr_driver.cpp:2193` updates world displacement when a world is
  loaded.
- `src/graphics/irr_driver.cpp:2198` calls `m_renderer->render(dt, is_loading)`.
- `src/graphics/irr_driver.cpp:2508` handles resize, camera setup and RTT
  reloads.
- `src/graphics/shader_based_renderer.cpp:109` computes matrices and camera
  state.
- `src/graphics/shader_based_renderer.cpp:224` starts deferred scene rendering.
- `src/graphics/shader_based_renderer.cpp:416` starts non-deferred scene
  rendering.
- `src/graphics/shader_based_renderer.cpp:545` starts post-processing to the
  active viewport.
- `src/graphics/shader_based_renderer.cpp:627` creates main RTTs on world load.
- `src/graphics/shader_based_renderer.cpp:714` starts the main render loop.
- `src/graphics/shader_based_renderer.cpp:734` loops over `Camera::getNumCameras`.
- `src/graphics/shader_based_renderer.cpp:746` activates each camera.
- `src/graphics/shader_based_renderer.cpp:750` computes matrices per camera.
- `src/graphics/shader_based_renderer.cpp:753` renders deferred scene views.
- `src/graphics/shader_based_renderer.cpp:770` runs post-processing per camera.
- `src/graphics/shader_based_renderer.cpp:795` renders player race GUI per view.
- `src/graphics/shader_based_renderer.cpp:807` renders global GUI.
- `src/graphics/shader_based_renderer.cpp:844` renders a camera to a
  `GL3RenderTarget`.
- `src/graphics/render_target.hpp:41` defines `RenderTarget`.
- `src/graphics/render_target.hpp:48` exposes `renderToTexture(camera, dt)`.
- `src/graphics/render_target.cpp:127` forwards GL render target draws to
  `ShaderBasedRenderer::renderToTexture`.

Render hook plan:

- Keep the normal `ShaderBasedRenderer::render` path intact for vanilla window
  output and UI.
- Add `ImmersiveRenderGraph` under `src/immersive`, enabled only by feature flag
  and runtime profile.
- Use existing `RenderTarget`/`ShaderBasedRenderer::renderToTexture` to render
  offscreen views for the MVP.
- Compose the fisheye/domemaster output in a separate projection pass before
  publishing to any output adapter.
- Do not place Syphon, Spout or NDI logic in renderer gameplay code. Publish a
  GL texture through `OutputAdapter`.
- Avoid GUI/HUD in the dome texture unless a relay cupola overlay is explicitly
  rendered by the immersive layer.

## Input and controller map

Core files:

```text
src/input/input_manager.cpp
src/karts/controller/player_controller.hpp
src/karts/controller/player_controller.cpp
src/karts/controller/local_player_controller.hpp
src/karts/controller/local_player_controller.cpp
src/modes/world.cpp
```

Key points:

- `src/input/input_manager.cpp:616` starts `InputManager::dispatchInput`.
- `src/input/input_manager.cpp:667` translates raw device input through the
  device manager.
- `src/input/input_manager.cpp:832` enters the in-game input path.
- `src/input/input_manager.cpp:844` resolves the active player's kart.
- `src/input/input_manager.cpp:854` sends translated actions to
  `controller->action(action, abs(value))`.
- `src/karts/controller/player_controller.hpp:27` stores steering and previous
  acceleration/brake state.
- `src/karts/controller/player_controller.hpp:43` exposes `update`.
- `src/karts/controller/player_controller.hpp:49` exposes `action`.
- `src/karts/controller/player_controller.cpp:78` resets input state.
- `src/karts/controller/player_controller.cpp:112` maps `PlayerAction` values
  into kart controls.
- `src/karts/controller/player_controller.cpp:268` computes steering.
- `src/karts/controller/player_controller.cpp:327` updates controls each tick.
- `src/karts/controller/local_player_controller.cpp:172` handles local action
  behavior, startup boost, dry run and network history before delegating.
- `src/karts/controller/local_player_controller.cpp:226` delegates steering.
- `src/karts/controller/local_player_controller.cpp:248` updates local player
  controller effects such as reverse camera and nitro sounds.
- `src/modes/world.cpp:491` starts `World::createKart`.
- `src/modes/world.cpp:532` creates `LocalPlayerController` for local players.
- `src/modes/world.cpp:565` assigns the controller to the kart.

Input hook plan:

- Do not change SDL/gamepad/wheel mapping for Phase 1.
- Add a relay-specific controller only when the relay mode is active, for
  example `RelayPlayerController : LocalPlayerController`.
- Hook controller creation at `World::createKart` behind a relay runtime check.
- Let wheel/pedals pass through normally when the pilot is active.
- During handoff/autopilot, suppress or ignore human driver actions inside the
  relay controller and apply safe control values from `RelayAutopilotHandoff`.
- Keep smartphone crew actions out of the continuous input pipeline. They should
  enter through `CrewActionHandler` as atomic commands.

## Race, world and checkpoint map

Core files:

```text
src/race/race_manager.hpp
src/race/race_manager.cpp
src/modes/standard_race.hpp
src/modes/standard_race.cpp
src/modes/linear_world.hpp
src/modes/linear_world.cpp
src/modes/world.hpp
src/modes/world.cpp
src/tracks/check_manager.hpp
src/tracks/check_manager.cpp
src/tracks/check_structure.hpp
src/tracks/check_structure.cpp
```

Key points:

- `src/race/race_manager.hpp:112` defines minor race mode types.
- `src/race/race_manager.hpp:720` exposes linear race mode checks.
- `src/race/race_manager.hpp:770` exposes lap support checks.
- `src/race/race_manager.cpp:645` starts world factory logic in
  `RaceManager::startNextRace`.
- `src/race/race_manager.cpp:653` creates `StandardRace` for normal/time trial.
- `src/modes/standard_race.hpp:21` defines `StandardRace : public LinearWorld`.
- `src/modes/world.cpp:155` starts `World::init`.
- `src/modes/world.cpp:191` loads `scripting.as` for the current track.
- `src/modes/world.cpp:935` moves a kart to a transform.
- `src/modes/world.cpp:957` resets check manager state after moving a kart.
- `src/modes/world.cpp:1024` starts `World::updateWorld`.
- `src/modes/linear_world.cpp:174` starts `LinearWorld::update`.
- `src/modes/linear_world.cpp:269` updates track sectors from kart position.
- `src/modes/linear_world.cpp:376` starts `LinearWorld::newLap`.
- `src/modes/linear_world.cpp:572` notifies controllers of a new lap.
- `src/tracks/check_structure.hpp:68` defines check types such as new lap,
  trigger, cannon and goal.
- `src/tracks/check_structure.cpp:111` updates check structures per kart.
- `src/tracks/check_structure.cpp:220` starts `CheckStructure::trigger`.
- `src/tracks/check_structure.cpp:225` calls `World::getWorld()->newLap`.
- `src/tracks/check_manager.hpp:45` exposes check manager load/update/reset.
- `src/tracks/check_manager.hpp:60` exposes `getChecklineTriggering`.
- `src/tracks/check_manager.cpp:181` updates all checks.
- `src/tracks/check_manager.cpp:219` finds triggered non-lap checklines between
  two check states.

Checkpoint and relay mode hook plan:

- Add `RelayRaceMode` under `src/modes/immersive_relay`.
- Prefer deriving from `StandardRace` first to preserve normal linear race
  behavior.
- In `RaceManager::startNextRace`, add a guarded branch that creates
  `RelayRaceMode` only when `ENABLE_IMMERSIVE_RELAY` is compiled and the relay
  runtime/session flag is active.
- Override or extend `newLap` in `RelayRaceMode` for lap-based relay events,
  always calling the base implementation.
- For segment handoffs, use either:
  - `CheckManager::getChecklineTriggering(previous, current)` for existing
    track checklines, or
  - relay track script/distance thresholds for tracks without authored segment
    markers.
- Keep handoff safety checks in `RelayPilotRotation` rather than in
  `CheckStructure`.
- Avoid modifying core check structure semantics for Phase 1.

## Track object and crew effect map

Core files:

```text
src/tracks/track_object.hpp
src/tracks/track_object.cpp
src/tracks/track_object_manager.hpp
src/tracks/track_object_manager.cpp
src/scriptengine/script_track.cpp
src/karts/kart.hpp
src/items/projectile_manager.hpp
```

Key points:

- Track scripts are loaded from `scripting.as` in `World::init`.
- `script_track.cpp` already exposes track object lookup and trigger creation
  to AngelScript.
- `TrackObject` supports enabling/disabling and moving objects.
- `Kart` already has gameplay primitives useful for MVP crew effects, including
  energy/nitro, zipper, speed and shield timing.
- Existing item/projectile systems can support early gunner and defender
  prototypes without inventing a new weapon stack.

Crew hook plan:

- Add `CrewActionHandler` and `CrewRoleManager` under
  `src/modes/immersive_relay`.
- MVP actions should map to existing primitives first:
  - Engineer: energy/nitro/zipper and temporary grip/stability modifiers.
  - Defender: shield timer and hit mitigation hooks.
  - Hacker: track object enable/disable/move by relay-authored IDs.
  - Gunner: projectile manager or target status modifiers.
  - Navigator: route markers, ghost line and hazard pings through relay overlay
    or track-script metadata.
- Require relay track metadata for object IDs, route IDs, hazards and support
  windows.

## Local API map

Relevant existing files:

```text
src/online/http_request.hpp
src/online/http_request.cpp
src/online/http_request_curl.cpp
src/network/*
src/io/rich_presence.cpp
```

Findings:

- Existing HTTP code is a client for online services, not a local REST server.
- Existing network code is game networking/lobby/ENet-oriented, not the local
  Swift app control API.
- There is no ready-made REST/WebSocket server abstraction for the required
  local API.

Local API hook plan:

- Add `LocalControlServer`, `RelayCommandRouter`, and `RelayStatePublisher`
  under `src/remote`.
- Bind to localhost by default.
- Run socket handling off the render thread.
- Queue validated commands onto the main/game thread before touching world,
  kart, renderer or track state.
- Require session token, idempotency keys, stale state rejection and per-role
  action rate limits from the first usable implementation.

## Minimal hook plan by phase

Phase 1:

```text
1. Add CMake options and compile definitions, default off.
2. Add RelaySessionController with arcade session states.
3. Add RelayRaceMode deriving from StandardRace.
4. Hook RaceManager::startNextRace with a guarded relay branch.
5. Add RelayPilotRotation and handoff state, initially using lap or distance
   thresholds.
6. Add RelayPlayerController or a narrow controller wrapper for input lockout
   during handoff.
7. Add LocalControlServer skeleton with start, state and reset endpoints only.
8. Add relay telemetry logging.
```

Phase 2:

```text
1. Add CrewRoleManager and CrewActionHandler.
2. Implement action validation/reject reasons.
3. Map role actions to existing kart/track primitives.
4. Add fake local API/phone clients.
5. Add focused tests for accepted/rejected crew actions.
```

Phase 3:

```text
1. Add CrewEventDirector.
2. Add relay track script format and standard 7-minute event schedule.
3. Add score/combo/idle-time telemetry.
```

Phase 4:

```text
1. Add ImmersiveCameraRig.
2. Add ProjectionAdapter and FisheyeDomemasterProjection.
3. Add ImmersiveRenderGraph with offscreen render targets.
4. Add OutputAdapter and SyphonOutputAdapter_macOS.
5. Add WindowOutputAdapter and NullOutputAdapter for fallback/testing.
```

## Real risks found

- The original workspace did not contain the actual STK repository. The current
  `stk-code` directory is an upstream vanilla clone, now switched to a local
  branch from release tag `1.5`.
- Full runtime assets are now present in `../stk-assets` and a headless vanilla
  race smoke test passes. The source of truth for assets is currently the
  verified 1.5 release tarball, not an SVN working copy, because `svn` is not
  installed on this machine.
- macOS dependency discovery is sensitive to Homebrew paths on this machine.
  Reproducible build docs or presets are needed.
- The main renderer has a usable render-to-texture path, but MVP dome output
  will likely require six offscreen views plus fisheye composition. That is a
  real M1 performance risk at 2048x2048.
- `RenderTarget` does not expose a generic output texture handle. Syphon will
  likely need a narrow GL texture access point or an immersive-owned FBO.
- There is no existing local REST/WebSocket server. Adding it safely without
  blocking render/game threads is non-trivial.
- Checkpoint structures are track-authored. Existing tracks may not contain
  suitable segment or safe handoff markers.
- Hackable gates, ramps and barriers require relay-authored track object IDs.
  Vanilla tracks will need metadata or relay variants before hacker actions can
  feel material.
- `CameraNormal` follows kart motion in ways that may be uncomfortable in a
  dome. Immersive camera must clamp roll and stabilize horizon independently.
- GPL and dependency compatibility must be reviewed before adding Syphon, Spout,
  NDI or any HTTP/WebSocket library.

## Next recommended step

Proceed with Phase 1 on branch `immersive-relay-1.5`: add relay feature flags
and a no-op `RelayRaceMode` skeleton that preserves normal behavior with all
immersive options disabled.
