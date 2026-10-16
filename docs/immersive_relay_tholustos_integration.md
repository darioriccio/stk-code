# Immersive Relay TholusOS Integration Contract

Phase 5.3 freezes the STK-side contract for TholusOS integration. TholusOS is
the operator/session/phone authority. SuperTuxKart remains the local gameplay
engine, render engine, Syphon publisher, and trusted local API provider.

## MVP update — 2026-10-07

The operator flow now supports **1–6 human pilots**, sharing one physical kart,
with AI opponents. Phones are optional even when `crew_mode` is `enabled`.
In solo play the optional p1 phone has support roles; with multiple pilots the
crew roles rotate between participants. `crew_mode: disabled` publishes no phone
actions and rejects crew commands. Phones provide crew actions, not steering.

`GET /tracks` returns installed MVP tracks as
`{"ok":true,"tracks":[{"id":"snowtuxpeak","name":"Snow Peak"}, ...]}`.
The preferred IDs are `snowtuxpeak`, `lighthouse`, and `olivermath`; missing assets
are omitted. Configure validates track, kart, difficulty and AI count before
changing the session. Difficulty accepts `easy`, `medium`, `hard`, or `best`.

`POST /operator/configure` now loads a real world on the game thread before
returning success. `loaded_track_id` is recorded when that world attaches;
`world_elapsed_ms` records the world clock, separately from session metadata.
Reset and start from results reset the real world. `--relay-operator-controlled`
holds physics and input during preparation/briefing/countdown/results. Countdown
uses update time while the race clock is held. Start enters briefing; Countdown
releases the race after the countdown. Natural finish publishes results when the
last lap crosses the finish line, including when vanilla already marks the kart
finished. Rendering remains live while held.

Game mutations are queued from the socket thread and drained between game frames.
Scene loading has a 30-second server timeout; clients should allow 40 seconds for
configure/reset. A timeout asks the client to refresh state before retrying.
`ENABLE_RELAY_TEST_TOOLS=OFF` is the MVP build default; no simulated progress is
used by the production launcher.

Build with `tools/immersive/BuildRelayMVP.sh`, then launch through
`tools/immersive/RunRelayTholusOS.command` (default one pilot, three AI karts,
OpenGL and Syphon safe 1536/768 preset). The launcher resolves workspace symlinks and uses
`exec`, allowing TholusOS to own exactly the process it starts. It accepts
`RELAY_API_TOKEN`, `RELAY_API_PORT`, `RELAY_PLAYER_COUNT`, `RELAY_BUILD_DIR`, and
`SUPERTUXKART_ASSETS_DIR`. TholusOS can configure the launcher path with its UI,
UserDefaults `tholus.stk.relayLauncherPath`, or `STK_RELAY_LAUNCHER`. Automatic
launch is limited to loopback; an already reachable producer remains externally
owned. The single shared kart accepts configured keyboard/gamepad devices;
gamepad events remain enabled while TholusOS is foreground. Keyboard play
requires focusing the STK window. The Syphon server uses app name `SuperTuxKart` and server name
`SuperTuxKart Immersive Relay Domemaster`.

`tools/immersive/ProbeRelayMVP.py` exercises an independently launched producer
on a spare port, loading all three real tracks and checking preparation,
countdown, pause, completion and replay. It does not certify driving, venue
hardware or a natural full-race playtest. Historical fixtures below remain
captures of the earlier contract, not evidence of this implementation.

## SC1 source extension — 2026-10-14

The opt-in authenticated `/spatial/*` namespace implements `tholus.stk-spatial/1`
with canonical `tholus.spatial-input/1` records, independent reticles, measured
presentation correlation and historical depth picking. The host uses the common
Spatial Input service and approved SI5 endpoint; no STK tracking driver exists.
[SC1 implementation and API](immersive/scintilla/sc1-2026-10-14/README.md)
documents admission, lifecycle, limits and explicit preparation. The
[15 October source remediation](immersive/scintilla/sc1-remediation-2026-10-15/README.md)
adds actual `/spatial/history` admission, renewable operational resource-loss
handling, bounded replay protection and separate visual outcome feedback.
The local HTTP worker now multiplexes bounded nonblocking clients and uses
asynchronous game-queue tickets; strict framing/authentication, absolute deadlines
and worker-owned socket shutdown protect the renewal/control path.
This extension
has source/math evidence only: builds, native regressions and physical/runtime
qualification were not executed. SC2 gameplay effects are not implemented.

## Base URL and Auth

Default base URL:

```text
http://127.0.0.1:8890/immersive/v1
```

All calls require either:

```http
X-STK-Relay-Token: immersive-dev-token
Authorization: Bearer immersive-dev-token
```

The API is a trusted local API. Default bind is `127.0.0.1`; phones must not
call STK directly. Phones talk to TholusOS, TholusOS talks to STK, and Syphon is
video only from STK to TholusOS.

## Frozen Endpoints

```text
GET  /state
GET  /operator/state
GET  /capabilities
GET  /tracks
POST /operator/configure
POST /operator/start
POST /operator/countdown
POST /operator/pause
POST /operator/resume
POST /operator/abort
POST /operator/reset
POST /operator/force-finish
POST /operator/handoff/confirm
POST /operator/render-preset
POST /render/config
POST /crew/action
```

The implementation still has a few legacy/dev endpoints such as `/session/*`,
`/operator/handoff`, `/operator/next-state`, and `/test/progress`. TholusOS
should not depend on those for production control.

## Capabilities

`GET /immersive/v1/capabilities` lets TholusOS discover outputs, projections,
render profiles, policy, endpoint availability, and local security assumptions.
The Syphon build reports `supported_outputs` including `syphon`; non-Syphon
builds report Syphon as unavailable in `output_adapters.syphon.status`.

See `docs/fixtures/tholustos_integration/capabilities.json` for a captured
Syphon-build response.

## Safe Render Preset

The provisional TholusOS venue preset is:

```json
{
  "render_profile": "fulldome_safe",
  "projection": "fisheye_domemaster",
  "output": "syphon",
  "domemaster_size": 1536,
  "cubemap_size": 768,
  "camera": {
    "mode": "stabilized_chase",
    "pitch_deg": 35,
    "forward_offset": 0.25,
    "vertical_offset": 0.4,
    "smoothing_factor": 0.16,
    "horizon_stabilization": true,
    "roll_limit_deg": 12
  },
  "syphon_server_name": "SuperTuxKart Immersive Relay Domemaster"
}
```

`target`/2048 remains available for explicit testing, but it is not the venue
default until the Mac mini M1 base benchmark accepts it.

## Session Configure

The frozen 6-player TholusOS request is:

```json
{
  "player_count": 6,
  "track_id": "snowtuxpeak",
  "kart_id": "tux",
  "difficulty": "easy",
  "ai_karts": 5,
  "lap_policy": "player_count_based",
  "handoff_policy": "lap_based_handoff",
  "crew_mode": "enabled",
  "encounter_script": "snowtuxpeak_relay_encounters",
  "render_profile": "fulldome_safe",
  "output": "syphon",
  "projection": "fisheye_domemaster",
  "operator_mode": "arcade",
  "allow_pause": true,
  "allow_force_finish": true,
  "allow_abort": true
}
```

Expected state after configure: `configured=true`, `player_count=6`,
`total_laps=12`, `handoff_policy=lap_based_handoff`,
`render_profile=fulldome_safe`, `projection=fisheye_domemaster`,
and `output=syphon`.

## Crew Action Forwarding

Canonical payload:

```json
{
  "session_id": "relay-local",
  "player_id": "p2",
  "role": "navigator",
  "type": "nav_select_route",
  "action_id": "phone-generated-action-id",
  "idempotency_key": "uuid",
  "state_version_seen": 123,
  "payload": {
    "route_id": "ridge_left"
  }
}
```

Compatibility accepted for TholusOS Codable tests:

- `state_version` is accepted as an alias for `state_version_seen`.
- numeric `player_id` such as `2` is accepted as `p2`.
- missing `session_id` is filled from the active trusted local session.
- `action_id=navigator_mark_left_route` maps to the existing
  `nav_select_route` action with `route_id=ridge_left`.

Stable crew rejection reasons include `wrong_role`, `wrong_window`, `cooldown`,
`already_used`, `encounter_expired`, `stale_state`, `invalid_token`, and
`invalid_payload`.

## Official Startup Command

Use the Syphon build for TholusOS end-to-end tests:

```bash
cmake_build_phase45_syphon/bin/supertuxkart.app/Contents/MacOS/supertuxkart \
  --race-now \
  --track=snowtuxpeak \
  --kart=tux \
  --numkarts=6 \
  --difficulty=0 \
  --unlock-all \
  --immersive-relay \
  --relay-local-api \
  --relay-api-bind=127.0.0.1 \
  --relay-api-port=8890 \
  --relay-api-token=immersive-dev-token \
  --relay-player-count=6 \
  --relay-handoff-policy=lap_based_handoff \
  --immersive-output=syphon \
  --immersive-projection=fisheye_domemaster \
  --immersive-domemaster-size=1536 \
  --immersive-cubemap-size=768 \
  --immersive-camera=stabilized_chase \
  --immersive-pitch-deg=35 \
  --immersive-forward-offset=0.25 \
  --immersive-vertical-offset=0.40 \
  --immersive-smoothing-factor=0.16 \
  --immersive-horizon-stabilization=on \
  --immersive-roll-limit-deg=12 \
  --immersive-stats=on \
  --syphon-server-name="SuperTuxKart Immersive Relay Domemaster"
```

The startup race gives STK a live render context. TholusOS test setup should
call `POST /operator/reset`, then configure/start/countdown through the frozen
operator endpoints.

## Smoke

API-only:

```bash
RUN_SYPHON=0 tools/immersive/RunRelayTholusIntegrationSmoke.sh
```

Full Syphon:

```bash
RUN_SYPHON=1 RUN_SYPHON_RECEIVER=1 tools/immersive/RunRelayTholusIntegrationSmoke.sh
```

The smoke verifies capabilities, state, safe render config, 6-player configure,
start/countdown, crew forwarding, score growth, optional Syphon 1536x1536
publish, reset, and a final readable report.


## Cooperative MVP extension — 7 October 2026

These additive source changes are implemented and reviewed; they have not been
compiled or exercised in this delivery. Prior build logs do not qualify them.
The consumer flow uses `tools/immersive/RunRelayTholusOS.command`, one human by
default, the three installed MVP tracks, and Syphon `fulldome_safe`.

Capabilities include `feature_flags`: `crew_presence`, `pilot_ready`,
`continuous_crew_actions`, `input_devices`. TholusOS rejects older producers
with an explicit missing-feature diagnostic before configuring a session.
All endpoints below use the existing authenticated loopback API; mutations
are queued onto the main loop. Header values must match the entire token,
case-sensitively; body text or partial token matches never authenticate.

| Method /immersive/v1 path | Request / response |
| --- | --- |
| GET `/karts` | `ok`, installed `karts:[{id,name}]` |
| POST `/crew/presence` | `session_id`, `connected_player_ids:["p1",…]`; unknown or duplicate IDs rejected |
| POST `/operator/ready` | Optional `player_id`; must be the expected pilot when present |
| GET `/input/devices` | `devices:[{id,name,kind,connected,axis_count,button_count}]`, `selected_device_id` |
| GET `/input/state` | `configuration`, `connected`, `recovery_required`, raw `axes`, `buttons`, normalized `steering`, `accelerator`, `brake` |
| POST `/input/config` | Input configuration; returns the effective input state. `/input/configure` is an alias. |

Input configuration fields: `device_id` (`keyboard` or `sdl:<GUID>:<ordinal>`),
`steering_axis`, `accelerator_axis`, `brake_axis`, `ready_button`,
`steering_inverted`, `accelerator_inverted`, `brake_inverted`,
`accelerator_full_range`, `brake_full_range`, `deadzone` (finite, 0–0.5).
Indices are zero-based. Steering cannot share a pedal axis; two pedals may
share an axis with half-range/inversion configured. Full-range pedals map the
whole SDL range to 0–1. Device selection/configuration requires pre-race or
paused state. The existing SDL/STK device ownership is reused.

`operator/configure` additionally accepts `handoff_seconds` (default 10),
`min_handoff_seconds` (default 6), `handoff_operator_confirm` (default true),
`auto_confirm_handoff` (default false), `music_enabled`, `sfx_enabled`,
`music_volume` (default .7), `sfx_volume` (default .8). Effective values are
published in `arcade_session.config` and verified by TholusOS.

Presence assigns all five roles to connected non-driver phones; a sole player
may receive all roles on an optional phone. A driver's phone in multiplayer
shows turn status. Presence changes never replenish action cooldowns.
Ordinary activities remain available between encounters. Turbo charge/release
works in every 1–6 configuration; an ordinary action cannot complete an
absent encounter. Encounter requirements include only available crew roles.
The gunner has a defensive decoy activity even with zero opponents.

Enter or the selected SDL button queues the same Ready command as the rightful
pilot's phone. On a handoff, confirmation waits at least six seconds, then a
three-second countdown; timeout never confirms automatically. Automatic
handoff is a separately selected option. Disconnect freezes the race; restart
commands reject `input_device_unavailable` until the selected input returns
or another device is explicitly applied. Ready from results resets and reloads
the same configuration between frames, then starts the next countdown.

State additions: `connected_player_ids`, `countdown_remaining_ms`,
`crew_feedback:{id,message}`. Feedback ID is session plus accepted action
idempotency key, stable across polling; repeated identical effects get distinct
IDs. TholusOS projects this authoritative state through existing MediaBus
text overlays and removes only its own elements when ownership ends.

Source regression suites are prepared in `ProbeRelayCooperativeMVP.py` and
`ProbeRelayInputMVP.py`, plus TholusOS unit test sources. They are deliberately
not executed in this phase. Future acceptance requires three complete track
races/replay, 1–6 participants with partial/no phones, input loss/recovery,
a real chosen wheel/pedals, six-person autonomous relay and dome projection.
See `immersive_relay_future_interactive_tracks.md` for the separate evolution.


## Native full-sphere equirectangular extension (2026-10-07)

This is a source implementation extension to the frozen domemaster contract.
`fisheye_domemaster`, all `fulldome_*` presets and the existing Syphon server
identity remain compatible. `equirectangular` is now an implemented projection
adapter: six rendered scene faces -> shared seam-aware GPU sampler -> 2:1 GPU
texture -> existing generic output adapter. No intermediate domemaster or CPU
readback is used for the equirectangular output.

Capability `status:production` denotes an implemented selectable adapter, not
build, runtime or venue acceptance. Equirectangular metadata separately reports
`venue_validation:pending`; source-only delivery has not compiled shaders, run
STK, received Syphon pixels or measured frame rate. The legacy `true_fulldome`
field continues to mean successful native scene capture/projection for the last
frame; it does not certify any venue geometry.

### Projection and dimensions

`POST /render/config` accepts existing flat fields and the nested contract:

```json
{
  "projection": {"type":"equirectangular", "width":2048, "height":1024, "debug_grid":false},
  "capture": {"type":"full_sphere_cubemap", "cubemap_size":768},
  "output": {"type":"syphon", "server_name":"SuperTuxKart Immersive Relay Domemaster"},
  "camera": {"mode":"stabilized_chase", "pitch_deg":0}
}
```

The adapter permits 2048x1024, 4096x2048 and 8192x4096. Unsupported or noninteger
dimensions, a nonpositive dimension and a non-2:1 request are rejected before
configuration mutation. Omit `domemaster_size` for an equirectangular request:
it is the legacy square override. Changing projection without explicit dimensions
selects its safe dimensions; changing camera projection selects a level pitch
for equirectangular or legacy 35 degrees for domemaster unless pitch is explicit.
`dome_tilt_deg` applies only to the domemaster shader. Camera yaw/pitch/roll retain
their camera-space meaning; the receiver owns room alignment.

Presets added: `equirectangular_debug` (2048x1024/512),
`equirectangular_safe` (2048x1024/768), `equirectangular_target`
(4096x2048/1024). `POST /operator/render-preset` accepts those names with
`projection:equirectangular`; generic/legacy quality names can also be used with
explicit equirectangular projection. Target and 8192-wide manual configuration
require later resource/performance qualification.

CLI options added: `--immersive-projection-width`, `--immersive-projection-height`.
The existing `--immersive-projection=equirectangular` accepts its `equirect` alias.
`RunRelayTholusOS.command` accepts optional `RELAY_PROJECTION`,
`RELAY_PROJECTION_WIDTH`, `RELAY_PROJECTION_HEIGHT`, `RELAY_CUBEMAP_SIZE`.
The launcher accepts equal square width/height 1024, 1536 or 2048 for domemaster.
Its defaults remain the safe domemaster; equirectangular defaults to 2048x1024/768
and zero camera pitch. A malformed environment configuration exits before launch.

Projection resources are created/replaced on the game/render thread; HTTP POST
uses the existing main-thread dispatcher. GET reads immutable copies under a
short mutex, not live mutable render state. The response marks
`application:pending_render_frame`: actual dimensions/output/failure are read
from subsequent `/state.immersive_render`. `configuration_applied` is false
after a request and true only when a frame of that configuration has published
successfully; pending dimensions cannot authorize play. The receiver verifies fresh
texture dimensions before authorizing play. Allocation failure retains old GPU
resources where possible and restores the previous applied configuration (or
disables output when none has applied). It does not publish an old texture under
the new format. Reconfiguration during an unpaused live race/countdown/handoff
returns `render_reconfiguration_requires_pause`.

### Top-left texture convention

The published source uses +Z forward at UV(0.5,0.5), +X right at UV(0.75,0.5),
-X left at UV(0.25,0.5), -Z behind at the horizontal seam, +Y zenith at the top
and -Y nadir at the bottom. The GLSL fragment uses bottom-left `gl_FragCoord`,
so its vertical coordinate is `vGL=1-vTop`. The unchanged Syphon adapter publishes
with `flipped:NO`; receivers retain the existing texture-origin conversion and
apply the stated top-left convention exactly once. Debug grid cardinal colours:
front red, right green, left magenta, behind orange, zenith cyan, nadir yellow.
The horizontal seam belongs behind the pilot.

### Presentation readiness gate

Capability flag `presentation_readiness_gate` advertises the optional
`POST /render/readiness` route with strict JSON `{"ready":false}` or
`{"ready":true}`. The corresponding `/state` and `/operator/state` field is
`presentation_ready`. Standalone sessions default to true. Setting false freezes
world/human input and countdown without deleting race, players or score, and
rejects pilot Ready (keyboard, SDL and phone), start, countdown, resume and
handoff confirmation with `presentation_not_ready`. Pause/abort/reset remain
available; resets/configuration do not silently reopen the gate.

`--relay-presentation-required` closes the gate during startup, before local pilot
inputs can run. TholusOS-owned launch sets `RELAY_PRESENTATION_REQUIRED=1` for
`RunRelayTholusOS.command`, which emits this flag. Standalone launcher defaults
remain 0/true. Camera capture, projection and frame publication continue while
the gate is closed, so receiving the first validated frame does not deadlock.

TholusOS sets false before preparing/changing presentation and true only after
producer format, fresh owned Syphon frame, selected mappings and HUD placement
have been checked. It pauses a live race before changing projection/surfaces and
requires preparation before resume. STK does not infer venue readiness from
projection type or server name.

### Verification recorded for this source delivery

`python3 tools/immersive/CheckRelayProjectionSource.py` passes five mathematical
and static wiring tests: actual GLSL cardinal mapping, 4,000 independent receiver
round trips, seam/poles, coverage/orientation across 7,381 sphere positions and
readiness/thread wiring. Launcher `zsh -n`, Python AST and `git diff --check`
pass. No build/compiler/graphics/runtime/network probe was executed.

`ProbeRelayProjectionMVP.py --execute` is prepared for a later explicitly
authorized, already running test producer. It checks gate rejection and invalid
dimension preservation, restoring the previous gate value. Shader compilation,
GL allocation/resize failure injection, live Syphon ownership/reconnection,
physical orientation and performance remain acceptance checks for that phase.

## SC4 additive source contract — 16 October 2026

Missione Scintilla SC4 adds `scintilla_mission_sc4`, the authenticated
`/immersive/v1/scintilla/mission/` namespace and host mode `missionSC4`.
It requires the existing SC1/SC2/SC3 authorities and the exact conditional
`front_rear_cargo_v2`/2 layout; non-SC4 sessions retain their original contracts.
Wide mission identities remain canonical decimal strings. The official natural
finish freezes rank and secured cargo before report publication; crew score and
beacons do not determine mission victory.

The [SC4 contract](immersive/scintilla/sc4-implementation-2026-10-16/SC4_API.md)
specifies intent/prepare/state, correlated prerecorded intro completion,
diagnostic capture and qualification evidence. The
[implementation report](immersive/scintilla/sc4-implementation-2026-10-16/README.md)
separates source/offline checks from pending executable acceptance.
No SC4 build, native/GPU execution or real anchor qualification was run for this
delivery. Development anchors remain unqualified and public readiness stays
closed until matching native evidence and ordinary preparation are available.
